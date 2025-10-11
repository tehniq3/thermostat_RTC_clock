/*
 * Racov feb.2021
 -- 20x4 LCD (paralel) display 
 -- clock with big numbers and I2C-DS3231 Real Time clock
 -- temperature and hunidity interior/exterior with I2C-BMP280
 -- rotary encoder for TIME set
 racov.ro

v.1 - Nicu FLORICA (niq_ro) changed t i2c interface for LCD2004
v.1.a - changed sensors to DHT22 
v.2 - added Doz (A.G.Doswell) mode to change the time with encoder: http://andydoz.blogspot.ro/2014_08_01_archive.html
v.3 - removed both DHT22 sensors and added one DS18B20 sensor 
v.3a - added adjustment also for seconds, show temperature without delay 
v.3b - changed DS18B20 request and set the resolution 
v.4 - switch big fonts for clock with temperature 
v.5 - added thermostat function
v.5a - added adjust for temperature an hysteresis plusEEPROM store
*/

// Use: Pressing and holding the button will enter the clock set mode (on release of the button). Clock is set using the rotary encoder. 
// The clock must be set to UTC.
// Pressing and releasing the button quickly will display info about author (me)

#include <Wire.h>
#include "RTClib.h" // from https://github.com/adafruit/RTClib
#include <LiquidCrystal_I2C.h> // used Adafruit library
#include <Wire.h>
#include <Encoder.h> // from http://www.pjrc.com/teensy/td_libs_Encoder.html
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>  // http://tronixstuff.com/2011/03/16/tutorial-your-arduinos-inbuilt-eeprom/

// Data wire is connected to pin 6
#define ONE_WIRE_BUS 6

#define sw1 4  // encoder switch
int PinA = 2;  // encoder right
int PinB = 3;  // encoder left


#define lednok 7  // red led
#define relay  8
#define ledok  9  // green led

RTC_DS1307 RTC; // Tells the RTC library that we're using a DS1307 RTC
Encoder knob(PinB, PinA); //encoder connected to pins 2 and 3 (and ground)
LiquidCrystal_I2C lcd(0x27, 20, 4); // adress can be 0x3F

//the variables provide the holding values for the set clock routine
int setyeartemp; 
int setmonthtemp;
int setdaytemp;
int sethourstemp;
int setminstemp;
int setsecstemp;
int maxday; // maximum number of days in the given month
// These variables are for the push button routine
int buttonstate = 0; //flag to see if the button has been pressed, used internal on the subroutine only
int pushlengthset = 3000; // value for a long push in mS
int pushlength = pushlengthset; // set default pushlength
int pushstart = 0;// sets default push value for the button going low
int pushstop = 0;// sets the default value for when the button goes back high

int knobval; // value for the rotation of the knob
boolean buttonflag = false; // default value for the button flag

//const int TIMEZONE = 0; //UTC
const int TIMEZONE = 2; //UTC Craiova (Romania) - http://www.worldtimebuddy.com/utc-to-romania-craiova

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress insideThermometer;

boolean intds = true;

int h,m,s,yr,mt,dt,dy,olds;   // hous, minutes, seconds, year, month, date of the month, day, previous second
//char *DOW[]={"MAR","MIE","JOI","VIN","SAM","DUM","LUN"};  //define day of the week
//char *DOW[]={"Dum","Lun","Mar","Mie","Joi","Vin","Sam"};  //define day of the week
char *DOW[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};  //define day of the week
//char *MTH[]={"Ian","Feb","Mar","Apr","Mai","Iun","Iul","Aug","Sep","Oct","Noi","Dec"};  //define month
char *MTH[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};  //define month

unsigned long tpcitit = 10000;
unsigned long tpcitire;

float tempC;
int tempC1;
int tz, tu, ts;
byte mod = 0;     

int te1 = 50;   // value for store the desired temperature (int value)
float te = te1/2.;    // desired temperature
int dete1 = 5;  // value for storefor desired hysteresis
float dete = 0.5;  // hysteresis value
int temin = 20;    // min. 10 degree Celsius
int temax = 100;   // max. 50 degree Celsius
int detemin = 2;    // min. 0.2 degree Celsius
int detemax = 10;   // max. 1.0 degree Celsius

