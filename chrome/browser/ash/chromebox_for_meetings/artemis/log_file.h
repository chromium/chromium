// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_LOG_FILE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_LOG_FILE_H_

#include <fstream>
#include <string>

#include "base/files/file_path.h"

namespace ash::cfm {

// This is an log file abstraction that is used by a LogSource object.
// LogSources that represent a file that can be rotated will track one
// LogFile per rotated file.
class LogFile {
 public:
  explicit LogFile(const std::string& filepath);
  ~LogFile();
  LogFile(const LogFile&) = delete;
  LogFile& operator=(const LogFile&) = delete;

  const std::string& GetFilePath() const;
  bool OpenAtOffset(std::streampos offset);
  void CloseStream();
  std::streampos GetCurrentOffset();
  bool IsInFailState() const;
  bool IsAtEOF() const;
  bool Refresh();
  std::vector<std::string> RetrieveNextLogs(size_t count);

 private:
  const base::FilePath filepath_;
  std::ifstream file_stream_;
  std::streampos last_read_offset_;
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_LOG_FILE_H_
