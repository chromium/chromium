// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_SCOPED_LOGGING_SETTINGS_H_
#define BASE_TEST_SCOPED_LOGGING_SETTINGS_H_

#include "base/logging.h"

namespace logging {
// Saves the current logging settings and restores them when destroyed.
// This is used by logging tests to avoid affecting later tests that
// may run afterward, in the same process.
// Note that the log_file setting is not currently saved/restored.
class BASE_EXPORT ScopedLoggingSettings {
 public:
  ScopedLoggingSettings();
  ~ScopedLoggingSettings();

  ScopedLoggingSettings(const ScopedLoggingSettings&) = delete;
  ScopedLoggingSettings& operator=(const ScopedLoggingSettings&) = delete;

#if defined(OS_CHROMEOS)
  void SetLogFormat(LogFormat) const;
#endif

 private:
  bool enable_process_id_;
  bool enable_thread_id_;
  bool enable_timestamp_;
  bool enable_tickcount_;
  int min_log_level_;
  LogMessageHandlerFunction message_handler_;

#if defined(OS_CHROMEOS)
  LogFormat log_format_;
#endif  // defined(OS_CHROMEOS)
};
}  // namespace logging

#endif  // BASE_TEST_SCOPED_LOGGING_SETTINGS_H_
