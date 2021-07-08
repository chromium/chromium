// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"

// location.h is a widely included header and its size can significantly impact
// build time. Try not to raise this limit unless absolutely necessary. See
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/wmax_tokens.md
#ifndef NACL_TC_REV
#pragma clang max_tokens_here 240000
#endif

#if defined(COMPILER_MSVC)
#include <intrin.h>
#endif

#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"

namespace base {

namespace {

// Returns the length of the given null terminated c-string.
constexpr size_t StrLen(const char* str) {
  size_t str_len = 0;
  for (str_len = 0; str[str_len] != '\0'; ++str_len)
    ;
  return str_len;
}

// Finds the length of the build folder prefix from the file path.
// TODO(ssid): Strip prefixes from stored strings in the binary. This code only
// skips the prefix while reading the file name strings at runtime.
constexpr size_t StrippedFilePathPrefixLength() {
  constexpr char path[] = __FILE__;
  // Only keep the file path starting from the src directory.
  constexpr char stripped[] = "base/location.cc";
  constexpr size_t path_len = StrLen(path);
  constexpr size_t stripped_len = StrLen(stripped);
  static_assert(path_len >= stripped_len,
                "Invalid file path for base/location.cc.");
  return path_len - stripped_len;
}

constexpr size_t kStrippedPrefixLength = StrippedFilePathPrefixLength();

// Returns true if the |name| string has |prefix_len| characters in the prefix
// and the suffix matches the |expected| string.
// TODO(ssid): With C++20 we can make base::EndsWith() constexpr and use it
//  instead.
constexpr bool StrEndsWith(const char* name,
                           size_t prefix_len,
                           const char* expected) {
  const size_t name_len = StrLen(name);
  const size_t expected_len = StrLen(expected);
  if (name_len != prefix_len + expected_len)
    return false;
  for (size_t i = 0; i < expected_len; ++i) {
    if (name[i + prefix_len] != expected[i])
      return false;
  }
  return true;
}

static_assert(StrEndsWith(__FILE__, kStrippedPrefixLength, "base/location.cc"),
              "The file name does not match the expected prefix format.");

}  // namespace

Location::Location() = default;
Location::Location(const Location& other) = default;
Location& Location::operator=(const Location& other) = default;

Location::Location(const char* file_name, const void* program_counter)
    : file_name_(file_name), program_counter_(program_counter) {}

Location::Location(const char* function_name,
                   const char* file_name,
                   int line_number,
                   const void* program_counter)
    : function_name_(function_name),
      file_name_(file_name),
      line_number_(line_number),
      program_counter_(program_counter) {
#if !defined(OS_NACL)
  // The program counter should not be null except in a default constructed
  // (empty) Location object. This value is used for identity, so if it doesn't
  // uniquely identify a location, things will break.
  //
  // The program counter isn't supported in NaCl so location objects won't work
  // properly in that context.
  DCHECK(program_counter);
#endif
}

std::string Location::ToString() const {
  if (has_source_info()) {
    return std::string(function_name_) + "@" + file_name_ + ":" +
           NumberToString(line_number_);
  }
  return StringPrintf("pc:%p", program_counter_);
}

#if defined(COMPILER_MSVC)
#define RETURN_ADDRESS() _ReturnAddress()
#elif defined(COMPILER_GCC) && !defined(OS_NACL)
#define RETURN_ADDRESS() \
  __builtin_extract_return_addr(__builtin_return_address(0))
#else
#define RETURN_ADDRESS() nullptr
#endif

#if !BUILDFLAG(FROM_HERE_USES_LOCATION_BUILTINS)
#if !BUILDFLAG(ENABLE_LOCATION_SOURCE)

// static
NOINLINE Location Location::CreateFromHere(const char* file_name) {
  return Location(file_name + kStrippedPrefixLength, RETURN_ADDRESS());
}

#else

// static
NOINLINE Location Location::CreateFromHere(const char* function_name,
                                           const char* file_name,
                                           int line_number) {
  return Location(function_name, file_name + kStrippedPrefixLength, line_number,
                  RETURN_ADDRESS());
}

#endif
#endif

#if SUPPORTS_LOCATION_BUILTINS && BUILDFLAG(ENABLE_LOCATION_SOURCE)
// static
NOINLINE Location Location::Current(const char* function_name,
                                    const char* file_name,
                                    int line_number) {
  return Location(function_name, file_name + kStrippedPrefixLength, line_number,
                  RETURN_ADDRESS());
}
#elif SUPPORTS_LOCATION_BUILTINS
// static
NOINLINE Location Location::Current(const char* file_name) {
  return Location(file_name + kStrippedPrefixLength, RETURN_ADDRESS());
}
#else
// static
NOINLINE Location Location::Current() {
  return Location(nullptr, RETURN_ADDRESS());
}
#endif

//------------------------------------------------------------------------------
NOINLINE const void* GetProgramCounter() {
  return RETURN_ADDRESS();
}

}  // namespace base
