// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/saved_tab_groups_helper.h"

#include <vector>

#include "base/uuid.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
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
    TabGroupSyncService* service,
    const base::Uuid& uuid)
    : uuid_(uuid), service_(service) {
  CHECK(service_);
  service_->AddObserver(this);
}

SavedTabOrGroupExistsChecker::~SavedTabOrGroupExistsChecker() {
  service_->RemoveObserver(this);
}

bool SavedTabOrGroupExistsChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for data for uuid '" + uuid_.AsLowercaseString() +
             "' to be added.";

  // Expect that `uuid_` exists in the SavedTabGroupModel.
  for (const SavedTabGroup& group : service_->GetAllGroups()) {
    if (group.saved_guid() == uuid_ || group.ContainsTab(uuid_)) {
      return true;
    }
  }

  return false;
}

void SavedTabOrGroupExistsChecker::OnTabGroupAdded(const SavedTabGroup& group,
                                                   TriggerSource source) {
  if (TriggerSource::LOCAL == source) {
    return;
  }

  CheckExitCondition();
}

void SavedTabOrGroupExistsChecker::OnTabGroupUpdated(const SavedTabGroup& group,
                                                     TriggerSource source) {
  if (TriggerSource::LOCAL == source) {
    return;
  }

  CheckExitCondition();
}

// ==========================================
// --- SavedTabOrGroupDoesNotExistChecker ---
// ==========================================
SavedTabOrGroupDoesNotExistChecker::SavedTabOrGroupDoesNotExistChecker(
    TabGroupSyncService* service,
    const base::Uuid& uuid)
    : uuid_(uuid), service_(service) {
  CHECK(service_);
  service_->AddObserver(this);
}

SavedTabOrGroupDoesNotExistChecker::~SavedTabOrGroupDoesNotExistChecker() {
  service_->RemoveObserver(this);
}

bool SavedTabOrGroupDoesNotExistChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for data for uuid '" + uuid_.AsLowercaseString() +
             "' to be deleted.";

  // Expect that `uuid_` does not exist in the SavedTabGroupModel.
  for (const SavedTabGroup& group : service_->GetAllGroups()) {
    if (group.saved_guid() == uuid_ || group.ContainsTab(uuid_)) {
      return false;
    }
  }

  return true;
}

void SavedTabOrGroupDoesNotExistChecker::OnTabGroupUpdated(
    const SavedTabGroup& group,
    TriggerSource source) {
  if (TriggerSource::LOCAL == source) {
    return;
  }

  CheckExitCondition();
}

void SavedTabOrGroupDoesNotExistChecker::OnTabGroupRemoved(
    const base::Uuid& sync_id,
    TriggerSource source) {
  if (TriggerSource::LOCAL == source) {
    return;
  }

  CheckExitCondition();
}

// ===================================
// --- SavedTabGroupMatchesChecker ---
// ===================================
SavedTabGroupMatchesChecker::SavedTabGroupMatchesChecker(
    TabGroupSyncService* service,
    SavedTabGroup group)
    : group_(group), service_(service) {
  CHECK(service_);
  service_->AddObserver(this);
}

SavedTabGroupMatchesChecker::~SavedTabGroupMatchesChecker() {
  service_->RemoveObserver(this);
}

bool SavedTabGroupMatchesChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for data for group with uuid '" +
             group_.saved_guid().AsLowercaseString() + "' to be updated.";

  // Expect that a group exists in the model with the same synced data as
  // `group_`.
  for (const SavedTabGroup& group : service_->GetAllGroups()) {
    if (group.IsSyncEquivalent(group_)) {
      return true;
    }
  }

  return false;
}

void SavedTabGroupMatchesChecker::OnTabGroupAdded(const SavedTabGroup& group,
                                                  TriggerSource source) {
  if (TriggerSource::LOCAL == source) {
    return;
  }
  CheckExitCondition();
}
void SavedTabGroupMatchesChecker::OnTabGroupUpdated(const SavedTabGroup& group,
                                                    TriggerSource source) {
  if (TriggerSource::LOCAL == source) {
    return;
  }
  CheckExitCondition();
}

