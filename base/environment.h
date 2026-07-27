// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ENVIRONMENT_H_
#define BASE_ENVIRONMENT_H_

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/base_export.h"
#include "base/strings/cstring_view.h"
#include "build/build_config.h"

namespace base {

namespace env_vars {

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
// On Posix systems, this variable contains the location of the user's home
// directory. (e.g, /home/username/).
inline constexpr char kHome[] = "HOME";
#endif

}  // namespace env_vars

class BASE_EXPORT Environment {
 public:
  // TODO(crbug.com/40943356): Remove and just create on the stack instead.
  static std::unique_ptr<Environment> Create();

  Environment();
  virtual ~Environment();

  // Returns an environment variable's value.
  // Returns std::nullopt if the key is unset.
  // Note that the variable may be set to an empty string.
  //
  // Most methods in this class are virtual for testing purposes.
  virtual std::optional<std::string> GetVar(cstring_view variable_name);

  // Syntactic sugar for GetVar(variable_name).has_value();
  bool HasVar(cstring_view variable_name);

  // Returns true on success, otherwise returns false. This method should not
  // be called in a multi-threaded process.
  virtual bool SetVar(cstring_view variable_name, const std::string& new_value);

  // Returns true on success, otherwise returns false. This method should not
  // be called in a multi-threaded process.
  virtual bool UnSetVar(cstring_view variable_name);
};

#if BUILDFLAG(IS_WIN)
using NativeEnvironmentString = std::wstring;
using NativeEnvironmentStringView = std::wstring_view;
using NativeEnvironmentCStringView = base::wcstring_view;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
using NativeEnvironmentString = std::string;
using NativeEnvironmentStringView = std::string_view;
using NativeEnvironmentCStringView = base::cstring_view;
#endif
// EnvironmentMap uses std::less<> to enable transparent (heterogeneous) lookup,
// allowing NativeEnvironmentCStringView to be used for search without creating
// temporary std::string or std::wstring objects.
using EnvironmentMap =
    std::map<NativeEnvironmentString, NativeEnvironmentString, std::less<>>;

}  // namespace base

#endif  // BASE_ENVIRONMENT_H_
