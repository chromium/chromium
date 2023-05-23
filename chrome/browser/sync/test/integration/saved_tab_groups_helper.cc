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

// ===================================
// --- SavedTabGroupMatchesChecker ---
// ===================================
SavedTabGroupMatchesChecker::SavedTabGroupMatchesChecker(
    SavedTabGroupKeyedService* service,
    SavedTabGroup group)
    : group_(group), service_(service) {
  CHECK(service_);
  service_->model()->AddObserver(this);
}

SavedTabGroupMatchesChecker::~SavedTabGroupMatchesChecker() {
  service_->model()->RemoveObserver(this);
}

bool SavedTabGroupMatchesChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for data for group with uuid '" +
             group_.saved_guid().AsLowercaseString() + "' to be updated.";

  const SavedTabGroupModel* const model = service_->model();

  // Expect that a group in the model has the id, title, and color of `group_`.
  // Other fields are either not synced (e.g. `local_group_id_`), have to do
  // with the group's relationship to other entities (e.g. `position_`,
  // `saved_tabs_`), or are implementation details (creation/update time).
  for (const SavedTabGroup& group : model->saved_tab_groups()) {
    if (group.saved_guid() == group_.saved_guid()) {
      return group.title() == group_.title() && group.color() == group_.color();
    }
  }

  return false;
}

void SavedTabGroupMatchesChecker::SavedTabGroupAddedFromSync(
    const base::Uuid& uuid) {
  CheckExitCondition();
}

void SavedTabGroupMatchesChecker::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_uuid,
    const absl::optional<base::Uuid>& tab_uuid) {
  CheckExitCondition();
}

// ==============================
// --- SavedTabMatchesChecker ---
// ==============================
SavedTabMatchesChecker::SavedTabMatchesChecker(
    SavedTabGroupKeyedService* service,
    SavedTabGroupTab tab)
    : tab_(tab), service_(service) {
  CHECK(service_);
  service_->model()->AddObserver(this);
}

SavedTabMatchesChecker::~SavedTabMatchesChecker() {
  service_->model()->RemoveObserver(this);
}

bool SavedTabMatchesChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for data for tab with uuid '" +
             tab_.saved_tab_guid().AsLowercaseString() + "' to be updated.";

  const SavedTabGroupModel* const model = service_->model();

  // Expect that a tab in the model has the id, url, and title of `tab_`. Other
  // fields are either not synced (e.g. `local_tab_id_`, `favicon_`), have to do
  // with the group's relationship to other entities (e.g. `position_`,
  // `saved_group_`), or are implementation details (creation/update time).
  for (const SavedTabGroup& group : model->saved_tab_groups()) {
    for (const SavedTabGroupTab& tab : group.saved_tabs()) {
      if (tab.saved_tab_guid() == tab_.saved_tab_guid()) {
        return tab.url() == tab_.url() && tab.title() == tab_.title();
      }
    }
  }

  return false;
}

void SavedTabMatchesChecker::SavedTabGroupAddedFromSync(
    const base::Uuid& uuid) {
  CheckExitCondition();
}

void SavedTabMatchesChecker::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_uuid,
    const absl::optional<base::Uuid>& tab_uuid) {
  CheckExitCondition();
}

// =========================
// --- GroupOrderChecker ---
// =========================
GroupOrderChecker::GroupOrderChecker(SavedTabGroupKeyedService* service,
                                     std::vector<base::Uuid> group_ids)
    : group_ids_(group_ids), service_(service) {
  CHECK(service_);
  service_->model()->AddObserver(this);
}

GroupOrderChecker::~GroupOrderChecker() {
  service_->model()->RemoveObserver(this);
}

bool GroupOrderChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for saved group ordering to be updated.";

  const SavedTabGroupModel* const model = service_->model();

  if (model->saved_tab_groups().size() != group_ids_.size()) {
    return false;
  }

  // Expect that the model has the same groups in the same order as
  // `group_ids_`.
  for (size_t i = 0; i < group_ids_.size(); i++) {
    if (model->saved_tab_groups()[i].saved_guid() != group_ids_[i]) {
      return false;
    }
  }

  return true;
}

void GroupOrderChecker::SavedTabGroupAddedFromSync(const base::Uuid& uuid) {
  CheckExitCondition();
}

void GroupOrderChecker::SavedTabGroupRemovedFromSync(
    const SavedTabGroup* removed_group) {
  CheckExitCondition();
}

void GroupOrderChecker::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_uuid,
    const absl::optional<base::Uuid>& tab_uuid) {
  CheckExitCondition();
}

// =======================
// --- TabOrderChecker ---
// =======================
TabOrderChecker::TabOrderChecker(SavedTabGroupKeyedService* service,
                                 base::Uuid group_id,
                                 std::vector<base::Uuid> tab_ids)
    : group_id_(group_id), tab_ids_(tab_ids), service_(service) {
  CHECK(service_);
  service_->model()->AddObserver(this);
}

TabOrderChecker::~TabOrderChecker() {
  service_->model()->RemoveObserver(this);
}

bool TabOrderChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for saved tab ordering to be updated for group with id "
      << group_id_.AsLowercaseString();

  const SavedTabGroupModel* const model = service_->model();

  // Expect that a group with the saved id exists.
  const SavedTabGroup* const group = model->Get(group_id_);
  if (!group) {
    return false;
  }

  if (group->saved_tabs().size() != tab_ids_.size()) {
    return false;
  }

  // Expect that the group has the same tabs in the same order as `tab_ids_`.
  for (size_t i = 0; i < tab_ids_.size(); i++) {
    if (group->saved_tabs()[i].saved_tab_guid() != tab_ids_[i]) {
      return false;
    }
  }

  return true;
}

void TabOrderChecker::SavedTabGroupAddedFromSync(const base::Uuid& uuid) {
  CheckExitCondition();
}

void TabOrderChecker::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_uuid,
    const absl::optional<base::Uuid>& tab_uuid) {
  CheckExitCondition();
}
}  // namespace saved_tab_groups_helper
