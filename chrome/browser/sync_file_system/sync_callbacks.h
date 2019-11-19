// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_CALLBACKS_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_CALLBACKS_H_

#include "base/callback_forward.h"
#include "chrome/browser/sync_file_system/sync_file_status.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "storage/browser/file_system/file_system_url.h"

namespace storage {
class FileSystemURL;
}

namespace sync_file_system {

class SyncFileMetadata;

typedef base::Callback<void(SyncStatusCode status)>
    SyncStatusCallback;

typedef base::Callback<
    void(SyncStatusCode status, const storage::FileSystemURL& url)>
    SyncFileCallback;

typedef base::Callback<void(SyncStatusCode status,
                            const SyncFileMetadata& metadata)>
    SyncFileMetadataCallback;

typedef base::Callback<
    void(SyncStatusCode status, const storage::FileSystemURLSet& urls)>
    SyncFileSetCallback;

typedef base::Callback<void(SyncStatusCode status,
                            SyncFileStatus sync_file_status)>
    SyncFileStatusCallback;

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_CALLBACKS_H_
