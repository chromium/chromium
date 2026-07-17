// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/tab_group_store/in_memory_tab_group_store.h"

#include <utility>

#include "base/containers/adapters.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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
    if (group.id.empty()) {
      group.id =
          base::StrCat({"group_", base::NumberToString(next_group_id_++)});
    }
    groups_.Put(group.id, std::move(group));
  }
  if (callback) {
    std::move(callback).Run();
  }
}

void InMemoryTabGroupStore::DeleteAllGroups(OperationCallback callback) {
  groups_.Clear();
  next_group_id_ = 1;
  if (callback) {
    std::move(callback).Run();
  }
}

}  // namespace context_hub
