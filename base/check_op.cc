// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_op.h"

#include <string.h>

#include <cstdio>
#include <sstream>

#include "base/logging.h"

namespace logging {

char* CheckOpValueStr(int v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%d", v);
  return strdup(buf);
}

char* CheckOpValueStr(unsigned v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%u", v);
  return strdup(buf);
}

char* CheckOpValueStr(long v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%ld", v);
  return strdup(buf);
}

char* CheckOpValueStr(unsigned long v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%lu", v);
  return strdup(buf);
}

char* CheckOpValueStr(long long v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%lld", v);
  return strdup(buf);
}

char* CheckOpValueStr(unsigned long long v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%llu", v);
  return strdup(buf);
}

char* CheckOpValueStr(const void* v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%p", v);
  return strdup(buf);
}

char* CheckOpValueStr(std::nullptr_t v) {
  return strdup("nullptr");
}

char* CheckOpValueStr(const std::string& v) {
  return strdup(v.c_str());
}

char* CheckOpValueStr(float f) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%.6f", f);
  return strdup(buf);
}

char* CheckOpValueStr(double v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%.6lf", v);
  return strdup(buf);
}

char* StreamValToStr(const void* v,
                     void (*stream_func)(std::ostream&, const void*)) {
  std::stringstream ss;
  stream_func(ss, v);
  return strdup(ss.str().c_str());
}

CheckOpResult::CheckOpResult(const char* expr_str, char* v1_str, char* v2_str) {
  std::ostringstream ss;
  ss << expr_str << " (" << v1_str << " vs. " << v2_str << ")";
  message_ = strdup(ss.str().c_str());
  free(v1_str);
  free(v2_str);
}

#if !CHECK_WILL_STREAM()

void CheckOpFailureStr(char* v1_str, char* v2_str) {
  LOG(FATAL) << "Check failed (" << v1_str << " vs. " << v2_str << ")";
  __builtin_unreachable();
}

template <>
[[noreturn]] BASE_EXPORT void CheckOpFailure<int, int>(int v1, int v2) {
  CheckOpFailureStr(CheckOpValueStr(v1), CheckOpValueStr(v2));
}

template <>
[[noreturn]] BASE_EXPORT void CheckOpFailure<unsigned, unsigned>(unsigned v1,
                                                                 unsigned v2) {
  CheckOpFailureStr(CheckOpValueStr(v1), CheckOpValueStr(v2));
}

template <>
[[noreturn]] BASE_EXPORT void CheckOpFailure<long, long>(long v1, long v2) {
  CheckOpFailureStr(CheckOpValueStr(v1), CheckOpValueStr(v2));
}

template <>
[[noreturn]] BASE_EXPORT void CheckOpFailure<unsigned long, unsigned long>(
    unsigned long v1,
    unsigned long v2) {
  CheckOpFailureStr(CheckOpValueStr(v1), CheckOpValueStr(v2));
}

template <>
[[noreturn]] BASE_EXPORT void CheckOpFailure<long long, long long>(
    long long v1,
    long long v2) {
  CheckOpFailureStr(CheckOpValueStr(v1), CheckOpValueStr(v2));
}

template <>
[[noreturn]] BASE_EXPORT void
CheckOpFailure<unsigned long long, unsigned long long>(unsigned long long v1,
                                                       unsigned long long v2) {
  CheckOpFailureStr(CheckOpValueStr(v1), CheckOpValueStr(v2));
}

template <>
[[noreturn]] BASE_EXPORT void CheckOpFailure<float, float>(float v1, float v2) {
  CheckOpFailureStr(CheckOpValueStr(v1), CheckOpValueStr(v2));
}

template <>
[[noreturn]] BASE_EXPORT void CheckOpFailure<double, double>(double v1,
                                                             double v2) {
  CheckOpFailureStr(CheckOpValueStr(v1), CheckOpValueStr(v2));
}

#endif  // !CHECK_WILL_STREAM()

}  // namespace logging
