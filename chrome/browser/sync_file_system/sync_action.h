// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_ACTION_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_ACTION_H_

namespace sync_file_system {

enum SyncAction {
  // Indicates no action has been made.
  SYNC_ACTION_NONE,

  // Indicates a new file or directory has been added.
  SYNC_ACTION_ADDED,

  // Indicates an existing file or directory has been updated.
  SYNC_ACTION_UPDATED,

  // Indicates a file or directory has been deleted.
  SYNC_ACTION_DELETED,
};

const char* SyncActionToString(SyncAction action);

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_ACTION_H_
