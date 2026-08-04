// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_TAB_GROUP_UTILS_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_TAB_GROUP_UTILS_H_

#include <vector>

#include "components/tab_groups/tab_group_id.h"

class BrowserWindowInterface;
class Profile;

namespace tabs {
class TabInterface;
}

namespace glic {

BrowserWindowInterface* FindBrowserWithTabGroup(
    Profile* profile,
    tab_groups::TabGroupId group_id);

std::vector<tabs::TabInterface*> GetTabsInTabGroup(
    BrowserWindowInterface* window,
    tab_groups::TabGroupId group_id);

std::vector<tabs::TabInterface*> GetTabsInTabGroup(
    Profile* profile,
    tab_groups::TabGroupId group_id);

tabs::TabInterface* GetGlicTabInGroup(Profile* profile,
                                      tab_groups::TabGroupId group_id);

tabs::TabInterface* CreatePlaceholderTabInGroup(BrowserWindowInterface* window,
                                                tab_groups::TabGroupId group_id,
                                                int index);

void EnsureTabInGroup(tabs::TabInterface* tab, tab_groups::TabGroupId group_id);
void EnsureTabNotInGroup(tabs::TabInterface* tab,
                         tab_groups::TabGroupId group_id);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_TAB_GROUP_UTILS_H_
