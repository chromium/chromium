// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_TAB_GROUP_STORE_TAB_GROUP_STORE_H_
#define CHROME_BROWSER_CONTEXT_HUB_TAB_GROUP_STORE_TAB_GROUP_STORE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"

namespace context_hub {

struct TabGroupEntry;

// Abstract interface for tab group store in Context Hub.
class TabGroupStore {
 public:
  virtual ~TabGroupStore() = default;

  using GetAllGroupsCallback =
      base::OnceCallback<void(std::vector<TabGroupEntry>)>;
  // Retrieves all stored tab groups via callback.
  virtual void GetAllGroups(GetAllGroupsCallback callback) const = 0;

  using OperationCallback = base::OnceClosure;
  // Appends or updates tab groups in the store.
  virtual void AddAllGroups(std::vector<TabGroupEntry> groups,
                            OperationCallback callback) = 0;

  // Purges all tab groups from store.
  virtual void DeleteAllGroups(OperationCallback callback) = 0;

  // Appends or updates a single tab group in the store.
  virtual void AddOrUpdateGroup(TabGroupEntry group,
                                OperationCallback callback) = 0;

  // Deletes a specific tab group by its ID.
  virtual void DeleteGroup(const std::string& group_id,
                           OperationCallback callback) = 0;

  // Removes `tab_id` from ALL stored groups. Automatically deletes any group
  // whose remaining tab count drops to 0 (empty group).
  virtual void PruneTabFromAllGroups(int64_t tab_id,
                                     OperationCallback callback) = 0;

  // Adds `tab_id` to `group_id` (Manual grouping by user).
  virtual void AddTabToGroup(const std::string& group_id,
                             int64_t tab_id,
                             OperationCallback callback) = 0;

  // Updates the last_accessed_timestamp for any group containing `tab_id`.
  virtual void UpdateGroupTimestampForTab(int64_t tab_id,
                                          base::Time timestamp,
                                          OperationCallback callback) = 0;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_TAB_GROUP_STORE_TAB_GROUP_STORE_H_
