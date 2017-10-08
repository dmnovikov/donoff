 
 /*
   1MB flash sizee
   sonoff header
   1 - vcc 3v3
   2 - rx
   3 - tx
   4 - gnd
   5 - gpio 14
   esp8266 connections
   gpio  0 - button
   gpio 12 - relay
   gpio 15 - relay2
   gpio 13 - green led - active low
   gpio 14 - pin 5 on header xxxxxxdczz
   
*/
#define REVERSE_RELAY 0
#define TWO_SENSORS 1
#define ASK_TEMP 1
#define BUTTON_PIN   0
#define SONOFF_RELAY    5
#define SONOFF_LED      2  //( old:15)
#define IN_WIRE_BUS    14
#define OUT_WIRE_BUS 4

#define CURRENT_IN A0

#define WAIT_AS_MENU 1
#define WIFI_POINT "irozetka21"
#define AUTO_TEMP_ONOFF 1
#define CURRENT_CHECK 0
#define BUTTON_DELAY 50



#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>
#include <WidgetRTC.h>
#include <Bounce2.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <EEPROM.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <Ticker.h>
//#include <SimpleTimer.h>
 
static bool BLYNK_ENABLED = true;




SimpleTimer timer;
WiFiManager wifiManager;
#define EEPROM_SALT 12663

typedef struct {
  int   salt = EEPROM_SALT;
  char  blynkToken[33]  = "";
  char  blynkServer[33] = "blynk-cloud.com";
  char  blynkPort[6]    = "8442";
} WMSettings;

WMSettings settings;
WiFiManagerParameter custom_blynk_text("Blynk config. <br/> No token to disable.");
WiFiManagerParameter custom_blynk_token("blynk-token", "blynk token", settings.blynkToken, 33);
WiFiManagerParameter custom_blynk_server("blynk-server", "blynk server", settings.blynkServer, 33);
WiFiManagerParameter custom_blynk_port("blynk-port", "blynk port", settings.blynkPort, 6);

WidgetRTC rtc;
Bounce debouncer = Bounce();

  OneWire oneWireIn(IN_WIRE_BUS);
  
  OneWire oneWireOut(OUT_WIRE_BUS);
  
  DallasTemperature sensorsIn(&oneWireIn);

 DallasTemperature sensorsOut(&oneWireOut);
  


float temp1=0;
float temp2=0;
int wrn_level=30;
long start_ms=0;
long sensor_timer=0;
long change_ms=0;
int  sensor_ask=0;
static char str[12];
int auto_off=8;
boolean isFirstConnect=true;
int heater_type=0;
long temp_level1=0;
long temp_level2=0;
long start_bt=0;
long last_connect_attempt;
int hours_working=0;
int minutes_working=0;
int sec_working=0;

int mVperAmp = 66; // use 100 for 20A Module and 66 for 30A Module

double Voltage = 0;
double VRMS = 0;
double AmpsRMS = 0;

uint8_t modes[] = {
   0B00000000, //Светодиод выключен
   0B11111111, //Горит постоянно
   0B00001111, //Мигание по 0.5 сек
   0B00000001, //Короткая вспышка раз в секунду
   0B00000101, //Две короткие вспышки раз в секунду
   0B00010101, //Три короткие вспышки раз в секунду
   0B01010101,  //Частые короткие вспышки (4 раза в секунду)
   0B11111110
};

uint8_t  blink_loop = 0;
uint8_t  blink_mode = 0;
int blink_type=0;

#define BL_CONNECTED_ON 0
#define BL_CONNECTED_OFF 1
#define BL_CONNECTING 2
#define BL_CONFIG 6
#define BL_OFFLINE_ON 7
#define BL_OFFLINE_OFF 4



//#include <ArduinoOTA.h>


//for LED status

Ticker ticker;
int hronometer;

void tick()
{
   if(  modes[blink_type] & 1<<(blink_loop&0x07) ) digitalWrite(2, HIGH); 
   else  digitalWrite(2, LOW);
   blink_loop++;  
}

float getVPP()
{
  float result;
  
  int readValue;             //value read from the sensor
  int maxValue = 0;          // store max value here
  int minValue = 1024;          // store min value here
  
   uint32_t start_time = millis();
   while((millis()-start_time) < 20) //sample for 1 Sec
   {
       readValue = analogRead(CURRENT_IN);
       // see if you have a new maxValue
       if (readValue > maxValue) 
       {
           /*record the maximum sensor value*/
           maxValue = readValue;
       }
       if (readValue < minValue) 
       {
           /*record the maximum sensor value*/
           minValue = readValue;
       }
   }
   
   // Subtract min from max
   Serial.println("MAX="+String(maxValue)+"   MIN="+String(minValue));
   result = ((maxValue - minValue) * 3.3)/1024.0; //5
      
   return result;
 }



