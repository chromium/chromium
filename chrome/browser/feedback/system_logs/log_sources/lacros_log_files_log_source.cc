// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/lacros_log_files_log_source.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "components/feedback/feedback_util.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

namespace {

// Maximum buffer size for user logs in bytes.
const int64_t kMaxLogSize = 1024 * 1024;
constexpr char kLogTruncated[] = "<earlier logs truncated>\n";
constexpr char kNotAvailable[] = "<not available>";

}  // namespace

LacrosLogFilesLogSource::LacrosLogFilesLogSource(
    const base::FilePath& log_base_path,
    const std::string& log_key_base)
    : SystemLogsSource("UserLoggedFiles"),
      log_base_path_(log_base_path),
      log_key_base_(log_key_base) {}

LacrosLogFilesLogSource::~LacrosLogFilesLogSource() = default;

void LacrosLogFilesLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  auto response = std::make_unique<SystemLogsResponse>();
  auto* response_ptr = response.get();
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LacrosLogFilesLogSource::FindFiles,
                     weak_ptr_factory_.GetWeakPtr(), log_base_path_,
                     log_key_base_, response_ptr),
      base::BindOnce(std::move(callback), std::move(response)));
}

void LacrosLogFilesLogSource::FindFiles(const base::FilePath& log_base_path,
                                        const std::string& log_key_base,
                                        SystemLogsResponse* response) {
  base::FilePath log_path = log_base_path.Append("lacros.log");
  ReadFile(log_path, log_key_base, response);

  base::FilePath previous_log_path =
      log_base_path.Append("lacros.log.PREVIOUS");

  ReadFile(previous_log_path, log_key_base + "_previous", response);
}

void LacrosLogFilesLogSource::ReadFile(const base::FilePath& log_file_path,
                                       const std::string& log_key,
                                       SystemLogsResponse* response) {
  std::string value;
  const bool read_success =
      feedback_util::ReadEndOfFile(log_file_path, kMaxLogSize, &value);

  if (read_success && value.length() == kMaxLogSize) {
    value.replace(0, strlen(kLogTruncated), kLogTruncated);
    LOG(WARNING) << "Large log file was likely truncated: " << log_file_path;
  }

  response->emplace(log_key, (read_success && !value.empty()) ? std::move(value)
                                                              : kNotAvailable);
}

}  // namespace system_logs
