// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_SCOPED_LOGGING_SETTINGS_H_
#define BASE_TEST_SCOPED_LOGGING_SETTINGS_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "build/chromeos_buildflags.h"

namespace logging {
// Saves the current logging settings and restores them when destroyed.
// This is used by logging tests to avoid affecting later tests that
// may run afterward, in the same process.
// Note that this helper cannot be used when an un-named log-file is configured
// via |LoggingSettings::log_file|.
class BASE_EXPORT ScopedLoggingSettings {
 public:
  ScopedLoggingSettings();
  ~ScopedLoggingSettings();

  ScopedLoggingSettings(const ScopedLoggingSettings&) = delete;
  ScopedLoggingSettings& operator=(const ScopedLoggingSettings&) = delete;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetLogFormat(LogFormat) const;
#endif

 private:
  // Please keep the following fields in the same order as the corresponding
  // globals in //base/logging.cc

  const int min_log_level_;
  const uint32_t logging_destination_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const LogFormat log_format_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  std::unique_ptr<base::FilePath::StringType> log_file_name_;

  const bool enable_process_id_;
  const bool enable_thread_id_;
  const bool enable_timestamp_;
  const bool enable_tickcount_;
  const char* const log_prefix_;

  const LogMessageHandlerFunction message_handler_;
};
}  // namespace logging

#endif  // BASE_TEST_SCOPED_LOGGING_SETTINGS_H_
