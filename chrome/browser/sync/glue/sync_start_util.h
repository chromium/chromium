// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Various utilities for kicking off sync initialization from data types or
// other services.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNC_START_UTIL_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNC_START_UTIL_H_

#include "components/sync/model/syncable_service.h"

namespace base {
class FilePath;
}

namespace sync_start_util {

// Creates a StartSyncFlare that a SyncableService can use to tell SyncService
// it needs sync to start ASAP. Typically this would be given to the
// SyncableService on construction.
//
// The flare built by this function is designed to be Run()able from any thread
// so that non-UI types don't have to deal with posting tasks.
//
// |profile_path| is used to get a hold of the actual Profile* once the
// request to start sync is safely in UI Thread land.
syncer::SyncableService::StartSyncFlare GetFlareForSyncableService(
    const base::FilePath& profile_path);

}  // namespace sync_start_util

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNC_START_UTIL_H_
