// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_CACHE_H_
#define CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_CACHE_H_

#include "base/files/file_path.h"
#include "chrome/browser/apps/app_deduplication_service/proto/deduplication_data.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps::deduplication {

// The AppDeduplicationCache is used to store deduplicate app data on disk
// and read the stored data from disk.
class AppDeduplicationCache {
 public:
  // `path` refers to path of the folder on disk which will store the data.
  explicit AppDeduplicationCache(base::FilePath& path);
  AppDeduplicationCache(const AppDeduplicationCache&) = delete;
  AppDeduplicationCache& operator=(const AppDeduplicationCache&) = delete;
  ~AppDeduplicationCache();

  // Writes data to deduplication file on disk. If the write operation
  // fails, the existing data on disk will be unaffected. Returns true if
  // data is written to disk successfully and false otherwise.
  bool WriteDeduplicateDataToDisk(proto::DeduplicateData& data);

  // Reads and returns deduplicate data from file.
  absl::optional<proto::DeduplicateData> ReadDeduplicateDataFromDisk();

 private:
  // Absolute path to the file where deduplication data is stored.
  base::FilePath file_path_;
};

}  // namespace apps::deduplication

#endif  // CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_CACHE_H_
