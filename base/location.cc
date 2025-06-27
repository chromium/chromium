// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"

#include <string_view>

#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"

#if defined(COMPILER_MSVC)
#include <intrin.h>
#endif

namespace base {

namespace {

#if defined(__clang__) && defined(_MSC_VER)
constexpr std::string_view kThisFilePath = "base\\location.cc";
#else
constexpr std::string_view kThisFilePath = "base/location.cc";
#endif

// Finds the length of the build folder prefix from the file path.
// TODO(ssid): Strip prefixes from stored strings in the binary. This code only
// skips the prefix while reading the file name strings at runtime.
constexpr size_t StrippedFilePathPrefixLength() {
  constexpr std::string_view kPath = __FILE__;
  // Only keep the file path starting from the src directory.

  constexpr size_t kPathLen = kPath.size();
  constexpr size_t kStrippedLen = kThisFilePath.size();
  static_assert(kPathLen >= kStrippedLen,
                "Invalid file path for base/location.cc.");
  return kPathLen - kStrippedLen;
}

constexpr size_t kStrippedPrefixLength = StrippedFilePathPrefixLength();

// Returns true if the |name| string has |prefix_len| characters in the prefix
// and the suffix matches the |expected| string.
// TODO(ssid): With C++20 we can make base::EndsWith() constexpr and use it
//  instead.
constexpr bool StrEndsWith(std::string_view name,
                           size_t prefix_len,
                           std::string_view expected) {
  return name.substr(prefix_len) == expected;
}

static_assert(StrEndsWith(__FILE__, kStrippedPrefixLength, kThisFilePath),
              "The file name does not match the expected prefix format.");

}  // namespace

Location::Location() = default;
Location::Location(const Location& other) = default;
Location::Location(Location&& other) noexcept = default;
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
  // The program counter should not be null except in a default constructed
  // (empty) Location object. This value is used for identity, so if it doesn't
  // uniquely identify a location, things will break.
  DCHECK(program_counter);
}

std::string Location::ToString() const {
  if (has_source_info()) {
    return std::string(function_name_) + "@" + file_name_ + ":" +
           NumberToString(line_number_);
  }
  return StringPrintf("pc:%p", program_counter_);
}

void Location::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("function_name", function_name_);
  dict.Add("file_name", file_name_);
  dict.Add("line_number", line_number_);
}

#if defined(COMPILER_MSVC)
#define RETURN_ADDRESS() _ReturnAddress()
#elif defined(COMPILER_GCC)
#define RETURN_ADDRESS() \
  __builtin_extract_return_addr(__builtin_return_address(0))
#else
#define RETURN_ADDRESS() nullptr
#endif

// static
NOINLINE Location Location::Current(const char* function_name,
                                    const char* file_name,
                                    int line_number) {
  return Location(function_name, UNSAFE_TODO(file_name + kStrippedPrefixLength),
                  line_number, RETURN_ADDRESS());
}

// static
NOINLINE Location Location::CurrentWithoutFunctionName(const char* file_name,
                                                       int line_number) {
  return Location(nullptr, UNSAFE_TODO(file_name + kStrippedPrefixLength),
                  line_number, RETURN_ADDRESS());
}

//------------------------------------------------------------------------------
NOINLINE const void* GetProgramCounter() {
  return RETURN_ADDRESS();
}

}  // namespace base
