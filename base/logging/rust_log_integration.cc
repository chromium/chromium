// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging/rust_log_integration.h"

#include "base/logging.h"

// TODO: https://crbug.com/372907698 - This can go away when we remove
// RustLogSeverity and use the base severity constants.
#ifdef ERROR
#undef ERROR
#endif

namespace logging {
namespace internal {

BASE_EXPORT void print_rust_log(const char* msg,
                                const char* file,
                                int line,
                                enum RustLogSeverity severity) {
  // Drop debug and trace logs when DCHECK_IS_ON() is false. Otherwise, log them
  // as info, since the C++ implementation lacks support for debug and trace
  // logs.
#if !DCHECK_IS_ON()
  if (severity == RustLogSeverity::DEBUG ||
      severity == RustLogSeverity::TRACE) {
    return;
  }
#endif

  logging::LogSeverity log_severity;
  switch (severity) {
    case RustLogSeverity::DEBUG:
      log_severity = logging::LOGGING_INFO;
      break;
    case RustLogSeverity::TRACE:
      log_severity = logging::LOGGING_INFO;
      break;
    case RustLogSeverity::INFO:
      log_severity = logging::LOGGING_INFO;
      break;
    case RustLogSeverity::WARNING:
      log_severity = logging::LOGGING_WARNING;
      break;
    case RustLogSeverity::ERROR:
      log_severity = logging::LOGGING_ERROR;
      break;
  }
  logging::LogMessage log_message(file, line, log_severity);
  log_message.stream() << msg;
}

}  // namespace internal
}  // namespace logging
