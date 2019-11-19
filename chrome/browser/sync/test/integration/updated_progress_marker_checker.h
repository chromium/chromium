// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_UPDATED_PROGRESS_MARKER_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_UPDATED_PROGRESS_MARKER_CHECKER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "components/sync/driver/sync_service_observer.h"

// Waits until all local changes have been committed and progress markers are
// updated. This includes local changes posted to the sync thread before the
// construction of this object.
//
// It relies on the test-only 'self-notify' to trigger an extra GetUpdate cycle
// after every commit.
//
// Because of these limitations, we intend to eventually migrate all tests off
// of this checker.  Please do not use it in new tests.
class UpdatedProgressMarkerChecker : public SingleClientStatusChangeChecker {
 public:
  explicit UpdatedProgressMarkerChecker(syncer::ProfileSyncService* service);
  ~UpdatedProgressMarkerChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // syncer::SyncServiceObserver implementation.
  void OnSyncCycleCompleted(syncer::SyncService* sync) override;

 private:
  void GotHasUnsyncedItems(bool has_unsynced_items);

  base::Optional<bool> has_unsynced_items_;

  base::WeakPtrFactory<UpdatedProgressMarkerChecker> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_UPDATED_PROGRESS_MARKER_CHECKER_H_
