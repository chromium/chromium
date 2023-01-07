// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_AUTO_OPENER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_AUTO_OPENER_H_

#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class DevToolsAutoOpener : public TabStripModelObserver {
 public:
  DevToolsAutoOpener();

  DevToolsAutoOpener(const DevToolsAutoOpener&) = delete;
  DevToolsAutoOpener& operator=(const DevToolsAutoOpener&) = delete;

  ~DevToolsAutoOpener() override;

 private:
  // TabStripModelObserver overrides.
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  BrowserTabStripTracker browser_tab_strip_tracker_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_AUTO_OPENER_H_