//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  //ticker.attach(0.2, tick);
}

void TturnOn() {
    Serial.println("Try true ON");
    start_ms=millis();
    int s=LOW;
    if(REVERSE_RELAY){
      digitalWrite(SONOFF_RELAY, s);
    }else{
      digitalWrite(SONOFF_RELAY, !s);
    }
    blink_type=BL_CONNECTED_ON;
    
    //digitalWrite(SONOFF_LED, LOW); // led is active low
    //Blynk.virtualWrite(6, 220);
    //Blynk.virtualWrite(12, "00:00:00");
}

void TturnOff() {
    Serial.println("Try true OFF");
    start_ms=0;
    int s=HIGH;
    if(REVERSE_RELAY){
      digitalWrite(SONOFF_RELAY, s);
    }else{
      digitalWrite(SONOFF_RELAY, !s);
    }
    //digitalWrite(SONOFF_LED, HIGH); // led is active low
    blink_type=BL_CONNECTED_OFF;
    //Blynk.virtualWrite(6, 0);  
    //Blynk.virtualWrite(12, "OFF");
   
}


//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


void toggle2() {
if (start_ms==0) {
    TturnOn();
    Serial.println("Try turn on");
  }
  else if (start_ms!=0){
    TturnOff();
    Serial.println("Try turn off");
      if(AUTO_TEMP_ONOFF && heater_type!=0){
      heater_type=0;
      //Blynk.virtualWrite(14, 1);  //set "off" of heater menu
      //Blynk.virtualWrite(16, 0);  //set 0 to check
      
    }
  }
  
}


void restart() {
  ESP.reset();
  delay(1000);
}

void reset() {
  //reset settings to defaults
  /*
    WMSettings defaults;
    settings = defaults;
    EEPROM.begin(512);
    EEPROM.put(0, settings);
    EEPROM.end();
  */
  //reset wifi credentials
  WiFi.disconnect();
  delay(1000);
  ESP.reset();
  delay(1000);
}

//toggle - button
BLYNK_WRITE(2) {
  int a = param.asInt();
  Serial.println("Blynk Button pressed:"+String(a));
  //Serial.println(a);
if(a==1){  
if (start_ms==0) {
    TturnOn();
    Blynk.virtualWrite(6, 220);
    Blynk.virtualWrite(12, "00:00:00");
  }
  else if (start_ms!=0){
    TturnOff();
    Blynk.virtualWrite(6, 0);  
    Blynk.virtualWrite(12, "OFF");
    
     if(AUTO_TEMP_ONOFF && heater_type!=0){
      heater_type=0;
      Blynk.virtualWrite(14, 1);  //set "off" of heater menu
      Blynk.virtualWrite(16, 0);  //set 0 to check
      
    }
  }
}
}


//switch - button
BLYNK_WRITE(1) {
  int a = param.asInt();
  Serial.print("SW pressed:");
  Serial.println(a);
if(a==1){  
  if (start_ms==0) {
    Serial.println("Try SW turn on");
    TturnOn();
    
  }
  }
 if(a==0){
    if(start_ms!=0){
      Serial.println("Try SW turn off");
      TturnOff();
     
    }
  }
  
}

 int in_working_interval=0;
 int temp_ask_on=0;

//SW with temp and time check
BLYNK_WRITE(3) {
  int a = param.asInt();
  Serial.print("SW pressed:");
  Serial.println(a);
if(a==1){  
  if (start_ms==0) {
    Serial.println("Try SW turn on");
    if(in_working_interval) TturnOn();
    temp_ask_on=1;
    
  }
  }
 if(a==0){
    if(start_ms!=0 || in_working_interval==0){
      Serial.println("Try SW turn off");
      TturnOff();
      temp_ask_on=0;
    }
  }

 
}

//check working_time from Blynk

BLYNK_WRITE(4) {
  int a = param.asInt();
  if(a==1){
    in_working_interval=1;
    if(temp_ask_on==1) TturnOn();
    
  }
  if(a==0){
    in_working_interval=0;
    if(start_ms!=0) TturnOff();
  }

}

//temp level
BLYNK_WRITE(15) {
  int a = param.asInt();
  temp_level1=a;
  Blynk.virtualWrite(V17,temp_level1);
}



//auto_off read param
BLYNK_WRITE(10) {
  int a = param.asInt();
  if(WAIT_AS_MENU) auto_off=a-1;
  else auto_off=a;
  Serial.print("Set auto_off =");
  Serial.println(auto_off); 
    Blynk.virtualWrite(V9,auto_off);
}

