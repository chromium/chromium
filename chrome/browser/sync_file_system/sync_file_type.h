// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_TYPE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_TYPE_H_

namespace sync_file_system {

enum SyncFileType {
  // For non-existent files or for files whose type is not known yet.
  SYNC_FILE_TYPE_UNKNOWN,

  // For regular files.
  SYNC_FILE_TYPE_FILE,

  // For directories/folders.
  SYNC_FILE_TYPE_DIRECTORY,
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_TYPE_H_
