// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FUCHSIA_CHROME_BROWSER_MAIN_PARTS_FUCHSIA_H_
#define CHROME_BROWSER_FUCHSIA_CHROME_BROWSER_MAIN_PARTS_FUCHSIA_H_

#include <memory>

#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/ui/browser_list_observer.h"

namespace base {
class ProcessLifecycle;
}

class ScopedKeepAlive;
class ElementManagerImpl;

class ChromeBrowserMainPartsFuchsia : public ChromeBrowserMainParts,
                                      public BrowserListObserver {
 public:
  ChromeBrowserMainPartsFuchsia(bool is_integration_test,
                                StartupData* startup_data);

  ChromeBrowserMainPartsFuchsia(const ChromeBrowserMainPartsFuchsia&) = delete;
  ChromeBrowserMainPartsFuchsia& operator=(
      const ChromeBrowserMainPartsFuchsia&) = delete;
  ~ChromeBrowserMainPartsFuchsia() override;

  // ChromeBrowserMainParts overrides.
  void ShowMissingLocaleMessageBox() override;

  // content::BrowserMainParts overrides.
  int PreEarlyInitialization() override;
  void PostCreateMainMessageLoop() override;
  int PreMainMessageLoopRun() override;

 private:
  class ViewPresenter;

  // BrowserListObserver implementation.
  void OnBrowserAdded(Browser* browser) override;

  // Instantiated when running in production, to allow the framework to
  // request graceful teardown (e.g. during session logout or reboot).
  std::unique_ptr<base::ProcessLifecycle> lifecycle_;

  // Implements the ElementManager protocol, used by the shell to request that
  // Chrome re-open the browsing session, or open a new window in the current
  // session.
  std::unique_ptr<ElementManagerImpl> element_manager_;

  // Keeps Chrome running in the background, ready to service ElementManager
  // requests, until explicitly stopped by the framework (see above).
  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // Implements display of top-level Ozone windows via the GraphicalPresenter
  // service.
  std::unique_ptr<ViewPresenter> view_presenter_;
};

#endif  // CHROME_BROWSER_FUCHSIA_CHROME_BROWSER_MAIN_PARTS_FUCHSIA_H_
