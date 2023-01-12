// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_INVALIDATIONS_INVALIDATIONS_STATUS_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_INVALIDATIONS_INVALIDATIONS_STATUS_CHECKER_H_

#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"

namespace syncer {
class SyncServiceImpl;
}  // namespace syncer

// Waits for invalidations to be enabled or disabled (i.e.
// SyncStatus::notifications_enabled).
class InvalidationsStatusChecker : public SingleClientStatusChangeChecker {
 public:
  InvalidationsStatusChecker(syncer::SyncServiceImpl* sync_service,
                             bool expected_status);

  // SingleClientStatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const bool expected_status_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_INVALIDATIONS_INVALIDATIONS_STATUS_CHECKER_H_
