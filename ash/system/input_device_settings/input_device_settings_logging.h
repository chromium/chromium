// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_LOGGING_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_LOGGING_H_

#include <sstream>

#include "ash/ash_export.h"
#include "base/logging.h"

namespace ash {

// Use the IDS_LOG() macro for all logging related to Input Device.
#define IDS_LOG(severity) \
  ScopedLogMessage(__FILE__, __LINE__, logging::LOG_##severity).stream()

// An intermediate object used by the IDS_LOG macro, wrapping a
// logging::LogMessage instance. When this object is destroyed, the message will
// be logged with the standard logging system.
class ASH_EXPORT ScopedLogMessage {
 public:
  ScopedLogMessage(const char* file, int line, logging::LogSeverity severity);
  ScopedLogMessage(const ScopedLogMessage&) = delete;
  ScopedLogMessage& operator=(const ScopedLogMessage&) = delete;
  ~ScopedLogMessage();

  std::ostream& stream() { return stream_; }

 private:
  const char* file_;
  int line_;
  logging::LogSeverity severity_;
  std::ostringstream stream_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_LOGGING_H_