//auto_off read param
BLYNK_WRITE(14) {
  int a = param.asInt();
  heater_type=a-1;
  Serial.print("Set heater_type =");
  Serial.println(heater_type); 
  //turn off if no need auto onoff
  if(heater_type==0 && start_ms!=0){
    TturnOff();
  }
  Blynk.virtualWrite(V16,heater_type);
}


BLYNK_CONNECTED() {
if (isFirstConnect) {
// Request Blynk server to re-send latest values for all pins
//Console.println(F("Sync"));
Blynk.syncVirtual(V10);
Blynk.syncVirtual(V15);
Blynk.syncVirtual(V14);
Serial.println("Start RTC");
rtc.begin();
isFirstConnect = false;
}
}

// ********************************** SETUP ***************************************************

void setup()
{
  Serial.begin(115200);
  
  pinMode(SONOFF_LED, OUTPUT);
  ticker.attach(0.25, tick);
    
  // start ticker with 0.5 because we start in AP mode and try to connect
  blink_type=BL_CONNECTING;

/*
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  int setup_button=digitalRead(BUTTON_PIN);
  if(setup_button){
    Serial.println("***Setup button = ON");
  } else{
    Serial.println("***Setup button = OFF");
  }
*/

  const char *hostname = WIFI_POINT;

 /*
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //timeout - this will quit WiFiManager if it's not configured in 3 minutes, causing a restart
  wifiManager.setConfigPortalTimeout(180);
*/

  //custom params
  EEPROM.begin(512);
  EEPROM.get(0, settings);
  EEPROM.end();

  if (settings.salt != EEPROM_SALT) {
    Serial.println("Invalid settings in EEPROM, trying with defaults");
    WMSettings defaults;
    settings = defaults;
  }

  Serial.println("Parameter1="+String(settings.blynkToken));
  Serial.println("Parameter2="+String(settings.blynkServer));
  Serial.println("Parameter3="+String(settings.blynkPort));

  wifiManager.addParameter(&custom_blynk_text);
  wifiManager.addParameter(&custom_blynk_token);
  wifiManager.addParameter(&custom_blynk_server);
  wifiManager.addParameter(&custom_blynk_port);
/*
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
*/
  //wifiManager.startConfigPortal("OnDemandAP");

  //WiFi.softAPdisconnect();
  //WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  //delay(100);

 if (WiFi.SSID()) {
      Serial.println("Using last saved values, should be faster");
      //trying to fix connection in progress hanging

      WiFi.begin();
    } else {
      Serial.println("No saved credentials");
    }
  
  
/*
  if (!wifiManager.autoConnect(hostname)) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

*/

/*
  

  //Serial.println(custom_blynk_token.getValue());
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("Saving config");

    strcpy(settings.blynkToken, custom_blynk_token.getValue());
    strcpy(settings.blynkServer, custom_blynk_server.getValue());
    strcpy(settings.blynkPort, custom_blynk_port.getValue());

    Serial.println(settings.blynkToken);
    Serial.println(settings.blynkServer);
    Serial.println(settings.blynkPort);

    EEPROM.begin(512);
    EEPROM.put(0, settings);
    EEPROM.end();
  }
*/
  //config blynk
 // if (strlen(settings.blynkToken) == 0) {
 //   BLYNK_ENABLED = false;
 //   Serial.println("***Blynk disabled");
 // }
 // if (BLYNK_ENABLED) {
 
    Blynk.config(settings.blynkToken, settings.blynkServer, atoi(settings.blynkPort));
    Blynk.connect();
    last_connect_attempt=millis();
    if(Blynk.connected()) Serial.println("*** wifi - ok, Blynk - connected");
    
    
 // }

  //OTA
  
  /*
  ArduinoOTA.onStart([]() {
    Serial.println("Start OTA");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.begin();
  */


  if(WL_CONNECTED && Blynk.connected() ){
  //if you get here you have connected to the WiFi
   Serial.println("connected...yeey :)");
   //finish
   blink_mode=1;
  }
  
  //setup button
  //pinMode(SONOFF_BUTTON, INPUT_PULLUP);
  //attachInterrupt(SONOFF_BUTTON, toggleStateN, CHANGE);

  //setup relay
  pinMode(SONOFF_RELAY, OUTPUT);

  //setup reading current
  pinMode(CURRENT_IN, INPUT);


  TturnOff();
  //turnOn();
  
  Serial.println("done setup");
  timer.setInterval(5000L, sendUptime);


  
   if(ASK_TEMP) {
    sensorsIn.begin();
    if(TWO_SENSORS) sensorsOut.begin();
   }
   
   start_ms=0;
   hronometer=1;
   sensor_timer=millis();
   sensor_ask=0;
   //Blynk.virtualWrite(V10, auto_off);
   //Blynk.syncVirtual(V10);
   //BLYNK_WRITE(10);

   Blynk.virtualWrite(V12, "OFF");
   Blynk.virtualWrite(V6, 0);
   //Blynk.virtualWrite(V3, 0);
   Blynk.virtualWrite(V16,heater_type);
   
  debouncer.attach(BUTTON_PIN);
  debouncer.interval(5);
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             
   
}

