#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool PreprocessInternal(ifstream& input, ofstream&output, const path& in_file, const vector<path>&include_directories) {
    static regex custom_dir(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static regex std_dir(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    smatch m;

    string str;
    int str_line = 0;
    path file_path;
    bool finded_custom = true;

    while(getline(input, str)) {
        ++str_line;
        if (regex_match(str, m, custom_dir)) {
            file_path = in_file.parent_path() / string(m[1]);
            if (filesystem::exists(file_path)) {
                ifstream new_input(file_path);
                if(new_input.is_open()) {
                    if (!PreprocessInternal(new_input, output, file_path, include_directories)) {
                        cout << "unknown include file "s << file_path.filename().string() 
                        << " at file "s << in_file.string() << " at line "s << str_line << "\n";
                        return false;
                    }
                    continue;
                }else {
                    cout << "unknown include file "s << file_path.filename().string() 
                        << " at file "s << in_file.string() << " at line "s << str_line << "\n";
                        return false;
                }
            }else {
                finded_custom = false;
            }
        }

        if (!finded_custom || regex_match(str, m, std_dir)) {
            bool finded_cus_or_std = false;
            for (const auto& dir : include_directories) {
                file_path = dir / string(m[1]);
                if (!filesystem::exists(file_path)) {
                    finded_cus_or_std = false;
                }else {
                    ifstream new_input(file_path);
                    if (new_input.is_open()) {
                        if (!PreprocessInternal(new_input, output, file_path, include_directories)) {
                            cout << "unknown include file "s << file_path.filename().string() 
                            << " at file "s << in_file.string() << " at line "s << str_line << "\n";
                            return false;
                        }
                        finded_cus_or_std = true;
                        break;
                    }
                }
            }
            if (!finded_cus_or_std) {
                cout << "unknown include file "s << file_path.filename().string() 
                        << " at file "s << in_file.string() << " at line "s << str_line << "\n";
                        return false;
            }
            finded_custom = true;
            continue;
        }

        output << str << "\n"s;
    }

    return true;
}


bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    if (!filesystem::exists(in_file)) {
        return false;
    }
    ifstream input(in_file);
    if (!input.is_open()) {
        return false;
    }
    ofstream output(out_file);
    if (!output.is_open()) {
        return false;
    }

    return PreprocessInternal(input, output, in_file, include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);

    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}