void setup () {
    Serial.begin(9600); //start debug serial interface
    Wire.begin(); //start I2C interface

    lcd.begin();
    lcd.backlight();
    lcd.clear(); 
    pinMode(sw1,INPUT);//push button on encoder connected to A0 (and GND)
    digitalWrite(sw1,HIGH); //Pull sw1 to high
    pinMode(relay,OUTPUT); //Relay (yellow led) connected to D8
    pinMode(ledok,OUTPUT); //green led connected to D7
    pinMode(lednok,OUTPUT); //red led connected to D7
    digitalWrite (relay, LOW); //sets relay off (default condition)
    digitalWrite (ledok, LOW);
    digitalWrite (lednok, LOW);


// just first time... after must put commnent (//)
/*
EEPROM.write(601,50);   // te1 = for store the temperature set    
EEPROM.write(602,1);    // dete1 = for store the hysteresis set
*/
  
te1 = EEPROM.read(601);
dete1 = EEPROM.read(602);

if ((te1 < temin) or (te1 > temax))
{
 EEPROM.write(601,50); // 50/2 = 25 degree Celsius
 te1 = EEPROM.read(601);
}
if ((dete1 < detemin) or (dete1 > detemax))
{
 EEPROM.write(602,5); // 5/10 = 0.5 degree Celsius
 dete1 = EEPROM.read(602);
}

te = te1/2.;
dete = dete1/10.;

// *******DEFINE CUSTOM CHARACTERS FOR BIG FONT*****************
  byte A[8] =
  {
  B00011,
  B00111,
  B01111,
  B01111,
  B01111,
  B01111,
  B01111,
  B01111
  };
  byte B[8] =
  {
  B11111,
  B11111,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B11111
  };
  byte C[8] =
  {
  B11000,
  B11100,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110
  };
  byte D[8] =
  {
  B01111,
  B01111,
  B01111,
  B01111,
  B01111,
  B01111,
  B00111,
  B00011
  };
  byte E[8] =
  {
  B11111,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B11111,
  B11111
  };
  byte F[8] =
  {
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11100,
  B11000
  };
  byte G[8] =
  {
  B11111,
  B11111,
  B11111,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
  };
  byte H[8] =
  {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B11111,
  B11111,
  B11111
  };
  
  lcd.createChar(8,A);
  lcd.createChar(6,B);
  lcd.createChar(2,C);
  lcd.createChar(3,D);
  lcd.createChar(7,E);
  lcd.createChar(5,F);
  lcd.createChar(1,G);
  lcd.createChar(4,H);
 
  // Print a message to the LCD.
  // lcd.setCursor(col, row);
  // lcd.print("text");
  lcd.setCursor(4, 0);
  lcd.print("Thermostat");
  lcd.setCursor(4, 1);
  lcd.print("CLOCK  v.5");
  lcd.setCursor(5, 2);
  lcd.print("Oct.2025");
  lcd.setCursor(5, 3);
  lcd.print("by niq_ro");
  delay(1000);
  
  // initializing the modules
  lcd.setCursor(0, 0);
  lcd.print("CHECKING MODULES....");
  delay(100);
  
  lcd.setCursor(0, 1);
  lcd.print("1.REAL CLOCK......OK");
  if (! RTC.begin()) {
    lcd.setCursor(16, 1);
    lcd.print("FAIL");
    delay(5000);
  }
  delay(500);

  lcd.setCursor(0, 2);
  lcd.print("2.SENSOR TEMP.....OK");

  // Start up the library
  sensors.begin();// Start up the library

  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0");
  // show the addresses we found on the bus
  Serial.print("Device 0 Address: ");
    for (uint8_t i = 0; i < 8; i++)
  {
    if (insideThermometer[i] < 16) Serial.print("0");
    Serial.print(insideThermometer[i], HEX);
  //  lcd.print(insideThermometer[i], HEX);
  }
  
 // printAddress(insideThermometer);
  Serial.println();
   
  // set the resolution to 9.. 12 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(insideThermometer, 11);
  //  9-bit: 0.5°C increments     93.75 ms
  // 10-bit: 0.25°C increments   187.50 ms
  // 11-bit: 0.125°C increments  375.00 ms
  // 12-bit: 0.0625°C increments 750.00 ms

  // Request temperature from the sensor
  sensors.requestTemperatures();

  // Read temperature in Celsius
  //tempC = sensors.getTempCByIndex(0);
   tempC = sensors.getTempC(insideThermometer);
  
  // Check if any reads failed and exit early (to try again).
  if (isnan(tempC))
   {
    //Serial.println(F("Failed to read from sensor !"));
    lcd.setCursor(16, 2);
    lcd.print("FAIL");
    intds = false;
   }

  delay(500);
  
  lcd.setCursor(0, 3);
  lcd.print(" ID:");
  for (uint8_t i = 0; i < 8; i++)
  {
    if (insideThermometer[i] < 16) lcd.print("0");
    lcd.print(insideThermometer[i], HEX);
  }
    
  delay(2000);
    
    //Checks to see if the RTC is runnning, and if not, sets the time to the time this sketch was compiled.
    if (! RTC.isrunning()) {
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }
lcd.clear();
promo();
tpcitire = tpcitit + millis();
}
           

