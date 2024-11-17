// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_TAB_UTILS_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_TAB_UTILS_H_

#include <optional>
#include <string_view>

#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_color.h"
#include "url/gurl.h"

// Helper platform-agnostic methods to support single-client sync tests using
// tabs.
namespace sync_test_tab_utils {

// Opens a new tab, loads the given `url` and waits until it's loaded. The new
// tab is appended at the end of the tab strip in either the active TabModel
// (Android) or the first browser (Desktop). Returns tab index if succeeds,
// nullopt otherwise.
std::optional<size_t> OpenNewTab(const GURL& url);

// Creates a tab group from a single tab at the given index. The group will have
// given `title` and `color`.
tab_groups::LocalTabGroupID CreateGroupFromTab(
    size_t tab_index,
    std::string_view title,
    tab_groups::TabGroupColorId color);

// Returns true if the tab group is open in the tab strip model.
bool IsTabGroupOpen(const tab_groups::LocalTabGroupID& local_group_id);

// Updates title and color for the given group (must be open in the tab strip).
void UpdateTabGroupVisualData(const tab_groups::LocalTabGroupID& local_group_id,
                              const std::string_view& title,
                              tab_groups::TabGroupColorId color);

}  // namespace sync_test_tab_utils

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_TAB_UTILS_H_
