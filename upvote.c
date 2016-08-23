#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

#define DEBUG   //If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.

#ifdef DEBUG    //Macros are usually in all capital letters.
  #define DEBUG_PRINT(...)    Serial.print(__VA_ARGS__)     //DPRINT is a macro, debug print
  #define DEBUG_PRINTLN(...)  Serial.println(__VA_ARGS__)   //DPRINTLN is a macro, debug print with new line
#else
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
#endif

#define STARTTIME 60
#define BRIGHTNESS 128
#define ANIMSPEED 10

#define NUMPIXELS 3
#define BUTTONPIN 2
#define PIXELPIN  0
#define headerHtml "<!DOCTYPE HTML><html><meta name='viewport' content='width=device-width, initial-scale=1.0'><head><style>table, th, td {border: 3px outset #545454;border-collapse:collapse;padding:10px;text-align:center;}</style></head><body style='background-color:#121211;color:#9896B3;font-family:Verdana,Geneva,sans-serif;font-weight:100;'><center><div style='display:table;position:absolute;height:100%;width:100%;text-align:center;'><div style='display:table-cell;vertical-align:middle;'><div style='margin-left:auto;margin-right:auto;width:90%;max-width:550px;'>"
#define footerHtml "</div></div></div></body></html>\n"

long COMMENTMAXID = 8675309;
String clientID = "put your client id from imgur here";

IPAddress myIp(1, 1, 1, 1);
IPAddress myGateway(1, 1, 1, 2);
IPAddress mySubnet(255, 255, 255, 0);

ESP8266WebServer server(80);
WiFiClientSecure client;

const char* host = "api.imgur.com";
const int httpsPort = 443;

String eTag;
String userString;
long rep;

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIXELPIN, NEO_RGB + NEO_KHZ800);

void upArrow() {
  DEBUG_PRINTLN("Up Arrow Called");
  for (int i = STARTTIME; i < BRIGHTNESS; i++){
    pixels.setPixelColor(1, pixels.Color(0,i,0));
    pixels.show();
    delay(ANIMSPEED);
  }
  for (int i = BRIGHTNESS; i >= 0; i--){
    pixels.setPixelColor(1, pixels.Color(0,i,0));
    pixels.show();
    delay(ANIMSPEED);
  }
}

void downArrow() {
  DEBUG_PRINTLN("Down Arrow Called");
  for (int i = STARTTIME; i < BRIGHTNESS; i++){
    pixels.setPixelColor(2, pixels.Color(i,0,0));
    pixels.show();
    delay(ANIMSPEED);
  }
  for (int i = BRIGHTNESS; i >= 0; i--){
    pixels.setPixelColor(2, pixels.Color(i,0,0));
    pixels.show();
    delay(ANIMSPEED);
  }
}

void safeRestart() {
  delay(1000);
  WiFi.mode(WIFI_OFF);
  delay(1000);
  DEBUG_PRINTLN("Restarting...");
  ESP.deepSleep(1000000,RF_DISABLED);
  //ESP.restart();
  delay(10000);
}
String getDataFromUrl(String url, int saveETag);
String getDataFromUrl(String url) {return getDataFromUrl(url, 0);}
String getDataFromUrl(String url, int saveETag) {
  DEBUG_PRINT("requesting URL: ");
  DEBUG_PRINTLN(url);
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Authorization: Client-ID " + clientID + "\r\n" +
               "If-None-Match: " + eTag + "\r\n" +
               "Connection: close\r\n\r\n");
  DEBUG_PRINTLN("request sent");
  String line;
  int flag = 0;
  while (client.connected()) {
    line = client.readStringUntil('\n');
    DEBUG_PRINTLN(line);
    if (saveETag && line.indexOf("ETag:")!=-1) eTag = line.substring(6,line.length()-1);
    if (line.indexOf("HTTP/1.1 304 Not Modified")==0) flag = 1;
    if (line.indexOf("X-RateLimit-UserRemaining: 450")==0) {DEBUG_PRINT("X-RateLimit-UserRemaining: "); DEBUG_PRINTLN(line.substring(27, line.length()-1));}
    if (line.indexOf("{\"data\":")!=-1) break;
  }
  if (flag == 1) return String("304 Not Modified");
  if (line.length() > 1){
    DEBUG_PRINTLN("reply was:");
    DEBUG_PRINTLN("==========");
    DEBUG_PRINTLN(line);
    DEBUG_PRINTLN("==========");
    DEBUG_PRINTLN("closing connection");
    client.flush();
    return line;
  }
  else {
    DEBUG_PRINTLN("No Reply!");
    return String("noreply");
  }
}

