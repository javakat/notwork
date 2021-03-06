
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <boost/lexical_cast.hpp>
#include "packet.h"

#define BUFSIZE 505
#define FILENAME "Testfile"
#define TEST_FILENAME "Testfile2"
#define PORT 10038
#define PAKSIZE 512
#define ACK 0
#define NAK 1
#define WIN_SIZE 16

using namespace std;

bool gremlin(Packet * pack, int corruptProb, int lossProb);
bool init(int argc, char** argv);
bool loadFile();
bool sendFile();
bool getFile();
char * recvPkt();
bool isvpack(unsigned char * p);
Packet createPacket(int index);
bool sendPacket();
bool isAck();
void handleAck();
void handleNak(int& x);

int seqNum;
int probCorrupt;
int probLoss;
string hs;
short int port;
char * file;
unsigned char* window[16]; //packet window
int base; //used to determine position in window of arriving packets
int length;
struct sockaddr_in a;
struct sockaddr_in sa;
socklen_t salen;
string fstr;
bool dropPck;
Packet p;
int delayT;
int s;
unsigned char b[BUFSIZE];

int main(int argc, char** argv) {
  
  if(!init(argc, argv)) return -1;
 
  if(sendto(s, "GET Testfile", BUFSIZE + 7, 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
    cout << "Package sending failed. (socket s, server address sa, message m)" << endl;
    return false;
  }
  
  getFile();

  return 0;
}

bool init(int argc, char** argv) {
  base = 0; //initialize base
  s = 0;

  hs = string("131.204.14.") + argv[1]; /* Needs to be updated? Might be a string like "tux175.engr.auburn.edu." */
  port = 10038; /* Can be any port within 10038-10041, inclusive. */

  char* delayTStr = argv[2];
  delayT = boost::lexical_cast<int>(delayTStr);

  /*if(!loadFile()) {
    cout << "Loading file failed. (filename FILENAME)" << endl;
    return false;
  }*/

  if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    cout << "Socket creation failed. (socket s)" << endl;
    return false;
  }

  memset((char *)&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_ANY); //why does this always give us 0? 
  a.sin_port = htons(0);

  if (bind(s, (struct sockaddr *)&a, sizeof(a)) < 0){
    cout << "Socket binding failed. (socket s, address a)" << endl;
    return false;
  }

  memset((char *)&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  inet_pton(AF_INET, hs.c_str(), &(sa.sin_addr));

  cout << endl;

  cout << "Server address (inet mode): " << inet_ntoa(sa.sin_addr) << endl;
  cout << "Port: " << ntohs(sa.sin_port) << endl;

  cout << endl << endl;

  /*fstr = string(file);

  cout << "File: " << endl << fstr << endl << endl;*/

  seqNum = 0;
  dropPck = false;
  return true;
}

bool loadFile() {

  ifstream is (FILENAME, ifstream::binary);

  if(is) {
    is.seekg(0, is.end);
    length = is.tellg();
    is.seekg(0, is.beg);

    file = new char[length];

    cout << "Reading " << length << " characters..." << endl;
    is.read(file, length);

    if(!is) { cout << "File reading failed. (filename " << FILENAME << "). Only " << is.gcount() << " could be read."; return false; }
    is.close();
  }
  return true;
}

bool sendFile() {

  for(int x = 0; x <= length / BUFSIZE; x++) {
    p = createPacket(x);
    if(!sendPacket()) continue;

    if(isAck()) { 
      handleAck();
    } else { 
      handleNak(x);
    }

    memset(b, 0, BUFSIZE);
  }
  return true;
}

Packet createPacket(int index){
    cout << endl;
    cout << "=== TRANSMISSION START" << endl;
    string mstr = fstr.substr(index * BUFSIZE, BUFSIZE);
    if(index * BUFSIZE + BUFSIZE > length) {
      mstr[length - (index * BUFSIZE)] = '\0';
    }
    return Packet (seqNum, mstr.c_str());
}

bool sendPacket(){
    int pc = probCorrupt; int pl = probLoss;
    if((dropPck = gremlin(&p, pc, pl)) == false){
      if(sendto(s, p.str(), BUFSIZE + 7, 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        cout << "Package sending failed. (socket s, server address sa, message m)" << endl;
        return false;
      }
      return true;
    } else return false;
}
bool isAck() {
    recvfrom(s, b, BUFSIZE + 7, 0, (struct sockaddr *)&sa, &salen);

    cout << endl << "=== SERVER RESPONSE TEST" << endl;
    cout << "Data: " << b << endl;
    if(b[6] == '0') return true;
    else return false;
}
void handleAck() {

}
void handleNak(int& x) {

      char * sns = new char[2];
      memcpy(sns, &b[0], 1);
      sns[1] = '\0';

      char * css = new char[5];
      memcpy(css, &b[1], 5);
      
      char * db = new char[BUFSIZE + 1];
      memcpy(db, &b[2], BUFSIZE);
      db[BUFSIZE] = '\0';

      cout << "Sequence number: " << sns << endl;
      cout << "Checksum: " << css << endl;

      Packet pk (0, db);
      pk.setSequenceNum(boost::lexical_cast<int>(sns));
      pk.setCheckSum(boost::lexical_cast<int>(css));

      if(!pk.chksm()) x--; 
      else x = (x - 2 > 0) ? x - 2 : 0;
}

bool gremlin(Packet * pack, int corruptProb, int lossProb){
  bool dropPacket = false;
  int r = rand() % 100;

  cout << "Corruption probability: " << corruptProb << endl;
  cout << "Random number: " << r << endl;

  if(r <= (lossProb)){
    dropPacket = true;
    cout << "Dropped!" << endl;
  }  
  else if(r <= (corruptProb)){
    cout << "Corrupted!" << endl;
    pack->loadDataBuffer((char*)"GREMLIN LOL");
  }
  else seqNum = (seqNum) ? false : true; 
  cout << "Seq. num: " << pack->getSequenceNum() << endl;
  cout << "Checksum: " << pack->getCheckSum() << endl;
  cout << "Message: "  << pack->getDataBuffer() << endl;

  return dropPacket;
}

bool isvpack(unsigned char * p) {

  char * sns = new char[3];
  memcpy(sns, &p[0], 3);
  sns[2] = '\0';

  char * css = new char[6];
  memcpy(css, &p[2], 5);
  css[5] = '\0';
      
  char * db = new char[BUFSIZE + 1];
  memcpy(db, &p[8], BUFSIZE);
  db[BUFSIZE] = '\0';

  int sn = boost::lexical_cast<int>(sns);
  int cs = boost::lexical_cast<int>(css);

  Packet pk (0, db);
  pk.setSequenceNum(sn);



  if(!(sn >= (base % 32) && sn <= (base % 32) + WIN_SIZE - 1)) { cout << "Bad sequence number." << endl; return false; }
  if(cs != pk.generateCheckSum()) { cout << "Bad checksum." << endl; return false; }
  return true;
}


bool getFile(){
  /* Loop forever, waiting for messages from a client. */
  cout << "Waiting on port " << PORT << "..." << endl;

  ofstream file("Dumpfile");

  int rlen;
  int ack;
  
  for (;;) {
    unsigned char packet[PAKSIZE + 1];
    unsigned char dataPull[BUFSIZE];
    rlen = recvfrom(s, packet, PAKSIZE + 1, 0, (struct sockaddr *)&sa, &salen);

	if(packet[0] == '\0') break;

	for(int x = 0; x < BUFSIZE; x++) {
      dataPull[x] = packet[x + 8];
    }
	dataPull[BUFSIZE] = '\0';
    if (rlen > 0) {
	  char * sns = new char[3];
	  memcpy(sns, &packet[0], 3);
	  sns[2] = '\0';
		
      char * css = new char[6];
      memcpy(css, &packet[2], 5);
      css[5] = '\0';
      cout << endl << endl << "=== RECEIPT" << endl;
      cout << "Seq. num: " << sns << endl;
      cout << "Checksum: " << css << endl;
	  cout << "Payload: " << dataPull << endl;
	  int pid = boost::lexical_cast<int>(sns);
      if(isvpack(packet)) {
		if(pid == base % 32) { 
			base++; //increment base of window
			file << dataPull;
			file.flush();
		}
      } else cout << "%%% ERROR IN PACKET " << pid << "%%%" << endl;

      cout << "Sent response: ";
      cout << "ACK " << base << endl;

	  if(packet[6] == '1') usleep(delayT*1000);

	  string wbs = to_string((long long)base);
	  const char * ackval = wbs.c_str();

      if(sendto(s, ackval, 10, 0, (struct sockaddr *)&sa, salen) < 0) {
        cout << "Acknowledgement failed. (socket s, acknowledgement message ack, client address ca, client address length calen)" << endl;
		perror("sendto()");
        return 0;
      }
	  delete sns;
      delete css;
    }
  }
  cout << "GET Testfile into Dumpfile complete." << endl;
  file.close();
  return true;
}
