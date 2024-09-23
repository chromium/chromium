// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_op.h"

#include <string.h>

#include <algorithm>
#include <cstdio>
#include <sstream>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/cstring_view.h"

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

char* CheckOpValueStr(std::string_view v) {
  // Ideally this would be `strndup`, but `strndup` is not portable. We have to
  // use malloc() instead of HeapArray in order to match strdup() in the other
  // overloads. The API contract is that the caller uses free() to release the
  // pointer returned here.
  char* ret = static_cast<char*>(malloc(v.size() + 1u));
  auto [val, nul] =
      // SAFETY: We allocated `ret` as `v.size() + 1` bytes above.
      UNSAFE_BUFFERS(base::span<char>(ret, v.size() + 1u)).split_at(v.size());
  val.copy_from(v);
  nul.copy_from({{'\0'}});
  return ret;
}

char* CheckOpValueStr(base::cstring_view v) {
  return strdup(v.c_str());
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

char* CreateCheckOpLogMessageString(const char* expr_str,
                                    char* v1_str,
                                    char* v2_str) {
  std::stringstream ss;
  ss << "Check failed: " << expr_str << " (" << v1_str << " vs. " << v2_str
     << ")";
  free(v1_str);
  free(v2_str);
  return strdup(ss.str().c_str());
}

}  // namespace logging
