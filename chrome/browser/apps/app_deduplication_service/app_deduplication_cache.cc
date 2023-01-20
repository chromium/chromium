// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_deduplication_service/app_deduplication_cache.h"

#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"

namespace apps::deduplication {

AppDeduplicationCache::AppDeduplicationCache(base::FilePath& path)
    : folder_path_(path) {
  if (!base::PathExists(folder_path_)) {
    base::CreateDirectory(folder_path_);
  }
}

AppDeduplicationCache::~AppDeduplicationCache() = default;

bool AppDeduplicationCache::WriteDeduplicateDataToDisk(
    const base::FilePath& deduplicate_data_path,
    proto::DeduplicateData& data) {
  base::File file(deduplicate_data_path,
                  (base::File::FLAG_CREATE | base::File::FLAG_WRITE));

  if (!file.IsValid() || !file.created()) {
    LOG(ERROR) << "Failed to create deduplicate data file at "
               << deduplicate_data_path.MaybeAsASCII();
    return false;
  }

  std::string deduplicate_data_string;
  data.SerializeToString(&deduplicate_data_string);

  const int written = file.WriteAtCurrentPos(deduplicate_data_string.c_str(),
                                             deduplicate_data_string.length());

  if (written != static_cast<int>(deduplicate_data_string.length())) {
    LOG(ERROR) << "Failed to write all data to deduplicate data file.";
    base::DeleteFile(deduplicate_data_path);
    return false;
  }

  return true;
}

absl::optional<proto::DeduplicateData>
AppDeduplicationCache::ReadDeduplicateDataFromDisk(
    const base::FilePath& deduplicate_data_path) {
  std::string deduplicate_data_string;

  if (!base::PathExists(deduplicate_data_path)) {
    LOG(ERROR) << "Path to deduplicate data file does not exist.";
    return absl::nullopt;
  }

  if (!base::ReadFileToString(deduplicate_data_path,
                              &deduplicate_data_string)) {
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