// ==============================
// --- SavedTabMatchesChecker ---
// ==============================
SavedTabMatchesChecker::SavedTabMatchesChecker(TabGroupSyncService* service,
                                               SavedTabGroupTab tab)
    : tab_(tab), service_(service) {
  CHECK(service_);
  service_->AddObserver(this);
}

SavedTabMatchesChecker::~SavedTabMatchesChecker() {
  service_->RemoveObserver(this);
}

bool SavedTabMatchesChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for data for tab with uuid '" +
             tab_.saved_tab_guid().AsLowercaseString() + "' to be updated.";

  // Expect that a tab exists in the model with the same synced data as `tab_`.
  for (const SavedTabGroup& group : service_->GetAllGroups()) {
    for (const SavedTabGroupTab& tab : group.saved_tabs()) {
      if (tab.IsSyncEquivalent(tab_)) {
        return true;
      }
    }
  }

  return false;
}

void SavedTabMatchesChecker::OnTabGroupAdded(const SavedTabGroup& group,
                                             TriggerSource source) {
  if (TriggerSource::LOCAL == source) {
    return;
  }

  CheckExitCondition();
}

void SavedTabMatchesChecker::OnTabGroupUpdated(const SavedTabGroup& group,
                                               TriggerSource source) {
  if (TriggerSource::LOCAL == source) {
    return;
  }

  CheckExitCondition();
}

// =========================
// --- GroupOrderChecker ---
// =========================
GroupOrderChecker::GroupOrderChecker(TabGroupSyncService* service,
                                     std::vector<base::Uuid> group_ids)
    : group_ids_(group_ids), service_(service) {
  CHECK(service_);
  service_->AddObserver(this);
}

GroupOrderChecker::~GroupOrderChecker() {
  service_->RemoveObserver(this);
}

bool GroupOrderChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for saved group ordering to be updated.";

  if (service_->GetAllGroups().size() != group_ids_.size()) {
    return false;
  }

  // Expect that the model has the same groups in the same order as
  // `group_ids_`.
  for (size_t i = 0; i < group_ids_.size(); i++) {
    if (service_->GetAllGroups()[i].saved_guid() != group_ids_[i]) {
      return false;
    }
  }

  return true;
}

void GroupOrderChecker::OnTabGroupAdded(const SavedTabGroup& group,
                                        TriggerSource source) {
  if (TriggerSource::LOCAL == source) {
    return;
  }

  CheckExitCondition();
}

void GroupOrderChecker::OnTabGroupUpdated(const SavedTabGroup& group,
                                          TriggerSource source) {
  if (TriggerSource::LOCAL == source) {
    return;
  }

  CheckExitCondition();
}

void GroupOrderChecker::OnTabGroupRemoved(const base::Uuid& sync_id,
                                          TriggerSource source) {
  if (TriggerSource::LOCAL == source) {
    return;
  }

  CheckExitCondition();
}

// =======================
// --- TabOrderChecker ---
// =======================
TabOrderChecker::TabOrderChecker(TabGroupSyncService* service,
                                 base::Uuid group_id,
                                 std::vector<base::Uuid> tab_ids)
    : group_id_(group_id), tab_ids_(tab_ids), service_(service) {
  CHECK(service_);
  service_->AddObserver(this);
}

TabOrderChecker::~TabOrderChecker() {
  service_->RemoveObserver(this);
}

bool TabOrderChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for saved tab ordering to be updated for group with id "
      << group_id_.AsLowercaseString();

  // Expect that a group with the saved id exists.
  const std::optional<SavedTabGroup> group = service_->GetGroup(group_id_);
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

void TabOrderChecker::OnTabGroupAdded(const SavedTabGroup& group,
                                      TriggerSource source) {
  if (TriggerSource::LOCAL == source) {
    return;
  }

  CheckExitCondition();
}

void TabOrderChecker::OnTabGroupUpdated(const SavedTabGroup& group,
                                        TriggerSource source) {
  if (TriggerSource::LOCAL == source) {
    return;
  }

  CheckExitCondition();
}

// =======================================
// --- ServerSavedTabGroupMatchChecker ---
// =======================================
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
