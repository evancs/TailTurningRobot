/*****************************************************************
  MobileXbeeSensorNode.ino //maybe change in branch // second change
  SFE_MPU9150 Library AHRS Data Fusion Example Code
  Kris Winer for Sparkfun Electronics
  Original Creation Date: April 8, 2014
  https://github.com/sparkfun/MPU9150_Breakout

  The MPU9150 is a versatile 9DOF sensor. It has a built-in
  accelerometer, gyroscope, and magnetometer that
  functions over I2C. It is very similar to the 6 DoF MPU6050 for which an extensive library has already been built.
  Most of the function of the MPU9150 can utilize the MPU6050 library.

  This Arduino sketch utilizes Jeff Rowberg's MPU6050 library to generate the basic sensor data
  for use in two sensor fusion algorithms becoming increasingly popular with DIY quadcopter and robotics engineers.
  I have added and slightly modified Jeff's library here.

  This simple sketch will demo the following:
  How to create a MPU6050 object, using a constructor (global variables section).
  How to use the initialize() function of the MPU6050 class.
  How to read the gyroscope, accelerometer, and magnetometer
  using the readAcceleration(), readRotation(), and readMag() functions and the
  gx, gy, gz, ax, ay, az, mx, my, and mz variables.
  How to calculate actual acceleration, rotation speed, magnetic
  field strength using the  specified ranges as described in the data sheet:
  http://dlnmh9ip6v2uc.cloudfront.net/datasheets/Sensors/IMU/PS-MPU-9150A.pdf
  and
  http://dlnmh9ip6v2uc.cloudfront.net/datasheets/Sensors/IMU/RM-MPU-9150A-00.pdf.

  In addition, the sketch will demo:
  How to check for data updates using the data ready status register
  How to display output at a rate different from the sensor data update and fusion filter update rates
  How to specify the accelerometer and gyro sampling and bandwidth rates
  How to use the data from the MPU9150 to fuse the sensor data into a quaternion representation of the sensor frame
  orientation relative to a fixed Earth frame providing absolute orientation information for subsequent use.
  An example of how to use the quaternion data to generate standard aircraft orientation data in the form of
  Tait-Bryan angles representing the sensor yaw, pitch, and roll angles suitable for any vehicle stablization control application.

  Hardware setup: This library supports communicating with the
  MPU9150 over I2C. These are the only connections that need to be made:
	MPU9150 --------- Arduino
	 SCL ---------- SCL (A5 on older 'Duinos')
	 SDA ---------- SDA (A4 on older 'Duinos')
	 VDD ------------- 3.3V
	 GND ------------- GND

  The MPU9150 has a maximum voltage of 3.5V. Make sure you power it
  off the 3.3V rail! And either use level shifters between SCL
  and SDA or just use a 3.3V Arduino Pro.

  Development environment specifics:
	IDE: Arduino 1.0.5
	Hardware Platform: Arduino Pro 3.3V/8MHz
	MPU9150 Breakout Version: 1.0

  This code is beerware. If you see me (or any other SparkFun
  employee) at the local, and you've found our code helpful, please
  buy us a round!

  Distributed as-is; no warranty is given.
*****************************************************************/

#include <Wire.h>
#include "I2Cdev.h"
#include "MPU6050_9Axis_MotionApps41.h"

#include "TinyGPS.h"
#include <SoftwareSerial.h>


//Addresses
//Add more IDs and MACs with more xbee nodes

//IDs
int xbeeID_BCast = 0;
int xbeeID_A = 1;
int xbeeID_B = 2;
int xbeeID_C = 3;
int xbeeID_D = 4;

//MACs
byte xbeeAdrs_BCast[] = {(byte)0x0, (byte)0x0, (byte)0x0, (byte)0x0, (byte)0x0, (byte)0x0, 0xFF, 0xFF};

