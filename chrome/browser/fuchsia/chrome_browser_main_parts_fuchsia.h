// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FUCHSIA_CHROME_BROWSER_MAIN_PARTS_FUCHSIA_H_
#define CHROME_BROWSER_FUCHSIA_CHROME_BROWSER_MAIN_PARTS_FUCHSIA_H_

#include <memory>

#include "chrome/browser/chrome_browser_main.h"

namespace base {
class ProcessLifecycle;
}

class ScopedKeepAlive;
class ElementManagerImpl;

class ChromeBrowserMainPartsFuchsia : public ChromeBrowserMainParts {
 public:
  ChromeBrowserMainPartsFuchsia(content::MainFunctionParams parameters,
                                StartupData* startup_data);

  ChromeBrowserMainPartsFuchsia(const ChromeBrowserMainPartsFuchsia&) = delete;
  ChromeBrowserMainPartsFuchsia& operator=(
      const ChromeBrowserMainPartsFuchsia&) = delete;
  ~ChromeBrowserMainPartsFuchsia() override;

  // ChromeBrowserMainParts overrides.
  void ShowMissingLocaleMessageBox() override;

  // content::BrowserMainParts overrides.
  int PreEarlyInitialization() override;
  int PreMainMessageLoopRun() override;

 private:
  class ViewPresenter;

  std::unique_ptr<base::ProcessLifecycle> lifecycle_;

  // Used to allow the shell to (re)open Chrome windows
  std::unique_ptr<ElementManagerImpl> element_manager_;

  // Under CFv2 Chrome runs in the background, only opening windows when
  // requested to via the fuchsia.element.Manager service. The browser process
  // must remain live until explicitly torn-down by the ELF runner.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // Helper class that configures Ozone to use GraphicalPresenter to display a
  // new View for each new top-level window.
  std::unique_ptr<ViewPresenter> view_presenter_;
};

#endif  // CHROME_BROWSER_FUCHSIA_CHROME_BROWSER_MAIN_PARTS_FUCHSIA_H_
