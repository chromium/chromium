// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_BROWSER_ACTIVITY_WATCHER_H_
#define CHROME_BROWSER_METRICS_BROWSER_ACTIVITY_WATCHER_H_

#include "base/functional/callback.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

// Helper that fires a callback every time a Browser object is added or removed
// from the BrowserList, or a browser's tab strip model is modified. This class
// primarily exists to encapsulate this behavior and reduce the number of
// platform-specific macros as Android doesn't use BrowserList or TabStripModel.
class BrowserActivityWatcher : public BrowserListObserver,
                               public TabStripModelObserver {
 public:
  explicit BrowserActivityWatcher(
      const base::RepeatingClosure& on_browser_activity);

  BrowserActivityWatcher(const BrowserActivityWatcher&) = delete;
  BrowserActivityWatcher& operator=(const BrowserActivityWatcher&) = delete;

  ~BrowserActivityWatcher() override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  base::RepeatingClosure on_browser_activity_;
};

#endif  // CHROME_BROWSER_METRICS_BROWSER_ACTIVITY_WATCHER_H_
