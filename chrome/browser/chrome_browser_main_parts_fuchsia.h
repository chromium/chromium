// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_PARTS_FUCHSIA_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_PARTS_FUCHSIA_H_

#include <memory>

#include "chrome/browser/chrome_browser_main.h"

namespace base {
class ProcessLifecycle;
}

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
  void PostMainMessageLoopRun() override;

 private:
  class ViewProviderRouter;

  std::unique_ptr<base::ProcessLifecycle> lifecycle_;
  std::unique_ptr<ViewProviderRouter> view_provider_;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_PARTS_FUCHSIA_H_
