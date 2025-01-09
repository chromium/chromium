// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_GROUP_SYNC_TAB_GROUP_TRIAL_H_
#define CHROME_BROWSER_TAB_GROUP_SYNC_TAB_GROUP_TRIAL_H_

#include <string_view>

namespace tab_groups {

class TabGroupTrial {
 public:
  // Called to report synthetic field trial on whether tabgroup sync is enabled.
  static void OnTabGroupSyncEnabled(bool enabled);

  // Called to report synthetic field trial on whether the client had a sync
  // tabgroup.
  static void OnHadSyncedTabGroup(bool had_synced_group);

  // Called to report synthetic field trial on whether the client had a shared
  // tabgroup.
  static void OnHadSharedTabGroup(bool had_shared_group);

 private:
  static void RegisterFieldTrial(std::string_view trial_name,
                                 std::string_view group_name);
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_TAB_GROUP_SYNC_TAB_GROUP_TRIAL_H_
