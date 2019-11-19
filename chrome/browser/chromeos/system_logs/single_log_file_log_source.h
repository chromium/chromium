// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_SINGLE_LOG_FILE_LOG_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_SINGLE_LOG_FILE_LOG_SOURCE_H_

#include <stddef.h>
#include <sys/types.h>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace base {
class Time;
}

namespace system_logs {

// Gathers log data from a single source, possibly incrementally.
class SingleLogFileLogSource : public SystemLogsSource {
 public:
  enum class SupportedSource {
    // For /var/log/messages.
    kMessages,

    // For /var/log/ui/ui.LATEST.
    kUiLatest,

    // For /var/log/atrus.log.
    kAtrusLog,

    // For /var/log/net.log.
    kNetLog,

    // For /var/log/eventlog.txt.
    kEventLog,

    // For /var/log/update_engine.log.
    kUpdateEngineLog,

    // For /var/log/power_manager/powerd.LATEST.
    kPowerdLatest,

    // For /var/log/power_manager/powerd.PREVIOUS.
    kPowerdPrevious,
  };

  explicit SingleLogFileLogSource(SupportedSource source);
  ~SingleLogFileLogSource() override;

  // During testing, use this to set a custom Chrome start time to override the
  // actual start time. Does not take ownership of |start_time|. Call this again
  // with |start_time|=nullptr when done with testing.
  static void SetChromeStartTimeForTesting(const base::Time* start_time);

  // system_logs::SystemLogsSource:
  void Fetch(SysLogsSourceCallback callback) override;

 private:
  friend class SingleLogFileLogSourceTest;

  // Returns the full path of the log file.
  base::FilePath GetLogFilePath() const;

  // Reads all available content from |file_| that has not already been read.
  // Stores results as a single entry in |result|, with |source_name()| as key
  // and the read log contents as value.
  //
  // Handles rotation of underlying log file by reading all remaining contents
  // of old file and then opening and reading from new file.
  //
  // |num_rotations_allowed| limits the number of rotations that can take place
  // before the function returns. This avoids this function never returning due
  // to indefinitely repeated log file rotation. If this number is exceeded
  // during a call, ReadFile() stops checking for log file rotation for the
  // remainder of its execution. Any further rotation could result in missed log
  // data.
  void ReadFile(size_t num_rotations_allowed, SystemLogsResponse* result);

  // The source type.
  const SupportedSource source_type_;

  // Path to system log file directory.
  base::FilePath log_file_dir_path_;

  // Keeps track of how much data has been read from |file_|.
  size_t num_bytes_read_;

  // Handle for reading the log file that is source of logging data.
  base::File file_;

  // File system inode value that was associated with |log_file_path_| when it
  // was originally opened for reading.
  ino_t file_inode_;

  base::WeakPtrFactory<SingleLogFileLogSource> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SingleLogFileLogSource);
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_SINGLE_LOG_FILE_LOG_SOURCE_H_
