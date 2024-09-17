// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"

#include <errno.h>
#include <stddef.h>

#include <vector>

#include "base/logging.h"
#include "base/scoped_clear_last_error.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

namespace base {

std::string StringPrintV(const char* format, va_list ap) {
  std::string result;
  StringAppendV(&result, format, ap);
  return result;
}

void StringAppendV(std::string* dst, const char* format, va_list ap) {
  // First try with a small fixed size buffer.
  // This buffer size should be kept in sync with StringUtilTest.GrowBoundary
  // and StringUtilTest.StringPrintfBounds.
  char stack_buf[1024];

  va_list ap_copy;
  va_copy(ap_copy, ap);

  base::ScopedClearLastError last_error;
  int result = vsnprintf(stack_buf, std::size(stack_buf), format, ap_copy);
  va_end(ap_copy);

  if (result >= 0 && static_cast<size_t>(result) < std::size(stack_buf)) {
    // It fit.
    dst->append(stack_buf, static_cast<size_t>(result));
    return;
  }

  // Repeatedly increase buffer size until it fits.
  size_t mem_length = std::size(stack_buf);
  while (true) {
    if (result < 0) {
#if BUILDFLAG(IS_WIN)
      // On Windows, vsnprintf always returns the number of characters in a
      // fully-formatted string, so if we reach this point, something else is
      // wrong and no amount of buffer-doubling is going to fix it.
      return;
#else
      if (errno != 0 && errno != EOVERFLOW) {
        return;
      }
      // Try doubling the buffer size.
      mem_length *= 2;
#endif
    } else {
      // We need exactly "result + 1" characters.
      mem_length = static_cast<size_t>(result) + 1;
    }

    if (mem_length > 32 * 1024 * 1024) {
      // That should be plenty, don't try anything larger.  This protects
      // against huge allocations when using vsnprintf implementations that
      // return -1 for reasons other than overflow without setting errno.
      DLOG(WARNING) << "Unable to printf the requested string due to size.";
      return;
    }

    std::vector<char> mem_buf(mem_length);

    // NOTE: You can only use a va_list once.  Since we're in a while loop, we
    // need to make a new copy each time so we don't use up the original.
    va_copy(ap_copy, ap);
    result = vsnprintf(&mem_buf[0], mem_length, format, ap_copy);
    va_end(ap_copy);

    if ((result >= 0) && (static_cast<size_t>(result) < mem_length)) {
      // It fit.
      dst->append(&mem_buf[0], static_cast<size_t>(result));
      return;
    }
  }
}

}  // namespace base
