// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_window_util.h"

#include <vector>

#include "base/containers/contains.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "components/tabs/public/tab_interface.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions::browser_window_util {

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
    if (base::Contains(all_tabs, tab)) {
      return browser;  // Found it!
    }
  }

  return nullptr;
}

}  // namespace extensions::browser_window_util