int openConnection() {
  DEBUG_PRINT("connecting to ");
  DEBUG_PRINTLN(host);
  if (!client.connect(host, httpsPort)) {
    DEBUG_PRINTLN("connection failed"); 
    return 0;
  }
  return 1;
}

int getCredits() {
  if(!openConnection()) return 0;
  String url = "/3/credits";
  String credits = getDataFromUrl(url);
  DEBUG_PRINT("Credits String: ");
  DEBUG_PRINTLN(credits);
  String creditsString = credits.substring(credits.indexOf("UserRemaining")+15,credits.indexOf(",\"UserReset"));
  DEBUG_PRINT("Credit String: ");
  DEBUG_PRINTLN(creditsString); 
  int creditsLeft = 0;
  creditsLeft = creditsString.toInt();
  return creditsLeft;
}

String getComment() {
  if(!openConnection()) return String("Connection Failed!");
  String url = "/3/comment/";
  int randomSeedValue = analogRead(A0);
  randomSeed(randomSeedValue);
  url += String(random(COMMENTMAXID));
  String comment = getDataFromUrl(url);
  DEBUG_PRINT("Comment String: ");
  DEBUG_PRINTLN(comment);
  String commentString = comment.substring(comment.indexOf("comment")+10,comment.indexOf("\",\"author"));
  DEBUG_PRINT("Comment String: ");
  DEBUG_PRINTLN(commentString); 
  return commentString;
}

long getRep() {
  if(!openConnection()) return 0;
  String url = "/3/account/";
  url += userString;
  String account = getDataFromUrl(url, 1);
  if (account.indexOf("304 Not Modified")==0) {DEBUG_PRINTLN("ETag match, no change"); return -2;}
  if (account.indexOf("noreply")==0) {DEBUG_PRINTLN("Error No Reply!"); return -3;}
  DEBUG_PRINT("Account String: ");
  DEBUG_PRINTLN(account);
  String repString = account.substring(account.indexOf("reputation")+12,account.indexOf(",\"created"));
  DEBUG_PRINT("Rep String: ");
  DEBUG_PRINTLN(repString); 
  DEBUG_PRINT("ETag String: ");
  DEBUG_PRINTLN(eTag); 
  int rep = 0;
  rep = repString.toInt();
  return rep;
}

void errorPage(byte reasonCode){
  String s;
  DEBUG_PRINTLN("Error Page");
  s += "<span style='font-weight:600;'>Sorry, but I ran into an error during setup!</span><br><br>";
  switch(reasonCode){
    case 0: s += "<span style='font-weight:300;'>That WiFi Network Name is too long for my memory, can you please shorten it and try again?</span><br><br>";
    case 1: s += "<span style='font-weight:300;'>That WiFi Network Password is too long for my memory, can you please shorten it and try again?</span><br><br>";
    case 2: s += "<span style='font-weight:300;'>That Name is too long for my memory, can you please shorten it and try again?</span><br><br>";
    case 3: s += "<span style='font-weight:300;'>That Password for me is too long for my memory, can you please shorten it and try again?</span><br><br>";
    case 4: s += "<span style='font-weight:300;'>That User Name is too long for my memory, can you please shorten it and try again?</span><br><br>";
  }
  s += "<br><br><input type='button' onClick='history.go(-1)' value='Back to Setup'>";
  s += footerHtml;
  server.send(200, "text/html", s); // Send the response to the client
  DEBUG_PRINTLN("Client disconnected");  // The client will actually be disconnected when the function returns and 'client' object is detroyed
}

