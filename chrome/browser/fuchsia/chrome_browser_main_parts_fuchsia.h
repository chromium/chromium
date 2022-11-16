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
  void PostMainMessageLoopRun() override;

 private:
  class UseGraphicalPresenter;
  class ViewProviderRouter;

  // BrowserListObserver implementation.
  void OnBrowserAdded(Browser* browser) override;

  std::unique_ptr<base::ProcessLifecycle> lifecycle_;

  // Implementations used when running under CFv2. Under CFv2 Chrome runs in the
  // background, only opening windows when requested to via the
  // fuchsia.element.Manager service. The browser process must remain live until
  // explicitly torn-down by the ELF runner.
  std::unique_ptr<ElementManagerImpl> element_manager_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<UseGraphicalPresenter> use_graphical_presenter_;

  // TODO(crbug.com/1284806): Remove this once ViewProvider is deprecated.
  std::unique_ptr<ViewProviderRouter> view_provider_;
};

#endif  // CHROME_BROWSER_FUCHSIA_CHROME_BROWSER_MAIN_PARTS_FUCHSIA_H_
