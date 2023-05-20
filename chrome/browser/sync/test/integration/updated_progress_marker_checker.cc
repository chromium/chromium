// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"

#include "base/functional/bind.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/service/sync_service_impl.h"

UpdatedProgressMarkerChecker::UpdatedProgressMarkerChecker(
    syncer::SyncServiceImpl* service)
    : SingleClientStatusChangeChecker(service) {
  DCHECK(sync_datatype_helper::test()->TestUsesSelfNotifications());

  // HasUnsyncedItemsForTest() posts a task to the sync thread which guarantees
  // that all tasks posted to the sync thread before this constructor have been
  // processed.
  service->HasUnsyncedItemsForTest(
      base::BindOnce(&UpdatedProgressMarkerChecker::GotHasUnsyncedItems,
                     weak_ptr_factory_.GetWeakPtr()));
}

UpdatedProgressMarkerChecker::~UpdatedProgressMarkerChecker() = default;

bool UpdatedProgressMarkerChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for progress markers... ";

  if (!has_unsynced_items_.has_value()) {
    *os << "Unknown synced values state.";
    return false;
  }

  const syncer::SyncCycleSnapshot& snap =
      service()->GetLastCycleSnapshotForDebugging();
  // Assuming the lack of ongoing remote changes, the progress marker can be
  // considered updated when:
  // 1. Progress markers are non-empty (which discards the default value for
  //    GetLastCycleSnapshot() prior to the first sync cycle).
  // 2. Our last sync cycle committed no changes (because commits are followed
  //    by the test-only 'self-notify' cycle).
  // 3. No pending local changes (which will ultimately generate new progress
  //    markers once submitted to the server).
  if (snap.download_progress_markers().empty()) {
    *os << "Progress markers are empty.";
    return false;
  }

  if (snap.model_neutral_state().num_successful_commits > 0) {
    *os << "Last sync cycle wasn't empty.";
    return false;
  }

  if (has_unsynced_items_.value()) {
    *os << "Has unsynced items.";
    return false;
  }

  return true;
}

void UpdatedProgressMarkerChecker::GotHasUnsyncedItems(
    bool has_unsynced_items) {
  has_unsynced_items_ = has_unsynced_items;
  CheckExitCondition();
}

void UpdatedProgressMarkerChecker::OnSyncCycleCompleted(
    syncer::SyncService* sync) {
  // Ignore sync cycles that were started before our constructor posted
  // HasUnsyncedItemsForTest() to the sync thread.
  if (!has_unsynced_items_.has_value()) {
    return;
  }

  // Override |has_unsynced_items_| with the result of the sync cycle.
  const syncer::SyncCycleSnapshot& snap =
      service()->GetLastCycleSnapshotForDebugging();
  has_unsynced_items_ = snap.has_remaining_local_changes();
  CheckExitCondition();
}
