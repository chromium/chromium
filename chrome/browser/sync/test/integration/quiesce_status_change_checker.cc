// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/quiesce_status_change_checker.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/test/fake_server/fake_server.h"

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
  marker1.clear_gc_directive();
  marker2.clear_gc_directive();
  DCHECK(marker1.data_type_id() == marker2.data_type_id());

  if (syncer::GetModelTypeFromSpecificsFieldNumber(marker1.data_type_id()) ==
      syncer::AUTOFILL_WALLET_DATA) {
    return fake_server::AreWalletDataProgressMarkersEquivalent(marker1,
                                                               marker2);
  }
  return marker1.SerializeAsString() == marker2.SerializeAsString();
}

// Returns true if these services have matching progress markers.
bool ProgressMarkersMatch(const syncer::ProfileSyncService* service1,
                          const syncer::ProfileSyncService* service2) {
  // GetActiveDataTypes() is always empty during configuration, so progress
  // markers cannot be compared.
  if (service1->GetTransportState() !=
          syncer::SyncService::TransportState::ACTIVE ||
      service2->GetTransportState() !=
          syncer::SyncService::TransportState::ACTIVE) {
    return false;
  }

  const syncer::ModelTypeSet common_types =
      Intersection(service1->GetActiveDataTypes(),
                   service2->GetActiveDataTypes());

  const syncer::SyncCycleSnapshot& snap1 =
      service1->GetLastCycleSnapshotForDebugging();
  const syncer::SyncCycleSnapshot& snap2 =
      service2->GetLastCycleSnapshotForDebugging();

  for (syncer::ModelType type : common_types) {
    // Look up the progress markers.  Fail if either one is missing.
    auto pm_it1 = snap1.download_progress_markers().find(type);
    if (pm_it1 == snap1.download_progress_markers().end()) {
      return false;
    }

    auto pm_it2 = snap2.download_progress_markers().find(type);
    if (pm_it2 == snap2.download_progress_markers().end()) {
      return false;
    }

    // Fail if any of them don't match.
    if (!AreProgressMarkersEquivalent(pm_it1->second, pm_it2->second)) {
      return false;
    }
  }
  return true;
}

}  // namespace

// Variation of UpdateProgressMarkerChecker that intercepts calls to
// CheckExitCondition() and forwards them to a parent checker.
class QuiesceStatusChangeChecker::NestedUpdatedProgressMarkerChecker
    : public UpdatedProgressMarkerChecker {
 public:
  NestedUpdatedProgressMarkerChecker(
      syncer::ProfileSyncService* service,
      const base::RepeatingClosure& check_exit_condition_cb)
      : UpdatedProgressMarkerChecker(service),
        check_exit_condition_cb_(check_exit_condition_cb) {}

  ~NestedUpdatedProgressMarkerChecker() override {}

 protected:
  void CheckExitCondition() override { check_exit_condition_cb_.Run(); }

 private:
  const base::RepeatingClosure check_exit_condition_cb_;
};

QuiesceStatusChangeChecker::QuiesceStatusChangeChecker(
    std::vector<syncer::ProfileSyncService*> services)
    : MultiClientStatusChangeChecker(services) {
  DCHECK_LE(1U, services.size());
  for (size_t i = 0; i < services.size(); ++i) {
    checkers_.push_back(std::make_unique<NestedUpdatedProgressMarkerChecker>(
        services[i],
        base::BindRepeating(&QuiesceStatusChangeChecker::CheckExitCondition,
                            base::Unretained(this))));
  }
}

QuiesceStatusChangeChecker::~QuiesceStatusChangeChecker() {}

bool QuiesceStatusChangeChecker::IsExitConditionSatisfied(std::ostream* os) {
  // Check that all progress markers are up to date.
  std::vector<syncer::ProfileSyncService*> enabled_services;
  for (const auto& checker : checkers_) {
    enabled_services.push_back(checker->service());

    if (!checker->IsExitConditionSatisfied(os)) {
      *os << "Not quiesced: Progress markers are old.";
      return false;
    }
  }

  for (size_t i = 1; i < enabled_services.size(); ++i) {
    // Return false if there is a progress marker mismatch.
    if (!ProgressMarkersMatch(enabled_services[i - 1], enabled_services[i])) {
      *os << "Not quiesced: Progress marker mismatch.";
      return false;
    }
  }

  return true;
}
