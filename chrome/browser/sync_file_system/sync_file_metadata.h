// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_METADATA_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_METADATA_H_

#include <stdint.h>

#include "base/time/time.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "storage/browser/file_system/file_system_url.h"

namespace sync_file_system {

class SyncFileMetadata {
 public:
  SyncFileMetadata();
  SyncFileMetadata(SyncFileType file_type,
                   int64_t size,
                   const base::Time& last_modified);
  ~SyncFileMetadata();

  SyncFileType file_type;
  int64_t size;
  base::Time last_modified;

  bool operator==(const SyncFileMetadata& that) const;
};

struct LocalFileSyncInfo {
  LocalFileSyncInfo();
  LocalFileSyncInfo(const LocalFileSyncInfo& other);
  ~LocalFileSyncInfo();

  storage::FileSystemURL url;
  base::FilePath local_file_path;
  SyncFileMetadata metadata;
  FileChangeList changes;
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_METADATA_H_
