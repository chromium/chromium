// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_CONSTANTS_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_CONSTANTS_H_

#include <stdint.h>

#include "base/files/file_path.h"

namespace sync_file_system {
namespace drive_backend {

extern const char kSyncRootFolderTitle[];
extern const char kMimeTypeOctetStream[];

extern const base::FilePath::CharType kDatabaseName[];

extern const char kDatabaseVersionKey[];
extern const int64_t kCurrentDatabaseVersion;
extern const int64_t kDatabaseOnDiskVersion;
extern const char kServiceMetadataKey[];
extern const char kFileMetadataKeyPrefix[];
extern const char kFileTrackerKeyPrefix[];
extern const char kLastValidationTimeKey[];

extern const char kAppRootIDByAppIDKeyPrefix[];
extern const char kActiveTrackerIDByFileIDKeyPrefix[];
extern const char kTrackerIDByFileIDKeyPrefix[];
extern const char kMultiTrackerByFileIDKeyPrefix[];
extern const char kActiveTrackerIDByParentAndTitleKeyPrefix[];
extern const char kTrackerIDByParentAndTitleKeyPrefix[];
extern const char kMultiBackingParentAndTitleKeyPrefix[];
extern const char kDirtyIDKeyPrefix[];
extern const char kDemotedDirtyIDKeyPrefix[];

extern const int kMaxRetry;
extern const int64_t kListChangesRetryDelaySeconds;

extern const int64_t kInvalidTrackerID;

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_CONSTANTS_H_
