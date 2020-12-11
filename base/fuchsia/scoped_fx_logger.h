// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_SCOPED_FX_LOGGER_H_
#define BASE_FUCHSIA_SCOPED_FX_LOGGER_H_

#include <fuchsia/logger/cpp/fidl.h>
#include <lib/syslog/logger.h>

#include <memory>

#include "base/base_export.h"
#include "base/strings/string_piece_forward.h"

namespace base {

namespace internal {

struct FxLoggerDeleter {
  inline void operator()(fx_logger_t* ptr) const { fx_logger_destroy(ptr); }
};

}  // namespace internal

using ScopedFxLogger = std::unique_ptr<fx_logger_t, internal::FxLoggerDeleter>;

// Returns a new logger connected to the specified |log_sink| service.
// The logger is initially configured to log all severities of message.
// Returns null if creation of the new logger fails.
BASE_EXPORT ScopedFxLogger
CreateFxLoggerFromLogSink(::fuchsia::logger::LogSinkHandle log_sink);

// Similar to CreateFxLoggerFromLogSink(), but returns a logger which annotates
// messages with the specified |tag|, if non-empty.
BASE_EXPORT ScopedFxLogger
CreateFxLoggerFromLogSinkWithTag(::fuchsia::logger::LogSinkHandle log_sink,
                                 base::StringPiece tag);

}  // namespace base

#endif  // BASE_FUCHSIA_SCOPED_FX_LOGGER_H_
