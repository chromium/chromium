// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_SOURCE_OBSERVER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_SOURCE_OBSERVER_H_

#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

namespace content {
class WebContents;
}

class TabStripModel;
class Profile;

namespace glic {

class GlicWindowController;

class GlicTabSourceObserver : public TabStripModelObserver,
                              public BrowserListObserver {
 public:
  explicit GlicTabSourceObserver(GlicWindowController* coordinator,
                                 Profile* profile);
  ~GlicTabSourceObserver() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  void MaybeAddSidePanel(tabs::TabInterface* tab,
                         content::WebContents* web_contents);

  raw_ptr<GlicWindowController> coordinator_;
  raw_ptr<Profile> profile_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_SOURCE_OBSERVER_H_
