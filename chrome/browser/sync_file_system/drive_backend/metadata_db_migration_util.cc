// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_db_migration_util.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "url/gurl.h"

namespace sync_file_system {
namespace drive_backend {

SyncStatusCode MigrateDatabaseFromV4ToV3(leveldb::DB* db) {
  // Rollback from version 4 to version 3.
  // Please see metadata_database_index.cc for version 3 format, and
  // metadata_database_index_on_disk.cc for version 4 format.

  const char kDatabaseVersionKey[] = "VERSION";
  const char kServiceMetadataKey[] = "SERVICE";
  const char kFileMetadataKeyPrefix[] = "FILE: ";
  const char kFileTrackerKeyPrefix[] = "TRACKER: ";

  // Key prefixes used in version 4.
  const char kAppRootIDByAppIDKeyPrefix[] = "APP_ROOT: ";
  const char kActiveTrackerIDByFileIDKeyPrefix[] = "ACTIVE_FILE: ";
  const char kTrackerIDByFileIDKeyPrefix[] = "TRACKER_FILE: ";
  const char kMultiTrackerByFileIDKeyPrefix[] = "MULTI_FILE: ";
  const char kActiveTrackerIDByParentAndTitleKeyPrefix[] = "ACTIVE_PATH: ";
  const char kTrackerIDByParentAndTitleKeyPrefix[] = "TRACKER_PATH: ";
  const char kMultiBackingParentAndTitleKeyPrefix[] = "MULTI_PATH: ";
  const char kDirtyIDKeyPrefix[] = "DIRTY: ";
  const char kDemotedDirtyIDKeyPrefix[] = "DEMOTED_DIRTY: ";

  leveldb::WriteBatch write_batch;
  write_batch.Put(kDatabaseVersionKey, "3");

  std::unique_ptr<leveldb::Iterator> itr(
      db->NewIterator(leveldb::ReadOptions()));
  for (itr->SeekToFirst(); itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();

    // Do nothing for valid entries in both versions.
    if (base::StartsWith(key, kServiceMetadataKey,
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(key, kFileMetadataKeyPrefix,
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(key, kFileTrackerKeyPrefix,
                         base::CompareCase::SENSITIVE)) {
      continue;
    }

    // Drop entries used in version 4 only.
    if (base::StartsWith(key, kAppRootIDByAppIDKeyPrefix,
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(key, kActiveTrackerIDByFileIDKeyPrefix,
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(key, kTrackerIDByFileIDKeyPrefix,
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(key, kMultiTrackerByFileIDKeyPrefix,
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(key, kActiveTrackerIDByParentAndTitleKeyPrefix,
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(key, kTrackerIDByParentAndTitleKeyPrefix,
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(key, kMultiBackingParentAndTitleKeyPrefix,
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(key, kDirtyIDKeyPrefix,
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(key, kDemotedDirtyIDKeyPrefix,
                         base::CompareCase::SENSITIVE)) {
      write_batch.Delete(key);
      continue;
    }

    DVLOG(3) << "Unknown key: " << key << " was found.";
  }

  return LevelDBStatusToSyncStatusCode(
      db->Write(leveldb::WriteOptions(), &write_batch));
}

}  // namespace drive_backend
}  // namespace sync_file_system
