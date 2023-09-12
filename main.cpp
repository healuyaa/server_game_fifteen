#include "./httplib.h"
#include <windows.h>
#include <winsock.h>
#include "./json11.hpp"
#include <fstream>
#include <string>
#include <vector>
#include <ctime>
#include <random>
#include <filesystem>

struct session {
    std::string access_id = "";
    std::string login = "";
};

std::vector<session> sessions;

std::string gen_guid() {
    std::random_device rd;  
    std::mt19937_64 eng(rd());

    std::uniform_int_distribution<long long> dist(std::numeric_limits<long long>::min(),
                                                 std::numeric_limits<long long>::max());

    long long randomNum = dist(eng);

    return std::to_string(randomNum);
}

std::string check_session(std::string id) {
    for (size_t i = 0; i < sessions.size(); i++)
        if(sessions[i].access_id == id)
            return sessions[i].login;
    return "";
    
}

void delete_session(std::string id) {
    for (auto i = sessions.begin(); i != sessions.end(); i++) 
        if((*i).access_id == id) {
            sessions.erase(i);
            return;
        }
}

struct Rating {
    int result;
    std::string login;
};

std::string get_results() {
    std::vector<Rating> out;
    std::string folderPath = "./db";
    std::string err;
    for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
        if (entry.is_regular_file()) {            
            std::fstream file(entry.path(), std::fstream::in);
            std::string read_s = "";
            getline(file, read_s);
            file.close();
            json11::Json results_user = json11::Json::parse(read_s, err, json11::JsonParse::COMMENTS);
            if (err.empty()) {
                auto obj_result = results_user.object_items();
                auto result = obj_result["result"].int_value();
                std::string fut_login = std::string(entry.path().generic_string());
                size_t i_slash = fut_login.find_last_of('/');
                size_t i_dot = fut_login.find_last_of('.');
                std::string fn = fut_login.substr(i_slash + 1, i_dot - i_slash - 1);
                out.push_back({result, fn});
            }
        }
    }
    std::sort(out.begin(), out.end(),[](const Rating& rat1, const Rating& rat2) {
        return rat1.result < rat2.result;
    });
    std::string str = "[";
    bool it_com = true;
    for(auto o: out) {
        it_com ? str : str += ",";
        it_com = false;
        str += "{\"login\":\"" + o.login + "\", \"result\":" + std::to_string(o.result) + "}";    
    }    
    return str + "]";
}

