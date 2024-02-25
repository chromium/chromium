// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_COMMITTED_ALL_NUDGED_CHANGES_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_COMMITTED_ALL_NUDGED_CHANGES_CHECKER_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"

namespace syncer {
class SyncServiceImpl;
}  // namespace syncer

// Checker to block until all nudged changes have been committed to the server.
// It doesn't rely on self invalidations and it's safe to be used in tests which
// turn off self notifications.
class CommittedAllNudgedChangesChecker
    : public SingleClientStatusChangeChecker {
 public:
  explicit CommittedAllNudgedChangesChecker(syncer::SyncServiceImpl* service);
  ~CommittedAllNudgedChangesChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // syncer::SyncServiceObserver implementation.
  void OnSyncCycleCompleted(syncer::SyncService* sync) override;

 private:
  void GotHasUnsyncedItems(bool has_unsynced_items);

  std::optional<bool> has_unsynced_items_;
  base::WeakPtrFactory<CommittedAllNudgedChangesChecker> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_COMMITTED_ALL_NUDGED_CHANGES_CHECKER_H_
