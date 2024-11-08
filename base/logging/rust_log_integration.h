// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LOGGING_RUST_LOG_INTEGRATION_H_
#define BASE_LOGGING_RUST_LOG_INTEGRATION_H_

#include "base/base_export.h"

namespace logging {
namespace internal {

// TODO(thiruak1024@gmail.com): We need to use the existing logging severity
// type and remove the 'RustLogSeverity' enum. https://crbug.com/372907698
enum RustLogSeverity { INFO, WARNING, ERROR, DEBUG, TRACE };
void BASE_EXPORT print_rust_log(const char* msg,
                                const char* file,
                                int line,
                                enum RustLogSeverity severity);
}  // namespace internal
}  // namespace logging

#endif  // BASE_LOGGING_RUST_LOG_INTEGRATION_H_
