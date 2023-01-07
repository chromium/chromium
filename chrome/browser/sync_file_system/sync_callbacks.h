// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_CALLBACKS_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_CALLBACKS_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/sync_file_system/sync_file_status.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "storage/browser/file_system/file_system_url.h"

namespace storage {
class FileSystemURL;
}

namespace sync_file_system {

class SyncFileMetadata;

using SyncStatusCallback = base::OnceCallback<void(SyncStatusCode status)>;

using SyncFileCallback =
    base::OnceCallback<void(SyncStatusCode status,
                            const storage::FileSystemURL& url)>;

using SyncFileMetadataCallback =
    base::OnceCallback<void(SyncStatusCode status,
                            const SyncFileMetadata& metadata)>;

using SyncFileStatusCallback =
    base::OnceCallback<void(SyncStatusCode status,
                            SyncFileStatus sync_file_status)>;

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_CALLBACKS_H_
