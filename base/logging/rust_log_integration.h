// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LOGGING_RUST_LOG_INTEGRATION_H_
#define BASE_LOGGING_RUST_LOG_INTEGRATION_H_

#include "base/base_export.h"
#include "base/logging/log_severity.h"

namespace logging {
namespace internal {

// Receives a log line from Rust and forwards it to base logging, because
// logging::LogMessage is not accessible from Rust yet with our current interop
// tools.
//
// TODO(danakj): Should this helper function be replaced with C-like apis next
// to logging::LogMessage that Rust uses more directly?
void BASE_EXPORT print_rust_log(const char* msg,
                                const char* file,
                                int line,
                                LogSeverity severity,
                                bool verbose);

}  // namespace internal
}  // namespace logging

#endif  // BASE_LOGGING_RUST_LOG_INTEGRATION_H_
