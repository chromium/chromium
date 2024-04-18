// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/fuchsia_logging.h"

#include <zircon/status.h>

#include <iomanip>
#include <string_view>

#include "base/location.h"
#include "base/process/process.h"
#include "base/scoped_clear_last_error.h"
#include "base/strings/stringprintf.h"

namespace logging {

ZxLogMessage::ZxLogMessage(const char* file_path,
                           int line,
                           LogSeverity severity,
                           zx_status_t zx_status)
    : LogMessage(file_path, line, severity), zx_status_(zx_status) {}

ZxLogMessage::~ZxLogMessage() {
  AppendError();
}

void ZxLogMessage::AppendError() {
  // Don't let actions from this method affect the system error after returning.
  base::ScopedClearLastError scoped_clear_last_error;

  // zx_status_t error values are negative, so log the numeric version as
  // decimal rather than hex. This is also useful to match zircon/errors.h for
  // grepping.
  stream() << ": " << zx_status_get_string(zx_status_) << " (" << zx_status_
           << ")";
}

ZxLogMessageFatal::~ZxLogMessageFatal() {
  AppendError();
  Flush();
  base::ImmediateCrash();
}

}  // namespace logging

namespace base {

namespace internal {

std::string FidlConnectionErrorMessage(std::string_view protocol_name,
                                       std::string_view status_string) {
  return base::StringPrintf("Failed to connect to %s: %s", protocol_name.data(),
                            status_string.data());
}

std::string FidlMethodResultErrorMessage(std::string_view formatted_error,
                                         std::string_view method_name) {
  return base::StringPrintf("Error calling %s: %s", method_name.data(),
                            formatted_error.data());
}

}  // namespace internal

fit::function<void(zx_status_t)> LogFidlErrorAndExitProcess(
    const Location& from_here,
    std::string_view protocol_name) {
  return [from_here, protocol_name](zx_status_t status) {
    {
      logging::ZxLogMessage(from_here.file_name(), from_here.line_number(),
                            logging::LOGGING_ERROR, status)
              .stream()
          << protocol_name << " disconnected unexpectedly, exiting";
    }
    base::Process::TerminateCurrentProcessImmediately(1);
  };
}

std::string FidlMethodResultErrorMessage(
    const fit::result<fidl::OneWayError>& result,
    std::string_view method_name) {
  CHECK(result.is_error());
  return internal::FidlMethodResultErrorMessage(
      result.error_value().FormatDescription(), method_name);
}

fit::function<void(fidl::UnbindInfo)> FidlBindingClosureWarningLogger(
    std::string_view protocol_name) {
  return [protocol_name](fidl::UnbindInfo info) {
    ZX_LOG(WARNING, info.status()) << protocol_name << " unbound";
  };
}

}  // namespace base
