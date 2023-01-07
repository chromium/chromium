// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_DISABLED_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_DISABLED_CHECKER_H_

#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "components/sync/engine/sync_status.h"

// A helper class to wait for Sync-the-feature to become disabled. Note that
// Sync-the-transport might still run.
class SyncDisabledChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncDisabledChecker(syncer::SyncServiceImpl* service);

  ~SyncDisabledChecker() override;

  // A snapshot of the SyncStatus from the point in time where Sync-the-feature
  // was disabled. Meant to be called after Wait() has returned.
  // This is useful because the SyncService may immediately start up again in
  // transport mode, which clears the SyncStatus there.
  const syncer::SyncStatus& status_on_sync_disabled() const {
    return status_on_sync_disabled_;
  }

 protected:
  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
  void WaitDone() override;

 private:
  syncer::SyncStatus status_on_sync_disabled_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_DISABLED_CHECKER_H_
