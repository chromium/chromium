// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/workspace_desk_helper.h"

#include <sstream>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_model_observer.h"
#include "components/desks_storage/core/desk_sync_service.h"

namespace workspace_desk_helper {

DeskUuidChecker::DeskUuidChecker(desks_storage::DeskSyncService* service,
                                 const base::Uuid& uuid)
    : uuid_(uuid), service_(service) {
  DCHECK(service);
  service->GetDeskModel()->AddObserver(this);
}

DeskUuidChecker::~DeskUuidChecker() {
  service_->GetDeskModel()->RemoveObserver(this);
}

bool DeskUuidChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for data for uuid '" + uuid_.AsLowercaseString() +
             "' to be added/updated.";

  desks_storage::DeskModel* model = service_->GetDeskModel();
  for (const base::Uuid& uuid : model->GetAllEntryUuids()) {
    if (uuid == uuid_) {
      return true;
    }
  }
  return false;
}

void DeskUuidChecker::DeskModelLoaded() {
  CheckExitCondition();
}

void DeskUuidChecker::EntriesAddedOrUpdatedRemotely(
    const std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>>&
        new_entries) {
  CheckExitCondition();
}

void DeskUuidChecker::EntriesRemovedRemotely(
    const std::vector<base::Uuid>& uuids) {
  CheckExitCondition();
}

// DeskUuidDeletedChecker
DeskUuidDeletedChecker::DeskUuidDeletedChecker(
    desks_storage::DeskSyncService* service,
    const base::Uuid& uuid)
    : uuid_(uuid), service_(service) {
  DCHECK(service);
  service->GetDeskModel()->AddObserver(this);
}

DeskUuidDeletedChecker::~DeskUuidDeletedChecker() {
  service_->GetDeskModel()->RemoveObserver(this);
}

bool DeskUuidDeletedChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for data for uuid '" + uuid_.AsLowercaseString() +
             "' to be deleted.";

  desks_storage::DeskModel* model = service_->GetDeskModel();
  for (const base::Uuid& uuid : model->GetAllEntryUuids()) {
    if (uuid == uuid_) {
      return false;
    }
  }
  return true;
}

void DeskUuidDeletedChecker::DeskModelLoaded() {
  CheckExitCondition();
}

void DeskUuidDeletedChecker::EntriesAddedOrUpdatedRemotely(
    const std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>>&
        new_entries) {
  CheckExitCondition();
}

void DeskUuidDeletedChecker::EntriesRemovedRemotely(
    const std::vector<base::Uuid>& uuids) {
  CheckExitCondition();
}

// DeskModelReadyChecker
DeskModelReadyChecker::DeskModelReadyChecker(
    desks_storage::DeskSyncService* service)
    : service_(service) {
  DCHECK(service);
  service->GetDeskModel()->AddObserver(this);
}

DeskModelReadyChecker::~DeskModelReadyChecker() {
  service_->GetDeskModel()->RemoveObserver(this);
}

bool DeskModelReadyChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for model to become ready.";
  return service_->GetDeskModel()->IsReady();
}

void DeskModelReadyChecker::DeskModelLoaded() {
  CheckExitCondition();
}

void DeskModelReadyChecker::EntriesAddedOrUpdatedRemotely(
    const std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>>&
        new_entries) {
  CheckExitCondition();
}

void DeskModelReadyChecker::EntriesRemovedRemotely(
    const std::vector<base::Uuid>& uuids) {
  CheckExitCondition();
}

}  // namespace workspace_desk_helper
