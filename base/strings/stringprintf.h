// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRINGPRINTF_H_
#define BASE_STRINGS_STRINGPRINTF_H_

#include <stdarg.h>  // va_list

#include <string>
#include <string_view>

#include "base/base_export.h"
#include "base/compiler_specific.h"

namespace base {

// Returns a C++ string given `printf()`-like input. The format string should be
// a compile-time constant (like with `std::format()`).
// TODO(crbug.com/1371963): Implement in terms of `std::format()`,
// `absl::StrFormat()`, or similar.
[[nodiscard]] BASE_EXPORT std::string StringPrintf(const char* format, ...)
    PRINTF_FORMAT(1, 2);

// Returns a C++ string given `printf()`-like input. The format string must be a
// run-time value (like with `std::vformat()`), or this will not compile.
// Because this does not check arguments at compile-time, prefer
// `StringPrintf()` whenever possible.
template <typename... Args>
[[nodiscard]] std::string StringPrintfNonConstexpr(std::string_view format,
                                                   const Args&... args) {
  // TODO(crbug.com/1371963): Implement in terms of `std::vformat()`,
  // `absl::FormatUntyped()`, or similar.
  return StringPrintf(std::string(format).c_str(), args...);
}

// If possible, guide users to use `StringPrintf()` instead of
// `StringPrintfNonConstexpr()` when the format string is constexpr.
//
// It would be nice to do this with `std::enable_if`, but I don't know of a way;
// whether a string constant's value is available at compile time is not
// something easily obtained from the type system, and trying to pass various
// forms of string constant to non-type template parameters produces a variety
// of compile errors.
#if HAS_ATTRIBUTE(enable_if)
// Disable calling with a constexpr `std::string_view`.
template <typename... Args>
[[nodiscard]] std::string StringPrintfNonConstexpr(std::string_view format,
                                                   const Args&... args)
    __attribute__((enable_if(
        [](std::string_view s) { return s.empty() || s[0] == s[0]; }(format),
        "Use StringPrintf() for constexpr format strings"))) = delete;
// Disable calling with a constexpr `char[]` or `char*`.
template <typename... Args>
[[nodiscard]] std::string StringPrintfNonConstexpr(const char* format,
                                                   const Args&... args)
    __attribute__((
        enable_if([](const char* s) { return !!s; }(format),
                  "Use StringPrintf() for constexpr format strings"))) = delete;
#endif

// Returns a C++ string given `vprintf()`-like input.
[[nodiscard]] BASE_EXPORT std::string StringPrintV(const char* format,
                                                   va_list ap)
    PRINTF_FORMAT(1, 0);

// Like `StringPrintf()`, but appends result to a supplied string.
// TODO(crbug.com/1371963): Implement in terms of `std::format_to()`,
// `absl::StrAppendFormat()`, or similar.
BASE_EXPORT void StringAppendF(std::string* dst, const char* format, ...)
    PRINTF_FORMAT(2, 3);

// Like `StringPrintV()`, but appends result to a supplied string.
BASE_EXPORT void StringAppendV(std::string* dst, const char* format, va_list ap)
    PRINTF_FORMAT(2, 0);

}  // namespace base

#endif  // BASE_STRINGS_STRINGPRINTF_H_
