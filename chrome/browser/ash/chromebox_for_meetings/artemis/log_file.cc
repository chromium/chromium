// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/artemis/log_file.h"

#include <unistd.h>

#include <fstream>

#include "base/logging.h"
#include "base/threading/scoped_blocking_call.h"

namespace ash::cfm {

LogFile::LogFile(const std::string& filepath) : filepath_(filepath) {}

LogFile::~LogFile() {
  CloseStream();
}

const std::string& LogFile::GetFilePath() const {
  return filepath_.value();
}

bool LogFile::OpenAtOffset(std::streampos offset) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (access(GetFilePath().c_str(), F_OK) != 0) {
    LOG(ERROR) << "File " << filepath_ << " doesn't exist";
    return false;
  }

  if (access(GetFilePath().c_str(), R_OK) != 0) {
    LOG(ERROR) << "Not enough permissions on file " << filepath_;
    return false;
  }

  file_stream_ = std::ifstream(GetFilePath(), std::ios::in);
  file_stream_.seekg(offset);

  if (IsInFailState()) {
    LOG(ERROR) << "Unable to seek to offset " << offset << " in file "
               << filepath_;
    return false;
  }

  is_open_ = true;
  return true;
}

void LogFile::CloseStream() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  file_stream_.close();
  is_open_ = false;
}

bool LogFile::IsInFailState() const {
  return file_stream_.bad();
}

bool LogFile::IsAtEOF() const {
  return file_stream_.eof() && !IsInFailState();
}

bool LogFile::IsOpen() const {
  return is_open_;
}

std::streampos LogFile::GetCurrentOffset() {
  std::streampos offset;

  // tellg() will report -1 if the last read resulted in an EOF,
  // but we want the true offset value. Use the last-known read
  // offset as it will be the offset right before we hit the EOF.
  if (IsAtEOF()) {
    return last_read_offset_;
  }

  return file_stream_.tellg();
}

bool LogFile::Refresh() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::streampos curr_pos = GetCurrentOffset();
  CloseStream();
  return OpenAtOffset(curr_pos);
}

std::vector<std::string> LogFile::RetrieveNextLogs(size_t line_count,
                                                   size_t max_byte_limit) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::vector<std::string> logs;
  size_t num_read_lines = 0;
  size_t num_read_bytes = 0;

  std::string line;
  while (!IsAtEOF() && !IsInFailState() && num_read_lines < line_count &&
         num_read_bytes < max_byte_limit && std::getline(file_stream_, line)) {
    num_read_bytes += line.size();
    num_read_lines++;

    logs.push_back(std::move(line));
    last_read_offset_ = file_stream_.tellg();
  }

  if (IsInFailState()) {
    LOG(ERROR) << "Error reading file " << filepath_ << " after "
               << num_read_lines << " lines";
  } else if (num_read_lines < line_count && num_read_bytes > max_byte_limit &&
             !IsAtEOF()) {
    LOG(WARNING) << "Requested " << line_count << " lines for " << GetFilePath()
                 << ", but only read " << num_read_lines
                 << " due to byte cap. Limit exceeded by "
                 << num_read_bytes - max_byte_limit << " bytes.";

    // Drop logs until we're within our limit. This is a highly unlikely
    // scenario, so this shouldn't impact our data analysis too much.
    size_t orig_size = logs.size();
    while (num_read_bytes > max_byte_limit && !logs.empty()) {
      num_read_bytes -= logs.back().size();
      logs.pop_back();
    }

    LOG(WARNING) << "Dropped " << orig_size - logs.size() << " logs.";
  } else {
    VLOG(3) << "Read " << num_read_bytes << " bytes from " << GetFilePath();
  }

  return logs;
}

}  // namespace ash::cfm
