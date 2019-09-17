//rv12
#include <ESP8266WiFi.h>
#include <TimeLib.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <WiFiUdp.h>
#include "Gsender.h"          // gmail
#include <Wire.h>             // I2C
#include <Adafruit_ADS1015.h> // ADC

bool lowvolt = false;                         //used to set low voltage alert
int i;                                        // used as an index into the array
const char ssid[] = "ssid";             //  your network SSID (name)
const char pass[] = "pass";             // your network password
const char wherebattery[] = "RV";             // where are your batteries located
const char sendto[] = "test@gmail.com"; // email address to send messages, make sure to edit file for the send from email address
int numbatt = 3;                              // number of batteries to monitor 1-4

int16_t adc[] = {0, 0, 0, 0};

const int timeZone = -4;  // Eastern Daylight Time (USA)

char server[] = "smtp.gmail.com"; // The SMTP Server
unsigned int localPort = 8888;  // local port to listen for UDP packets

const float RATIO[] = {.0020779, .0022106912, .0022106912, .0020779};  //need to calculate your own value based on resistors

float LOW_THRESHOLD[] = {12.1, 12.2, 12.3, 12.4}; // alarm threshold voltage
float BATTERYVOLTS[] = {14, 14, 14, 14};  // defines array for starting voltages for batteries

Adafruit_ADS1115 ads(0x48);

IPAddress timeServer(184, 105, 182, 15); // pool.ntp.org

WiFiUDP Udp;

void setup()
{
  Serial.begin(115200);
  Wire.begin(0, 2);  // set pins for I2C for ESP-01
  ads.begin();

  delay (5000);  //delay 5 sec to get serial putty session up
 
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  Serial.println("");
  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
 
  UpdateNTP();
 
  Serial.println("Setting Alarms...");
  Alarm.alarmRepeat(8,0,0, SendMail);  // 8am email status update************************************************************************
//  Alarm.timerRepeat(120, SendMail); //2 min used to test ***********************************************************************
  Alarm.timerRepeat(3600, SendMailLowVolt); // Check every hour if a battery is low
//  Alarm.timerRepeat(120, SendMailLowVolt); // Check every hour if a battery is low 2 min test
  Alarm.alarmRepeat(0,5,0, UpdateNTP); // updates ntp every morning

}

  time_t prevDisplay = 0; // when the digital clock was displayed

void loop(){
   
//Prints time to serial interface
  if (now() != prevDisplay) { // update the display only if time has changed
    prevDisplay = now();
    Serial.println(" ");
    digitalClockDisplay();
    }
 
//read adc values
  for (i = 0; i < numbatt; i ++) {
    adc[i] = ads.readADC_SingleEnded(i);
  }

  //Calculate battery voltages
  for (i = 0; i < numbatt; i ++) {
    BATTERYVOLTS[i] = RATIO[i] * adc[i];
  }
 
 //Print battery voltage to serial
  for (i = 0; i < numbatt; i ++) {
    Serial.print("Battery ");
    Serial.print(i+1);
    Serial.print(": ");
    Serial.print(BATTERYVOLTS[i]);
    Serial.println(" volts");
  }
 
  // Low voltage battery check
  lowvolt = false;
  for (i = 0; i < numbatt; i ++) {
    if (BATTERYVOLTS[i] < LOW_THRESHOLD[i]){
      Serial.print("Low Voltage has been identified on battery ");
      Serial.print(i+1);
      Serial.print(" : Low threshold value = ");
      Serial.print(LOW_THRESHOLD[i]);
      Serial.println(" volts");
      lowvolt = true;
    }
  }
 
  Alarm.delay(1000);

}

void SendMail() {
  Serial.println("Sending Mail Routine");
  Gsender *gsender = Gsender::Instance();    // Getting pointer to class instance
  String subject = wherebattery;
  subject += " Battery Status";
 
  String message = "Battery voltage measurements are:\n";
  for (i = 0; i < numbatt; i ++) {
    message += "\nBattery ";
  message += String (i+1);
  message += " : ";
  message += String (BATTERYVOLTS[i]);
  message += " volts";
    }
 
  if(gsender->Subject(subject)->Send((sendto), (message))) {
      Serial.println("Message sent.");
  } else {
      Serial.print("Error sending message: ");
      Serial.println(gsender->getError());
  }
}

void SendMailLowVolt() {
  Serial.println("Starting Low Voltage Alert Email Routine");
  if (lowvolt){
      Serial.println("Low Voltage has been identified, sending email alert");
      Gsender *gsender = Gsender::Instance();    // Getting pointer to class instance
      String subject = "ALERT - RV Battery Low Voltage Alert";
     
    String message = "Battery voltage measurements are:\n";
    for (i = 0; i < numbatt; i ++) {
      message += "\nBattery ";
      message += String (i+1);
      message += " : ";
      message += String (BATTERYVOLTS[i]);
      message += " volts , min value is : ";
      message += String (LOW_THRESHOLD[i]);
      message += " volts";  
    }
   
      if(gsender->Subject(subject)->Send((sendto), (message))) {
          Serial.println("Message sent.");
      } else {
          Serial.print("Error sending message: ");
          Serial.println(gsender->getError());
      }
    } else {
      Serial.println("All batteries are good.");
    }
}

void UpdateNTP() {
  Serial.println("Time NTP Routine");
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
}

void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(month());
  Serial.print("/");
  Serial.print(day());
  Serial.print("/");
  Serial.print(year());
  Serial.println();
}

void printDigits(int digits){
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}