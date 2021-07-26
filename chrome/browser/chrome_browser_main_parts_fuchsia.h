// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_PARTS_FUCHSIA_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_PARTS_FUCHSIA_H_

#include "chrome/browser/chrome_browser_main.h"

class ChromeBrowserMainPartsFuchsia : public ChromeBrowserMainParts {
 public:
  ChromeBrowserMainPartsFuchsia(const content::MainFunctionParams& parameters,
                                StartupData* startup_data);

  ChromeBrowserMainPartsFuchsia(const ChromeBrowserMainPartsFuchsia&) = delete;
  ChromeBrowserMainPartsFuchsia& operator=(
      const ChromeBrowserMainPartsFuchsia&) = delete;

  // ChromeBrowserMainParts overrides.
  void ShowMissingLocaleMessageBox() override;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_PARTS_FUCHSIA_H_
