// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_TAB_GROUP_STORE_IN_MEMORY_TAB_GROUP_STORE_H_
#define CHROME_BROWSER_CONTEXT_HUB_TAB_GROUP_STORE_IN_MEMORY_TAB_GROUP_STORE_H_

#include <cstdint>
#include <string>

#include "base/containers/lru_cache.h"
#include "base/containers/span.h"
#include "chrome/browser/context_hub/tab_group_store/tab_group_entry.h"
#include "chrome/browser/context_hub/tab_group_store/tab_group_store.h"

namespace context_hub {

// In-memory implementation of TabGroupStore backed by base::LRUCache.
class InMemoryTabGroupStore : public TabGroupStore {
 public:
  InMemoryTabGroupStore();

  InMemoryTabGroupStore(const InMemoryTabGroupStore&) = delete;
  InMemoryTabGroupStore& operator=(const InMemoryTabGroupStore&) = delete;

  ~InMemoryTabGroupStore() override;

  void GetAllGroups(GetAllGroupsCallback callback) const override;
  void AddAllGroups(std::vector<TabGroupEntry> groups,
                    OperationCallback callback) override;
  void DeleteAllGroups(OperationCallback callback) override;
  void AddOrUpdateGroup(TabGroupEntry group,
                        OperationCallback callback) override;
  void DeleteGroup(const std::string& group_id,
                   OperationCallback callback) override;
  void PruneTabFromAllGroups(int64_t tab_id,
                             OperationCallback callback) override;
  void AddTabToGroup(const std::string& group_id,
                     int64_t tab_id,
                     OperationCallback callback) override;
  void UpdateGroupTimestampForTab(int64_t tab_id,
                                  base::Time timestamp,
                                  OperationCallback callback) override;

 private:
  // Defensive check utility to check and remove tab_ids from any OTHER existing
  // groups to ensure a tab belongs to only one group.
  void PruneTabsFromOtherGroups(base::span<const int64_t> tab_ids,
                                const std::string& excluded_group_id);

  base::LRUCache<std::string, TabGroupEntry> groups_;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_TAB_GROUP_STORE_IN_MEMORY_TAB_GROUP_STORE_H_
