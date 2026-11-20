// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/quiesce_status_change_checker.h"

#include <stddef.h>

#include <optional>
#include <ostream>
#include <string>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server.h"

namespace {

// Compares two serialized progress markers for equivalence to determine client
// side progress. Some aspects of the progress markers like
// GarbageCollectionDirectives are irrelevant for this, as they can vary between
// requests -- for example a version_watermark could be based on request time.
bool AreProgressMarkersEquivalent(const std::string& serialized1,
                                  const std::string& serialized2) {
  sync_pb::DataTypeProgressMarker marker1;
  sync_pb::DataTypeProgressMarker marker2;
  CHECK(marker1.ParseFromString(serialized1));
  CHECK(marker2.ParseFromString(serialized2));
  CHECK_EQ(marker1.data_type_id(), marker2.data_type_id());
  CHECK(!marker1.has_gc_directive());
  CHECK(!marker2.has_gc_directive());

  return marker1.SerializeAsString() == marker2.SerializeAsString();
}

// Returns true if these services have matching progress markers.
bool ProgressMarkersMatch(const syncer::SyncServiceImpl* service1,
                          const syncer::SyncServiceImpl* service2,
                          std::ostream* os) {
  // GetActiveDataTypes() is always empty during configuration, so progress
  // markers cannot be compared.
  if (service1->GetTransportState() !=
          syncer::SyncService::TransportState::ACTIVE ||
      service2->GetTransportState() !=
          syncer::SyncService::TransportState::ACTIVE) {
    *os << "Transport state differs";
    return false;
  }

  const syncer::DataTypeSet common_types = Intersection(
      service1->GetActiveDataTypes(), service2->GetActiveDataTypes());

  const syncer::SyncCycleSnapshot& snap1 =
      service1->GetLastCycleSnapshotForDebugging();
  const syncer::SyncCycleSnapshot& snap2 =
      service2->GetLastCycleSnapshotForDebugging();

  for (syncer::DataType type : common_types) {
    if (!syncer::ProtocolTypes().Has(type)) {
      continue;
    }

    // Look up the progress markers.  Fail if either one is missing.
    auto pm_it1 = snap1.download_progress_markers().find(type);
    if (pm_it1 == snap1.download_progress_markers().end()) {
      *os << "Progress marker missing in client 1 for "
          << syncer::DataTypeToDebugString(type);
      return false;
    }

    auto pm_it2 = snap2.download_progress_markers().find(type);
    if (pm_it2 == snap2.download_progress_markers().end()) {
      *os << "Progress marker missing in client 2 for "
          << syncer::DataTypeToDebugString(type);
      return false;
    }

    // Fail if any of them don't match.
    if (!AreProgressMarkersEquivalent(pm_it1->second, pm_it2->second)) {
      *os << "Progress markers don't match for "
          << syncer::DataTypeToDebugString(type);
      return false;
    }
  }
  return true;
}

}  // namespace