void loop () {

    pushlength = pushlengthset;
    pushlength = getpushlength ();
    delay (10);
    
    if (pushlength <pushlengthset) 
    {
      ShortPush ();   
      promo();
      readds();
    }  
       //This runs the setclock routine if the knob is pushed for a long time
       if (pushlength >pushlengthset) 
       {
         lcd.clear();
         DateTime now = RTC.now();
         setyeartemp=now.year(),DEC;
         setmonthtemp=now.month(),DEC;
         setdaytemp=now.day(),DEC;
         sethourstemp=now.hour(),DEC;
         setminstemp=now.minute(),DEC;
         setsecstemp=now.second(),DEC;
         setclock();
         pushlength = pushlengthset;
       };

  DateTime now = RTC.now();
  s  = now.second();                        // read seconds
  if (olds != s) 
  {                          // if seconds changed
/*    Serial.println(dy);
      Serial.println(mt);
      Serial.println(dt);
      Serial.println(yr); */

      if (mod%2 == 0)
      {
      printbig((s%10),17);                  // display seconds
      printbig((s/10),14); 
      }
      else
      {
       lcd.setCursor(17, 3);
       lcd.print(":");
       lcd.print(s/10);
       lcd.print(s%10);
      }
             
      olds = s;
      if ( s == 0 ) {                       // minutes change
          m  = now.minute();                // read minutes
          if (mod%2 == 0)
          {
          printbig((m%10),10);              // display minutes
          printbig((m/10),7);
          }
          else
          {
          lcd.setCursor(14, 3);
          lcd.print(":");
          lcd.print(m/10);
          lcd.print(m%10);  
          }
          if (m == 0) 
          {                     // hours change
            h  = now.hour();                // read hours
            if (mod%2 == 0)
            {
            printbig((h%10),3);             // dislay hours
            printbig((h/10),0);
            }
            else
            {
            lcd.setCursor(12, 3);
            lcd.print(h/10);
            lcd.print(h%10);  
            }
            if (h == 0) 
            {                   // day change
              promo();
            }
          } 
  }

    if (millis() - tpcitire > tpcitit)
     {           
        readds();
     } 
    // do other things every second
  }

if (tempC < te)  // cold, start the heating
   {
    lcd.setCursor (12,3-mod%2);
    lcd.print ("*");
    digitalWrite(lednok, HIGH);
    digitalWrite(ledok, LOW);
    digitalWrite(relay, HIGH);
   }
   
if (tempC > te+dete)  // stop the heating 
   {
    lcd.setCursor (12,3-mod%2);
    lcd.print (" ");
 //   digitalWrite(ledok, LOW);
 //   digitalWrite(lednok, LOW);
    digitalWrite(relay, LOW);
   }

if (tempC >= te)  // ok
   {
    digitalWrite(ledok, HIGH);
    digitalWrite(lednok, LOW);
   // digitalWrite(relay, LOW);
   }

delay(100); 
}  // end main loop




