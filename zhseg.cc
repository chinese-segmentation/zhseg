// 
//   Copyright 2009 Chris Dyer <redpony@umd.edu>
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//

#include <iostream>
#include <cassert>
#include <fstream>
#include <vector>
#include <cmath>
#include <ext/hash_map>
#include "utf8.h"

#define MAX_WORD_SIZE 6

// hash function
namespace __gnu_cxx {
  template<> struct hash<std::string> {
    inline size_t operator()(const std::string& s) const {
      size_t x = 5381;
      for (int i = 0; i < s.size(); ++i) {
        x = ((x << 5) + x) ^ s[i];
      }
      return x;
    }
  };
};

using namespace std;
using namespace __gnu_cxx;

// read lines like this:
// 12 哎哟 ai1yo1
float load_dict(const string& fname, hash_map<string, float>* dict) {
  hash_map<string, float>& d = *dict;
  ifstream in(fname.c_str());
  assert(in);
  float total = 0;
  while(in) {
    string line;
    getline(in, line);
    if (line.empty() || line[0] == '#') continue;
    size_t s1 = line.find(' ');
    assert(s1 != string::npos);
    size_t s2 = line.find(' ', s1 + 1);
    string word;
    if (s2 == string::npos) {
      word = line.substr(s1 + 1);
    } else {
      word = line.substr(s1 + 1, s2 - s1 - 1);
    }
    line[s1] = 0;
    float count = log(atof(line.c_str()) + 1);
    float &val = d[word];
    if (!val) {
      d[word] = count;
      total += count;
    } else {
      if (val != count) { cerr << word << " has multiple counts!\n"; }
    }
  }
  return log(total);
}

inline bool is_punctuation(uint32_t cp) {
  return (cp > 0x3000 && cp <= 0x3020) ||
         (cp >= 0xFF1A && cp <= 0xFF1B) ||
         (cp >= 0xFF0C && cp <= 0xFF0E) || (cp == 32);
}

string process(const string& us, const hash_map<string, float>& dict, float total) {
  vector<bool> is_ok(us.size() + 1, false);
  vector<bool> is_punc(us.size() + 1, false);
  is_ok.back() = true;
  for (string::const_iterator i = us.begin(); i != us.end(); ) {
    int pos = i - us.begin();
    uint32_t cp = utf8::next(i, us.end());
    is_punc[pos] = is_punctuation(cp);
    is_ok[pos] = true;
  }

  // cost/backtracking table
  vector<pair<int, float> > m(us.size() + 1, pair<int, float>(0, -1999999.0f));
  m[0].second = 0.0f;
  for (int i = 0; i < us.size(); ++i) {
    if (!is_ok[i]) continue;
    // spaces are forced breaks
    if (us[i] == ' ') {
      m[i + 1].first = 1;
      m[i + 1].second = m[i].second;
      continue;
    }
    int end = i + MAX_WORD_SIZE * 3;
    if (end >= us.size())
      end = us.size();
    bool is_first = true;
    for (int j = i; j < end; ++j) {
      if (!is_first && is_punc[j]) break;
      if (!is_ok[j + 1]) continue;
      if (us[j] == ' ') break;
      const string cand = us.substr(i, j - i + 1);
      pair<int,float>& target = m[j + 1];
//      cerr << cand << endl;
      if (is_first) {
        is_first = false;
        string::const_iterator it = cand.begin();
        uint32_t cp = utf8::next(it, cand.end());

        // always segment punctuation
        if (is_punctuation(cp)) {
          target.first = j - i + 1;
          target.second = m[i].second;
          break;
        }
      }
      hash_map<string,float>::const_iterator it = dict.find(cand);
      float freq = 0.3;
      float bonus = 0.0;
      if (it != dict.end()) {
        freq = it->second;
        bonus = ((j - i + 1) / 3) - 1;
        bonus *= (total * 2);  // favor longer matches (this should be tuned)
      }
//      cerr << " (freq=" << freq << ")  bonus=" << bonus << endl;
      float cand_score = freq + m[i].second + bonus;
      if (cand_score > target.second) {
        target.first = j - i + 1;
        target.second = cand_score;
//        cerr << "C=" << (j+1) << ": NEW_BEST=" << cand_score << "\n";
      }
    }
  }
  int j = us.size();
  string res(us.size() * 2, ' ');
  int c = 0;
  while(j > 0) {
    int i = j - m[j].first;
//    cerr << "[" << i << "," << j << ")" << endl;
//    const string chunk = us.substr(i, j-i);
//    cerr << "  " << chunk << endl;
    for (int k = j-1; k >= i; --k)
      res[c++] = us[k];
    if (res[c-1] != ' ') c++; else c--;
    j = i;
  }
  assert(c > 0);
  res.resize(c - 1);
  reverse(res.begin(), res.end());

  return res;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    cerr << "\nUsage: " << argv[0] << " chinese.freq\n\n  Simple Chinese segmenter.  Input (STDIN) and frequency dictionary\n  must be in UTF-8 encoding.\n\n";
    return 1;
  }
  hash_map<string, float> fdict;
  float total = load_dict(argv[1], &fdict);
  //cerr << "Total: " << total << endl;
  while(cin) {
    string line;
    getline(cin, line);
    if (line.empty()) {
      if (cin.eof()) continue;
      cout << endl;
    } else {
      cout << process(line, fdict, total) << endl << flush;
    }
  }
  return 0;
}