byte xbeeAdrs_A[] = {(byte)0x0, 0x13, 0xA2, (byte)0x0, 0x41, 0x03, 0xB3, 0x70};
//byte xbeeAdrs_A[] = {(byte)0x0, 0x13, 0xA2, (byte)0x0, 0x41, 0x52, 0x78, 0xA0};
byte xbeeAdrs_B[] = {(byte)0x0, 0x13, 0xA2, (byte)0x0, 0x41, 0x52, 0x78, 0xB6};
byte xbeeAdrs_C[] = {(byte)0x0, 0x13, 0xA2, (byte)0x0, 0x41, 0x52, 0x78, 0xC4};
byte xbeeAdrs_D[] = {(byte)0x0, 0x13, 0xA2, (byte)0x0, 0x41, 0x03, 0xB3, 0x66};



// Declare device MPU6050 class
MPU6050 mpu;

// The TinyGPS++ object
TinyGPS gps;

// global constants for 9 DoF fusion and AHRS (Attitude and Heading Reference System)
#define GyroMeasError PI * (40.0f / 180.0f)       // gyroscope measurement error in rads/s (shown as 3 deg/s)
#define GyroMeasDrift PI * (0.0f / 180.0f)      // gyroscope measurement drift in rad/s/s (shown as 0.0 deg/s/s)
// There is a tradeoff in the beta parameter between accuracy and response speed.
// In the original Madgwick study, beta of 0.041 (corresponding to GyroMeasError of 2.7 degrees/s) was found to give optimal accuracy.
// However, with this value, the LSM9SD0 response time is about 10 seconds to a stable initial quaternion.
// Subsequent changes also require a longish lag time to a stable output, not fast enough for a quadcopter or robot car!
// By increasing beta (GyroMeasError) by about a factor of fifteen, the response time constant is reduced to ~2 sec
// I haven't noticed any reduction in solution accuracy. This is essentially the I coefficient in a PID control sense;
// the bigger the feedback coefficient, the faster the solution converges, usually at the expense of accuracy.
// In any case, this is the free parameter in the Madgwick filtering and fusion scheme.
#define beta sqrt(3.0f / 4.0f) * GyroMeasError   // compute beta
#define zeta sqrt(3.0f / 4.0f) * GyroMeasDrift   // compute zeta, the other free parameter in the Madgwick scheme usually set to a small or zero value
#define Kp 2.0f * 5.0f // these are the free parameters in the Mahony filter and fusion scheme, Kp for proportional feedback, Ki for integral
#define Ki 0.0f

//const int GPS_ONOFFPin = A3;
//const int GPS_SYSONPin = A2;

//const uint32_t GPSBaud = 9600;

int16_t a1, a2, a3, g1, g2, g3, m1, m2, m3;     // raw data arrays reading
uint16_t count = 0;  // used to control display output rate
uint16_t delt_t = 0; // used to control display output rate
uint16_t mcount = 0; // used to control display output rate
uint8_t MagRate;     // read rate for magnetometer data

float pitch, yaw, roll;
float deltat = 0.0f;        // integration interval for both filter schemes
uint16_t lastUpdate = 0; // used to calculate integration interval
uint16_t now = 0;        // used to calculate integration interval

float ax, ay, az, gx, gy, gz, mxraw, myraw, mzraw; // variables to hold latest sensor data values
float q[4] = {1.0f, 0.0f, 0.0f, 0.0f};    // vector to hold quaternion
float eInt[3] = {0.0f, 0.0f, 0.0f};       // vector to hold integral error for Mahony method


const int RXPin = 12, TXPin = 13;
SoftwareSerial ss(RXPin, TXPin);

//Variables for Send Side
float  gyrofSend[] = {yaw, pitch, roll}; //float datatype coming from gyro
long  gyroiSend[] = {0, 0, 0}; // unsigned long datatype that will be gyro x 1000

const int array_size = 6; //Match this with size of data and receive code
int datapacket[array_size];


