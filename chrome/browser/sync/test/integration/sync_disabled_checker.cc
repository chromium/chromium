// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_disabled_checker.h"

#include "build/build_config.h"

SyncDisabledChecker::SyncDisabledChecker(syncer::SyncServiceImpl* service)
    : SingleClientStatusChangeChecker(service) {}

SyncDisabledChecker::~SyncDisabledChecker() = default;

bool SyncDisabledChecker::IsExitConditionSatisfied(std::ostream* os) {
  // TODO(crbug.com/1445931): Revisit condition below to simplify and/or unify
  // across platforms.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  *os << "Waiting until sync is disabled via dashboard.";
  return service()->IsSyncFeatureDisabledViaDashboard();
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  *os << "Waiting until sync is disabled."
      << " IsSetupInProgress:" << service()->IsSetupInProgress()
      << " IsInitialSyncFeatureSetupComplete:"
      << service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete();
  return !service()->IsSetupInProgress() &&
         !service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void SyncDisabledChecker::WaitDone() {
  service()->QueryDetailedSyncStatusForDebugging(&status_on_sync_disabled_);
}
