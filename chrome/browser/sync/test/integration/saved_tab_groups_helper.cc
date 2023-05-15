// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/saved_tab_groups_helper.h"

#include <vector>

#include "base/uuid.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "components/saved_tab_groups/saved_tab_group_model_observer.h"

class SavedTabGroupKeyedService;

namespace saved_tab_groups_helper {

// ====================================
// --- SavedTabOrGroupExistsChecker ---
// ====================================
SavedTabOrGroupExistsChecker::SavedTabOrGroupExistsChecker(
    SavedTabGroupKeyedService* service,
    const base::Uuid& uuid)
    : uuid_(uuid), service_(service) {
  CHECK(service_);
  service_->model()->AddObserver(this);
}

SavedTabOrGroupExistsChecker::~SavedTabOrGroupExistsChecker() {
  service_->model()->RemoveObserver(this);
}

bool SavedTabOrGroupExistsChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for data for uuid '" + uuid_.AsLowercaseString() +
             "' to be added.";

  const SavedTabGroupModel* const model = service_->model();

  // Expect that `uuid_` exists in the SavedTabGroupModel.
  for (const SavedTabGroup& group : model->saved_tab_groups()) {
    if (group.saved_guid() == uuid_ || group.ContainsTab(uuid_)) {
      return true;
    }
  }

  return false;
}

void SavedTabOrGroupExistsChecker::SavedTabGroupAddedFromSync(
    const base::Uuid& uuid) {
  CheckExitCondition();
}

void SavedTabOrGroupExistsChecker::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_uuid,
    const absl::optional<base::Uuid>& tab_uuid) {
  CheckExitCondition();
}

// ==========================================
// --- SavedTabOrGroupDoesNotExistChecker ---
// ==========================================
SavedTabOrGroupDoesNotExistChecker::SavedTabOrGroupDoesNotExistChecker(
    SavedTabGroupKeyedService* service,
    const base::Uuid& uuid)
    : uuid_(uuid), service_(service) {
  CHECK(service_);
  service_->model()->AddObserver(this);
}

SavedTabOrGroupDoesNotExistChecker::~SavedTabOrGroupDoesNotExistChecker() {
  service_->model()->RemoveObserver(this);
}

bool SavedTabOrGroupDoesNotExistChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for data for uuid '" + uuid_.AsLowercaseString() +
             "' to be deleted.";

  const SavedTabGroupModel* const model = service_->model();

  // Expect that `uuid_` does not exist in the SavedTabGroupModel.
  for (const SavedTabGroup& group : model->saved_tab_groups()) {
    if (group.saved_guid() == uuid_ || group.ContainsTab(uuid_)) {
      return false;
    }
  }

  return true;
}

void SavedTabOrGroupDoesNotExistChecker::SavedTabGroupRemovedFromSync(
    const SavedTabGroup* removed_group) {
  CheckExitCondition();
}

void SavedTabOrGroupDoesNotExistChecker::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_uuid,
    const absl::optional<base::Uuid>& tab_uuid) {
  CheckExitCondition();
}
}  // namespace saved_tab_groups_helper
