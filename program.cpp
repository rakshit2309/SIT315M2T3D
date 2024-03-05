#include <iostream>
#include <fstream>
#include <sstream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <algorithm>

using namespace std;

struct TrafficData {
    string timestamp;
    int trafficLightId;
    int numberOfCars;
};

class TrafficBuffer {
private:
    queue<TrafficData> buffer;
    mutex mtx;
    condition_variable cv;
    int maxSize;

public:
    TrafficBuffer(int size) : maxSize(size) {}

    void addData(const TrafficData& data) {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this] { return buffer.size() < maxSize; });
        buffer.push(data);
        lock.unlock();
        cv.notify_all();
    }

    TrafficData getData() {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this] { return !buffer.empty(); });
        TrafficData data = buffer.front();
        buffer.pop();
        lock.unlock();
        cv.notify_all();
        return data;
    }
};

void producer(TrafficBuffer& buffer, const vector<TrafficData>& data) {
    for (const auto& traffic : data) {
        buffer.addData(traffic);
        this_thread::sleep_for(chrono::milliseconds(500)); // Simulate traffic generation
    }
}

void consumer(TrafficBuffer& buffer, int n) {
    vector<pair<int, int>> congestion; // {trafficLightId, totalCars}
    for (int i = 0; i < 12; ++i) {
        congestion.clear();
        for (int j = 0; j < 12; ++j) {
            TrafficData data = buffer.getData();
            auto it = find_if(congestion.begin(), congestion.end(), [&](const pair<int, int>& elem) {
                return elem.first == data.trafficLightId;
            });
            if (it == congestion.end()) {
                congestion.push_back({data.trafficLightId, data.numberOfCars});
            } else {
                it->second += data.numberOfCars;
            }
        }
        sort(congestion.begin(), congestion.end(), [](const pair<int, int>& a, const pair<int, int>& b) {
            return a.second > b.second;
        });
        cout << "Hour " << i + 1 << " Top " << n << " congested traffic lights: ";
        for (int k = 0; k < min(n, static_cast<int>(congestion.size())); ++k) {
            cout << "(Light " << congestion[k].first << ", Cars " << congestion[k].second << ") ";
        }
        cout << endl;
        this_thread::sleep_for(chrono::hours(1)); // Process every hour
    }
}

vector<TrafficData> readTrafficDataFromFile(const string& filename) {
    vector<TrafficData> data;
    ifstream file(filename);
    if (file.is_open()) {
        string line;
        while (getline(file, line)) {
            stringstream ss(line);
            TrafficData traffic;
            ss >> traffic.timestamp;
            ss >> traffic.trafficLightId;
            ss >> traffic.numberOfCars;
            data.push_back(traffic);
        }
        file.close();
    }
    return data;
}

int main() {
    srand(time(nullptr)); // Seed random number generator

    TrafficBuffer buffer(5); // Bounded buffer size 5
    int numLights = 5; // Number of traffic lights
    int topN = 3; // Top N most congested traffic lights

    vector<thread> producers;
    vector<thread> consumers;

    vector<TrafficData> inputData = readTrafficDataFromFile("data.txt");

    consumers.emplace_back(consumer, ref(buffer), topN); // Start consumer thread before producers

    for (int i = 0; i < numLights; ++i) {
        producers.emplace_back(producer, ref(buffer), inputData);
    }

    for (auto& p : producers) {
        p.join(); // Wait for all producer threads to finish
    }

    for (auto& c : consumers) {
        c.join(); // Wait for the consumer thread to finish
    }

    return 0;
}