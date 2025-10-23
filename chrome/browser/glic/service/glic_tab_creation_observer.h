// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_TAB_CREATION_OBSERVER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_TAB_CREATION_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/base/page_transition_types.h"

class Profile;
class Browser;
class TabStripModel;

// GlicTabCreationObserver monitors tab creation events across all browser
// windows for a specific profile. It filters for tabs created by explicit
// user actions (e.g., Ctrl+T, '+' button) and identifies the new tab
// and the previously active tab.
class GlicTabCreationObserver : public BrowserListObserver,
                                public TabStripModelObserver {
 public:
  // Callback to be called when a new tab is created. This will only be called
  // when the new tab is created via the plus button or Ctrl+T.
  using TabCreatedCallback =
      base::RepeatingCallback<void(tabs::TabInterface& old_tab,
                                   tabs::TabInterface& new_tab)>;

  explicit GlicTabCreationObserver(Profile* profile,
                                   TabCreatedCallback callback);
  ~GlicTabCreationObserver() override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  raw_ptr<Profile> profile_;
  TabCreatedCallback callback_;
};

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_TAB_CREATION_OBSERVER_H_
