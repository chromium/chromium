// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_LACROS_LOG_FILES_LOG_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_LACROS_LOG_FILES_LOG_SOURCE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// This class gathers log data from the Lacros browser, when running as Ash on a
// Lacros-enabled install of ChromeOS. It will attach the current and previous
// Lacros log files, if they exist.
class LacrosLogFilesLogSource : public SystemLogsSource {
 public:
  LacrosLogFilesLogSource(const base::FilePath& log_base_path,
                          const std::string& log_key_base);
  LacrosLogFilesLogSource(const LacrosLogFilesLogSource&) = delete;
  LacrosLogFilesLogSource& operator=(const LacrosLogFilesLogSource&) = delete;
  ~LacrosLogFilesLogSource() override;

  // SystemLogsSource override:
  void Fetch(SysLogsSourceCallback callback) override;

 private:
  // This method must run on a blocking sequence. It attempts to attach both the
  // current and previous log files to |response|.
  void FindFiles(const base::FilePath& log_base_path,
                 const std::string& log_key_base,
                 SystemLogsResponse* response);
  // Reads the last 1 MB of content of the file at |log_file_path| and adds it
  // to |response| under the given |log_key|.
  void ReadFile(const base::FilePath& log_file_path,
                const std::string& log_key,
                SystemLogsResponse* response);

  // This is the base path for all Lacros logs. This class will search for a
  // 'lacros.log' and 'lacros.log.PREVIOUS' file in this directory.
  const base::FilePath log_base_path_;
  // This is the key base for Lacros logs in Ash feedback reports. This class
  // attaches the key base to the newest log, and appends '_previous' to the key
  // base for the second newest log.
  const std::string log_key_base_;

  base::WeakPtrFactory<LacrosLogFilesLogSource> weak_ptr_factory_{this};
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_LACROS_LOG_FILES_LOG_SOURCE_H_
