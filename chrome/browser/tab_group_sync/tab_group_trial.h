// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_GROUP_SYNC_TAB_GROUP_TRIAL_H_
#define CHROME_BROWSER_TAB_GROUP_SYNC_TAB_GROUP_TRIAL_H_

namespace tab_groups {

class TabGroupTrial {
 public:
  // Called to report synthetic field trial on whether tabgroup sync is enabled.
  static void OnTabgroupSyncEnabled(bool enabled);
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_TAB_GROUP_SYNC_TAB_GROUP_TRIAL_H_
