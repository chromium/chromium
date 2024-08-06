// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/saved_tab_groups_helper.h"

#include <vector>

#include "base/uuid.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_model_observer.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"

namespace tab_groups {

namespace {

std::vector<sync_pb::SavedTabGroupSpecifics>
SyncEntitiesToSavedTabGroupSpecifics(
    std::vector<sync_pb::SyncEntity> entities) {
  std::vector<sync_pb::SavedTabGroupSpecifics> saved_tab_groups;
  for (sync_pb::SyncEntity& entity : entities) {
    CHECK(entity.specifics().has_saved_tab_group());
    sync_pb::SavedTabGroupSpecifics specifics;
    specifics.Swap(entity.mutable_specifics()->mutable_saved_tab_group());
    saved_tab_groups.push_back(std::move(specifics));
  }
  return saved_tab_groups;
}

}  // namespace

// ====================================
// --- SavedTabOrGroupExistsChecker ---
// ====================================
SavedTabOrGroupExistsChecker::SavedTabOrGroupExistsChecker(
    SavedTabGroupModel* model,
    const base::Uuid& uuid)
    : uuid_(uuid), model_(model) {
  CHECK(model_);
  model_->AddObserver(this);
}

SavedTabOrGroupExistsChecker::~SavedTabOrGroupExistsChecker() {
  model_->RemoveObserver(this);
}

bool SavedTabOrGroupExistsChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for data for uuid '" + uuid_.AsLowercaseString() +
             "' to be added.";

  // Expect that `uuid_` exists in the SavedTabGroupModel.
  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
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
    const std::optional<base::Uuid>& tab_uuid) {
  CheckExitCondition();
}

// ==========================================
// --- SavedTabOrGroupDoesNotExistChecker ---
// ==========================================
SavedTabOrGroupDoesNotExistChecker::SavedTabOrGroupDoesNotExistChecker(
    SavedTabGroupModel* model,
    const base::Uuid& uuid)
    : uuid_(uuid), model_(model) {
  CHECK(model_);
  model_->AddObserver(this);
}

SavedTabOrGroupDoesNotExistChecker::~SavedTabOrGroupDoesNotExistChecker() {
  model_->RemoveObserver(this);
}

bool SavedTabOrGroupDoesNotExistChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for data for uuid '" + uuid_.AsLowercaseString() +
             "' to be deleted.";

  // Expect that `uuid_` does not exist in the SavedTabGroupModel.
  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    if (group.saved_guid() == uuid_ || group.ContainsTab(uuid_)) {
      return false;
    }
  }

  return true;
}

void SavedTabOrGroupDoesNotExistChecker::SavedTabGroupRemovedFromSync(
    const SavedTabGroup& removed_group) {
  CheckExitCondition();
}

void SavedTabOrGroupDoesNotExistChecker::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_uuid,
    const std::optional<base::Uuid>& tab_uuid) {
  CheckExitCondition();
}

// ===================================
// --- SavedTabGroupMatchesChecker ---
// ===================================
SavedTabGroupMatchesChecker::SavedTabGroupMatchesChecker(
    SavedTabGroupModel* model,
    SavedTabGroup group)
    : group_(group), model_(model) {
  CHECK(model_);
  model_->AddObserver(this);
}

SavedTabGroupMatchesChecker::~SavedTabGroupMatchesChecker() {
  model_->RemoveObserver(this);
}

bool SavedTabGroupMatchesChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for data for group with uuid '" +
             group_.saved_guid().AsLowercaseString() + "' to be updated.";

  // Expect that a group exists in the model with the same synced data as
  // `group_`.
  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    if (group.IsSyncEquivalent(group_)) {
      return true;
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
    const std::optional<base::Uuid>& tab_uuid) {
  CheckExitCondition();
}

// ==============================
// --- SavedTabMatchesChecker ---
// ==============================
SavedTabMatchesChecker::SavedTabMatchesChecker(SavedTabGroupModel* model,
                                               SavedTabGroupTab tab)
    : tab_(tab), model_(model) {
  CHECK(model_);
  model_->AddObserver(this);
}

SavedTabMatchesChecker::~SavedTabMatchesChecker() {
  model_->RemoveObserver(this);
}

bool SavedTabMatchesChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for data for tab with uuid '" +
             tab_.saved_tab_guid().AsLowercaseString() + "' to be updated.";

  // Expect that a tab exists in the model with the same synced data as `tab_`.
  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    for (const SavedTabGroupTab& tab : group.saved_tabs()) {
      if (tab.IsSyncEquivalent(tab_)) {
        return true;
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
    const std::optional<base::Uuid>& tab_uuid) {
  CheckExitCondition();
}

// =========================
// --- GroupOrderChecker ---
// =========================
GroupOrderChecker::GroupOrderChecker(SavedTabGroupModel* model,
                                     std::vector<base::Uuid> group_ids)
    : group_ids_(group_ids), model_(model) {
  CHECK(model_);
  model_->AddObserver(this);
}

GroupOrderChecker::~GroupOrderChecker() {
  model_->RemoveObserver(this);
}

bool GroupOrderChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for saved group ordering to be updated.";

  if (model_->saved_tab_groups().size() != group_ids_.size()) {
    return false;
  }

  // Expect that the model has the same groups in the same order as
  // `group_ids_`.
  for (size_t i = 0; i < group_ids_.size(); i++) {
    if (model_->saved_tab_groups()[i].saved_guid() != group_ids_[i]) {
      return false;
    }
  }

  return true;
}

void GroupOrderChecker::SavedTabGroupAddedFromSync(const base::Uuid& uuid) {
  CheckExitCondition();
}

void GroupOrderChecker::SavedTabGroupRemovedFromSync(
    const SavedTabGroup& removed_group) {
  CheckExitCondition();
}

void GroupOrderChecker::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_uuid,
    const std::optional<base::Uuid>& tab_uuid) {
  CheckExitCondition();
}

// =======================
// --- TabOrderChecker ---
// =======================
TabOrderChecker::TabOrderChecker(SavedTabGroupModel* model,
                                 base::Uuid group_id,
                                 std::vector<base::Uuid> tab_ids)
    : group_id_(group_id), tab_ids_(tab_ids), model_(model) {
  CHECK(model_);
  model_->AddObserver(this);
}

TabOrderChecker::~TabOrderChecker() {
  model_->RemoveObserver(this);
}

bool TabOrderChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for saved tab ordering to be updated for group with id "
      << group_id_.AsLowercaseString();

  // Expect that a group with the saved id exists.
  const SavedTabGroup* const group = model_->Get(group_id_);
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
    const std::optional<base::Uuid>& tab_uuid) {
  CheckExitCondition();
}

ServerSavedTabGroupMatchChecker::ServerSavedTabGroupMatchChecker(
    const Matcher& matcher)
    : matcher_(matcher) {}

ServerSavedTabGroupMatchChecker::~ServerSavedTabGroupMatchChecker() = default;

bool ServerSavedTabGroupMatchChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for the tab groups committed to the server. ";

  std::vector<sync_pb::SavedTabGroupSpecifics> entities =
      SyncEntitiesToSavedTabGroupSpecifics(
          fake_server()->GetSyncEntitiesByDataType(syncer::SAVED_TAB_GROUP));

  testing::StringMatchResultListener result_listener;
  const bool matches =
      testing::ExplainMatchResult(matcher_, entities, &result_listener);
  *os << result_listener.str();
  return matches;
}

}  // namespace tab_groups
