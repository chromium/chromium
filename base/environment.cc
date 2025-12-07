// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/memory/ptr_util.h"
#include "base/strings/cstring_view.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/strings/utf_string_conversions.h"
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <stdlib.h>
#endif

namespace base {

namespace {

class EnvironmentImpl : public Environment {
 public:
  std::optional<std::string> GetVar(cstring_view variable_name) override {
    auto result = GetVarImpl(variable_name);
    if (result.has_value()) {
      return result;
    }

    // Some commonly used variable names are uppercase while others
    // are lowercase, which is inconsistent. Let's try to be helpful
    // and look for a variable name with the reverse case.
    // I.e. HTTP_PROXY may be http_proxy for some users/systems.
    char first_char = variable_name[0];
    std::string alternate_case_var;
    if (IsAsciiLower(first_char)) {
      alternate_case_var = ToUpperASCII(variable_name);
    } else if (IsAsciiUpper(first_char)) {
      alternate_case_var = ToLowerASCII(variable_name);
    } else {
      return std::nullopt;
    }
    return GetVarImpl(alternate_case_var);
  }

  bool SetVar(cstring_view variable_name,
              const std::string& new_value) override {
    return SetVarImpl(variable_name, new_value);
  }

  bool UnSetVar(cstring_view variable_name) override {
    return UnSetVarImpl(variable_name);
  }

 private:
  std::optional<std::string> GetVarImpl(cstring_view variable_name) {
#if BUILDFLAG(IS_WIN)
    std::wstring wide_name = UTF8ToWide(variable_name);
    // Documented to be the maximum environment variable size in characters.
    static constexpr size_t kMaxLength = 32767;
    auto value = base::HeapArray<wchar_t>::Uninit(kMaxLength);
    const DWORD value_length =
        ::GetEnvironmentVariable(wide_name.c_str(), value.data(), kMaxLength);
    if (value_length == 0 || value_length >= kMaxLength) {
      return std::nullopt;  // Ignore errors and excessively large values.
    }
    return WideToUTF8(std::wstring_view(value.data(), value_length));
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    const char* env_value = getenv(variable_name.c_str());
    if (!env_value) {
      return std::nullopt;
    }
    return std::string(env_value);
#endif
  }

  bool SetVarImpl(cstring_view variable_name, const std::string& new_value) {
#if BUILDFLAG(IS_WIN)
    // On success, a nonzero value is returned.
    return !!SetEnvironmentVariable(UTF8ToWide(variable_name).c_str(),
                                    UTF8ToWide(new_value).c_str());
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    // On success, zero is returned.
    return !setenv(variable_name.c_str(), new_value.c_str(), 1);
#endif
  }

  bool UnSetVarImpl(cstring_view variable_name) {
#if BUILDFLAG(IS_WIN)
    // On success, a nonzero value is returned.
    return !!SetEnvironmentVariable(UTF8ToWide(variable_name).c_str(), nullptr);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    // On success, zero is returned.
    return !unsetenv(variable_name.c_str());
#endif
  }
};

}  // namespace

Environment::~Environment() = default;

// static
std::unique_ptr<Environment> Environment::Create() {
  return std::make_unique<EnvironmentImpl>();
}

bool Environment::HasVar(cstring_view variable_name) {
  return GetVar(variable_name).has_value();
}

}  // namespace base
