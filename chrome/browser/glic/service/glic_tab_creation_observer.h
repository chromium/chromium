// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_TAB_CREATION_OBSERVER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_TAB_CREATION_OBSERVER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/base/page_transition_types.h"

class Profile;
class TabStripModel;
class BrowserCollection;
class BrowserWindowInterface;

// GlicTabCreationObserver monitors tab creation events across all browser
// windows for a specific profile. It filters for tabs created by explicit
// user actions (e.g., Ctrl+T, '+' button) and identifies the new tab
// and the previously active tab.
class GlicTabCreationObserver : public BrowserCollectionObserver,
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

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  raw_ptr<Profile> profile_;
  TabCreatedCallback callback_;
  base::ScopedObservation<BrowserCollection, BrowserCollectionObserver>
      browser_observation_{this};
};

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_TAB_CREATION_OBSERVER_H_
