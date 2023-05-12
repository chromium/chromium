// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_disabled_checker.h"

SyncDisabledChecker::SyncDisabledChecker(syncer::SyncServiceImpl* service)
    : SingleClientStatusChangeChecker(service) {}

SyncDisabledChecker::~SyncDisabledChecker() = default;

bool SyncDisabledChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting until sync is disabled."
      << " IsSetupInProgress:" << service()->IsSetupInProgress()
      << " IsInitialSyncFeatureSetupComplete:"
      << service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete();
  return !service()->IsSetupInProgress() &&
         !service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete();
}

void SyncDisabledChecker::WaitDone() {
  service()->QueryDetailedSyncStatusForDebugging(&status_on_sync_disabled_);
}