void setup()
{
  Serial.begin(9600); // Start serial at 38400 bps
  ss.begin(9600);


  // initialize MPU6050 device
  Serial.println(F("Initializing I2C devices..."));
  mpu.initialize();

  // verify connection
  Serial.println(F("Testing device connections..."));
  Serial.println(mpu.testConnection() ? F("MPU9150 connection successful") : F("MPU9150 connection failed"));

  // Set up the accelerometer, gyro, and magnetometer for data output

  mpu.setRate(7); // set gyro rate to 8 kHz/(1 * rate) shows 1 kHz, accelerometer ODR is fixed at 1 KHz

  MagRate = 10; // set magnetometer read rate in Hz; 10 to 100 (max) Hz are reasonable values

  // Digital low pass filter configuration.
  // It also determines the internal sampling rate used by the device as shown in the table below.
  // The accelerometer output rate is fixed at 1kHz. This means that for a Sample
  // Rate greater than 1kHz, the same accelerometer sample may be output to the
  // FIFO, DMP, and sensor registers more than once.
  /*
              |   ACCELEROMETER    |           GYROSCOPE
     DLPF_CFG | Bandwidth | Delay  | Bandwidth | Delay  | Sample Rate
     ---------+-----------+--------+-----------+--------+-------------
     0        | 260Hz     | 0ms    | 256Hz     | 0.98ms | 8kHz
     1        | 184Hz     | 2.0ms  | 188Hz     | 1.9ms  | 1kHz
     2        | 94Hz      | 3.0ms  | 98Hz      | 2.8ms  | 1kHz
     3        | 44Hz      | 4.9ms  | 42Hz      | 4.8ms  | 1kHz
     4        | 21Hz      | 8.5ms  | 20Hz      | 8.3ms  | 1kHz
     5        | 10Hz      | 13.8ms | 10Hz      | 13.4ms | 1kHz
     6        | 5Hz       | 19.0ms | 5Hz       | 18.6ms | 1kHz
  */
  mpu.setDLPFMode(4); // set bandwidth of both gyro and accelerometer to ~20 Hz

  // Full-scale range of the gyro sensors:
  // 0 = +/- 250 degrees/sec, 1 = +/- 500 degrees/sec, 2 = +/- 1000 degrees/sec, 3 = +/- 2000 degrees/sec
  mpu.setFullScaleGyroRange(0); // set gyro range to 250 degrees/sec

  // Full-scale accelerometer range.
  // The full-scale range of the accelerometer: 0 = +/- 2g, 1 = +/- 4g, 2 = +/- 8g, 3 = +/- 16g
  mpu.setFullScaleAccelRange(0); // set accelerometer to 2 g range

  mpu.setIntDataReadyEnabled(true); // enable data ready interrupt

}



