// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_GROUP_SYNC_TAB_GROUP_SYNC_UTILS_H_
#define CHROME_BROWSER_TAB_GROUP_SYNC_TAB_GROUP_SYNC_UTILS_H_

#include "url/gurl.h"

namespace content {
class NavigationHandle;
}

namespace tab_groups {

class TabGroupSyncUtils {
 public:
  // Whether the destination URL from a NavigationHandle can be saved and
  // can be reloaded later on another machine.
  static bool IsSaveableNavigation(
      content::NavigationHandle* navigation_handle);

  // Returns whether the tab's URL is viable for saving in a saved tab
  // group.
  static bool IsURLValidForSavedTabGroups(const GURL& gurl);
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_TAB_GROUP_SYNC_TAB_GROUP_SYNC_UTILS_H_
