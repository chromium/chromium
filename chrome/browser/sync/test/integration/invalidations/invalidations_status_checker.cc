// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/invalidations/invalidations_status_checker.h"

#include "components/sync/service/sync_service_impl.h"

InvalidationsStatusChecker::InvalidationsStatusChecker(
    syncer::SyncServiceImpl* sync_service,
    bool expected_status)
    : SingleClientStatusChangeChecker(sync_service),
      expected_status_(expected_status) {}

bool InvalidationsStatusChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for invalidations status to be "
      << (expected_status_ ? "enabled " : "disabled ");

  syncer::SyncStatus sync_status;
  if (!service()->QueryDetailedSyncStatusForDebugging(&sync_status)) {
    *os << "Engine is not initialized yet.";
    return false;
  }

  return sync_status.notifications_enabled == expected_status_;
}
