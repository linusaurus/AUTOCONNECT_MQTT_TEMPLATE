



#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <Atm_esp8266.h>

//flag for saving data
bool shouldSaveConfig = false;

//define your default values here, if there are different values in config.json, they are overwritten.
//=====================================================================================================
#define mqtt_server       "192.168.10.62"
#define mqtt_port         "1883"
#define mqtt_user         ""
#define mqtt_pass         ""
#define outTopic = "STATUS"
#define inTopic = "DLIN"
//====================================================================================================

WiFiClient espClient;
PubSubClient client(espClient);

Atm_led led_2;
Atm_led led_12;
Atm_led led_13;
Atm_led led_14;
Atm_led led_15;

//====================================================================================================

//Main Callback handler for all incoming MQTT commands

//
void callback(char* topic, byte* payload, unsigned int length) {
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] "); 
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
 
  if ((char)payload[0] == '1') {
    
      led_2.trigger(led_2.EVT_TOGGLE_BLINK);
         
  } else if ((char)payload[0] == '2') {    
      
       led_12.trigger(led_12.EVT_TOGGLE_BLINK);
    
  } else if ((char)payload[0] == '3') {
    
        led_13.trigger(led_13.EVT_TOGGLE_BLINK); 
  }
   else if ((char)payload[0] == '4') {
    
        led_14.trigger(led_14.EVT_TOGGLE_BLINK); 
  }

    else if ((char)payload[0] == '5') {
    
        led_15.trigger(led_15.EVT_TOGGLE_BLINK); 
  }
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  // clean FS for testing 
  // SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

//-------------------------------------------------------------------------------------
//----------------------------ArduinoOTA Functions Block-------------------------------
  ArduinoOTA.onStart([]() {
    Serial.println("StartOTA");
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
  ArduinoOTA.begin();
//------------------------------------------------------------------------------------ 

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_pass, json["mqtt_pass"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 20);
  WiFiManagerParameter custom_mqtt_pass("pass", "mqtt pass", mqtt_pass, 20);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

// Reset Wifi settings for testing  
//wifiManager.resetSettings();

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
//wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimum quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connection Success :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_pass, custom_mqtt_pass.getValue());
 // strcpy(blynk_token, custom_blynk_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_pass"] = mqtt_pass;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  
//client.setServer(mqtt_server, 12025);
// this strange assignment of port needed to be changed
  const uint16_t mqtt_port_x = 1883; 
  client.setServer(mqtt_server, mqtt_port_x);
  client.setCallback(callback);

 //================================================================
 // Pin Assignment
  pinMode(2,OUTPUT);
  pinMode(12,OUTPUT);
  pinMode(13,OUTPUT);
  pinMode(14,OUTPUT);
  pinMode(15,OUTPUT);


  led_2.begin(2).blink(40,650);
  led_12.begin(12).blink(100,800);
  led_13.begin(13).blink(40,250);
  led_14.begin(14).blink(40,250);
  led_15.begin(15).blink(40,250);
 
 //================================================================ 
}


void reconnect() {
  // Loop until we're reconnected
  // char CHIP_ID[8];
  // String(ESP.getChipId()).toCharArray(CHIP_ID,8);
  
  
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
     // Create a random client ID
      String clientId = "ESP8266Client-";
      clientId += String(random(0xffff), HEX);
      Serial.println(clientId);
      if (client.connect(clientId.c_str())) {
       Serial.println("connected");
       client.publish("STATUS", "Connected-"); 
       client.subscribe("TEST");
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
   }
  
  }


void loop() {
 
   ArduinoOTA.handle();
   
  if (!client.connected()) {
    reconnect();
  }


  client.loop();
  automaton.run();
  
}
