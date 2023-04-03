/* Modified from https://github.com/pchretien/fibo */

#include "WiFiConnect.h"
#include "RTClib.h"

#include <WiFiClient.h>                      
#ifdef ARDUINO_ARCH_ESP8266  
#include <ESP8266HTTPClient.h>
#else  
#include <HTTPClient.h>
#endif

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <time.h>
#include <Timezone.h>    // https://github.com/JChristensen/Timezone

#include <Adafruit_NeoPixel.h>

#define STRIP_PIN 2
#define MAX_MODES 3
#define MAX_PALETTES 10
#define TOTAL_PALETTES 10
#define CLOCK_PIXELS 5
#define BTN1 13
#define BTN2 12         

WiFiConnect wc;
RTC_DS1307 rtc;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// For internet connection
WiFiClient client;
HTTPClient http;


// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(9, STRIP_PIN, NEO_RGB + NEO_KHZ800);

byte bits[CLOCK_PIXELS];

int palette = 0;
boolean on = true;
uint8_t previousminute = 0;
byte oldHours = 0;
byte oldMinutes = 0;

uint32_t black = strip.Color(0,0,0);
uint32_t colors[TOTAL_PALETTES][4] = 
  {
    {
      // #1 RGB
      strip.Color(255,255,255),    // off
      strip.Color(255,10,10),  // hours
      strip.Color(10,255,10),  // minutes
      strip.Color(10,10,255) // both;
    }, 
    {
      // #2 Mondrian
      strip.Color(255,255,255),    // off
      strip.Color(255,10,10),  // hours
      strip.Color(248,222,0),  // minutes
      strip.Color(10,10,255) // both;
    }, 
    {
      // #3 Basbrun
      strip.Color(255,255,255),    // off
      strip.Color(80,40,0),  // hours
      strip.Color(20,200,20),  // minutes
      strip.Color(255,100,10) // both;
    },
    {
      // #4 80's
      strip.Color(255,255,255),    // off
      strip.Color(245,100,201),  // hours
      strip.Color(114,247,54),  // minutes
      strip.Color(113,235,219) // both;
    }
    ,
    {
      // #5 Pastel
      strip.Color(255,255,255),    // off
      strip.Color(255,123,123),  // hours
      strip.Color(143,255,112),  // minutes
      strip.Color(120,120,255) // both;
    }
    ,
    {
      // #6 Modern
      strip.Color(255,255,255),    // off
      strip.Color(212,49,45),  // hours
      strip.Color(145,210,49),  // minutes
      strip.Color(141,95,224) // both;
    }
    ,
    {
      // #7 Cold
      strip.Color(255,255,255),    // off
      strip.Color(209,62,200),  // hours
      strip.Color(69,232,224),  // minutes
      strip.Color(80,70,202) // both;
    }
    ,
    {
      // #8 Warm
      strip.Color(255,255,255),    // off
      strip.Color(237,20,20),  // hours
      strip.Color(246,243,54),  // minutes
      strip.Color(255,126,21) // both;
    }
    ,
    {
      //#9 Earth
      strip.Color(255,255,255),    // off
      strip.Color(70,35,0),  // hours
      strip.Color(70,122,10),  // minutes
      strip.Color(200,182,0) // both;
    }
    ,
    {
      // #10 Dark
      strip.Color(255,255,255),    // off
      strip.Color(211,34,34),  // hours
      strip.Color(80,151,78),  // minutes
      strip.Color(16,24,149) // both;
    }
  }; 
 
WiFiUDP ntpUDP;
 
// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
// NTPClient timeClient(ntpUDP);
 
// You can specify the time server pool and the offset, (in seconds)
// additionaly you can specify the update interval (in milliseconds).
int GTMOffset = 0; // SET TO UTC TIME
NTPClient timeClient(ntpUDP, "au.pool.ntp.org", GTMOffset*60*60, 60*60*1000);
 
// Australia Eastern Time Zone (Sydney, Melbourne)
TimeChangeRule aEDT = {"AEDT", First, Sun, Oct, 2, 660};    // UTC + 11 hours
TimeChangeRule aEST = {"AEST", First, Sun, Apr, 3, 600};    // UTC + 10 hours
Timezone ausET(aEDT, aEST);


void configModeCallback(WiFiConnect *mWiFiConnect) {
  Serial.println("Entering Access Point");
}

void saveConfigCallback() {
  Serial.println("Saving WiFi details...");
}


