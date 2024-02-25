// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_BROWSER_TO_TAB_LIST_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_BROWSER_TO_TAB_LIST_H_

#include <list>

#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "ui/aura/window.h"

namespace aura {
class Window;
}

namespace apps {

// BrowserToTabList saves the map from browser windows to tab instance ids.
class BrowserToTabList {
 public:
  BrowserToTabList();
  ~BrowserToTabList();

  BrowserToTabList(const BrowserToTabList&) = delete;
  BrowserToTabList& operator=(const BrowserToTabList&) = delete;

  // Returns true if the browser with `browser_window` has activated tabs.
  // Otherwise, returns false.
  bool HasActivatedTab(const aura::Window* browser_window);

  // Returns the active tab's app id if the browser with `browser_window` has
  // activated tab for an app. Otherwise, returns an empty string.
  std::string GetActivatedTabAppId(const aura::Window* browser_window);

  // Returns the browser window for `tab_id`.
  aura::Window* GetBrowserWindow(const base::UnguessableToken& tab_id) const;

  // Adds `browser_window`, `tab_id`, `tab_add_id` to`active_browser_to_tabs_`.
  void AddActivatedTab(aura::Window* browser_window,
                       const base::UnguessableToken& tab_id,
                       const std::string& app_id);

  // Removes `tab_id` from `active_browser_to_tabs_`.
  void RemoveActivatedTab(const base::UnguessableToken& tab_id);

 private:
  struct BrowserToTab {
    BrowserToTab(aura::Window* browser_window,
                 const base::UnguessableToken& tab_id,
                 const std::string& app_id);
    raw_ptr<aura::Window> browser_window;
    base::UnguessableToken tab_id;
    std::string app_id;
  };

  // Stores the list of browser-tab instance id pairs.
  std::list<BrowserToTab> active_browsers_to_tabs_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_BROWSER_TO_TAB_LIST_H_
