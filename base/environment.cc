// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"

#include <array>
#include <string_view>

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <stdlib.h>
#endif

namespace base {

namespace {

class EnvironmentImpl : public Environment {
 public:
  bool GetVar(std::string_view variable_name, std::string* result) override {
    if (GetVarImpl(variable_name, result))
      return true;

    // Some commonly used variable names are uppercase while others
    // are lowercase, which is inconsistent. Let's try to be helpful
    // and look for a variable name with the reverse case.
    // I.e. HTTP_PROXY may be http_proxy for some users/systems.
    char first_char = variable_name[0];
    std::string alternate_case_var;
    if (IsAsciiLower(first_char))
      alternate_case_var = ToUpperASCII(variable_name);
    else if (IsAsciiUpper(first_char))
      alternate_case_var = ToLowerASCII(variable_name);
    else
      return false;
    return GetVarImpl(alternate_case_var, result);
  }

  bool SetVar(std::string_view variable_name,
              const std::string& new_value) override {
    return SetVarImpl(variable_name, new_value);
  }

  bool UnSetVar(std::string_view variable_name) override {
    return UnSetVarImpl(variable_name);
  }

 private:
  bool GetVarImpl(std::string_view variable_name, std::string* result) {
#if BUILDFLAG(IS_WIN)
    std::wstring wide_name = UTF8ToWide(variable_name);
    if (!result) {
      return ::GetEnvironmentVariable(wide_name.c_str(), nullptr, 0) != 0;
    }
    // Documented to be the maximum environment variable size.
    std::array<wchar_t, 32767> value;
    DWORD value_length =
        ::GetEnvironmentVariable(wide_name.c_str(), value.data(), value.size());
    if (value_length == 0) {
      return false;
    }
    CHECK_LE(value_length, value.size() - 1)
        << "value should fit in the buffer (including the null terminator)";
    WideToUTF8(value.data(), value_length, result);
    return true;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    const char* env_value = getenv(std::string(variable_name).c_str());
    if (!env_value)
      return false;
    // Note that the variable may be defined but empty.
    if (result)
      *result = env_value;
    return true;
#endif
  }

  bool SetVarImpl(std::string_view variable_name,
                  const std::string& new_value) {
#if BUILDFLAG(IS_WIN)
    // On success, a nonzero value is returned.
    return !!SetEnvironmentVariable(UTF8ToWide(variable_name).c_str(),
                                    UTF8ToWide(new_value).c_str());
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    // On success, zero is returned.
    return !setenv(variable_name.data(), new_value.c_str(), 1);
#endif
  }

  bool UnSetVarImpl(std::string_view variable_name) {
#if BUILDFLAG(IS_WIN)
    // On success, a nonzero value is returned.
    return !!SetEnvironmentVariable(UTF8ToWide(variable_name).c_str(), nullptr);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    // On success, zero is returned.
    return !unsetenv(variable_name.data());
#endif
  }
};

}  // namespace

namespace env_vars {

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
// On Posix systems, this variable contains the location of the user's home
// directory. (e.g, /home/username/).
const char kHome[] = "HOME";
#endif

}  // namespace env_vars

Environment::~Environment() = default;

// static
std::unique_ptr<Environment> Environment::Create() {
  return std::make_unique<EnvironmentImpl>();
}

bool Environment::HasVar(std::string_view variable_name) {
  return GetVar(variable_name, nullptr);
}

}  // namespace base
