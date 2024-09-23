// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/strings/cstring_builder.h"

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/strings/safe_sprintf.h"

#if !PA_BUILDFLAG(IS_WIN)
#include <unistd.h>
#endif

#include <cmath>
#include <cstring>
#include <limits>

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
#include "partition_alloc/partition_alloc_base/check.h"
#define PA_RAW_DCHECK PA_RAW_CHECK
#else
#define PA_RAW_DCHECK(x) \
  do {                   \
    if (x) {             \
    }                    \
  } while (0)
#endif

namespace partition_alloc::internal::base::strings {

namespace {

constexpr size_t kNumDigits10 = 5u;

constexpr uint64_t Pow10(unsigned exp) {
  uint64_t ret = 1;
  for (unsigned i = 0; i < exp; ++i) {
    ret *= 10U;
  }
  return ret;
}

constexpr uint64_t Log10(uint64_t value) {
  uint64_t ret = 0;
  while (value != 0u) {
    value = value / 10u;
    ++ret;
  }
  return ret;
}

constexpr uint64_t GetDigits10(unsigned num_digits10) {
  return Pow10(num_digits10);
}

}  // namespace

template <typename T>
void CStringBuilder::PutInteger(T value) {
  // We need an array of chars whose size is:
  // - floor(log10(max value)) + 1 chars for the give value, and
  // - 1 char for '-' (if negative)
  // - 1 char for '\0'
  char buffer[Log10(std::numeric_limits<T>::max()) + 3];
  ssize_t n = base::strings::SafeSPrintf(buffer, "%d", value);
  PA_RAW_DCHECK(n >= 0);
  PA_RAW_DCHECK(static_cast<size_t>(n) < sizeof(buffer));
  PutText(buffer, n);
}

CStringBuilder& CStringBuilder::operator<<(char ch) {
  PutText(&ch, 1);
  return *this;
}

CStringBuilder& CStringBuilder::operator<<(const char* text) {
  PutText(text);
  return *this;
}

CStringBuilder& CStringBuilder::operator<<(float value) {
  PutFloatingPoint(value, kNumDigits10);
  return *this;
}

CStringBuilder& CStringBuilder::operator<<(double value) {
  PutFloatingPoint(value, kNumDigits10);
  return *this;
}

CStringBuilder& CStringBuilder::operator<<(int value) {
  PutInteger(value);
  return *this;
}

CStringBuilder& CStringBuilder::operator<<(unsigned int value) {
  PutInteger(value);
  return *this;
}

CStringBuilder& CStringBuilder::operator<<(long value) {
  PutInteger(value);
  return *this;
}

CStringBuilder& CStringBuilder::operator<<(unsigned long value) {
  PutInteger(value);
  return *this;
}

CStringBuilder& CStringBuilder::operator<<(long long value) {
  PutInteger(value);
  return *this;
}

CStringBuilder& CStringBuilder::operator<<(unsigned long long value) {
  PutInteger(value);
  return *this;
}

CStringBuilder& CStringBuilder::operator<<(const void* value) {
  if (!value) {
    PutText("(nil)");
  } else {
    // We need an array of chars whose size is:
    // - 2 chars per 1 byte(00-FF), totally sizeof(const void*) * 2 chars,
    // - 2 chars for "0x",
    // - 1 char for '\0',
    char buffer[sizeof(const void*) * 2 + 2 + 1];
    ssize_t n = base::strings::SafeSPrintf(buffer, "%p", value);
    PA_RAW_DCHECK(n > 0);
    PA_RAW_DCHECK(static_cast<size_t>(n) < sizeof(buffer));
    PutText(buffer, n);
  }
  return *this;
}

CStringBuilder& CStringBuilder::operator<<(std::nullptr_t) {
  PutText("nullptr");
  return *this;
}

const char* CStringBuilder::c_str() {
  PA_RAW_DCHECK(buffer_ <= ptr_ && ptr_ < buffer_ + kBufferSize);
  *ptr_ = '\0';
  return buffer_;
}

void CStringBuilder::PutFloatingPoint(double value, unsigned num_digits10) {
  switch (std::fpclassify(value)) {
    case FP_INFINITE:
      PutText(value < 0 ? "-inf" : "inf");
      break;
    case FP_NAN:
      PutText("NaN");
      break;
    case FP_ZERO:
      PutText("0");
      break;
    case FP_SUBNORMAL:
      // Denormalized values are not supported.
      PutNormalFloatingPoint(value > 0 ? std::numeric_limits<double>::min()
                                       : -std::numeric_limits<double>::min(),
                             num_digits10);
      break;
    case FP_NORMAL:
    default:
      PutNormalFloatingPoint(value, num_digits10);
      break;
  }
}

void CStringBuilder::PutNormalFloatingPoint(double value,
                                            unsigned num_digits10) {
  if (value < 0) {
    PutText("-", 1);
    value = -value;
  }

  int exponent = floor(log10(value));
  double significand = value / pow(10, exponent);

  char buffer[64];
  ssize_t n = base::strings::SafeSPrintf(
      buffer, "%d", lrint(significand * GetDigits10(num_digits10)));
  PA_RAW_DCHECK(n > 0);
  PA_RAW_DCHECK(static_cast<size_t>(n) < sizeof(buffer));
  PutText(buffer, 1);
  if (n > 1) {
    PutText(".", 1);
    PutText(buffer + 1, n - 1);
  }
  if (exponent != 0) {
    n = base::strings::SafeSPrintf(buffer, "e%s%d", exponent > 0 ? "+" : "",
                                   exponent);
    PA_RAW_DCHECK(n > 0);
    PA_RAW_DCHECK(static_cast<size_t>(n) < sizeof(buffer));
    PutText(buffer, n);
  }
}

void CStringBuilder::PutText(const char* text) {
  PutText(text, strlen(text));
}

void CStringBuilder::PutText(const char* text, size_t length) {
  PA_RAW_DCHECK(buffer_ <= ptr_ && ptr_ < buffer_ + kBufferSize);
  while (ptr_ < buffer_ + kBufferSize - 1 && length > 0 && *text != '\0') {
    *ptr_++ = *text++;
    --length;
  }
}

}  // namespace partition_alloc::internal::base::strings
