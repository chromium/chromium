// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BHO_MINI_BHO_UTIL_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BHO_MINI_BHO_UTIL_H_

#include <stdarg.h>

enum LogLevel {
  ERR,
  WARNING,
  INFO,
  DEBUG,
};

// Basically a fake stdlib. Most functions have the same name & signature as the
// ones in libc (mostly from string.h).
namespace util {

constexpr LogLevel kLogLevel = INFO;

extern const char* kLogPrefixes[];

// write(), puts(), and printf() log to this file:
// "AppData\LocalLow\Google\BrowserSwitcher\ie_bho_log2.txt"
void InitLog();
void CloseLog();
void SetLogFilePathForTesting(const wchar_t* log_path);

void write(const char* s, size_t num);
void puts(const char* s);
void puts(LogLevel lvl, const char* s);

void vprintf(const char* fmt, va_list arglist);
void printf(const char* fmt, ...);
void printf(LogLevel lvl, const char* fmt, ...);

// Fixed-sized vector of |T| that lives on the heap. Frees memory when deleted.
template <typename T>
class vector {
 public:
  vector() : vector(0) {}
  explicit vector(size_t size) {
    data_ = (size == 0) ? nullptr : new T[size];
    capacity_ = size;
  }
  ~vector() {
    if (data_ != nullptr)
      delete[] data_;
  }

  // Avoid accidental copy.
  vector(vector<T>&) = delete;

  vector(vector<T>&& that) : vector() { swap(that); }
  vector<T>& operator=(vector<T>&& that) {
    swap(that);
    return *this;
  }

  void swap(vector<T>& that) {
    T* tmp_data = data_;
    size_t tmp_capacity = capacity_;
    data_ = that.data_;
    capacity_ = that.capacity_;
    that.data_ = tmp_data;
    that.capacity_ = tmp_capacity;
  }

  size_t capacity() { return capacity_; }
  T* data() { return data_; }

  T* begin() { return data_; }
  const T* begin() const { return data_; }
  // This iterator is incorrect for strings. It'll include the terminating '\0'.
  T* end() { return data_ + capacity_; }
  const T* end() const { return data_ + capacity_; }

  // No bounds-checking.
  T& operator[](size_t pos) { return data_[pos]; }

 private:
  T* data_;
  size_t capacity_;
};

// A vector of bytes = poor man's string.
using string = vector<char>;
using wstring = vector<wchar_t>;

util::string empty_string();
util::wstring empty_wstring();

int max(int a, int b);
int min(int a, int b);

// Unlike strcpy(), etc., this can copy to overlapping memory areas.
void* memmove(void* dest, const void* src, size_t num);

char* strtok(char* str, const char* delimiters);

// Replaces the first occurrence of |orig| with |repl|. Returns true if a
// replacement was done.
bool wcs_replace_s(wchar_t* str,
                   size_t strsz,
                   const wchar_t* orig,
                   const wchar_t* repl);

string utf16_to_utf8(const wchar_t* utf16);
wstring utf8_to_utf16(const char* utf8);

}  // namespace util

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BHO_MINI_BHO_UTIL_H_
