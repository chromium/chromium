// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_OPERATION_TYPE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_OPERATION_TYPE_H_

namespace sync_file_system {

enum SyncOperationType {
  SYNC_OPERATION_ADD_FILE,
  SYNC_OPERATION_ADD_DIRECTORY,
  SYNC_OPERATION_UPDATE_FILE,
  SYNC_OPERATION_DELETE,
  SYNC_OPERATION_NONE,
  SYNC_OPERATION_CONFLICT,
  SYNC_OPERATION_RESOLVE_TO_LOCAL,
  SYNC_OPERATION_RESOLVE_TO_REMOTE,
  SYNC_OPERATION_DELETE_METADATA,
  SYNC_OPERATION_FAIL,
};

const char* SyncOperationTypeToString(SyncOperationType type);

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_OPERATION_TYPE_H_
