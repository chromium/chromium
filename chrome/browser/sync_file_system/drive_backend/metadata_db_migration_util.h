// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DB_MIGRATION_UTIL_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DB_MIGRATION_UTIL_H_

#include "chrome/browser/sync_file_system/sync_status_code.h"

namespace leveldb {
class DB;
}

namespace sync_file_system {
namespace drive_backend {

// Rollback |db| schema from version 4 to version 3.
SyncStatusCode MigrateDatabaseFromV4ToV3(leveldb::DB* db);

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DB_MIGRATION_UTIL_H_
