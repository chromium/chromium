// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_logging.h"

#include "ash/constants/ash_features.h"

namespace ash {

ScopedLogMessage::ScopedLogMessage(const char* file,
                                   int line,
                                   logging::LogSeverity severity)
    : file_(file), line_(line), severity_(severity) {}

ScopedLogMessage::~ScopedLogMessage() {
  if (features::IsInputDeviceSettingsLoggingEnabled()) {
    // Create a log for the standard logging system.
    logging::LogMessage log_message(file_, line_, severity_);
    log_message.stream() << stream_.str();
  }
}

}  // namespace ash
