// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_action.h"

#include "base/notreached.h"

namespace sync_file_system {

const char* SyncActionToString(SyncAction action) {
  switch (action) {
    case SYNC_ACTION_NONE:
      return "None";
    case SYNC_ACTION_ADDED:
      return "Added";
    case SYNC_ACTION_UPDATED:
      return "Updated";
    case SYNC_ACTION_DELETED:
      return "Deleted";
  }

  NOTREACHED_IN_MIGRATION();
  return "Unknown SyncAction";
}

}  // namespace sync_file_system
