// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_operation_type.h"

#include "base/notreached.h"

namespace sync_file_system {

const char* SyncOperationTypeToString(SyncOperationType type) {
  switch (type) {
    case SYNC_OPERATION_ADD_FILE:
      return "ADD_FILE";
    case SYNC_OPERATION_ADD_DIRECTORY:
      return "ADD_DIRECTORY";
    case SYNC_OPERATION_UPDATE_FILE:
      return "UPDATE_FILE";
    case SYNC_OPERATION_DELETE:
      return "DELETE";
    case SYNC_OPERATION_NONE:
      return "NONE";
    case SYNC_OPERATION_CONFLICT:
      return "CONFLICT";
    case SYNC_OPERATION_RESOLVE_TO_LOCAL:
      return "RESOLVE_TO_LOCAL";
    case SYNC_OPERATION_RESOLVE_TO_REMOTE:
      return "RESOLVE_TO_REMOTE";
    case SYNC_OPERATION_DELETE_METADATA:
      return "DELETE_METADATA";
    case SYNC_OPERATION_FAIL:
      return "FAIL";
  }
  NOTREACHED_IN_MIGRATION();
  return "UNKNOWN";
}

}  // namespace sync_file_system