int main() {
    httplib::Server svr;

    svr.Get("/api/hi", [](const httplib::Request &, httplib::Response &res) {
        res.set_content("{\"test\":\"hi\"}", "application/json");
    });

    svr.Post("/api/registration", [](const httplib::Request &request, httplib::Response &res) {
        std::string err;
        json11::Json json = json11::Json::parse(request.body, err, json11::JsonParse::COMMENTS);
        if (err.empty()) {
            auto objJson = json.object_items();
            auto login = objJson["login"];
            auto pass = objJson["pass"];
            std::fstream file(std::string("./db/") + login.string_value() + ".json", std::fstream::in);
            if(file.is_open()) {
                file.close();
                res.set_content("{\"result\":false, \"msg\":\"User was found\"}", "application/json");
                return;
            } else {
                std::cout << "login: " << login.string_value() << std::endl;
                std::fstream file(std::string("./db/") + login.string_value() + ".json", std::fstream::out);
                file << "{\"pass\":\"" << pass.string_value() << "\", \"result\":9999999}";
                file.close();
                res.set_content("{\"result\":true, \"msg\":\"User was created\"}", "application/json");
                return;
            }
        }
        res.set_content("{\"result\":false, \"msg\":\"Wrong data\"}", "application/json");
    });

    svr.Post("/api/login", [](const httplib::Request &request, httplib::Response &res) {
        std::string err;
        json11::Json json = json11::Json::parse(request.body, err, json11::JsonParse::COMMENTS);
        if (err.empty()) {
            auto objJson = json.object_items();
            auto login = objJson["login"];
            auto pass = objJson["pass"];
            std::fstream file(std::string("./db/") + login.string_value() + ".json", std::fstream::in);
            if(file.is_open()) {
                std::string readPass = "";
                getline(file, readPass);                
                file.close();
                json11::Json passJson = json11::Json::parse(readPass, err, json11::JsonParse::COMMENTS);
                auto objJson = passJson.object_items();
                auto passJ = objJson["pass"];
                if(pass.string_value() == passJ.string_value()) {
                    std::string ses = gen_guid();
                    sessions.push_back({ses, login.string_value()});
                    res.set_content("{\"result\":true, \"access_id\":\"" + ses + "\"}", "application/json");
                    std::cout << "login: " << login.string_value() << "     access_id: " << ses << std::endl;
                    return;
                }  else { res.set_content("{\"result\":false, \"msg\":\"Wrong pass\"}", "application/json"); }
                return;
            } else {
                res.set_content("{\"result\":false, \"msg\":\"Wrong pass or user\"}", "application/json");
                return;
            }
        }
        res.set_content("{\"result\":false, \"msg\":\"Wrong data\"}", "application/json");
    });

    svr.Post("/api/result/save", [](const httplib::Request &request, httplib::Response &res) {
        std::string err;
        json11::Json json = json11::Json::parse(request.body, err, json11::JsonParse::COMMENTS);
        if (err.empty()) {
            auto objJson = json.object_items();
            auto id = objJson["access_id"].string_value();            
            std::string s_login = check_session(id);
            if(s_login == "") {
                res.set_content("{\"result\":false, \"msg\":\"Undefined user session\"}", "application/json");
                return;
            }
            std::fstream file(std::string("./db/") + s_login + ".json", std::fstream::in);
            if(file.is_open()) {
                std::string read_s = "";
                getline(file, read_s);                
                file.close();
                json11::Json user_json = json11::Json::parse(read_s, err, json11::JsonParse::COMMENTS);
                auto user_i_json = user_json.object_items();
                if(objJson["result"].int_value() < user_i_json["result"].int_value()) {
                    json11::Json outj = json11::Json::object {{"result", objJson["result"].int_value()}, {"pass", user_json["pass"].string_value()}};
                    std::cout << "/api/result/save" << " id: " << id << std::endl; 
                    std::fstream file(std::string("./db/") + s_login + ".json", std::fstream::out);
                    file << outj.dump();
                    file.close();
                    std::cout << "login: " << s_login << "     access_id: " << id << "      file: " << outj.dump() << std::endl;  
                };
                return;
            } else {
                res.set_content("{\"result\":false, \"msg\":\"Bad database\"}", "application/json");
                return;
            }
        }
        res.set_content("{\"result\":false, \"msg\":\"Wrong data\"}", "application/json");
    });

    svr.Post("/api/logout", [](const httplib::Request &request, httplib::Response &res) {
        std::string err;
        json11::Json json = json11::Json::parse(request.body, err, json11::JsonParse::COMMENTS);
        if (err.empty()) {
            auto objJson = json.object_items();
            auto id = objJson["access_id"].string_value();
            std::string s_login = check_session(id);
            std::cout << "login: " << s_login << "     access_id: " << id << std::endl;
            delete_session(id);
        }            
        res.set_content("{\"result\":true, \"msg\":\"You are go out from system\"}", "application/json");
    }); 

    svr.Post("/api/result/get", [](const httplib::Request &request, httplib::Response &res) {
        std::string err;
        json11::Json json = json11::Json::parse(request.body, err, json11::JsonParse::COMMENTS);
        if (err.empty()) {
            auto objJson = json.object_items();
            auto id = objJson["access_id"].string_value();
            std::string s_login = check_session(id);
            if(s_login == "") {
                res.set_content("{\"result\":false, \"msg\":\"Undefined user session\"}", "application/json");
                return;
            }
            std::fstream file(std::string("./db/") + s_login + ".json", std::fstream::in);
            std::string read_s = "";
            getline(file, read_s);
            file.close();
            json11::Json results_user = json11::Json::parse(read_s, err, json11::JsonParse::COMMENTS);
            auto obj_result = results_user.object_items();
            auto result = obj_result["result"].number_value(); 
            std::cout << "login: " << s_login << "     access_id: " << id << std::endl;
            res.set_content("{\"result\":true, \"result\":\"" + std::to_string(result) + "\"}", "application/json");
            return;           
        }            
        res.set_content("{\"result\":false, \"msg\":\"File read error\"}", "application/json");
    });
    
    svr.Post("/api/result/list", [](const httplib::Request &request, httplib::Response &res) {
        std::string err;
        json11::Json json = json11::Json::parse(request.body, err, json11::JsonParse::COMMENTS);
        if (err.empty()) {
            auto objJson = json.object_items();
            auto id = objJson["access_id"].string_value();
            std::string s_login = check_session(id);
            if(s_login == "") {
                res.set_content("{\"result\":false, \"msg\":\"Undefined user session\"}", "application/json");
                return;
            }
            res.set_content("{\"result\":true, \"results\":" + get_results() + "}", "application/json");
            return;           
        }            
        res.set_content("{\"result\":false, \"msg\":\"File read error\"}", "application/json");
    });     
    svr.listen("0.0.0.0", 8080);
}