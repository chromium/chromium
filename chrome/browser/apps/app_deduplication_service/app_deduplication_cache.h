// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_CACHE_H_
#define CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_CACHE_H_

#include "base/files/file_path.h"
#include "chrome/browser/apps/app_deduplication_service/proto/deduplication_data.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps::deduplication {

// The AppDeduplicationCache is used to store deduplicate app data on disk and
// read the stored data from disk. Two versions of the data will be stored to
// the disk at a time in case reading data from the most recent version fails.
// TODO(b/266005828): add functionality to store two versions of data.
class AppDeduplicationCache {
 public:
  // `path` refers to path of the folder on disk which will store the data.
  explicit AppDeduplicationCache(base::FilePath& path);
  AppDeduplicationCache(const AppDeduplicationCache&) = delete;
  AppDeduplicationCache& operator=(const AppDeduplicationCache&) = delete;
  ~AppDeduplicationCache();

  // Creates a file at given file path and stores duplicate data on disk.
  // Returns true if all data is written successfully and false otherwise.
  bool WriteDeduplicateDataToDisk(const base::FilePath& deduplicate_data_path,
                                  proto::DeduplicateData& data);

  // Reads and returns deduplicate data from file at `deduplicate_data_path`.
  absl::optional<proto::DeduplicateData> ReadDeduplicateDataFromDisk(
      const base::FilePath& deduplicate_data_path);

 private:
  base::FilePath folder_path_;
};

}  // namespace apps::deduplication

#endif  // CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_CACHE_H_
