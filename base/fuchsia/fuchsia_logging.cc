// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/fuchsia_logging.h"

#include <zircon/status.h>

#include <iomanip>

#include "base/location.h"
#include "base/process/process.h"
#include "base/strings/string_piece.h"

namespace logging {

ZxLogMessage::ZxLogMessage(const char* file_path,
                           int line,
                           LogSeverity severity,
                           zx_status_t zx_err)
    : LogMessage(file_path, line, severity), zx_err_(zx_err) {}

ZxLogMessage::~ZxLogMessage() {
  // zx_status_t error values are negative, so log the numeric version as
  // decimal rather than hex. This is also useful to match zircon/errors.h for
  // grepping.
  stream() << ": " << zx_status_get_string(zx_err_) << " (" << zx_err_ << ")";
}

}  // namespace logging

namespace base {

fit::function<void(zx_status_t)> LogFidlErrorAndExitProcess(
    const Location& from_here,
    StringPiece protocol_name) {
  return [from_here, protocol_name](zx_status_t status) {
    {
      logging::ZxLogMessage(from_here.file_name(), from_here.line_number(),
                            logging::LOGGING_ERROR, status)
              .stream()
          << protocol_name << " disconnected unexpectedly";
    }
    base::Process::TerminateCurrentProcessImmediately(1);
  };
}

}  // namespace base
