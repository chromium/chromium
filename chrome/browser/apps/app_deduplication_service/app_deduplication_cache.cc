// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_deduplication_service/app_deduplication_cache.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/system/sys_info.h"

namespace {
// File name for the file where deduplication data is stored.
constexpr char kAppDeduplicationDataFileName[] = "deduplication_data.pb";

// Maximum size of App Deduplication Response is 1MB, current size of file
// at initial launch (v1 of deduplication endpoint) is ~6KB.
constexpr int kMaxRequiredDiskSpaceBytes = 1024 * 1024;
}  // namespace

namespace apps::deduplication {

AppDeduplicationCache::AppDeduplicationCache(base::FilePath& path) {
  file_path_ = path.AppendASCII(kAppDeduplicationDataFileName);
  if (!base::PathExists(file_path_)) {
    base::CreateDirectory(file_path_.DirName());
  }
}

AppDeduplicationCache::~AppDeduplicationCache() = default;

bool AppDeduplicationCache::WriteDeduplicateDataToDisk(
    proto::DeduplicateData& data) {
  if (base::SysInfo::AmountOfFreeDiskSpace(file_path_.DirName()) <
      kMaxRequiredDiskSpaceBytes) {
    LOG(ERROR) << "Not enough disk space left.";
    return false;
  }

  // Create temporary file.
  base::FilePath temp_file;
  if (!base::CreateTemporaryFileInDir(file_path_.DirName(), &temp_file)) {
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
  if (!base::ReplaceFile(temp_file, file_path_, /*error=*/nullptr)) {
    LOG(ERROR) << "Failed to replace the temporary file.";
    base::DeleteFile(temp_file);
    return false;
  }

  return true;
}

absl::optional<proto::DeduplicateData>
AppDeduplicationCache::ReadDeduplicateDataFromDisk() {
  std::string deduplicate_data_string;

  if (!base::ReadFileToString(file_path_, &deduplicate_data_string)) {
    LOG(ERROR) << "Reading deduplicate data file from disk failed.";
    return absl::nullopt;
  }

  proto::DeduplicateData deduplicate_data;
  if (!deduplicate_data.ParseFromString(deduplicate_data_string)) {
    LOG(ERROR) << "Parsing proto to string failed.";
    return absl::nullopt;
  }

  return deduplicate_data;
}

}  // namespace apps::deduplication
