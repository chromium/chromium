// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_BUTTER_BAR_H_
#define CHROME_BROWSER_LACROS_LACROS_BUTTER_BAR_H_

#include "chrome/browser/ui/browser_list_observer.h"

class Browser;

namespace content {
class WebContents;
}  // namespace content

// This class is responsible for showing a butter bar on the first BrowserWindow
// that is created.
class LacrosButterBar : public BrowserListObserver {
 public:
  LacrosButterBar();
  ~LacrosButterBar() override;

  LacrosButterBar(const LacrosButterBar&) = delete;
  LacrosButterBar& operator=(const LacrosButterBar&) = delete;

  // BrowserListObserver overrides:
  void OnBrowserAdded(Browser* browser) override;

 private:
  // Shows the banner.
  void ShowBanner(content::WebContents* web_contents);

  // Set to true once a butter bar is shown. Once this is set to true this class
  // does nothing.
  bool presented_butter_bar_ = false;

  // Set to true if and only if the BrowserList is being observed. We cannot use
  // a base::ScopedObservation because BrowserList::AddObserver is a static
  // method.
  bool observing_browser_list_ = false;
};

#endif  // CHROME_BROWSER_LACROS_LACROS_BUTTER_BAR_H_
