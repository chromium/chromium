// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/trigger_refresh_checker.h"

#include <ostream>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/enum_set.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server.h"

TriggerRefreshChecker::TriggerRefreshChecker(
    syncer::SyncServiceImpl* service,
    fake_server::FakeServer* fake_server,
    syncer::DataTypeSet types)
    : SingleClientStatusChangeChecker(service),
      fake_server_(CHECK_DEREF(fake_server)),
      types_(base::Difference(types, syncer::CommitOnlyTypes())) {
  CHECK(service->IsEngineInitialized());
  CHECK(!types_.empty());
  fake_server_observation_.Observe(&fake_server_.get());
}

TriggerRefreshChecker::~TriggerRefreshChecker() = default;

bool TriggerRefreshChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for refresh sync cycle completion for "
      << syncer::DataTypeSetToDebugString(types_);
  return refresh_cycle_completed_;
}

void TriggerRefreshChecker::OnSuccessfulGetUpdates() {
  sync_pb::ClientToServerMessage message;
  if (!fake_server_->GetLastGetUpdatesMessage(&message)) {
    return;
  }

  syncer::DataTypeSet refreshed_types;
  for (const sync_pb::DataTypeProgressMarker& progress_marker :
       message.get_updates().from_progress_marker()) {
    if (progress_marker.get_update_triggers().datatype_refresh_nudges() > 0) {
      const syncer::DataType type = syncer::GetDataTypeFromSpecificsFieldNumber(
          progress_marker.data_type_id());
      if (syncer::IsRealDataType(type)) {
        refreshed_types.Put(type);
      }
    }
  }

  const syncer::DataTypeSet expected_types =
      base::Intersection(types_, service()->GetActiveDataTypes());
  if (!expected_types.empty() && refreshed_types.HasAll(expected_types)) {
    refresh_get_updates_processed_ = true;
  }
}

void TriggerRefreshChecker::OnSyncCycleCompleted(syncer::SyncService* sync) {
  if (refresh_get_updates_processed_) {
    refresh_cycle_completed_ = true;
  }
  CheckExitCondition();
}
