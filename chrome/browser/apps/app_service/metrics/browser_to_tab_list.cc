// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/browser_to_tab_list.h"

#include "ui/aura/window.h"

namespace apps {

BrowserToTabList::BrowserToTabList() = default;

BrowserToTabList::~BrowserToTabList() = default;

BrowserToTabList::BrowserToTab::BrowserToTab(
    aura::Window* browser_window,
    const base::UnguessableToken& tab_id,
    const std::string& app_id)
    : browser_window(browser_window), tab_id(tab_id), app_id(app_id) {}

bool BrowserToTabList::HasActivatedTab(const aura::Window* browser_window) {
  for (const auto& it : active_browsers_to_tabs_) {
    if (it.browser_window == browser_window) {
      return true;
    }
  }
  return false;
}

std::string BrowserToTabList::GetActivatedTabAppId(
    const aura::Window* browser_window) {
  for (const auto& it : active_browsers_to_tabs_) {
    if (it.browser_window == browser_window) {
      return it.app_id;
    }
  }
  return std::string();
}

aura::Window* BrowserToTabList::GetBrowserWindow(
    const base::UnguessableToken& tab_id) const {
  for (const auto& it : active_browsers_to_tabs_) {
    if (it.tab_id == tab_id) {
      return it.browser_window;
    }
  }
  return nullptr;
}

void BrowserToTabList::AddActivatedTab(aura::Window* browser_window,
                                       const base::UnguessableToken& tab_id,
                                       const std::string& app_id) {
  bool found = false;
  for (const auto& it : active_browsers_to_tabs_) {
    if (it.browser_window == browser_window && it.tab_id == tab_id) {
      found = true;
      break;
    }
  }

  if (!found) {
    active_browsers_to_tabs_.emplace_back(browser_window, tab_id, app_id);
  }
}

void BrowserToTabList::RemoveActivatedTab(
    const base::UnguessableToken& tab_id) {
  active_browsers_to_tabs_.remove_if(
      [&](const BrowserToTab& item) { return item.tab_id == tab_id; });
}

}  // namespace apps