// Waits until all local changes have been committed and progress markers are
// updated for a single client, forwarding `CheckExitCondition()` calls to the
// parent `QuiesceStatusChangeChecker`.
//
// It relies on the test-only 'self-notify' to trigger an extra `GetUpdates`
// cycle after every commit.
class QuiesceStatusChangeChecker::NestedUpdatedProgressMarkerChecker
    : public SingleClientStatusChangeChecker {
 public:
  // Both `service` and `fake_server` must not be null and must outlive this
  // object.
  NestedUpdatedProgressMarkerChecker(
      syncer::SyncServiceImpl* service,
      fake_server::FakeServer* fake_server,
      const base::RepeatingClosure& check_exit_condition_cb)
      : SingleClientStatusChangeChecker(service),
        fake_server_(CHECK_DEREF(fake_server)),
        check_exit_condition_cb_(check_exit_condition_cb) {
    CHECK(sync_datatype_helper::test()->TestUsesSelfNotifications());

    // `HasUnsyncedItemsForTest()` requires the engine to be initialized.
    if (service->IsEngineInitialized()) {
      service->HasUnsyncedItemsForTest(base::BindOnce(
          &NestedUpdatedProgressMarkerChecker::GotHasUnsyncedItems,
          weak_ptr_factory_.GetWeakPtr()));
    } else {
      waiting_for_engine_initialization_ = true;
    }
  }

  ~NestedUpdatedProgressMarkerChecker() override = default;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for progress markers... ";

    CHECK_NE(service()->GetTransportState(),
             syncer::SyncService::TransportState::DISABLED);
    CHECK_NE(service()->GetTransportState(),
             syncer::SyncService::TransportState::PAUSED);

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
    //    `GetUpdates` cycle, and commit-only cycles do not update progress
    //    markers).
    if (!fake_server_->AreProgressMarkersUpToDate(
            snap.download_progress_markers())) {
      *os << "Progress markers are not up to date.";
      return false;
    }

    return true;
  }

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override {
    if (waiting_for_engine_initialization_ &&
        service()->IsEngineInitialized()) {
      waiting_for_engine_initialization_ = false;
      service()->HasUnsyncedItemsForTest(base::BindOnce(
          &NestedUpdatedProgressMarkerChecker::GotHasUnsyncedItems,
          weak_ptr_factory_.GetWeakPtr()));
    }

    SingleClientStatusChangeChecker::OnStateChanged(sync);
  }

  void OnSyncCycleCompleted(syncer::SyncService* sync) override {
    weak_ptr_factory_.InvalidateWeakPtrs();
    has_unsynced_items_.reset();
    service()->HasUnsyncedItemsForTest(
        base::BindOnce(&NestedUpdatedProgressMarkerChecker::GotHasUnsyncedItems,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 protected:
  void CheckExitCondition() override { check_exit_condition_cb_.Run(); }

 private:
  void GotHasUnsyncedItems(bool has_unsynced_items) {
    has_unsynced_items_ = has_unsynced_items;
    CheckExitCondition();
  }

  const raw_ref<fake_server::FakeServer> fake_server_;
  const base::RepeatingClosure check_exit_condition_cb_;
  bool waiting_for_engine_initialization_ = false;
  std::optional<bool> has_unsynced_items_;

  base::WeakPtrFactory<NestedUpdatedProgressMarkerChecker> weak_ptr_factory_{
      this};
};

QuiesceStatusChangeChecker::QuiesceStatusChangeChecker(
    std::vector<raw_ptr<syncer::SyncServiceImpl>> services,
    fake_server::FakeServer* fake_server)
    : MultiClientStatusChangeChecker(services) {
  CHECK_LE(1U, services.size());
  for (syncer::SyncServiceImpl* service : services) {
    checkers_.push_back(std::make_unique<NestedUpdatedProgressMarkerChecker>(
        service, fake_server,
        base::BindRepeating(&QuiesceStatusChangeChecker::CheckExitCondition,
                            base::Unretained(this))));
  }
}

QuiesceStatusChangeChecker::~QuiesceStatusChangeChecker() = default;

bool QuiesceStatusChangeChecker::IsExitConditionSatisfied(std::ostream* os) {
  // Check that all progress markers are up to date for active services.
  std::vector<syncer::SyncServiceImpl*> active_services;
  for (const std::unique_ptr<NestedUpdatedProgressMarkerChecker>& checker :
       checkers_) {
    if (checker->service()->GetTransportState() ==
            syncer::SyncService::TransportState::DISABLED ||
        checker->service()->GetTransportState() ==
            syncer::SyncService::TransportState::PAUSED) {
      continue;
    }

    active_services.push_back(checker->service());

    if (!checker->IsExitConditionSatisfied(os)) {
      *os << "Not quiesced: Progress markers are old.";
      return false;
    }
  }

  for (size_t i = 1; i < active_services.size(); ++i) {
    // Return false if there is a progress marker mismatch.
    if (!ProgressMarkersMatch(active_services[i - 1], active_services[i], os)) {
      *os << "Not quiesced: Progress marker mismatch.";
      return false;
    }
  }

  return true;
}
