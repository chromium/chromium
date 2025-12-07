// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/browser_activity_watcher.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

BrowserActivityWatcher::BrowserActivityWatcher(
    const base::RepeatingClosure& on_browser_activity)
    : on_browser_activity_(on_browser_activity) {
  BrowserList::AddObserver(this);

  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this](BrowserWindowInterface* browser) {
        if (TabStripModel* const tab_strip_model =
                browser->GetTabStripModel()) {
          tab_strip_model->AddObserver(this);
        }
        return true;
      });
}

BrowserActivityWatcher::~BrowserActivityWatcher() {
  BrowserList::RemoveObserver(this);
}

void BrowserActivityWatcher::OnBrowserAdded(Browser* browser) {
  if (browser->tab_strip_model())
    browser->tab_strip_model()->AddObserver(this);

  on_browser_activity_.Run();
}

void BrowserActivityWatcher::OnBrowserRemoved(Browser* browser) {
  if (browser->tab_strip_model())
    browser->tab_strip_model()->RemoveObserver(this);

  on_browser_activity_.Run();
}

void BrowserActivityWatcher::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  on_browser_activity_.Run();
}
