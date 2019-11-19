// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/bho/mini_bho_util.h"

#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <windows.h>

namespace util {

const char* kLogPrefixes[] = {
    "[*ERROR!*] : ",
    "[WARN] : ",
    "[info] : ",
    "[debug] : ",
};

// Points to "AppData\LocalLow\Google\BrowserSwitcher\ie_bho_log2.txt".
HANDLE g_log_file = 0;

const wchar_t* g_log_file_path_for_testing = nullptr;

void GetLogFilePath(wchar_t* log_file_path) {
  if (g_log_file_path_for_testing != nullptr) {
    ::StringCchCopyW(log_file_path, MAX_PATH, g_log_file_path_for_testing);
    return;
  }

  OSVERSIONINFO info = {0};
  info.dwOSVersionInfoSize = sizeof(info);
  GetVersionEx(&info);
  if (info.dwMajorVersion >= 6) {
    wchar_t* path;
    // On modern Windows versions there is a special AppData folder for
    // processes with lowered execution rights, however older versions lack
    // this folder so be prepared to back off to the usual AppData folder.
    if (S_OK !=
        ::SHGetKnownFolderPath(FOLDERID_LocalAppDataLow, 0, nullptr, &path)) {
      if (S_OK !=
          ::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)) {
        return;
      }
    }
    ::StringCchCopyW(log_file_path, MAX_PATH, path);
    ::CoTaskMemFree(path);
  } else {
    // Old windows version only support SHGetSpecialFolderPath.
    if (!::SHGetSpecialFolderPath(0, log_file_path, CSIDL_LOCAL_APPDATA, false))
      return;
  }
  ::StringCchCatW(log_file_path, MAX_PATH, L"\\Google");
  ::CreateDirectory(log_file_path, nullptr);
  ::StringCchCatW(log_file_path, MAX_PATH, L"\\BrowserSwitcher");
  ::CreateDirectory(log_file_path, nullptr);
  ::StringCchCatW(log_file_path, MAX_PATH, L"\\ie_bho_log2.txt");
}

void InitLog() {
  wchar_t log_file_path[MAX_PATH] = L"";
  GetLogFilePath(log_file_path);
  if (*log_file_path == '\0')
    return;
  g_log_file =
      ::CreateFile(log_file_path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

void CloseLog() {
  ::CloseHandle(g_log_file);
}

void SetLogFilePathForTesting(const wchar_t* log_file_path) {
  g_log_file_path_for_testing = log_file_path;
}

void write(const char* s, size_t num) {
  if (g_log_file)
    ::WriteFile(g_log_file, s, num, nullptr, nullptr);
}

void vprintf(const char* fmt, va_list arglist) {
  // wvsprintfA() is limited to 1024 chars. The rest is truncated.
  char buffer[1025];
  wvsprintfA(buffer, fmt, arglist);
  write(buffer, ::lstrlenA(buffer));
}

void printf(const char* fmt, ...) {
  va_list arglist;
  va_start(arglist, fmt);
  vprintf(fmt, arglist);
  va_end(arglist);
}

void printf(LogLevel lvl, const char* fmt, ...) {
  if (kLogLevel < lvl)
    return;
  printf("%s", kLogPrefixes[lvl]);
  va_list arglist;
  va_start(arglist, fmt);
  vprintf(fmt, arglist);
  va_end(arglist);
}

void puts(const char* s) {
  printf("%s\n", s);
}

void puts(LogLevel lvl, const char* s) {
  if (kLogLevel < lvl)
    return;
  printf("%s", kLogPrefixes[lvl]);
  puts(s);
}

util::string empty_string() {
  util::string empty(1);
  empty[0] = '\0';
  return empty;
}

util::wstring empty_wstring() {
  util::wstring empty(1);
  empty[0] = '\0';
  return empty;
}

int max(int a, int b) {
  return (a < b) ? b : a;
}

int min(int a, int b) {
  return (a < b) ? a : b;
}

void* memmove(void* dest, const void* src, size_t num) {
  if (dest == src)
    return dest;
  // Copy backwards if src < dest.
  int dir = (src < dest) ? -1 : 1;
  char* dest_bytes = reinterpret_cast<char*>(dest);
  const char* src_bytes = reinterpret_cast<const char*>(src);
  if (src < dest) {
    dest_bytes += (num - 1);
    src_bytes += (num - 1);
  }
  for (size_t i = 0; i < num; i++) {
    *dest_bytes = *src_bytes;
    dest_bytes += dir;
    src_bytes += dir;
  }
  return dest;
}

char* strtok(char* str, const char* delimiters) {
  static char* last_str = nullptr;
  if (str == nullptr && last_str == nullptr)
    return nullptr;
  if (str == nullptr)
    str = last_str;
  size_t i = 0;
  for (; ::StrChrA(delimiters, str[i]) == nullptr; i++) {
    if (str[i] == '\0') {
      last_str = nullptr;
      return str;
    }
  }
  str[i] = '\0';
  last_str = str + i + 1;
  return str;
}

bool wcs_replace_s(wchar_t* str,
                   size_t strsz,
                   const wchar_t* orig,
                   const wchar_t* repl) {
  wchar_t* pos = ::StrStrW(str, orig);
  if (pos == nullptr)
    return false;

  size_t str_length = ::lstrlenW(str);
  size_t repl_length = ::lstrlenW(repl);
  size_t orig_length = ::lstrlenW(orig);
  size_t new_length = str_length + repl_length - orig_length;

  wchar_t* dest = pos + repl_length;
  wchar_t* src = pos + orig_length;

  size_t src_length = ::lstrlenW(src);

  // Move the rest of the string to the new position, making space for |repl|.
  memmove(dest, src,
          max(0, min(src_length, strsz - (dest - str))) * sizeof(*str));
  // Insert |repl| inside the string, writing over |orig|.
  memmove(pos, repl,
          max(0, min(repl_length, strsz - (pos - str))) * sizeof(*str));
  str[min(strsz - 1, new_length)] = '\0';

  return true;
}

string utf16_to_utf8(const wchar_t* utf16) {
  const size_t buffer_size = max(
      1,
      WideCharToMultiByte(CP_UTF8, 0, utf16, -1, nullptr, 0, nullptr, nullptr));
  string utf8(buffer_size);
  WideCharToMultiByte(CP_UTF8, 0, utf16, -1, utf8.data(), buffer_size, nullptr,
                      nullptr);
  return utf8;
}

wstring utf8_to_utf16(const char* utf8) {
  const size_t buffer_size =
      max(1, MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0));
  wstring utf16(buffer_size);
  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, utf16.data(), buffer_size);
  return utf16;
}

}  // namespace util
