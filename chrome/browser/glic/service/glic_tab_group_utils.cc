// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_tab_group_utils.h"

#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/gfx/range/range.h"

namespace glic {

BrowserWindowInterface* FindBrowserWithTabGroup(
    Profile* profile,
    tab_groups::TabGroupId group_id) {
  BrowserWindowInterface* found_window = nullptr;
  ProfileBrowserCollection::GetForProfile(profile)->ForEach(
      [&](BrowserWindowInterface* window) {
        if (auto* tab_list = TabListInterface::From(window)) {
          if (tab_list->ContainsTabGroup(group_id)) {
            found_window = window;
            return false;  // Found the group, stop iterating.
          }
        }
        return true;  // Continue iterating.
      });
  return found_window;
}

std::vector<tabs::TabInterface*> GetTabsInTabGroup(
    BrowserWindowInterface* window,
    tab_groups::TabGroupId group_id) {
  std::vector<tabs::TabInterface*> group_tabs;
  auto* tab_list = TabListInterface::From(window);
  if (!tab_list) {
    return group_tabs;
  }
  gfx::Range range = tab_list->GetTabGroupTabIndices(group_id);
  if (range.is_empty()) {
    return group_tabs;
  }
  for (uint32_t i = range.start(); i < range.end(); ++i) {
    if (tabs::TabInterface* tab = tab_list->GetTab(i)) {
      group_tabs.push_back(tab);
    }
  }
  return group_tabs;
}

void EnsureTabInGroup(tabs::TabInterface* tab,
                      tab_groups::TabGroupId group_id) {
  if (tab->GetGroup() == group_id) {
    return;
  }
  BrowserWindowInterface* window = tab->GetBrowserWindowInterface();
  if (!window) {
    return;
  }
  if (TabListInterface* tab_list = TabListInterface::From(window)) {
    tab_list->AddTabsToGroup(group_id, {tab->GetHandle()});
  }
}

}  // namespace glic
