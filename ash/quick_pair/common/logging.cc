// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/logging.h"

#include "ash/quick_pair/common/log_buffer.h"
#include "base/time/time.h"

namespace ash {
namespace quick_pair {

namespace {

bool g_logging_enabled = true;

}  // namespace

ScopedDisableLoggingForTesting::ScopedDisableLoggingForTesting() {
  g_logging_enabled = false;
}

ScopedDisableLoggingForTesting::~ScopedDisableLoggingForTesting() {
  g_logging_enabled = true;
}

ScopedLogMessage::ScopedLogMessage(std::string_view file,
                                   int line,
                                   logging::LogSeverity severity)
    : file_(file), line_(line), severity_(severity) {}

ScopedLogMessage::~ScopedLogMessage() {
  if (!g_logging_enabled)
    return;

  const std::string string_from_stream = stream_.str();
  LogBuffer::GetInstance()->AddLogMessage(LogBuffer::LogMessage(
      string_from_stream, base::Time::Now(), file_.data(), line_, severity_));

  // Don't emit VERBOSE-level logging to the standard logging system unless
  // verbose logging is enabled for the source file.
  if (severity_ <= logging::LOGGING_VERBOSE &&
      logging::GetVlogLevelHelper(file_.data(), file_.size()) <= 0) {
    return;
  }

  // The destructor of |log_message| also creates a log for the standard logging
  // system.
  logging::LogMessage log_message(file_.data(), line_, severity_);
  log_message.stream() << string_from_stream;
}

}  // namespace quick_pair
}  // namespace ash
