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
      << " IsFirstSetupComplete:"
      << service()->GetUserSettings()->IsFirstSetupComplete();
  return !service()->IsSetupInProgress() &&
         !service()->GetUserSettings()->IsFirstSetupComplete();
}

void SyncDisabledChecker::WaitDone() {
  service()->QueryDetailedSyncStatusForDebugging(&status_on_sync_disabled_);
}