//sets the clock
void setclock (){
   sette ();
   lcd.clear ();
   setdete ();
   lcd.clear ();
   setyear ();
   lcd.clear ();
   setmonth ();
   lcd.clear ();
   setday ();
   lcd.clear ();
   sethours ();
   lcd.clear ();
   setmins ();
   lcd.clear();
   setsecs ();
   lcd.clear();  

   te = te1/2.;
   dete = dete1/10.;
   EEPROM.write(601,te1); // 50/2 = 25 degree Celsius
   EEPROM.write(602,dete1); // 5/10 = 0.5 degree Celsius
   RTC.adjust(DateTime(setyeartemp,setmonthtemp,setdaytemp,sethourstemp,setminstemp,setsecstemp));
   delay (500);
   promo();
   readds();
}

// subroutine to return the length of the button push.
int getpushlength () {
  buttonstate = digitalRead(sw1);  
       if(buttonstate == LOW && buttonflag==false) {     
              pushstart = millis();
              buttonflag = true;
          };
          
       if (buttonstate == HIGH && buttonflag==true) {
         pushstop = millis ();
         pushlength = pushstop - pushstart;
         buttonflag = false;
       };
       return pushlength;
}
// The following subroutines set the individual clock parameters
int setyear () {
//lcd.clear();
    lcd.setCursor (0,0);
    lcd.print ("Set Year");
    pushlength = pushlengthset;
    pushlength = getpushlength ();
    if (pushlength != pushlengthset) {
      return setyeartemp;
    }

    lcd.setCursor (0,1);
    knob.write(0);
    delay (50);
    knobval=knob.read();
    if (knobval < -1) { //bit of software de-bounce
      knobval = -1;
    }
    if (knobval > 1) {
      knobval = 1;
    }
    setyeartemp=setyeartemp + knobval;
    if (setyeartemp < 2014) { //Year can't be older than currently, it's not a time machine.
      setyeartemp = 2014;
    }
    lcd.print (setyeartemp);
    lcd.print("  "); 
    setyear();
}
  
int setmonth () {
//lcd.clear();
   lcd.setCursor (0,0);
    lcd.print ("Set Month");
    pushlength = pushlengthset;
    pushlength = getpushlength ();
    if (pushlength != pushlengthset) {
      return setmonthtemp;
    }

    lcd.setCursor (0,1);
    knob.write(0);
    delay (50);
    knobval=knob.read();
    if (knobval < -1) {
      knobval = -1;
    }
    if (knobval > 1) {
      knobval = 1;
    }
    setmonthtemp=setmonthtemp + knobval;
    if (setmonthtemp < 1) {// month must be between 1 and 12
      setmonthtemp = 1;
    }
    if (setmonthtemp > 12) {
      setmonthtemp=12;
    }
    lcd.print (setmonthtemp/10);
    lcd.print (setmonthtemp%10);
    lcd.print("  "); 
    setmonth();
}

int setday () {
  if (setmonthtemp == 4 || setmonthtemp == 5 || setmonthtemp == 9 || setmonthtemp == 11) { //30 days hath September, April June and November
    maxday = 30;
  }
  else {
  maxday = 31; //... all the others have 31
  }
  if (setmonthtemp ==2 && setyeartemp % 4 ==0) { //... Except February alone, and that has 28 days clear, and 29 in a leap year.
    maxday = 29;
  }
  if (setmonthtemp ==2 && setyeartemp % 4 !=0) {
    maxday = 28;
  }
//lcd.clear();  
   lcd.setCursor (0,0);
    lcd.print ("Set Day");
    pushlength = pushlengthset;
    pushlength = getpushlength ();
    if (pushlength != pushlengthset) {
      return setdaytemp;
    }

    lcd.setCursor (0,1);
    knob.write(0);
    delay (50);
    knobval=knob.read();
    if (knobval < -1) {
      knobval = -1;
    }
    if (knobval > 1) {
      knobval = 1;
    }
    setdaytemp=setdaytemp+ knobval;
    if (setdaytemp < 1) {
      setdaytemp = 1;
    }
    if (setdaytemp > maxday) {
      setdaytemp = maxday;
    }
    lcd.print (setdaytemp/10);
    lcd.print (setdaytemp%10);
    lcd.print("  "); 
    setday();
}

