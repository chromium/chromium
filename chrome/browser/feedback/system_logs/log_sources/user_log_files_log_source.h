// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_USER_LOG_FILES_LOG_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_USER_LOG_FILES_LOG_SOURCE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// This class gathers log data from user log files.
class UserLogFilesLogSource : public SystemLogsSource {
 public:
  UserLogFilesLogSource(const base::FilePath& log_file_path,
                        const std::string& log_key);
  UserLogFilesLogSource(const UserLogFilesLogSource&) = delete;
  UserLogFilesLogSource& operator=(const UserLogFilesLogSource&) = delete;
  ~UserLogFilesLogSource() override;

  // SystemLogsSource override:
  void Fetch(SysLogsSourceCallback callback) override;

 private:
  void ReadFile(const base::FilePath& log_file_path,
                const std::string& log_key,
                SystemLogsResponse* response);

  const base::FilePath log_file_path_;
  const std::string log_key_;

  base::WeakPtrFactory<UserLogFilesLogSource> weak_ptr_factory_{this};
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_USER_LOG_FILES_LOG_SOURCE_H_
