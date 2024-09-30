#include "common.h"
#include "FIFORequestChannel.h"

using namespace std;

int main (int argc, char *argv[]) {
    int opt;
    int p = -1;
    double t = -1;
    int e = -1;
    int m = MAX_MESSAGE;
    char* buff_size;
    bool new_channel = false;
    bool buff_flag = false;
    vector<FIFORequestChannel*> chans;
    string filename = "";

    while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
        switch (opt) {
            case 'p':
                p = atoi(optarg);
                break;
            case 't':
                t = atof(optarg);
                break;
            case 'e':
                e = atoi(optarg);
                break;
            case 'f':
                filename = optarg;
                break;
            case 'm':
                m = atoi(optarg);
                buff_flag = true;
                buff_size = optarg;
                break;
            case 'c':
                new_channel = true;
                break;
        }
    }

    pid_t server = fork();
    if (server == 0) {
        // Running server in child
        if (buff_flag) {
            char* args[] = {(char*)"./server", (char*)"-m", buff_size, NULL};
            execvp(args[0], args);
			        exit(0);
        } 
    } 

    FIFORequestChannel control_chan("control", FIFORequestChannel::CLIENT_SIDE);
    chans.push_back(&control_chan);

    if (new_channel) {
        MESSAGE_TYPE newc = NEWCHANNEL_MSG;
        control_chan.cwrite(&newc, sizeof(MESSAGE_TYPE));
        char cbuf[30];
        control_chan.cread(cbuf, sizeof(cbuf));
        FIFORequestChannel* nchan = new FIFORequestChannel(cbuf, FIFORequestChannel::CLIENT_SIDE);
        chans.push_back(nchan);
    }
	FIFORequestChannel chan = *(chans.back());

    if (p != -1 && t != -1 && e != -1) {
        char buf[MAX_MESSAGE];
        datamsg x(p, t, e);
        memcpy(buf, &x, sizeof(datamsg));
        chan.cwrite(buf, sizeof(datamsg));

        double reply;
        chan.cread(&reply, sizeof(double));
        cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
    }
	 else if (p != -1) {
        ofstream file("received/x1.csv");
        char fbuf[MAX_MESSAGE];
        for (double i = 0.000; i < 4; i += 0.004) {
            datamsg y(p, i, 1);
            memcpy(fbuf, &y, sizeof(datamsg));
            chan.cwrite(fbuf, sizeof(datamsg));
            double valueOne;
            chan.cread(&valueOne, sizeof(double));

            datamsg z(p, i, 2);
            memcpy(fbuf, &z, sizeof(datamsg));
            chan.cwrite(fbuf, sizeof(datamsg));
            double valueTwo;
            chan.cread(&valueTwo, sizeof(double));

            file << i << "," << valueOne << "," << valueTwo << "\n";
        }
    } 
	else if (!filename.empty()) {
        filemsg fm(0, 0);
        int len = sizeof(filemsg) + filename.size() + 1;
        char* buf = new char[len];
        memcpy(buf, &fm, sizeof(filemsg));
        strcpy(buf + sizeof(filemsg), filename.c_str());
        chan.cwrite(buf, len);

        int64_t filesize;
        chan.cread(&filesize, sizeof(int64_t));

        ofstream output_file("received/" + filename, ios::binary);
        filemsg* file_req = (filemsg*)buf;

        int64_t remaining = filesize;
        while (remaining > 0) {
            file_req->length = min((__int64_t)m, remaining);
            chan.cwrite(buf, len);

            char* recv_buf = new char[file_req->length];
            chan.cread(recv_buf, file_req->length);
            output_file.write(recv_buf, file_req->length);

            delete[] recv_buf;
            remaining -= file_req->length;
            file_req->offset += file_req->length;
        }

        output_file.close();
        delete[] buf;
    }

    if (new_channel) {
        delete chans.back();
    }

    MESSAGE_TYPE q = QUIT_MSG;
    chan.cwrite(&q, sizeof(MESSAGE_TYPE));

    wait(NULL);

    return 0;
}