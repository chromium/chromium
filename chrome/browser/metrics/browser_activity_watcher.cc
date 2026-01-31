// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/browser_activity_watcher.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

BrowserActivityWatcher::BrowserActivityWatcher(
    const base::RepeatingClosure& on_browser_activity)
    : on_browser_activity_(on_browser_activity) {
  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());

  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this](BrowserWindowInterface* browser) {
        if (TabStripModel* const tab_strip_model =
                browser->GetTabStripModel()) {
          tab_strip_model->AddObserver(this);
        }
        return true;
      });
}

BrowserActivityWatcher::~BrowserActivityWatcher() = default;

void BrowserActivityWatcher::OnBrowserCreated(BrowserWindowInterface* browser) {
  // TODO(crbug.com/452120900): TabStripModel auto-unregistered by dtor
  browser->GetTabStripModel()->AddObserver(this);
  on_browser_activity_.Run();
}

void BrowserActivityWatcher::OnBrowserClosed(BrowserWindowInterface* browser) {
  on_browser_activity_.Run();
}

void BrowserActivityWatcher::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  on_browser_activity_.Run();
}