void confirmPage() {
  DEBUG_PRINTLN("Got Post");
  String apPassString;
  String apString;
  String passString;
  String ssidString;
  String eepromString;
  if (server.arg("ssid").length() > 32) {errorPage(0); return;}
  else ssidString = server.arg("ssid");
  if (server.arg("pass").length() > 32) {errorPage(1); return;}
  else passString = server.arg("pass");
  if (server.arg("ap").length() > 32) {errorPage(2); return;}
  else apString = server.arg("ap");
  if (server.arg("apPass").length() > 32) {errorPage(3); return;}
  else apPassString = server.arg("apPass");
  if (server.arg("userName").length() > 128) {errorPage(4); return;}
  else userString = server.arg("userName");

  String s = headerHtml;
  s += "Thanks! I'm now going to connect to ";
  s += ssidString;
  if (passString != "") s += " using the password you have given me";
  s += ".<br><br>You can reconnect to me at any time by connecting to the network ";
  s += apString;
  s += " and using the password you have specified. I'll be responding to changes for user ";
  s += userString;
  s += ".<br><br>If my connection light is Yellow for longer than a minute, please press my reset button and try setting me up again.";
  s += footerHtml;

  eepromString = "ssid=";
  eepromString += ssidString;
  eepromString += "pass=";
  eepromString += passString;
  eepromString += "ap=";
  eepromString += apString;
  eepromString += "apPass=";
  eepromString += apPassString;
  eepromString += "userName=";
  eepromString += userString;

  DEBUG_PRINTLN("Writing EEPROM");
  EEPROM.write(0,42);
  for(int i = 0; i < (eepromString.length()); i++) EEPROM.write(i+1,eepromString.charAt(i));
  EEPROM.write(eepromString.length()+2,42);
  EEPROM.commit();

  DEBUG_PRINT("EEPROM Written: ");
  for(int i = 0; i < (eepromString.length()+1); i++) DEBUG_PRINT(char(EEPROM.read(i)));
  DEBUG_PRINTLN("");

  server.send(200, "text/html", s); // Send the response to the client
  DEBUG_PRINTLN("Client disconnected");  // The client will actually be disconnected when the function returns and 'client' object is detroyed
  safeRestart();
}

void setupPage() {
  int accessPoints = WiFi.scanNetworks();
  String s = headerHtml;
  DEBUG_PRINTLN("Access Points Found:");
  s += "<span style='font-weight:600;'>Hello, I will light up when an Imgur user account gains or loses reputation. Let's quickly set me up!</span><br><br>"; 
  s += "<form action='' method='POST'><table style='width:100%;'><tr><td>Here are the networks I can see,<br>which one can I connect to?</td><td>";
  if (accessPoints == 0){
    s += "Can't Find Any Open WiFi Hotspots!";
    DEBUG_PRINTLN("No Access Points Found!");
  }
  else {
    s += "<input type='radio' name='ssid' value='";
    s += String(WiFi.SSID(0));
    s += "' checked>";
    s += String(WiFi.SSID(0));
    s += "<br>";
    DEBUG_PRINT("- ");
    DEBUG_PRINTLN(WiFi.SSID(0));
    for (int i = 1; i < accessPoints; i++) {
      s+="<input type='radio' name='ssid' value='";
      s+=String(WiFi.SSID(i));
      s+="'>";
      s+=String(WiFi.SSID(i));
      s+="<br>";
      DEBUG_PRINT("- ");
      DEBUG_PRINTLN(WiFi.SSID(i));
    }
  }
  s += "<br><br><input type='button' onClick='history.go(0)' value='Rescan Networks'></td></tr>";
  s += "<tr><td>If your WiFi Hotspot has a password,<br>please type it in here:</td><td><input type='password' style='text-align:center;' name='pass' value=''></td></tr>";
  s += "<tr><td>Please give me a name:</td><td><input type='textarea' name='ap' placeholder='Puck' style='text-align:center;'></td></tr>";
  s += "<tr><td>Please give me a password:<br>(minimum 8 characters)</td><td><input type='textarea' name='apPass' style='text-align:center;' value=''></td></tr>";
  s += "<tr><td>What is your Imgur user name?</td><td><input type='textarea' name='userName' style='text-align:center;' value=''></td></tr>";
  s += "<tr><td>Click Next to save settings</td><td><input type='submit' value='Next'></td></tr>";
  s += "</table></form>";
  s += footerHtml;
  server.send(200, "text/html", s); // Send the response to the client
  DEBUG_PRINTLN("Client disconnected");  // The client will actually be disconnected when the function returns and 'client' object is detroyed
}

