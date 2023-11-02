// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_STATUS_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_STATUS_H_

namespace sync_file_system {

enum SyncFileStatus {
  // The file has unknown sync status (e.g. the file is missing or there
  // was an error while retrieving the sync status for the file).
  SYNC_FILE_STATUS_UNKNOWN,

  // The file has no pending local changes (may have pending remote changes
  // though).
  SYNC_FILE_STATUS_SYNCED,

  // The file has pending local changes that haven't been reflected to the
  // remote side.
  SYNC_FILE_STATUS_HAS_PENDING_CHANGES,

  // The file is in conflicting state.
  SYNC_FILE_STATUS_CONFLICTING,
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_STATUS_H_