int sethours () {
//lcd.clear();
    lcd.setCursor (0,0);
    lcd.print ("Set Hours");
    pushlength = pushlengthset;
    pushlength = getpushlength ();
    if (pushlength != pushlengthset) {
      return sethourstemp;
    }

    lcd.setCursor (0,1);
    knob.write(0);
    delay (50);
    knobval=knob.read();
    if (knobval < -1) {
      knobval = -1;
    }
    if (knobval > 1) {
      knobval = 1;
    }
    sethourstemp=sethourstemp + knobval;
    if (sethourstemp < 1) {
      sethourstemp = 1;
    }
    if (sethourstemp > 23) {
      sethourstemp=23;
    }
    lcd.print (sethourstemp/10);
    lcd.print (sethourstemp%10);
    lcd.print("  "); 
    sethours();
}

int setmins () {
//lcd.clear();
   lcd.setCursor (0,0);
    lcd.print ("Set Mins");
    pushlength = pushlengthset;
    pushlength = getpushlength ();
    if (pushlength != pushlengthset) {
      return setminstemp;
    }

    lcd.setCursor (0,1);
    knob.write(0);
    delay (50);
    knobval=knob.read();
    if (knobval < -1) {
      knobval = -1;
    }
    if (knobval > 1) {
      knobval = 1;
    }
    setminstemp=setminstemp + knobval;
    if (setminstemp < 0) {
      setminstemp = 0;
    }
    if (setminstemp > 59) {
      setminstemp=59;
    }
    lcd.print (setminstemp/10);
    lcd.print (setminstemp%10);
    lcd.print("  "); 
    setmins();
}

int setsecs () {
//lcd.clear();
   lcd.setCursor (0,0);
    lcd.print ("Set Seconds");
    pushlength = pushlengthset;
    pushlength = getpushlength ();
    if (pushlength != pushlengthset) {
      return setsecstemp;
    }

    lcd.setCursor (0,1);
    knob.write(0);
    delay (50);
    knobval=knob.read();
    if (knobval < -1) {
      knobval = -1;
    }
    if (knobval > 1) {
      knobval = 1;
    }
    setsecstemp=setsecstemp + knobval;
    if (setsecstemp < 0) {
      setsecstemp = 0;
    }
    if (setsecstemp > 59) {
      setsecstemp=59;
    }
    lcd.print (setsecstemp/10);
    lcd.print (setsecstemp%10);
    lcd.print("  "); 
    setsecs();
}

int sette () {
//lcd.clear();
   lcd.setCursor (0,0);
    lcd.print ("Set Temperature");
    pushlength = pushlengthset;
    pushlength = getpushlength ();
    if (pushlength != pushlengthset) {
      return te1;
    }

    lcd.setCursor (0,1);
    knob.write(0);
    delay (50);
    knobval=knob.read();
    if (knobval < -1) {
      knobval = -1;
    }
    if (knobval > 1) {
      knobval = 1;
    }
    te1 = te1 + knobval;
    if (te1 < temin) {
      te1 = temin;
    }
    if (te1 > temax) {
      te1 = temax;
    }
    te = te1/2.;
    lcd.print (te,1);
    lcd.print(String(char(223)));
    lcd.print("C"); 

    dete = dete1/10.;
    lcd.setCursor (0,3);
    lcd.print ("Hysteresis: ");
    lcd.print (dete,1);
    lcd.print(String(char(223)));
    lcd.print("C");       
    sette();
}

