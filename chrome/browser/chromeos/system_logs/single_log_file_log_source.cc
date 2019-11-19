// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/single_log_file_log_source.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/process_info.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

namespace {

using SupportedSource = SingleLogFileLogSource::SupportedSource;

constexpr char kDefaultSystemLogDirPath[] = "/var/log";
constexpr int kMaxNumAllowedLogRotationsDuringFileRead = 3;

// A custom timestamp for when the current Chrome session started. Used during
// testing to override the actual time.
const base::Time* g_chrome_start_time_for_test = nullptr;

// Converts a logs source type to the corresponding file path, relative to the
// base system log directory path. In the future, if non-file source types are
// added, this function should return an empty file path.
base::FilePath::StringType GetLogFileSourceRelativeFilePathValue(
    SingleLogFileLogSource::SupportedSource source) {
  switch (source) {
    case SupportedSource::kMessages:
      return "messages";
    case SupportedSource::kUiLatest:
      return "ui/ui.LATEST";
    case SupportedSource::kAtrusLog:
      return "atrus.log";
    case SupportedSource::kNetLog:
      return "net.log";
    case SupportedSource::kEventLog:
      return "eventlog.txt";
    case SupportedSource::kUpdateEngineLog:
      return "update_engine.log";
    case SupportedSource::kPowerdLatest:
      return "power_manager/powerd.LATEST";
    case SupportedSource::kPowerdPrevious:
      return "power_manager/powerd.PREVIOUS";
  }
  NOTREACHED();
  return base::FilePath::StringType();
}

// Returns the inode value of file at |path|, or 0 if it doesn't exist or is
// otherwise unable to be accessed for file system info.
ino_t GetInodeValue(const base::FilePath& path) {
  struct stat file_stats;
  if (stat(path.value().c_str(), &file_stats) != 0)
    return 0;
  return file_stats.st_ino;
}

// Attempts to store a string |value| in |*response| under |key|. If there is
// already a string in |*response| under |key|, appends |value| to the existing
// string value.
void AppendToSystemLogsResponse(SystemLogsResponse* response,
                                const std::string& key,
                                const std::string& value) {
  auto iter = response->find(key);
  if (iter == response->end())
    response->emplace(key, value);
  else
    iter->second += value;
}

}  // namespace

SingleLogFileLogSource::SingleLogFileLogSource(SupportedSource source_type)
    : SystemLogsSource(GetLogFileSourceRelativeFilePathValue(source_type)),
      source_type_(source_type),
      log_file_dir_path_(kDefaultSystemLogDirPath),
      num_bytes_read_(0),
      file_inode_(0) {}

SingleLogFileLogSource::~SingleLogFileLogSource() {}

// static
void SingleLogFileLogSource::SetChromeStartTimeForTesting(
    const base::Time* start_time) {
  g_chrome_start_time_for_test = start_time;
}

void SingleLogFileLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  auto response = std::make_unique<SystemLogsResponse>();
  auto* response_ptr = response.get();
  base::PostTaskAndReply(
      FROM_HERE,
      base::TaskTraits({base::ThreadPool(), base::MayBlock(),
                        base::TaskPriority::BEST_EFFORT}),
      base::BindOnce(&SingleLogFileLogSource::ReadFile,
                     weak_ptr_factory_.GetWeakPtr(),
                     kMaxNumAllowedLogRotationsDuringFileRead, response_ptr),
      base::BindOnce(std::move(callback), std::move(response)));
}

base::FilePath SingleLogFileLogSource::GetLogFilePath() const {
  return log_file_dir_path_.Append(source_name());
}

void SingleLogFileLogSource::ReadFile(size_t num_rotations_allowed,
                                      SystemLogsResponse* result) {
  // Attempt to open the file if it was not previously opened.
  if (!file_.IsValid()) {
    file_.Initialize(GetLogFilePath(),
                     base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!file_.IsValid())
      return;

    num_bytes_read_ = 0;
    file_inode_ = GetInodeValue(GetLogFilePath());
  }

  // Check for file size reset.
  const size_t length = file_.GetLength();
  if (length < num_bytes_read_) {
    num_bytes_read_ = 0;
    file_.Seek(base::File::FROM_BEGIN, 0);
  }

  // Read from file until end.
  const size_t size_to_read = length - num_bytes_read_;
  std::string result_string;
  result_string.resize(size_to_read);
  size_t size_read = file_.ReadAtCurrentPos(&result_string[0], size_to_read);
  result_string.resize(size_read);

  const bool file_was_rotated = file_inode_ != GetInodeValue(GetLogFilePath());
  const bool should_handle_file_rotation =
      file_was_rotated && num_rotations_allowed > 0;

  // The reader may only read complete lines. The exception is when there is a
  // rotation, in which case all the remaining contents of the old log file
  // should be read before moving on to read the new log file.
  if ((result_string.empty() || result_string.back() != '\n') &&
      !should_handle_file_rotation) {
    // If an incomplete line was read, return only the part that includes whole
    // lines.
    size_t last_newline_pos = result_string.find_last_of('\n');
    if (last_newline_pos == std::string::npos) {
      file_.Seek(base::File::FROM_CURRENT, -size_read);
      AppendToSystemLogsResponse(result, source_name(), "");
      return;
    }
    // The part of the string that will be returned includes the newline itself.
    size_t adjusted_size_read = last_newline_pos + 1;
    file_.Seek(base::File::FROM_CURRENT, -size_read + adjusted_size_read);
    result_string.resize(adjusted_size_read);

    // Update |size_read| to reflect that the read was only up to the last
    // newline.
    size_read = adjusted_size_read;
  }

  num_bytes_read_ += size_read;

  // Pass it back to the callback.
  AppendToSystemLogsResponse(result, source_name(), result_string);

  // If the file was rotated, close the file handle and call this function
  // again, to read from the new file.
  if (should_handle_file_rotation) {
    file_.Close();
    num_bytes_read_ = 0;
    file_inode_ = 0;
    ReadFile(num_rotations_allowed - 1, result);
  }
}

}  // namespace system_logs
