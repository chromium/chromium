// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging/rust_log_integration.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/logging/log_severity.h"
#include "base/logging/rust_logger.rs.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace logging::internal {

LogMessageRustWrapper::LogMessageRustWrapper(const char* file,
                                             int line,
                                             ::logging::LogSeverity severity)
    : log_message(file, line, severity) {}

void LogMessageRustWrapper::write_to_stream(rust::Str str) {
  log_message.stream().write(str.data(),
                             static_cast<std::streamsize>(str.size()));
}

void print_rust_log(const RustFmtArguments& msg,
                    const char* file,
                    int32_t line,
                    int32_t severity,
                    bool verbose) {
  // TODO(danakj): If `verbose` make the log equivalent to VLOG instead of LOG.
  LogMessageRustWrapper wrapper(file, line, severity);
  msg.format(wrapper);
}

}  // namespace logging::internal
