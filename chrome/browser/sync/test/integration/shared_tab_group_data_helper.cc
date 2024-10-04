// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/shared_tab_group_data_helper.h"

#include "base/ranges/algorithm.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"

namespace tab_groups {

namespace {

std::vector<sync_pb::SharedTabGroupDataSpecifics>
SyncEntitiesToSharedTabGroupSpecifics(
    std::vector<sync_pb::SyncEntity> entities) {
  std::vector<sync_pb::SharedTabGroupDataSpecifics> shared_tab_groups;
  for (sync_pb::SyncEntity& entity : entities) {
    CHECK(entity.specifics().has_shared_tab_group_data());
    sync_pb::SharedTabGroupDataSpecifics specifics;
    specifics.Swap(entity.mutable_specifics()->mutable_shared_tab_group_data());
    shared_tab_groups.push_back(std::move(specifics));
  }
  return shared_tab_groups;
}

bool CompareSavedTabGroups(SavedTabGroup& a, SavedTabGroup& b) {
  return a.saved_guid() < b.saved_guid();
}

// Compares the given models to contain exactly the same shared tab groups. Note
// that the order of groups is not verified while the order of tabs within
// groups is important.
bool AreServicesEqual(TabGroupSyncService* service_1,
                      TabGroupSyncService* service_2,
                      std::ostream* os) {
  std::vector<SavedTabGroup> shared_tab_groups_1 = service_1->GetAllGroups();
  std::vector<SavedTabGroup> shared_tab_groups_2 = service_2->GetAllGroups();
  if (shared_tab_groups_1.size() != shared_tab_groups_2.size()) {
    *os << "Model 1 size: " << shared_tab_groups_1.size()
        << ", model 2 size: " << shared_tab_groups_2.size();
    return false;
  }

  base::ranges::sort(shared_tab_groups_1, &CompareSavedTabGroups);
  base::ranges::sort(shared_tab_groups_2, &CompareSavedTabGroups);

  for (size_t group_index = 0; group_index < shared_tab_groups_1.size();
       ++group_index) {
    const SavedTabGroup& group_1 = shared_tab_groups_1[group_index];
    const SavedTabGroup& group_2 = shared_tab_groups_2[group_index];
    if (group_1.saved_guid() != group_2.saved_guid()) {
      *os << "Different groups, model 1: " << group_1.saved_guid()
          << ", model 2: " << group_2.saved_guid();
      return false;
    }
    if (group_1.title() != group_2.title()) {
      *os << "Different group titles, model 1: " << group_1.title()
          << ", model 2: " << group_2.title();
      return false;
    }
    if (group_1.color() != group_2.color()) {
      *os << "Different group colors, model 1: "
          << static_cast<int>(group_1.color())
          << ", model 2: " << static_cast<int>(group_2.color());
      return false;
    }
    if (group_1.saved_tabs().size() != group_2.saved_tabs().size()) {
      *os << "Different group sizes, model 1: " << group_1.saved_tabs().size()
          << ", model 2: " << group_2.saved_tabs().size();
      return false;
    }

    for (size_t tab_index = 0; tab_index < group_1.saved_tabs().size();
         ++tab_index) {
      const SavedTabGroupTab& tab_1 = group_1.saved_tabs()[tab_index];
      const SavedTabGroupTab& tab_2 = group_2.saved_tabs()[tab_index];

      if (tab_1.saved_tab_guid() != tab_2.saved_tab_guid()) {
        *os << "Different tabs, model 1: " << tab_1.saved_tab_guid()
            << ", model 2: " << tab_2.saved_tab_guid();
        return false;
      }
      if (tab_1.title() != tab_2.title()) {
        *os << "Different tab titles, model 1: " << tab_1.title()
            << ", model 2: " << tab_2.title();
        return false;
      }
      if (tab_1.url() != tab_2.url()) {
        *os << "Different tab URLs, model 1: " << tab_1.url()
            << ", model 2: " << tab_2.url();
        return false;
      }
    }
  }

  return true;
}

}  // namespace

ServerSharedTabGroupMatchChecker::ServerSharedTabGroupMatchChecker(
    const Matcher& matcher)
    : matcher_(matcher) {}

ServerSharedTabGroupMatchChecker::~ServerSharedTabGroupMatchChecker() = default;

bool ServerSharedTabGroupMatchChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for the tab groups committed to the server. ";

  std::vector<sync_pb::SharedTabGroupDataSpecifics> entities =
      SyncEntitiesToSharedTabGroupSpecifics(
          fake_server()->GetSyncEntitiesByDataType(
              syncer::SHARED_TAB_GROUP_DATA));

  testing::StringMatchResultListener result_listener;
  const bool matches =
      testing::ExplainMatchResult(matcher_, entities, &result_listener);
  *os << result_listener.str();
  return matches;
}

SharedTabGroupsMatchChecker::SharedTabGroupsMatchChecker(
    TabGroupSyncService* service_1,
    TabGroupSyncService* service_2)
    : service_1_(service_1), service_2_(service_2) {
  observation_1_.Observe(service_1_.get());
  observation_2_.Observe(service_2_.get());
}

SharedTabGroupsMatchChecker::~SharedTabGroupsMatchChecker() = default;

bool SharedTabGroupsMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for shared tab group matching models. ";

  return AreServicesEqual(service_1_.get(), service_2_.get(), os);
}

void SharedTabGroupsMatchChecker::OnTabGroupAdded(const SavedTabGroup& group,
                                                  TriggerSource source) {
  CheckExitCondition();
}

void SharedTabGroupsMatchChecker::OnTabGroupUpdated(const SavedTabGroup& group,
                                                    TriggerSource source) {
  CheckExitCondition();
}

void SharedTabGroupsMatchChecker::OnTabGroupRemoved(
    const LocalTabGroupID& local_id,
    TriggerSource source) {
  CheckExitCondition();
}

void SharedTabGroupsMatchChecker::OnTabGroupRemoved(const base::Uuid& sync_id,
                                                    TriggerSource source) {
  CheckExitCondition();
}

}  // namespace tab_groups
