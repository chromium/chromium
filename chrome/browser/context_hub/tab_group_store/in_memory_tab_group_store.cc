// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/tab_group_store/in_memory_tab_group_store.h"

#include <algorithm>
#include <utility>

#include "base/containers/adapters.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/context_hub/features.h"

namespace context_hub {

InMemoryTabGroupStore::InMemoryTabGroupStore()
    : groups_(features::kMaxTabGroups.Get()) {}

InMemoryTabGroupStore::~InMemoryTabGroupStore() = default;

void InMemoryTabGroupStore::GetAllGroups(
    GetAllGroupsCallback callback) const {
  std::vector<TabGroupEntry> result;
  result.reserve(groups_.size());
  for (const auto& [id, group] : base::Reversed(groups_)) {
    result.push_back(group);
  }
  if (callback) {
    std::move(callback).Run(std::move(result));
  }
}

void InMemoryTabGroupStore::AddAllGroups(
    std::vector<TabGroupEntry> groups,
    OperationCallback callback) {
  for (auto& group : groups) {
    AddOrUpdateGroup(std::move(group), base::DoNothing());
  }
  if (callback) {
    std::move(callback).Run();
  }
}

void InMemoryTabGroupStore::DeleteAllGroups(OperationCallback callback) {
  groups_.Clear();
  if (callback) {
    std::move(callback).Run();
  }
}

void InMemoryTabGroupStore::AddOrUpdateGroup(TabGroupEntry group,
                                              OperationCallback callback) {
  base::ScopedClosureRunner runner(std::move(callback));
  base::Time now = base::Time::Now();
  if (group.id.empty()) {
    group.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  }
  auto existing_it = groups_.Peek(group.id);
  if (group.created_timestamp.is_null()) {
    group.created_timestamp =
        (existing_it != groups_.end()) ? existing_it->second.created_timestamp : now;
  }
  if (group.last_accessed_timestamp.is_null()) {
    group.last_accessed_timestamp = now;
  }

  // Deduplicate group's tab_ids while preserving original insertion order.
  std::vector<int64_t> unique_tab_ids;
  for (int64_t tab_id : group.tab_ids) {
    if (std::ranges::find(unique_tab_ids, tab_id) == unique_tab_ids.end()) {
      unique_tab_ids.push_back(tab_id);
    }
  }
  group.tab_ids = std::move(unique_tab_ids);

  // Remove group's tab_ids from any OTHER existing groups to ensure a tab
  // belongs to only one group.
  PruneTabsFromOtherGroups(group.tab_ids, group.id);

  if (!group.tab_ids.empty()) {
    groups_.Put(group.id, std::move(group));
  } else {
    DeleteGroup(group.id, base::DoNothing());
  }
}

void InMemoryTabGroupStore::DeleteGroup(const std::string& group_id,
                                         OperationCallback callback) {
  auto it = groups_.Peek(group_id);
  if (it != groups_.end()) {
    groups_.Erase(it);
  }
  if (callback) {
    std::move(callback).Run();
  }
}

void InMemoryTabGroupStore::PruneTabFromAllGroups(
    int64_t tab_id,
    OperationCallback callback) {
  std::vector<std::string> groups_to_delete;
  for (auto& [id, group] : groups_) {
    auto& tab_ids = group.tab_ids;
    if (std::erase(tab_ids, tab_id) > 0) {
      if (tab_ids.empty()) {
        groups_to_delete.push_back(id);
      }
      break;
    }
  }
  for (const std::string& group_id : groups_to_delete) {
    DeleteGroup(group_id, base::DoNothing());
  }
  if (callback) {
    std::move(callback).Run();
  }
}

void InMemoryTabGroupStore::AddTabToGroup(const std::string& group_id,
                                           int64_t tab_id,
                                           OperationCallback callback) {
  base::ScopedClosureRunner runner(std::move(callback));

  // Check and remove tab_id from any OTHER existing groups to ensure a tab
  // belongs to only one group.
  PruneTabsFromOtherGroups({tab_id}, group_id);

  auto it = groups_.Get(group_id);
  if (it == groups_.end()) {
    return;
  }
  TabGroupEntry& entry = it->second;
  if (std::ranges::find(entry.tab_ids, tab_id) == entry.tab_ids.end()) {
    entry.tab_ids.push_back(tab_id);
  }
  entry.last_accessed_timestamp = base::Time::Now();
}

void InMemoryTabGroupStore::UpdateGroupTimestampForTab(
    int64_t tab_id,
    base::Time timestamp,
    OperationCallback callback) {
  base::ScopedClosureRunner runner(std::move(callback));

  for (const auto& [id, group] : groups_) {
    if (std::ranges::find(group.tab_ids, tab_id) != group.tab_ids.end()) {
      auto it = groups_.Get(id);
      if (it != groups_.end()) {
        it->second.last_accessed_timestamp = timestamp;
      }
      break;
    }
  }
}

void InMemoryTabGroupStore::PruneTabsFromOtherGroups(
    base::span<const int64_t> tab_ids,
    const std::string& excluded_group_id) {
  std::vector<std::string> groups_to_delete;
  for (auto& [id, group] : groups_) {
    if (id != excluded_group_id) {
      bool erased = false;
      for (int64_t tab_id : tab_ids) {
        if (std::erase(group.tab_ids, tab_id) > 0) {
          erased = true;
        }
      }
      if (erased && group.tab_ids.empty()) {
        groups_to_delete.push_back(id);
      }
    }
  }
  for (const std::string& delete_id : groups_to_delete) {
    DeleteGroup(delete_id, base::DoNothing());
  }
}

}  // namespace context_hub
