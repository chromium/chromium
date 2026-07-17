// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_TAB_GROUP_STORE_TAB_GROUP_STORE_H_
#define CHROME_BROWSER_CONTEXT_HUB_TAB_GROUP_STORE_TAB_GROUP_STORE_H_

#include <vector>

#include "base/functional/callback.h"

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
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_TAB_GROUP_STORE_TAB_GROUP_STORE_H_
