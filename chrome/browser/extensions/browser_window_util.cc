// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_window_util.h"

#include <algorithm>
#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "components/tabs/public/tab_interface.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions::browser_window_util {

namespace {

bool BrowserMatchesHelper(BrowserWindowInterface& browser,
                          Profile& profile,
                          bool include_incognito_or_parent,
                          bool restrict_to_normal_browsers,
                          bool restrict_to_current_workspace) {
  if (browser.GetProfile() != &profile) {
    if (!include_incognito_or_parent ||
        !profile.IsSameOrParent(browser.GetProfile())) {
      return false;
    }
  }

  if (restrict_to_normal_browsers &&
      browser.GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (restrict_to_current_workspace) {
    Browser* browser_for_migration = browser.GetBrowserForMigrationOnly();
    if (!browser_for_migration->window() ||
        !browser_for_migration->window()->IsOnCurrentWorkspace()) {
      return false;
    }
  }
#endif

  return true;
}

}  // namespace

BrowserWindowInterface* GetBrowserForTabContents(
    content::WebContents& tab_contents) {
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(&tab_contents);
  if (!tab) {
    return nullptr;
  }

  std::vector<BrowserWindowInterface*> all_browsers =
      GetAllBrowserWindowInterfaces();
  for (auto* browser : all_browsers) {
    TabListInterface* tab_list = TabListInterface::From(browser);
    if (!tab_list) {
      continue;
    }
    std::vector<tabs::TabInterface*> all_tabs = tab_list->GetAllTabs();
    if (std::ranges::contains(all_tabs, tab)) {
      return browser;  // Found it!
    }
  }

  return nullptr;
}

BrowserWindowInterface* GetLastActiveBrowserWithProfile(
    Profile& profile,
    bool include_incognito_or_parent) {
  BrowserWindowInterface* last_active_browser = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        bool browser_matches =
            BrowserMatchesHelper(*browser, profile, include_incognito_or_parent,
                                 /*restrict_to_normal_browsers=*/false,
                                 /*restrict_to_current_workspace=*/false);
        if (browser_matches) {
          last_active_browser = browser;
          return false;  // Stop iterating.
        }
        return true;  // Continue iterating.
      });

  return last_active_browser;
}

BrowserWindowInterface* GetLastActiveNormalBrowserWithProfile(
    Profile& profile,
    bool include_incognito_or_parent) {
  BrowserWindowInterface* last_active_browser = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        bool browser_matches =
            BrowserMatchesHelper(*browser, profile, include_incognito_or_parent,
                                 /*restrict_to_normal_browsers=*/true,
                                 /*restrict_to_current_workspace=*/true);
        if (browser_matches) {
          last_active_browser = browser;
          return false;  // Stop iterating.
        }
        return true;  // Continue iterating.
      });

  return last_active_browser;
}

}  // namespace extensions::browser_window_util
