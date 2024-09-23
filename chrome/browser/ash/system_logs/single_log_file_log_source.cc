// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/system_logs/single_log_file_log_source.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/process/process_info.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

namespace {

using SupportedSource = SingleLogFileLogSource::SupportedSource;

constexpr char kDefaultSystemLogDirPath[] = "/var/log";
constexpr char kLogTruncated[] = "<earlier logs truncated>\n<partial line>";
constexpr int kMaxNumAllowedLogRotationsDuringFileRead = 3;

// We set a per-read limit of 5 MiB to avoid running out of memory. Clients are
// responsible for further bundling and truncating.
//  * This cap is applied to the read buffer before dropping trailing incomplete
//    lines.
constexpr size_t kMaxReadSize = 5 * 1024 * 1024;

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
  NOTREACHED_IN_MIGRATION();
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
      max_read_size_(kMaxReadSize),
      file_cursor_position_(0),
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

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&SingleLogFileLogSource::ReadFile,
                     weak_ptr_factory_.GetWeakPtr(),
                     kMaxNumAllowedLogRotationsDuringFileRead, response_ptr),
      base::BindOnce(std::move(callback), std::move(response)));
}

void SingleLogFileLogSource::SetMaxReadSizeForTesting(
    const size_t max_read_size) {
  max_read_size_ = max_read_size;
}

base::FilePath SingleLogFileLogSource::GetLogFilePath() const {
  return log_file_dir_path_.Append(source_name());
}

void SingleLogFileLogSource::ReadFile(size_t num_rotations_allowed,
                                      SystemLogsResponse* response) {
  auto result_string = std::make_unique<std::string>();
  // No bytes have been skipped yet, because the read hasn't started.
  constexpr bool bytes_skipped = false;
  ContinueReadFile(std::move(result_string), bytes_skipped,
                   num_rotations_allowed, response);
}

void SingleLogFileLogSource::ContinueReadFile(
    std::unique_ptr<std::string> result_string,
    bool bytes_skipped,
    size_t num_rotations_allowed,
    SystemLogsResponse* response) {
  // Attempt to open the file if it was not previously opened.
  if (!file_.IsValid()) {
    file_.Initialize(GetLogFilePath(),
                     base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!file_.IsValid())
      return;

    file_cursor_position_ = 0;
    file_inode_ = GetInodeValue(GetLogFilePath());
  }

  // Check for file size reset.
  const size_t length = file_.GetLength();
  if (length < file_cursor_position_) {
    file_cursor_position_ = 0;
    file_.Seek(base::File::FROM_BEGIN, 0);
  }

  // Check for large read and skip forward to avoid out-of-memory conditions.
  if (length - file_cursor_position_ > max_read_size_) {
    bytes_skipped = true;
    file_.Seek(base::File::FROM_END, -max_read_size_);
    // Update |file_cursor_position_| to support the file size reset check.
    file_cursor_position_ = length - max_read_size_;
  }

  // The calculated amount of data to read, after adjusting for
  // |max_read_size_|.
  const size_t size_to_read = length - file_cursor_position_;

  // Trim down the previously read data before starting a new read.
  const size_t available_previous_read_size = max_read_size_ - size_to_read;
  if (available_previous_read_size < result_string->size()) {
    result_string->erase(0,
                         result_string->size() - available_previous_read_size);
  }

  // Read from file until end.
  std::string new_result_string;
  new_result_string.resize(size_to_read);
  size_t size_read =
      file_.ReadAtCurrentPos(&new_result_string[0], size_to_read);
  new_result_string.resize(size_read);

  const bool file_was_rotated = file_inode_ != GetInodeValue(GetLogFilePath());
  const bool should_handle_file_rotation =
      file_was_rotated && num_rotations_allowed > 0;

  // The reader may only read complete lines. The exception is when there is a
  // rotation, in which case all the remaining contents of the old log file
  // should be read before moving on to read the new log file.
  if ((new_result_string.empty() || new_result_string.back() != '\n') &&
      !should_handle_file_rotation) {
    // If an incomplete line was read, return only the part that includes
    // whole lines.
    size_t last_newline_pos = new_result_string.find_last_of('\n');

    // The part of the string that will be returned includes the newline
    // itself.
    size_t adjusted_size_read =
        last_newline_pos == std::string::npos ? 0 : last_newline_pos + 1;
    file_.Seek(base::File::FROM_CURRENT, -size_read + adjusted_size_read);
    new_result_string.resize(adjusted_size_read);

    // Update |size_read| to reflect that the read was only up to the last
    // newline.
    size_read = adjusted_size_read;
  }

  file_cursor_position_ += size_read;

  result_string->append(new_result_string);

  // If the file was rotated, close the file handle and call this function
  // again, to read from the new file.
  if (should_handle_file_rotation) {
    file_.Close();
    file_cursor_position_ = 0;
    file_inode_ = 0;
    ContinueReadFile(std::move(result_string), bytes_skipped,
                     num_rotations_allowed - 1, response);
  } else {
    // Only write the log truncated sentinel value once we have something to
    // go after it, and any accumulation and rollover has been handled.
    if (bytes_skipped && result_string->size() > 0) {
      AppendToSystemLogsResponse(response, source_name(), kLogTruncated);
    }
    // Pass it back to the callback.
    AppendToSystemLogsResponse(response, source_name(), *result_string.get());
  }
}

}  // namespace system_logs
