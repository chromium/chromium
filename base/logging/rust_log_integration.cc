// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging/rust_log_integration.h"

#include "base/logging.h"
#include "base/logging/log_severity.h"

namespace logging {
namespace internal {

BASE_EXPORT void print_rust_log(const char* msg,
                                const char* file,
                                int line,
                                LogSeverity severity,
                                bool verbose) {
  // TODO(danakj): If `verbose` make the log equivalent to VLOG instead of LOG.
  logging::LogMessage log_message(file, line, severity);
  log_message.stream() << msg;
}

}  // namespace internal
}  // namespace logging
