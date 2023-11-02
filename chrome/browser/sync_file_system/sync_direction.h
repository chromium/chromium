// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_DIRECTION_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_DIRECTION_H_

namespace sync_file_system {

enum SyncDirection {
  SYNC_DIRECTION_NONE,
  SYNC_DIRECTION_LOCAL_TO_REMOTE,
  SYNC_DIRECTION_REMOTE_TO_LOCAL,
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_DIRECTION_H_
