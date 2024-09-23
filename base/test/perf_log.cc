// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/test/perf_log.h"

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/notreached.h"

namespace base {

static FILE* perf_log_file = nullptr;

bool InitPerfLog(const FilePath& log_file) {
  if (perf_log_file) {
    // trying to initialize twice
    NOTREACHED();
  }

  perf_log_file = OpenFile(log_file, "w");
  return perf_log_file != nullptr;
}

void FinalizePerfLog() {
  if (!perf_log_file) {
    // trying to cleanup without initializing
    NOTREACHED();
  }
  base::CloseFile(perf_log_file);
}

void LogPerfResult(const char* test_name, double value, const char* units) {
  CHECK(perf_log_file);

  fprintf(perf_log_file, "%s\t%g\t%s\n", test_name, value, units);
  printf("%s\t%g\t%s\n", test_name, value, units);
  fflush(stdout);
}

}  // namespace base