void startWiFi(boolean showParams = true) {
 
  wc.setDebug(true);
  
  /* Set our callbacks */
  wc.setAPCallback(configModeCallback);
  wc.setSaveConfigCallback(saveConfigCallback);
  
  //wc.resetSettings(); //helper to remove the stored wifi connection, comment out after first upload and re upload
    /*
       AP_NONE = Continue executing code
       AP_LOOP = Trap in a continuous loop - Device is useless
       AP_RESET = Restart the chip
       AP_WAIT  = Trap in a continuous loop with captive portal until we have a working WiFi connection
    */
    if (!wc.autoConnect()) { // try to connect to wifi
      /* We could also use button etc. to trigger the portal on demand within main loop */
      wc.startConfigurationPortal(AP_NONE);//if not connected show the configuration portal
      Serial.println("Could not auto connect to AP");
    }
    
}


/**
 * Input time in epoch format and return tm time format
 * by Renzo Mischianti <www.mischianti.org> 
 */
static tm getDateTimeByParams(long time){
    struct tm *newtime;
    const time_t tim = time;
    newtime = localtime(&tim);
    return *newtime;
}
/**
 * Input tm time format and return String with format pattern
 * by Renzo Mischianti <www.mischianti.org>
 */
static String getDateTimeStringByParams(tm *newtime, char* pattern = (char *)"%d/%m/%Y %H:%M:%S"){
    char buffer[30];
    strftime(buffer, 30, pattern, newtime);
    return buffer;
}
 
/**
 * Input time in epoch format format and return String with format pattern
 * by Renzo Mischianti <www.mischianti.org> 
 */
static String getEpochStringByParams(long time, char* pattern = (char *)"%d/%m/%Y %H:%M:%S"){
//    struct tm *newtime;
    tm newtime;
    newtime = getDateTimeByParams(time);
    return getDateTimeStringByParams(&newtime, pattern);
}

 void setTimeFibo(byte hours, byte minutes)
{
  if(oldHours == hours && oldMinutes/5 == minutes/5)
    return;
    
  oldHours = hours;
  oldMinutes = minutes;
  
  for(int i=0; i<CLOCK_PIXELS; i++)
    bits[i] = 0;
    
  setBits(hours, 0x01);
  setBits(minutes/5, 0x02);

  updateClock();
}

void updateClock(){
  for(int i=0; i<CLOCK_PIXELS; i++)
  {   
    setPixel(i, colors[palette][bits[i]]);
    strip.show();
  }
  
}
void setBits(byte value, byte offset)
{
  switch(value)
  {
    case 1:
      switch(random(2))
      {
        case 0:
          bits[0]|=offset;
          break;
        case 1:
          bits[1]|=offset;
          break;
      }
      break;
    case 2:
      switch(random(2))
      {
        case 0:
          bits[2]|=offset;
          break;
        case 1:
          bits[0]|=offset;
          bits[1]|=offset;
          break;
      }
      break;
    case 3:
      switch(random(3))
      {
        case 0:
          bits[3]|=offset;
          break;
        case 1:
          bits[0]|=offset;
          bits[2]|=offset;
          break;
        case 2:
          bits[1]|=offset;
          bits[2]|=offset;
          break;
      }
      break;
    case 4:
      switch(random(3))
      {
        case 0:
          bits[0]|=offset;
          bits[3]|=offset;
          break;
        case 1:
          bits[1]|=offset;
          bits[3]|=offset;
          break;
        case 2:
          bits[0]|=offset;
          bits[1]|=offset;
          bits[2]|=offset;
          break;
      }
      break;
    case 5:
      switch(random(3))
      {
        case 0:
          bits[4]|=offset;
          break;
        case 1:
          bits[2]|=offset;
          bits[3]|=offset;
          break;
        case 2:
          bits[0]|=offset;
          bits[1]|=offset;
          bits[3]|=offset;
          break;
      }
      break;
    case 6:
      switch(random(4))
      {
        case 0:
          bits[0]|=offset;
          bits[4]|=offset;
          break;
        case 1:
          bits[1]|=offset;
          bits[4]|=offset;
          break;
        case 2:
          bits[0]|=offset;
          bits[2]|=offset;
          bits[3]|=offset;
          break;
        case 3:
          bits[1]|=offset;
          bits[2]|=offset;
          bits[3]|=offset;
          break;
      }
      break;
    case 7:
      switch(random(3))
      {
        case 0:
          bits[2]|=offset;
          bits[4]|=offset;
          break;
        case 1:
          bits[0]|=offset;
          bits[1]|=offset;
          bits[4]|=offset;
          break;
        case 2:
          bits[0]|=offset;
          bits[1]|=offset;
          bits[2]|=offset;
          bits[3]|=offset;
          break;
      }
      break;
    case 8:
      switch(random(3))
      {
        case 0:
          bits[3]|=offset;
          bits[4]|=offset;
          break;
        case 1:
          bits[0]|=offset;
          bits[2]|=offset;
          bits[4]|=offset;
          break;
        case 2:
          bits[1]|=offset;
          bits[2]|=offset;
          bits[4]|=offset;
          break;
      }      
      break;
    case 9:
      switch(random(2))
      {
        case 0:
          bits[0]|=offset;
          bits[3]|=offset;
          bits[4]|=offset;
          break;
        case 1:
          bits[1]|=offset;
          bits[3]|=offset;
          bits[4]|=offset;
          break;
      }      
      break;
    case 10:
      switch(random(2))
      {
        case 0:
          bits[2]|=offset;
          bits[3]|=offset;
          bits[4]|=offset;
          break;
        case 1:
          bits[0]|=offset;
          bits[1]|=offset;
          bits[3]|=offset;
          bits[4]|=offset;
          break;
      }            
      break;
    case 11:
      switch(random(2))
      {
        case 0:
          bits[0]|=offset;
          bits[2]|=offset;
          bits[3]|=offset;
          bits[4]|=offset;      
          break;
        case 1:
          bits[1]|=offset;
          bits[2]|=offset;
          bits[3]|=offset;
          bits[4]|=offset; 
          break;
      }          

      break;
    case 12:
      bits[0]|=offset;
      bits[1]|=offset;
      bits[2]|=offset;
      bits[3]|=offset;
      bits[4]|=offset;        
      
      break;
  }
}