//****************************************** sendUPtime ***************************

void sendUptime()
{
  if(start_bt!=0){
    //Serial.print(cmd);
    Serial.println("Button pressed, DO Nothing");
    return;
  }

  get_time_working();
  
  
  if(!Blynk.connected()){
    Serial.println("No BLYNK, OFFLINE MODE");
    ticker.detach();
    sensorsIn.requestTemperatures();
    temp1=sensorsIn.getTempCByIndex(0);
    Serial.println("OFFLINE ASK T1->"+String(temp1)); 
     ticker.attach(0.25,tick);

    // Alert turn OFF
    if(temp1>75) TturnOff();

    //Auto turn off offline mode too
     if(auto_off!=0 && hours_working>=auto_off) TturnOff();

    if(!Blynk.connected() && millis()-last_connect_attempt>30000){
      Serial.println("*** Try to connect again");
      Blynk.connect(); 
      last_connect_attempt=millis();
      
    }

/*
    if(Blynk.connected()){
    if(start_ms!=0) blink_type=BL_CONNECTED_ON;
    else blink_type=BL_CONNECTED_OFF;
    //Serial.println("***End offline tick");
  }

*/
  
  if(!Blynk.connected()){                
    //Serial.println("***Begin offline tick");
    if(start_ms!=0) blink_type=BL_OFFLINE_ON;
    else blink_type=BL_OFFLINE_OFF;
  }

    return;
  }

  if(start_ms!=0) blink_type=BL_CONNECTED_ON;
  else blink_type=BL_CONNECTED_OFF;
    
  if(ASK_TEMP){
   ticker.detach();
   if(sensor_ask==0){
    sensorsIn.requestTemperatures();
    temp1=sensorsIn.getTempCByIndex(0);
    Serial.println("ASK T1->"+String(temp1)); 
    dtostrf(temp1,5,2,str);     
    Blynk.virtualWrite(V11, str);       
   }
  
   if(sensor_ask==1 && TWO_SENSORS){
    //Serial.print(sensor_ask);
    //Serial.print(" Sensor 2 ask:");
    sensorsOut.requestTemperatures();
    temp2=sensorsOut.getTempCByIndex(0);  
    Serial.println("ASK T2->"+String(temp2)); 
    dtostrf(temp2,5,2,str); 
    Blynk.virtualWrite(V8, str);
   }
   sensor_ask=!sensor_ask;
   ticker.attach(0.25, tick);
  }

  if(start_ms==0){
    Blynk.virtualWrite(V12,"OFF");
    Blynk.virtualWrite(6, 0);
    hours_working=0;
  }else{
    /*
    long uptime=millis()-start_ms;
    long wt=uptime/1000;
    //static char str[12];
    long h = wt / 3600;
    hours_working=h;
    wt = wt % 3600;
    int m = wt / 60;
    int s = wt % 60;
    */
    sprintf(str, "%02d:%02d:%02d",  hours_working, minutes_working, sec_working);    
    Blynk.virtualWrite(V12,str);
    Blynk.virtualWrite(6, 220);
    if(auto_off!=0 && hours_working>=auto_off){
      // ******************************** timer turn off
      // m - minutes for testing
      // h - hours for working
      //Serial.print("Check auto off =");
      //Serial.print(auto_off);
      Blynk.virtualWrite(9,auto_off);
      // Turn off by timer
      TturnOff();
      //auto off battary\cooler mode
      if(AUTO_TEMP_ONOFF && heater_type!=0){
         heater_type=0;
         Blynk.virtualWrite(14, 1);  //set "off" of heater menu
         Blynk.virtualWrite(16, 0);  //set 0 to check
      
      }
     }
     else if(auto_off!=0 && hours_working<auto_off){
      Blynk.virtualWrite(9,auto_off-hours_working);
             
     }
     
  }
  
  if(AUTO_TEMP_ONOFF){
       //Serial.println("Try auto_onoff");
      auto_onoff_temp();
  }
   if(hronometer==1)Blynk.virtualWrite(V13,"0");
   else Blynk.virtualWrite(V13,"255");
 
    hronometer=!hronometer;
    
  //String currentTime = String(hour()) + ":" + minute();; 
  //String currentDate = String(day()) + " " + month() + " " + year();
  // Send time to the App

  /*
  Serial.print("Current time: ");
  Serial.print(currentTime);
  Serial.print(" ");
  Serial.print(currentDate);
  Serial.println();
  */
//  Blynk.virtualWrite(V20, currentTime);
  // Send date to the App
  //Blynk.virtualWrite(V21, currentDate);

  if(CURRENT_CHECK && start_ms!=0) {
      Voltage = getVPP();
       VRMS = (Voltage/2) *0.707; 
       AmpsRMS = (VRMS * 1000)/mVperAmp;
       if(AmpsRMS<=0.15) AmpsRMS=0;
       static char outstr[5];
       dtostrf(AmpsRMS,5, 1, outstr);
       if(AmpsRMS==0) strcpy(outstr,"0+ ");
       Blynk.virtualWrite(V21, outstr);
       Serial.println("CURRENT="+String(AmpsRMS));
  }
 
}