void loop()
{

  if (mpu.getIntDataReadyStatus() == 1) { // wait for data ready status register to update all data registers
    mcount++;
    // read the raw sensor data
    mpu.getAcceleration  ( &a1, &a2, &a3  );
    ax = a1 * 2.0f / 32768.0f; // 2 g full range for accelerometer
    ay = a2 * 2.0f / 32768.0f;
    az = a3 * 2.0f / 32768.0f;

    mpu.getRotation  ( &g1, &g2, &g3  );
    gx = g1 * 250.0f / 32768.0f; // 250 deg/s full range for gyroscope
    gy = g2 * 250.0f / 32768.0f;
    gz = g3 * 250.0f / 32768.0f;

    //  The gyros and accelerometers can in principle be calibrated in addition to any factory calibration but they are generally
    //  pretty accurate. You can check the accelerometer by making sure the reading is +1 g in the positive direction for each axis.
    //  The gyro should read zero for each axis when the sensor is at rest. Small or zero adjustment should be needed for these sensors.
    //  The magnetometer is a different thing. Most magnetometers will be sensitive to circuit currents, computers, and
    //  other both man-made and natural sources of magnetic field. The rough way to calibrate the magnetometer is to record
    //  the maximum and minimum readings (generally achieved at the North magnetic direction). The average of the sum divided by two
    //  should provide a pretty good calibration offset. Don't forget that for the MPU9150, the magnetometer x- and y-axes are switched
    //  compared to the gyro and accelerometer!

    //  if (mcount > 1000 / MagRate) { // this is a poor man's way of setting the magnetometer read rate (see below)
    mpu.getMag  ( &m1, &m2, &m3 );
    mxraw = m1 * 10.0f * 1229.0f / 4096.0f + 0.0f;//+ 18.0f; // milliGauss (1229 microTesla per 2^12 bits, 10 mG per microTesla)
    myraw = m2 * 10.0f * 1229.0f / 4096.0f + 0.0f;//+ 70.0f; // apply calibration offsets in mG that correspond to your environment and magnetometer
    mzraw = m3 * 10.0f * 1229.0f / 4096.0f + 0.0f;//+ 270.0f;

    mcount = 0;
    // }
  }

  now = micros();
  deltat = ((now - lastUpdate) / 1000000.0f); // set integration time by time elapsed since last filter update
  lastUpdate = now;
  // Sensors x (y)-axis of the accelerometer is aligned with the y (x)-axis of the magnetometer;
  // the magnetometer z-axis (+ down) is opposite to z-axis (+ up) of accelerometer and gyro!
  // We have to make some allowance for this orientationmismatch in feeding the output to the quaternion filter.
  // For the MPU-9150, we have chosen a magnetic rotation that keeps the sensor forward along the x-axis just like
  // in the LSM9DS0 sensor. This rotation can be modified to allow any convenient orientation convention.
  // This is ok by aircraft orientation standards!
  // Pass gyro rate as rad/s
  MadgwickQuaternionUpdate(ax, ay, az, gx * PI / 180.0f, gy * PI / 180.0f, gz * PI / 180.0f,  myraw,  mxraw, -mzraw, 0);

  // Serial print and/or display at 0.5 s rate independent of data rates
  delt_t = millis() - count;
  if (delt_t > 500) { // update LCD once per half-second independent of read rate
    count = millis();
    Serial.print("ax = "); Serial.print((int)1000 * ax);
    Serial.print(" ay = "); Serial.print((int)1000 * ay);
    Serial.print(" az = "); Serial.print((int)1000 * az); Serial.println(" mg");
    Serial.print("gx = "); Serial.print( gx, 2);
    Serial.print(" gy = "); Serial.print( gy, 2);
    Serial.print(" gz = "); Serial.print( gz, 2); Serial.println(" deg/s");
    Serial.print("mx = "); Serial.print( (int)mxraw );
    Serial.print(" my = "); Serial.print( (int)myraw );
    Serial.print(" mz = "); Serial.print( (int)mzraw ); Serial.println(" mG");

    //Serial.print("q0 = "); Serial.print(q[0]);
    //Serial.print(" qx = "); Serial.print(q[1]);
    //Serial.print(" qy = "); Serial.print(q[2]);
    //Serial.print(" qz = "); Serial.println(q[3]);


    // Define output variables from updated quaternion---these are Tait-Bryan angles, commonly used in aircraft orientation.
    // In this coordinate system, the positive z-axis is down toward Earth.
    // Yaw is the angle between Sensor x-axis and Earth magnetic North (or true North if corrected for local declination, looking down on the sensor positive yaw is counterclockwise.
    // Pitch is angle between sensor x-axis and Earth ground plane, toward the Earth is positive, up toward the sky is negative.
    // Roll is angle between sensor y-axis and Earth ground plane, y-axis up is positive roll.
    // These arise from the definition of the homogeneous rotation matrix constructed from quaternions.
    // Tait-Bryan angles as well as Euler angles are non-commutative; that is, the get the correct orientation the rotations must be
    // applied in the correct order which for this configuration is yaw, pitch, and then roll.
    // For more see http://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles which has additional links.
    yaw   = atan2(2.0f * (q[1] * q[2] + q[0] * q[3]), q[0] * q[0] + q[1] * q[1] - q[2] * q[2] - q[3] * q[3]);
    pitch = -asin(2.0f * (q[1] * q[3] - q[0] * q[2]));
    roll  = atan2(2.0f * (q[0] * q[1] + q[2] * q[3]), q[0] * q[0] - q[1] * q[1] - q[2] * q[2] + q[3] * q[3]);
    pitch *= 180.0f / PI;
    yaw   *= 180.0f / PI ; //- 13.8; // Declination at Danville, California is 13 degrees 48 minutes and 47 seconds on 2014-04-04
    roll  *= 180.0f / PI;
    gyrofSend[0] = yaw;
    gyrofSend[1] = pitch;
    gyrofSend[2] = roll;

    Serial.print("Yaw, Pitch, Roll: ");
    Serial.print(yaw, 2);
    // Serial.print(gyrofSend[0]);
    Serial.print(", ");
    Serial.print(pitch, 2);
    //Serial.print( gyrofSend[1]);
    Serial.print(", ");
    Serial.println(roll, 2);
    // Serial.println( gyrofSend[2]);

    Serial.println();

    //Fill datapacket with 3 axis gyro data;
    int y = 0;
    for (int x = 0; x < 3; x++) {
      gyroiSend[x] = gyrofSend[x] * 1000;
      // Save data
      datapacket[y] = (gyroiSend[x] >> 16) & 0xFFFF;
      datapacket[y + 1] = gyroiSend[x] & 0xFFFF;
      y = y + 2;
    }

    //send wireless data
    sendWirelessData(datapacket, array_size, xbeeID_BCast);

  }
}
void sendWirelessData(int Data[], int datalength, int xbeeID) {
  unsigned long chexsum = (byte) 0x00; //intialize chesum
  uint8_t numEscaped = 0; //escape counter
  uint8_t framelength = 18 + 2 * datalength; //total number of unescaped frame elements
  uint8_t frame[framelength]; //Non escaped frame
  uint8_t framen[framelength * 2]; //Escaped frame needs to be longer
  uint8_t x; //data incrementer

  //Start delimiter
  frame[0] = 0x7E;

  //Data length
  frame[1] = (byte)0x0;
  frame[2] = 0x0E + (byte)(datalength * 2);
  //contains(frame[2], 0, escaped);

  //Frame Type
  frame[3] = 0x10;
  chexsum = chexsum + 0x10;

  //Frame ID
  frame[4] = 0x01;
  chexsum = chexsum + 0x01;

  //Choose correct ID
  switch (xbeeID) {
    case 0: //Broadcast ID
      for (int y = 0; y < 8; y++) {
        frame[5 + y] = xbeeAdrs_BCast[y];
        chexsum = chexsum + xbeeAdrs_BCast[y];
      }
      break;
    case 1: //XBEE A ID
      for (int y = 0; y < 8; y++) {
        frame[5 + y] = xbeeAdrs_A[y];
        chexsum = chexsum + xbeeAdrs_A[y];
      }
      break;
    case 2: //XBEE B ID
      for (int y = 0; y < 8; y++) {
        frame[5 + y] = xbeeAdrs_B[y];
        chexsum = chexsum + xbeeAdrs_B[y];
      }
      break;
    case 3: //XBEE C ID
      for (int y = 0; y < 8; y++) {
        frame[5 + y] = xbeeAdrs_C[y];
        chexsum = chexsum + xbeeAdrs_C[y];
      }
      break;
    case 4: //XBEE D ID
      for (int y = 0; y < 8; y++) {
        frame[5 + y] = xbeeAdrs_D[y];
        chexsum = chexsum + xbeeAdrs_D[y];
      }
      break;
    default:
      break;
  }

  frame[13] = 0xFF; //16-bit dest. address
  frame[14] = 0xFE; //16-bit dest. address
  frame[15] = (byte)0x0; //Broadcast radius
  frame[16] = (byte)0x0; //Options
  chexsum = chexsum + 0xFF + 0xFE;

  // RF data
  x = 0;
  for (int i = 0; i < (datalength * 2); i = i + 2) {
    frame[17 + i] = (byte)(Data[x] >> 8); //MSB
    frame[17 + i + 1] = (byte)Data[x]; //LSB
    chexsum = chexsum + (byte)(Data[x] >> 8) + (byte)Data[x]; //add all data packets
    x++;
  }

  //Compute Chexsum
  frame[framelength - 1] = 0xFF - (chexsum & 0xFF);

  //Compute escaped Frame
  framen[0] = frame[0];
  for (int i = 1; i < framelength; i++) {
    framen[i + numEscaped] = frame[i];
    switch (frame[i]) {
      case (0x7E):
        framen[i + numEscaped] = 0x7D;
        framen[i + numEscaped + 1] = 0x5E;
        numEscaped++;
        break;
      case (0x7D):
        framen[i + numEscaped] = 0x7D;
        framen[i + numEscaped + 1] = 0x5D;
        numEscaped++;
        break;
      case (0x11):
        framen[i + numEscaped] = 0x7D;
        framen[i + numEscaped + 1] = 0x31;
        numEscaped++;
        break;
      case (0x13):
        framen[i + numEscaped] = 0x7D;
        framen[i + numEscaped + 1] = 0x33;
        numEscaped++;
        break;
      default:
        break;
    }
  }

  //write escaped frame
  for (int z = 0; z < (framelength + numEscaped); z++) {
    ss.write(framen[z]);
    Serial.println(framen[z], HEX);
  }

}




