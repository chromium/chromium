// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_persister.h"

#include <sstream>

#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {
// The max number of files that will be stored on disk.
constexpr int kMaxNumFiles = 10;
// If a file is older than `kMaxFileAge` it will be deleted instead
// of read.
constexpr base::TimeDelta kMaxFileAge = base::Days(3);
// The initial index for the `read_index_` and the `write_index_`.
constexpr int kInitialWriteIndex = 0;
constexpr int kInitialReadIndex = -1;

void RecordWriteResult(bool success) {
  base::UmaHistogramBoolean("SafeBrowsing.ExtensionPersister.WriteResult",
                            success);
}

void RecordReadResult(bool success) {
  base::UmaHistogramBoolean("SafeBrowsing.ExtensionPersister.ReadResult",
                            success);
}

void RecordPersistedFileSize(size_t size) {
  base::UmaHistogramCounts1M(
      "SafeBrowsing.ExtensionPersister.PersistedFileSize", size);
}

void RecordNumberOfFilesInCacheOnStartup(int cache_size) {
  base::UmaHistogramExactLinear("SafeBrowsing.ExtensionPersister.CacheSize",
                                cache_size, kMaxNumFiles);
}

void RecordAgedFileFound(bool found) {
  base::UmaHistogramBoolean("SafeBrowsing.ExtensionPersister.AgedFileFound",
                            found);
}
}  // namespace

ExtensionTelemetryPersister::~ExtensionTelemetryPersister() = default;

ExtensionTelemetryPersister::ExtensionTelemetryPersister() {
  // Shutdown behavior is CONTINUE_ON_SHUTDOWN to ensure tasks
  // are run on the threads they were called on.
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
}

void ExtensionTelemetryPersister::PersisterInit() {
  base::ThreadPool::PostTask(
      FROM_HERE, base::MayBlock(),
      base::BindOnce(&ExtensionTelemetryPersister::InitHelper,
                     weak_factory_.GetWeakPtr()));
}

void ExtensionTelemetryPersister::WriteReport(const std::string write_string) {
  if (initialization_complete_ && !is_shut_down_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ExtensionTelemetryPersister::SaveFile,
                       weak_factory_.GetWeakPtr(), std::move(write_string)));
  }
}

void ExtensionTelemetryPersister::WriteReportDuringShutdown(
    const std::string write_string) {
  if (initialization_complete_ && !is_shut_down_) {
    is_shut_down_ = true;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ExtensionTelemetryPersister::SaveFileDuringShutdown,
                       std::move(write_string), dir_path_, write_index_));
  }
}

void ExtensionTelemetryPersister::ReadReport(
    base::OnceCallback<void(std::string, bool)> callback) {
  if (initialization_complete_ && read_index_ > kInitialReadIndex &&
      !is_shut_down_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ExtensionTelemetryPersister::LoadFile,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    std::move(callback).Run("", false);
  }
}

void ExtensionTelemetryPersister::ClearPersistedFiles() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ExtensionTelemetryPersister::DeleteAllFiles,
                                weak_factory_.GetWeakPtr()));
}

void ExtensionTelemetryPersister::InitHelper() {
  write_index_ = kInitialWriteIndex;
  read_index_ = kInitialReadIndex;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &dir_path_)) {
    dir_path_ = dir_path_.AppendASCII("CRXTelemetry");
    if (!base::DirectoryExists(dir_path_))
      base::CreateDirectory(dir_path_);
    while (base::PathExists(dir_path_.AppendASCII(
               ("CRXTelemetry_" + base::NumberToString(read_index_ + 1)))) &&
           (read_index_ < kMaxNumFiles - 1)) {
      read_index_++;
    }
    write_index_ = (read_index_ + 1) % kMaxNumFiles;
  }
  initialization_complete_ = true;
  RecordNumberOfFilesInCacheOnStartup(read_index_ + 1);
}

// Static
void ExtensionTelemetryPersister::SaveFileDuringShutdown(
    std::string write_string,
    base::FilePath dir_path,
    int write_index) {
  // If the persister directory does not exist the persister
  // will not write.
  if (!base::DirectoryExists(dir_path))
    return;
  base::FilePath path = dir_path.AppendASCII(
      ("CRXTelemetry_" + base::NumberToString(write_index)));
  bool success = base::WriteFile(path, write_string);
  RecordWriteResult(success);
}

void ExtensionTelemetryPersister::SaveFile(std::string write_string) {
  if (!base::DirectoryExists(dir_path_)) {
    base::CreateDirectory(dir_path_);
    write_index_ = kInitialWriteIndex;
    read_index_ = kInitialReadIndex;
  }
  base::FilePath path = dir_path_.AppendASCII(
      ("CRXTelemetry_" + base::NumberToString(write_index_)));
  bool success = base::WriteFile(path, write_string);
  if (success) {
    write_index_++;
    if (write_index_ - 1 > read_index_)
      read_index_ = write_index_ - 1;
    if (write_index_ >= kMaxNumFiles)
      write_index_ = 0;
  }
  RecordWriteResult(success);
}

void ExtensionTelemetryPersister::LoadFile(
    base::OnceCallback<void(std::string, bool)> callback) {
  bool read_success = false;
  std::string persisted_report = "";
  base::File::Info info;
  base::FilePath path = dir_path_.AppendASCII(
      ("CRXTelemetry_" + base::NumberToString(read_index_)));
  // Check file to see if it's older than `kMaxFileAge`,
  // if so, delete it and look for another file.
  while (base::PathExists(path) && base::DirectoryExists(dir_path_)) {
    // After reading a file, `write_index` is updated to make sure
    // it is `read_index_` + 1. Ensuring that anytime space is created in
    // the cache by a read, it is the next spot to be written to.
    read_index_--;
    write_index_ = read_index_ + 1;
    base::GetFileInfo(path, &info);
    if (info.creation_time + kMaxFileAge > base::Time::NowFromSystemTime()) {
      RecordAgedFileFound(false);
      if (base::ReadFileToString(path, &persisted_report))
        read_success = true;
      RecordPersistedFileSize(info.size);
      // Delete the file on disk regardless of age or read success.
      DeleteFile(path);
      break;
    }
    DeleteFile(path);
    RecordAgedFileFound(true);
    path = dir_path_.AppendASCII(
        ("CRXTelemetry_" + base::NumberToString(read_index_)));
  }
  RecordReadResult(read_success);
  std::move(callback).Run(std::move(persisted_report), std::move(read_success));
}

bool ExtensionTelemetryPersister::DeleteFile(const base::FilePath path) {
  return (base::PathExists(path) && base::DeleteFile(path));
}

void ExtensionTelemetryPersister::DeleteAllFiles() {
  write_index_ = kInitialWriteIndex;
  read_index_ = kInitialReadIndex;
  base::DeletePathRecursively(dir_path_);
}

}  // namespace safe_browsing
