// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_persister.h"

#include <sstream>

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

using WriteReportTrigger = ExtensionTelemetryPersister::WriteReportTrigger;

// If a file is older than `kMaxFileAge` it will be deleted instead
// of read.
constexpr base::TimeDelta kMaxFileAge = base::Days(3);
// The initial index for the `read_index_` and the `write_index_`.
constexpr int kInitialWriteIndex = 0;
constexpr int kInitialReadIndex = -1;
constexpr char kPersistedFileNamePrefix[] = "CRXTelemetry_";
// `kMaxCacheSize` is based off of a 12 hour reporting interval with a 15
// minute write interval. This value is used to size the UMA metric,
// there are no plans for the persister to have a cache this large.
constexpr int kMaxCacheSize = 48;

void RecordWriteResult(bool success, WriteReportTrigger trigger) {
  std::string metric = "SafeBrowsing.ExtensionPersister.WriteResult";
  std::string suffix;
  switch (trigger) {
    case WriteReportTrigger::kAtWriteInterval:
      suffix = ".AtWriteInterval";
      break;
    case WriteReportTrigger::kAtShutdown:
      suffix = ".AtShutdown";
      break;
  }
  base::UmaHistogramBoolean(metric, success);
  base::UmaHistogramBoolean(metric + suffix, success);
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
  // Add 1 to `kMaxCacheSize` to account for zero files in the cache.
  base::UmaHistogramExactLinear("SafeBrowsing.ExtensionPersister.CacheSize",
                                cache_size, kMaxCacheSize + 1);
}

void RecordAgedFileFound(bool found) {
  base::UmaHistogramBoolean("SafeBrowsing.ExtensionPersister.AgedFileFound",
                            found);
}
}  // namespace

ExtensionTelemetryPersister::~ExtensionTelemetryPersister() = default;

ExtensionTelemetryPersister::ExtensionTelemetryPersister(
    int max_num_files,
    base::FilePath profile_path)
    : dir_path_(profile_path), max_num_files_(max_num_files) {
  DCHECK(max_num_files <= kMaxCacheSize);
}

void ExtensionTelemetryPersister::PersisterInit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40225665): Remove old directory clean up code after
  // launch.
  base::FilePath old_dir;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &old_dir)) {
    old_dir = old_dir.AppendASCII("CRXTelemetry");
    if (base::DirectoryExists(old_dir)) {
      base::DeletePathRecursively(old_dir);
    }
  }
  write_index_ = kInitialWriteIndex;
  read_index_ = kInitialReadIndex;
  dir_path_ = dir_path_.AppendASCII("CRXTelemetry");
  if (!base::DirectoryExists(dir_path_))
    base::CreateDirectory(dir_path_);
  while (
      base::PathExists(dir_path_.AppendASCII((
          kPersistedFileNamePrefix + base::NumberToString(read_index_ + 1)))) &&
      (read_index_ < max_num_files_ - 1)) {
    read_index_++;
  }
  write_index_ = (read_index_ + 1) % max_num_files_;
  initialization_complete_ = true;
  RecordNumberOfFilesInCacheOnStartup(read_index_);
}

void ExtensionTelemetryPersister::WriteReport(const std::string write_string,
                                              WriteReportTrigger trigger) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (initialization_complete_) {
    if (!base::DirectoryExists(dir_path_)) {
      base::CreateDirectory(dir_path_);
      write_index_ = kInitialWriteIndex;
      read_index_ = kInitialReadIndex;
    }
    base::FilePath path = dir_path_.AppendASCII(
        (kPersistedFileNamePrefix + base::NumberToString(write_index_)));
    bool success = base::WriteFile(path, write_string);
    if (success) {
      write_index_++;
      if (write_index_ - 1 > read_index_)
        read_index_ = write_index_ - 1;
      if (write_index_ >= max_num_files_)
        write_index_ = 0;
    }
    RecordWriteResult(success, trigger);
  }
}

std::string ExtensionTelemetryPersister::ReadReport() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!initialization_complete_ || read_index_ <= kInitialReadIndex) {
    return std::string();
  }
  bool read_success = false;
  std::string persisted_report;
  base::File::Info info;
  base::FilePath path = dir_path_.AppendASCII(
      (kPersistedFileNamePrefix + base::NumberToString(read_index_)));
  // Check file to see if it's older than `kMaxFileAge`,
  // if so, delete it and look for another file.
  while (base::PathExists(path) && base::DirectoryExists(dir_path_)) {
    // After reading a file, `write_index` is updated to make sure
    // it is `read_index_` + 1. Ensuring that anytime space is created in
    // the cache by a read, it is the next spot to be written to.
    read_index_--;
    write_index_ = read_index_ + 1;
    base::GetFileInfo(path, &info);
    if (info.creation_time + kMaxFileAge > base::Time::Now()) {
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
        (kPersistedFileNamePrefix + base::NumberToString(read_index_)));
  }
  RecordReadResult(read_success);
  return persisted_report;
}

void ExtensionTelemetryPersister::ClearPersistedFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  write_index_ = kInitialWriteIndex;
  read_index_ = kInitialReadIndex;
  base::DeletePathRecursively(dir_path_);
}

int ExtensionTelemetryPersister::MaxFilesSupported() {
  return kMaxCacheSize;
}

bool ExtensionTelemetryPersister::DeleteFile(const base::FilePath path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return (base::PathExists(path) && base::DeleteFile(path));
}
}  // namespace safe_browsing