void autooff(){
  
}

void get_time_working(){
  if(start_ms!=0){
      long uptime=millis()-start_ms;
      long wt=uptime/1000;
      //static char str[12];
      long h = wt / 3600;
      wt = wt % 3600;
      int m = wt / 60;
      int s = wt % 60;
      hours_working=h;
      minutes_working=m;
      sec_working=s;
  }else{
      hours_working=0;
      minutes_working=0;
      sec_working=0;
  }
}

void auto_onoff_temp(){
  
  if(heater_type==0) return; //no
  if(temp1==-127) return;
  if(temp1==85) return;

  switch (heater_type){
     case 1:   //batterey
       
        if(temp1<temp_level1){
          if(start_ms==0){
             Serial.println("BATTEREY: try to auto ON");
             TturnOn();
          }
        }
        else if(temp1>=temp_level1){
          if(start_ms!=0){
            Serial.println("BATTEREY: try to auto OFF");
            TturnOff(); 
          } 
        }
        break;

     case 2:   //condicioner
        if(temp1>=temp_level1){
          if(start_ms==0){
             Serial.println("COND: try to auto ON");
             TturnOn();
          }
        }
        else if(temp1<temp_level1){
          if(start_ms!=0){
            Serial.println("COND: try to auto OFF");
            TturnOff(); 
          } 
        }      
        break;
    
  }
}

int bl=1;
int reconfigured=0;

 
void loop()
{
 
  if (WiFi.status()==WL_CONNECTED && Blynk.connected()) {
    Blynk.run();
  }

  timer.run();

  
  
  debouncer.update();
  if ( debouncer.fell()  ) {; 
    Serial.println( "down");
    start_bt=millis();
  
  }

   if ( debouncer.rose()  ) {;
    long duration=millis()-start_bt;
    start_bt=0;
    Serial.println( "up"+String(duration));
    if(duration <BUTTON_DELAY){           
            Serial.println("FAIL PRESS, DO NOTHING");
          }else if (duration>=BUTTON_DELAY && duration < 2000) {
            Serial.println("--->short press - toggle relay");
            toggle2();
          //} else if (duration >=2000 && duration < 5000) {
          //  Serial.print(duration);
          //  Serial.println("-->medium press - restart");
          //  restart();
          } else if (duration>=3000) {


 // ****** Config mode **************************************************************************************
            timer.disableAll();
            TturnOff();
            blink_type=BL_CONFIG;
            //ticker.attach(0.2, tick);
            Serial.println("--->long press - reset settings");
            wifiManager.startConfigPortal(WIFI_POINT);

           Serial.println("Saving config");

           strcpy(settings.blynkToken, custom_blynk_token.getValue());
           strcpy(settings.blynkServer, custom_blynk_server.getValue());
           strcpy(settings.blynkPort, custom_blynk_port.getValue());

            Serial.println("NEW Parameter1="+String(settings.blynkToken));
            Serial.println("NEW Parameter2="+String(settings.blynkServer));
            Serial.println("NEW Parameter3="+String(settings.blynkPort));


            EEPROM.begin(512);
            EEPROM.put(0, settings);
            EEPROM.end();
        
            // CHECK !
           WiFi.mode(WIFI_STA);
           Blynk.config(settings.blynkToken, settings.blynkServer, atoi(settings.blynkPort));
           blink_type=BL_CONNECTING;
           Blynk.connect();
           timer.enableAll();  
   }
             //ESP.restart();
      
          
  
  }
  


}
