// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_TAB_GROUP_STORE_TAB_GROUP_ENTRY_H_
#define CHROME_BROWSER_CONTEXT_HUB_TAB_GROUP_STORE_TAB_GROUP_ENTRY_H_

#include <cstdint>
#include <string>
#include <vector>

namespace context_hub {

// Represents stored tab group data within Context Hub.
struct TabGroupEntry {
  // Key identifying the tab group, formatted as "group_<int>".
  std::string id;

  // Descriptive label for tabs in the group.
  std::string label;

  // IDs of tabs belonging to this group.
  std::vector<int64_t> tab_ids;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_TAB_GROUP_STORE_TAB_GROUP_ENTRY_H_
