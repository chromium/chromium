// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_deduplication_service/app_deduplication_cache.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace {
// File name for the file where deduplication data is stored.
constexpr char kAppDeduplicationDataFileName[] = "deduplication_data.pb";

// Maximum size of App Deduplication Response is 1MB, current size of file
// at initial launch (v1 of deduplication endpoint) is ~6KB.
constexpr int kMaxRequiredDiskSpaceBytes = 1024 * 1024;

// Writes data to given `path` on disk. If the write operation fails, the
// existing data on disk will be unaffected. Returns true if data is
// written to disk successfully and false otherwise.
bool WriteDeduplicateDataToDisk(const base::FilePath& path,
                                const apps::proto::DeduplicateData& data) {
  if (!base::PathExists(path)) {
    if (!base::CreateDirectory(path.DirName())) {
      LOG(ERROR) << "Directory for deduplication data cannot be created";
    }
  }

  if (base::SysInfo::AmountOfFreeDiskSpace(path.DirName()) <
      kMaxRequiredDiskSpaceBytes) {
    LOG(ERROR) << "Not enough disk space left.";
    return false;
  }

  // Create temporary file.
  base::FilePath temp_file;
  if (!base::CreateTemporaryFileInDir(path.DirName(), &temp_file)) {
    LOG(ERROR) << "Failed to create a temporary file.";
    return false;
  }

  // Write to temporary file.
  std::string serialized_data = data.SerializeAsString();
  if (!base::WriteFile(temp_file, serialized_data)) {
    LOG(ERROR) << "Failed to write to temporary file.";
    base::DeleteFile(temp_file);
    return false;
  }

  // Replace the current file with the temporary file.
  if (!base::ReplaceFile(temp_file, path, /*error=*/nullptr)) {
    LOG(ERROR) << "Failed to replace the temporary file.";
    base::DeleteFile(temp_file);
    return false;
  }

  return true;
}

// Reads and returns deduplicate data from `path`.
absl::optional<apps::proto::DeduplicateData> ReadDeduplicateDataFromDisk(
    const base::FilePath path) {
  std::string deduplicate_data_string;
  if (!base::PathExists(path)) {
    LOG(ERROR) << "Path to data does not exist";
    return absl::nullopt;
  }

  if (!base::ReadFileToString(path, &deduplicate_data_string)) {
    LOG(ERROR) << "Reading deduplicate data file from disk failed.";
    return absl::nullopt;
  }

  apps::proto::DeduplicateData deduplicate_data;
  if (!deduplicate_data.ParseFromString(deduplicate_data_string)) {
    LOG(ERROR) << "Parsing proto to string failed.";
    return absl::nullopt;
  }

  return deduplicate_data;
}

}  // namespace

namespace apps::deduplication {

AppDeduplicationCache::AppDeduplicationCache(base::FilePath& path)
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {
  file_path_ = path.AppendASCII(kAppDeduplicationDataFileName);
}

AppDeduplicationCache::~AppDeduplicationCache() = default;

void AppDeduplicationCache::WriteDeduplicationCache(
    proto::DeduplicateData& data,
    base::OnceCallback<void(bool)> callback) {
  // Since `WriteDeduplicateDataToDisk` is a blocking function, the task is
  // being posted to another thread.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& path, const proto::DeduplicateData& data) {
            return WriteDeduplicateDataToDisk(path, data);
          },
          file_path_, data),
      std::move(callback));
}

void AppDeduplicationCache::ReadDeduplicationCache(
    GetDeduplicateDataCallback callback) {
  // Since `ReadDeduplicateDataFromDisk` is a blocking function, the task is
  // being posted to another thread.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& path) {
            return ReadDeduplicateDataFromDisk(path);
          },
          file_path_),
      std::move(callback));
}

}  // namespace apps::deduplication