int setdete () {
//lcd.clear();
   lcd.setCursor (0,0);
    lcd.print ("Set Hysteresys");
    pushlength = pushlengthset;
    pushlength = getpushlength ();
    if (pushlength != pushlengthset) {
      return dete1;
    }

    lcd.setCursor (0,1);
    knob.write(0);
    delay (50);
    knobval=knob.read();
    if (knobval < -1) {
      knobval = -1;
    }
    if (knobval > 1) {
      knobval = 1;
    }
    dete1 = dete1 + knobval;
    if (dete1 < detemin) {
      dete1 = detemin;
    }
    if (dete1 > detemax) {
      dete1 = detemax;
    }
    dete = dete1/10.;
    lcd.print (dete,1);
    lcd.print(String(char(223)));
    lcd.print("C");
    te = te1/2.;
    lcd.setCursor (0,3);
    lcd.print ("Temperature: ");
    lcd.print (te,1);
    lcd.print(String(char(223)));
    lcd.print("C");          
    setdete();
}




void ShortPush () {
  //This displays the calculated sunrise and sunset times when the knob is pushed for a short time.
lcd.clear();
mod++; 
delay(100);
lcd.clear();
}

void printbig(int i, int x)
{
  //  prints each segment of the big numbers

  if (i == 0) {
      lcd.setCursor(x,0);
      lcd.write(8);  
      lcd.write(1); 
      lcd.write(2);
      lcd.setCursor(x, 1);
      lcd.write(3);  
      lcd.write(4);  
      lcd.write(5);
    }
    else if  (i == 1) {
      lcd.setCursor(x,0);
      lcd.write(8);
      lcd.write(255);
      lcd.print(" ");
      lcd.setCursor(x, 1);
      lcd.print(" ");
      lcd.write(255);
      lcd.print(" ");
    }

    else if  (i == 2) {
      lcd.setCursor(x,0);
      lcd.write(1);
      lcd.write(6);
      lcd.write(2);
      lcd.setCursor(x, 1);
      lcd.write(3);
      lcd.write(7);
      lcd.write(4);
    }

    else if (i == 3) {
      lcd.setCursor(x,0);
      lcd.write(1);
      lcd.write(6);
      lcd.write(2);
      lcd.setCursor(x, 1);
      lcd.write(4);
      lcd.write(7);
      lcd.write(5); 
    }

    else if (i == 4) {
      lcd.setCursor(x,0);
      lcd.write(3);
      lcd.write(4);
      lcd.write(2);
      lcd.setCursor(x, 1);
      lcd.print("  ");
      lcd.write(5);
    }

    else if (i == 5) {
      lcd.setCursor(x,0);
      lcd.write(255);
      lcd.write(6);
      lcd.write(1);
      lcd.setCursor(x, 1);
      lcd.write(7);
      lcd.write(7);
      lcd.write(5);
    }

    else if (i == 6) {
      lcd.setCursor(x,0);
      lcd.write(8);
      lcd.write(6);
      lcd.print(" ");
      lcd.setCursor(x, 1);
      lcd.write(3);
      lcd.write(7);
      lcd.write(5);
    }

    else if (i == 7) {
      lcd.setCursor(x,0);
      lcd.write(1);
      lcd.write(1);
      lcd.write(5);
      lcd.setCursor(x, 1);
      lcd.print(" ");
      lcd.write(8);
      lcd.print(" ");
    }
    
    else if (i == 8) {
      lcd.setCursor(x,0);
      lcd.write(8);
      lcd.write(6);
      lcd.write(2);
      lcd.setCursor(x, 1);
      lcd.write(3);
      lcd.write(7);
      lcd.write(5);
    }

    else if (i == 9) {
      lcd.setCursor(x,0);
      lcd.write(8);
      lcd.write(6);
      lcd.write(2);
      lcd.setCursor(x, 1);
      lcd.print(" ");
      lcd.write(4);
      lcd.write(5);
    }

    else if (i == 10) {
      lcd.setCursor(x,0);
      lcd.write(8);
      lcd.write(6);
      lcd.write(2);
      lcd.setCursor(x, 1);
      lcd.print(" ");
   //   lcd.write(1);
      lcd.print(" ");
      lcd.print(" ");
    }
    
      if (i == 11) {
      lcd.setCursor(x,0);
      lcd.write(8);  
      lcd.write(1); 
      lcd.write(1);
      lcd.setCursor(x, 1);
      lcd.write(3);  
      lcd.write(4);  
      lcd.write(4);
     }
    
  } 

