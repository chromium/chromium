// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server.h"

UpdatedProgressMarkerChecker::UpdatedProgressMarkerChecker(
    syncer::SyncServiceImpl* service,
    fake_server::FakeServer* fake_server)
    : SingleClientStatusChangeChecker(service),
      fake_server_(CHECK_DEREF(fake_server)) {
  CHECK(sync_datatype_helper::test()->TestUsesSelfNotifications());

  // HasUnsyncedItemsForTest() requires the engine to be initialized.
  if (service->IsEngineInitialized()) {
    service->HasUnsyncedItemsForTest(
        base::BindOnce(&UpdatedProgressMarkerChecker::GotHasUnsyncedItems,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    waiting_for_engine_initialization_ = true;
  }
}

UpdatedProgressMarkerChecker::~UpdatedProgressMarkerChecker() = default;

bool UpdatedProgressMarkerChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for progress markers... ";

  if (service()->GetTransportState() ==
          syncer::SyncService::TransportState::DISABLED ||
      service()->GetTransportState() ==
          syncer::SyncService::TransportState::PAUSED) {
    return true;
  }

  if (!service()->IsEngineInitialized()) {
    *os << "Waiting for engine initialization...";
    return false;
  }

  if (!has_unsynced_items_.has_value()) {
    *os << "Unknown synced values state.";
    return false;
  }

  if (has_unsynced_items_.value()) {
    *os << "Has unsynced items.";
    return false;
  }

  const syncer::SyncCycleSnapshot& snap =
      service()->GetLastCycleSnapshotForDebugging();
  // Assuming the lack of ongoing remote changes, the progress marker can be
  // considered updated when:
  // 1. No pending local changes (which will ultimately generate new progress
  //    markers once submitted to the server).
  // 2. Progress markers are non-empty and match the latest state on
  //    `FakeServer` (commits are followed by the test-only 'self-notify'
  //    GetUpdates cycle, and commit-only cycles do not update progress
  //    markers).
  if (!fake_server_->AreProgressMarkersUpToDate(
          snap.download_progress_markers())) {
    *os << "Progress markers are not up to date.";
    return false;
  }

  return true;
}

void UpdatedProgressMarkerChecker::GotHasUnsyncedItems(
    bool has_unsynced_items) {
  has_unsynced_items_ = has_unsynced_items;
  CheckExitCondition();
}

void UpdatedProgressMarkerChecker::OnStateChanged(syncer::SyncService* sync) {
  if (waiting_for_engine_initialization_ && service()->IsEngineInitialized()) {
    waiting_for_engine_initialization_ = false;
    service()->HasUnsyncedItemsForTest(
        base::BindOnce(&UpdatedProgressMarkerChecker::GotHasUnsyncedItems,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  SingleClientStatusChangeChecker::OnStateChanged(sync);
}

void UpdatedProgressMarkerChecker::OnSyncCycleCompleted(
    syncer::SyncService* sync) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  has_unsynced_items_.reset();
  service()->HasUnsyncedItemsForTest(
      base::BindOnce(&UpdatedProgressMarkerChecker::GotHasUnsyncedItems,
                     weak_ptr_factory_.GetWeakPtr()));
}
