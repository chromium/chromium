// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging/rust_logger/print_rust_log_ffi.h"

#include <stdint.h>

#include "base/check.h"
#include "base/logging.h"
#include "base/logging/log_severity.h"
#include "base/logging/rust_logger/print_rust_log.rs.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace logging::internal {

LogMessageRustWrapper::LogMessageRustWrapper(const char* file,
                                             int line,
                                             ::logging::LogSeverity severity)
    : log_message(file, line, severity) {}

void LogMessageRustWrapper::write_str(rust::Str str) {
  log_message.stream().write(str.data(),
                             static_cast<std::streamsize>(str.size()));
}

void print_rust_log(const RustFmtArguments& msg,
                    rust::Slice<const uint8_t> file,
                    int32_t line,
                    int32_t severity) {
  CHECK(!file.empty());
  const char* file_cstr = reinterpret_cast<const char*>(file.data());
  if (severity < 0) {
    int verbose_level = -severity;
    if (verbose_level > ENABLED_VLOG_LEVEL &&
        verbose_level > ::logging::GetVlogLevelHelper(file_cstr, file.size())) {
      return;
    }
  } else if (!::logging::ShouldCreateLogMessage(severity)) {
    return;
  }

  LogMessageRustWrapper wrapper(file_cstr, line, severity);
  msg.format(wrapper);
}

}  // namespace logging::internal