void setPixel(byte pixel, uint32_t color)
{
  if(!on)
    return;
  
  switch(pixel)
  {
    case 0:
      strip.setPixelColor(0, color);
      break;
    case 1:
      strip.setPixelColor(1, color);
      break;
    case 2:
      strip.setPixelColor(2, color);
      break;
    case 3:
      strip.setPixelColor(3, color);
      strip.setPixelColor(4, color);
      break;
    case 4:
      strip.setPixelColor(5, color);
      strip.setPixelColor(6, color);
      strip.setPixelColor(7, color);
      strip.setPixelColor(8, color);
      strip.setPixelColor(9, color);
      break;
  };
}

ICACHE_RAM_ATTR void btnPushed(){
  palette++;  
  if (palette > 9 )
    palette = 0;
  updateClock();
}

void setup(){
  Serial.begin(115200);
  Serial.println("");
  
  //WiFi.begin(ssid, password);

  Wire.begin();
  rtc.begin();
    
  attachInterrupt(digitalPinToInterrupt(BTN1), btnPushed, FALLING);
  
  pinMode(BTN1,INPUT_PULLUP);
  pinMode(BTN2,OUTPUT);
  digitalWrite(BTN2, LOW);
  
  strip.begin();
  strip.show();

  //DateTime rtcNow = rtc.now();
  //setTime(ausET.toLocal(rtcNow.unixtime()));
  
  if (!digitalRead(BTN1)){
    startWiFi();   
    // Wifi Dies? Start Portal Again
    //if (WiFi.status() != WL_CONNECTED) {
    //  if (!wc.autoConnect()) wc.startConfigurationPortal(AP_NONE);
    //} 
    if (WiFi.status() == WL_CONNECTED) {
      timeClient.begin();
      delay ( 1000 );

      if (timeClient.update()){
        Serial.println ( "Adjust local clock" );
        unsigned long epoch = timeClient.getEpochTime();
        //setTime(epoch);
        rtc.adjust(epoch);
      }else{
        Serial.println ( "NTP Update not WORK!!" );
      }  
    }
  }
  getTime();
}

void getTime(){
  DateTime rtcNow = rtc.now();
  rtcNow = ausET.toLocal(rtcNow.unixtime());
   
  setTimeFibo( rtcNow.hour()%12, rtcNow.minute());
  
  Serial.print(rtcNow.year(), DEC);
  Serial.print('/');
  Serial.print(rtcNow.month(), DEC);
  Serial.print('/');
  Serial.print(rtcNow.day(), DEC);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[rtcNow.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(rtcNow.hour(), DEC);
  Serial.print(':');
  Serial.print(rtcNow.minute(), DEC);
  Serial.print(':');
  Serial.print(rtcNow.second(), DEC);
  Serial.print("  :  "); Serial.print(palette);
  Serial.println();
  //Serial.println(getEpochStringByParams(ausET.toLocal( now() ))); 
}

void loop() {
 
  getTime();
  delay(1000);
}