void setup() {
  Serial.begin(115200);
  DEBUG_PRINTLN("Serial Online");
  
  EEPROM.begin(256);
  DEBUG_PRINTLN("EEPROM Online");
  
  pinMode(BUTTONPIN, INPUT_PULLUP);
  DEBUG_PRINTLN("Button Online");
  
  pixels.begin();
  pixels.setPixelColor(0, pixels.Color(128,128,0));
  pixels.setPixelColor(1, pixels.Color(0,128,0));
  pixels.setPixelColor(2, pixels.Color(128,0,0));
  pixels.show();
  DEBUG_PRINTLN("Pixels Online");

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(myIp, myGateway, mySubnet);
  DEBUG_PRINTLN("WiFi Online");

  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  DEBUG_PRINT("WiFi MAC : ");
  DEBUG_PRINTLN(WiFi.softAPmacAddress());

  String ssidString = "";
  String passString = "";
  String apString = "";
  String apPassString = "";
  String storedRequestString = "";
  DEBUG_PRINTLN("Strings Initialized");

  server.begin();
  DEBUG_PRINTLN("Server Started");

  server.on("/", HTTP_POST, confirmPage);
  server.on("/", setupPage);
  DEBUG_PRINTLN("Server Pages Set, Waiting for Clients");
  
  DEBUG_PRINT("EEPROM(0) = ");
  DEBUG_PRINTLN(EEPROM.read(0));

 DEBUG_PRINT("Button Status: ");
 DEBUG_PRINTLN(digitalRead(BUTTONPIN));

  
  if ((EEPROM.read(0) != 42) || (digitalRead(BUTTONPIN) == LOW)){
    EEPROM.write(0,0);
    EEPROM.commit();
    DEBUG_PRINTLN("Low, Clearing EEPROM");
    pixels.setPixelColor(0, pixels.Color(0,16,0));
    pixels.show();
    DEBUG_PRINTLN("EEPROM(0) != 42, EEPROM invalid");
    
    for (int i = 0; i < 256; ++i) EEPROM.write(i, 0);
    DEBUG_PRINTLN("EEPROM cleared");
    
    String macId = String(mac[WL_MAC_ADDR_LENGTH-2],HEX)+String(mac[WL_MAC_ADDR_LENGTH-1],HEX);
    macId.toUpperCase();
    apString = "Puck -Setup- " + macId;
    WiFi.softAP(apString.c_str());
    DEBUG_PRINT("WiFi SSID Set : ");
    DEBUG_PRINTLN(apString);
    
    while (1) server.handleClient();
  }
  
  pixels.setPixelColor(0, pixels.Color(0,0,16));
  pixels.show();
  DEBUG_PRINTLN("EEPROM(0) = 42, using saved settings");

  for(int i = 0; i < 256; i++) storedRequestString += char(EEPROM.read(i));
  DEBUG_PRINT("EEPROM: ");
  DEBUG_PRINTLN(storedRequestString);
  
  ssidString = storedRequestString.substring(storedRequestString.indexOf("ssid=")+5,storedRequestString.indexOf("pass="));
  DEBUG_PRINT("SSID = ");DEBUG_PRINTLN(ssidString);
  
  passString = storedRequestString.substring(storedRequestString.indexOf("pass=")+5,storedRequestString.indexOf("ap="));
  DEBUG_PRINT("Pass = ");DEBUG_PRINTLN(passString);

  apString = storedRequestString.substring(storedRequestString.indexOf("ap=")+3,storedRequestString.indexOf("apPass="));
  DEBUG_PRINT("AP = ");DEBUG_PRINTLN(apString);

  apPassString = storedRequestString.substring(storedRequestString.indexOf("apPass=")+7,storedRequestString.lastIndexOf("userName"));
  DEBUG_PRINT("AP Pass = ");DEBUG_PRINTLN(apPassString);
  
  userString = storedRequestString.substring(storedRequestString.indexOf("userName=")+9,storedRequestString.lastIndexOf('*')-1);
  DEBUG_PRINT("User Name = ");DEBUG_PRINTLN(userString);
  
  pixels.setPixelColor(0, pixels.Color(16,16,0));
  pixels.show();

  WiFi.softAP(apString.c_str(),apPassString.c_str());
  
  DEBUG_PRINT("WiFi SSID Set : ");
  DEBUG_PRINTLN(apString);
  DEBUG_PRINT("WiFi Pass Set : ");
  DEBUG_PRINTLN(apPassString);
  
  WiFi.begin(ssidString.c_str(), passString.c_str());
  DEBUG_PRINT("Connecting to ");
  DEBUG_PRINTLN(ssidString);

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DEBUG_PRINT(".");
    timeout++;
    if(timeout>60){
      DEBUG_PRINTLN("Timeout Reached");
      EEPROM.write(0,0);
      EEPROM.commit();
      DEBUG_PRINTLN("EEPROM Reset");
      safeRestart();
    }
  }
  DEBUG_PRINTLN("");
  pixels.setPixelColor(0, pixels.Color(0,16,16));
  pixels.show();
  
  DEBUG_PRINTLN("WiFi connected");
  DEBUG_PRINTLN("IP address: ");
  DEBUG_PRINTLN(WiFi.softAPIP());

  int credits = getCredits();
  DEBUG_PRINT("Credits = ");
  DEBUG_PRINTLN(credits);
  delay(1000);
  String comment = getComment();
  DEBUG_PRINT("Comment = ");
  DEBUG_PRINTLN(comment);
  pixels.setPixelColor(0, pixels.Color(0,0,0));
  pixels.setPixelColor(1, pixels.Color(0,0,0));
  pixels.setPixelColor(2, pixels.Color(0,0,0));
  pixels.show();
  upArrow();
  downArrow();
  DEBUG_PRINTLN("---------------------------------------------------------");
  long newRep = getRep();
  switch(newRep){
    case -3: DEBUG_PRINT("No Reply!!"); break;
    case -2: DEBUG_PRINT("ETAG NATCH"); break;
    default: DEBUG_PRINT("newRep: ");
             DEBUG_PRINT(newRep);
             DEBUG_PRINT(" ");
             DEBUG_PRINT(rep);
             DEBUG_PRINTLN(" rep");
             rep = newRep;
             break;
  }
  delay(10000);
}

void loop()
{
  DEBUG_PRINTLN("---------------------------------------------------------");
  long newRep = getRep();
  switch(newRep){
    case -3: DEBUG_PRINT("No Reply!!"); break;
    case -2: DEBUG_PRINT("ETAG NATCH"); break;
    default: DEBUG_PRINT("newRep: ");
             DEBUG_PRINT(newRep);
             DEBUG_PRINT(" ");
             DEBUG_PRINT(rep);
             DEBUG_PRINTLN(" rep");
             if (newRep < rep) {downArrow();}
             if (newRep > rep) {upArrow();}
             rep = newRep;
             break;
  }
  delay(10000);
}