void promo()
{
  if (mod%2 == 0)
  {
   // print all the fix text - never changes
  lcd.setCursor(0, 0);
  lcd.print(" __ __"+String(char(165))+" __ __"+(char(165))+" __ __");  // char(165) is big point symbol
  lcd.setCursor(0, 1);
  lcd.print("      "+String(char(165))+"      "+(char(165))+"      ");
  }
  
// read all time and date and display
  DateTime now = RTC.now();
  h  = now.hour();
  m  = now.minute();
  s  = now.second();
  yr = now.year();
  mt = now.month();
  dt = now.day();
  dy = now.dayOfTheWeek();
  olds = s;
if (mod%2 == 0)
  {
// fill the display with all the data
  printbig((h%10),3);
  printbig((h/10),0);
  printbig((m%10),10);
  printbig((m/10),7);
  printbig((s%10),17);
  printbig((s/10),14);
  }
  else
  {
  lcd.setCursor(12, 3);
  lcd.print(h/10);
  lcd.print(h%10);
  lcd.print(":");
  lcd.print(m/10);
  lcd.print(m%10); 
  lcd.print(":");
  lcd.print(s/10);
  lcd.print(s%10);
  }
  if (mod%2 == 0)
  {
  lcd.setCursor(0, 2);
  lcd.print(DOW[dy]);
  lcd.print(","); 
  lcd.setCursor(4, 2);
  }
  else
  {
  lcd.setCursor(0, 3);  
  }
  lcd.print(dt/10);
  lcd.print(dt%10);
  lcd.print("-"); 
//  lcd.setCursor(7, 2);
  lcd.print(MTH[mt-1]);
  lcd.print("-");
  lcd.print(yr);
}

void readds()
  {
   // temperature
   if (intds)
    {         
     // Request temperature from the sensor
     sensors.requestTemperatures();

     // Read temperature in Celsius
     tempC = sensors.getTempCByIndex(0);
       //  tempC =  25.6;
       //  tempC =   5.6;
       //  tempC =  -3.8;
       //  tempC = -43.1;

       if (mod%2 == 0)
       {
          lcd.setCursor(0,3);
          lcd.print("[");
          lcd.print(te,1);
        //  lcd.print(String(char(223)));
        //  lcd.print("C");
        //  lcd.print("][");
          lcd.print("+");
          lcd.print(dete,1);
          lcd.print(String(char(223)));
          lcd.print("C");
          lcd.print("]");
          
          lcd.setCursor(13,3);
        if (tempC >= 10.) 
        {
          lcd.print(" ");
        }
        if ((tempC >= 0.) and (tempC < 10.)) 
        {
          lcd.print(" +");
        }
        if ((tempC > -10.) and (tempC < 0.)) 
        {
          lcd.print(" ");
        }
          lcd.print(tempC,1);
        lcd.print(String(char(223)));
        lcd.print("C");
        }
        else
        {
        tempC1 = (float)(10*abs(tempC));
        tz = tempC1/100;
        tu = (tempC1%100)/10;
        ts = (tempC1%100)%10;
          
        if ((tempC < 0) and (tz != 0))
        {
          lcd.setCursor(0,1);
          lcd.write(1);
          lcd.write(1);
          lcd.print(" ");
        }
        if (tz != 0)  
        printbig(tz, 3);
        else
        if (tempC < 0)
        {
          lcd.setCursor(3,1);
          lcd.write(1);
          lcd.write(1);
          lcd.print(" ");
        }
        printbig(tu, 7);
        lcd.setCursor(10,1);
        lcd.print(char(165));
        printbig(ts,11);
        printbig(10,14);                  // display degree                
        printbig(11,17);                  // display C  

          lcd.setCursor(0,2);
          lcd.print("[");
          lcd.print(te,1);
        //  lcd.print(String(char(223)));
        //  lcd.print("C");
        //  lcd.print("][");
          lcd.print("+");
          lcd.print(dete,1);
          lcd.print(String(char(223)));
          lcd.print("C");
          lcd.print("]");
        }
       }
     tpcitire = millis(); 
  }
