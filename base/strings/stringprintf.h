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

// Returns a C++ string given `printf()`-like input. The format string must be a
// compile-time constant (like with `std::format()`), or this will not compile.
// TODO(crbug.com/40241565): Implement in terms of `std::format()`,
// `absl::StrFormat()`, or similar.
[[nodiscard]] PRINTF_FORMAT(1, 2) BASE_EXPORT std::string
    StringPrintf(const char* format, ...)
        ENABLE_IF_ATTR(!!format,
                       "Make format string constexpr; use "
                       "StringPrintfNonConstexpr() if that's impossible");

#if HAS_ATTRIBUTE(enable_if)
// Returns a C++ string given `printf()`-like input. The format string must be a
// run-time value (like with `std::vformat()`), or this will not compile.
// Because this does not check arguments at compile-time, prefer
// `StringPrintf()` whenever possible.
// TODO(crbug.com/40241565): Implement in terms of `std::vformat()`,
// `absl::FormatUntyped()`, or similar.
[[nodiscard]] BASE_EXPORT std::string StringPrintfNonConstexpr(
    const char* format,
    ...);
[[nodiscard]] std::string StringPrintfNonConstexpr(const char* format, ...)
    ENABLE_IF_ATTR(!!format,
                   "Use StringPrintf() for constexpr format strings") = delete;
#else
// When `ENABLE_IF_ATTR()` is unavailable, just make calls to this function
// equivalent to `StringPrintf()`. Note that because of the varargs signature,
// we can't "forward" to `StringPrintf()` (we'd have to unpack to a `va_list`
// here and use `StringPrintV()` instead).
#define StringPrintfNonConstexpr StringPrintf
#endif

// Returns a C++ string given `vprintf()`-like input.
[[nodiscard]] PRINTF_FORMAT(1, 0) BASE_EXPORT std::string
    StringPrintV(const char* format, va_list ap);

// Like `StringPrintf()`, but appends result to a supplied string.
// TODO(crbug.com/40241565): Implement in terms of `std::format_to()`,
// `absl::StrAppendFormat()`, or similar.
PRINTF_FORMAT(2, 3)
BASE_EXPORT void StringAppendF(std::string* dst, const char* format, ...);

// Like `StringPrintV()`, but appends result to a supplied string.
PRINTF_FORMAT(2, 0)
BASE_EXPORT
void StringAppendV(std::string* dst, const char* format, va_list ap);

}  // namespace base

#endif  // BASE_STRINGS_STRINGPRINTF_H_