// Implementation of Sebastian Madgwick's "...efficient orientation filter for... inertial/magnetic sensor arrays"
// (see http://www.x-io.co.uk/category/open-source/ for examples and more details)
// which fuses acceleration, rotation rate, and magnetic moments to produce a quaternion-based estimate of absolute
// device orientation -- which can be converted to yaw, pitch, and roll. Useful for stabilizing quadcopters, etc.
// The performance of the orientation filter is at least as good as conventional Kalman-based filtering algorithms
// but is much less computationally intensive---it can be performed on a 3.3 V Pro Mini operating at 8 MHz!

void MadgwickQuaternionUpdate(float ax, float ay, float az, float gx, float gy, float gz, float mx, float my, float mz, bool MagOn)
{
  float q1 = q[0], q2 = q[1], q3 = q[2], q4 = q[3];   // short name local variable for readability
  float norm;
  float hx, hy, _2bx, _2bz;
  float s1, s2, s3, s4;
  float qDot1, qDot2, qDot3, qDot4;

  // Auxiliary variables to avoid repeated arithmetic
  float _2q1mx;
  float _2q1my;
  float _2q1mz;
  float _2q2mx;
  float _4bx;
  float _4bz;
  float _2q1 = 2.0f * q1;
  float _2q2 = 2.0f * q2;
  float _2q3 = 2.0f * q3;
  float _2q4 = 2.0f * q4;
  float _2q1q3 = 2.0f * q1 * q3;
  float _2q3q4 = 2.0f * q3 * q4;
  float q1q1 = q1 * q1;
  float q1q2 = q1 * q2;
  float q1q3 = q1 * q3;
  float q1q4 = q1 * q4;
  float q2q2 = q2 * q2;
  float q2q3 = q2 * q3;
  float q2q4 = q2 * q4;
  float q3q3 = q3 * q3;
  float q3q4 = q3 * q4;
  float q4q4 = q4 * q4;

  // Normalise accelerometer measurement
  norm = sqrt(ax * ax + ay * ay + az * az);
  if (norm == 0.0f) return; // handle NaN
  norm = 1.0f / norm;
  ax *= norm;
  ay *= norm;
  az *= norm;

  // Normalise magnetometer measurement
  norm = sqrt(mx * mx + my * my + mz * mz);
  if (norm == 0.0f) return; // handle NaN
  norm = 1.0f / norm;
  mx *= norm;
  my *= norm;
  mz *= norm;

  // Reference direction of Earth's magnetic field
  _2q1mx = 2.0f * q1 * mx;
  _2q1my = 2.0f * q1 * my;
  _2q1mz = 2.0f * q1 * mz;
  _2q2mx = 2.0f * q2 * mx;
  hx = mx * q1q1 - _2q1my * q4 + _2q1mz * q3 + mx * q2q2 + _2q2 * my * q3 + _2q2 * mz * q4 - mx * q3q3 - mx * q4q4;
  hy = _2q1mx * q4 + my * q1q1 - _2q1mz * q2 + _2q2mx * q3 - my * q2q2 + my * q3q3 + _2q3 * mz * q4 - my * q4q4;

  if (MagOn == 1) {
    _2bx = sqrt(hx * hx + hy * hy);
    _2bz = -_2q1mx * q3 + _2q1my * q2 + mz * q1q1 + _2q2mx * q4 - mz * q2q2 + _2q3 * my * q4 - mz * q3q3 + mz * q4q4;
    _4bx = 2.0f * _2bx;
    _4bz = 2.0f * _2bz;
  } else {
    //Set to Zero to Remove Magnetometer
    _2bx = 0;
    _2bz = 0;
    _4bx = 0;
    _4bz = 0;
  }

  // Gradient decent algorithm corrective step
  s1 = -_2q3 * (2.0f * q2q4 - _2q1q3 - ax) + _2q2 * (2.0f * q1q2 + _2q3q4 - ay) - _2bz * q3 * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (-_2bx * q4 + _2bz * q2) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + _2bx * q3 * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
  s2 = _2q4 * (2.0f * q2q4 - _2q1q3 - ax) + _2q1 * (2.0f * q1q2 + _2q3q4 - ay) - 4.0f * q2 * (1.0f - 2.0f * q2q2 - 2.0f * q3q3 - az) + _2bz * q4 * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (_2bx * q3 + _2bz * q1) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + (_2bx * q4 - _4bz * q2) * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
  s3 = -_2q1 * (2.0f * q2q4 - _2q1q3 - ax) + _2q4 * (2.0f * q1q2 + _2q3q4 - ay) - 4.0f * q3 * (1.0f - 2.0f * q2q2 - 2.0f * q3q3 - az) + (-_4bx * q3 - _2bz * q1) * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (_2bx * q2 + _2bz * q4) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + (_2bx * q1 - _4bz * q3) * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
  s4 = _2q2 * (2.0f * q2q4 - _2q1q3 - ax) + _2q3 * (2.0f * q1q2 + _2q3q4 - ay) + (-_4bx * q4 + _2bz * q2) * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (-_2bx * q1 + _2bz * q3) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + _2bx * q2 * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
  norm = sqrt(s1 * s1 + s2 * s2 + s3 * s3 + s4 * s4);    // normalise step magnitude
  norm = 1.0f / norm;
  s1 *= norm;
  s2 *= norm;
  s3 *= norm;
  s4 *= norm;

  // Compute rate of change of quaternion
  qDot1 = 0.5f * (-q2 * gx - q3 * gy - q4 * gz) - beta * s1;
  qDot2 = 0.5f * (q1 * gx + q3 * gz - q4 * gy) - beta * s2;
  qDot3 = 0.5f * (q1 * gy - q2 * gz + q4 * gx) - beta * s3;
  qDot4 = 0.5f * (q1 * gz + q2 * gy - q3 * gx) - beta * s4;

  // Integrate to yield quaternion
  q1 += qDot1 * deltat;
  q2 += qDot2 * deltat;
  q3 += qDot3 * deltat;
  q4 += qDot4 * deltat;
  norm = sqrt(q1 * q1 + q2 * q2 + q3 * q3 + q4 * q4);    // normalise quaternion
  norm = 1.0f / norm;
  q[0] = q1 * norm;
  q[1] = q2 * norm;
  q[2] = q3 * norm;
  q[3] = q4 * norm;

}



