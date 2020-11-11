// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_SCOPED_FX_LOGGER_H_
#define BASE_FUCHSIA_SCOPED_FX_LOGGER_H_

#include <fuchsia/logger/cpp/fidl.h>
#include <lib/syslog/logger.h>

#include <memory>

#include "base/base_export.h"

namespace base {

namespace internal {

struct FxLoggerDeleter {
  inline void operator()(fx_logger_t* ptr) const { fx_logger_destroy(ptr); }
};

}  // namespace internal

using ScopedFxLogger = std::unique_ptr<fx_logger_t, internal::FxLoggerDeleter>;

// Creates a new logger connected to the specified |log_sink| service.
// The logger is configured to log all severities of message, and has no
// custom tags set.
// Returns null if creation of the new logger fails.
BASE_EXPORT ScopedFxLogger
CreateFxLoggerFromLogSink(fuchsia::logger::LogSinkHandle log_sink);

}  // namespace base

#endif  // BASE_FUCHSIA_SCOPED_FX_LOGGER_H_
