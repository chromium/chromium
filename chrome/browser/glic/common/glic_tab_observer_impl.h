// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_COMMON_GLIC_TAB_OBSERVER_IMPL_H_
#define CHROME_BROWSER_GLIC_COMMON_GLIC_TAB_OBSERVER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/common/glic_tab_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class BrowserCollection;
class BrowserWindowInterface;
class TabStripModel;

// Non-Android implementation of GlicTabObserver.
class GlicTabObserverImpl : public GlicTabObserver,
                            public BrowserCollectionObserver,
                            public TabStripModelObserver {
 public:
  GlicTabObserverImpl(Profile* profile, EventCallback callback);
  ~GlicTabObserverImpl() override;

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabChangedAt(tabs::TabInterface* tab,
                      int index,
                      TabChangeType change_type) override;

 private:
  raw_ptr<Profile> profile_;
  EventCallback callback_;
  base::ScopedObservation<BrowserCollection, BrowserCollectionObserver>
      browser_observation_{this};

  TabCreationType DetermineTabCreationType(tabs::TabInterface* new_tab);
};

#endif  // CHROME_BROWSER_GLIC_COMMON_GLIC_TAB_OBSERVER_IMPL_H_
