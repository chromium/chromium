// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_POSIX_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_POSIX_H_

#include "base/macros.h"
#include "chrome/browser/chrome_browser_main.h"

class ChromeBrowserMainPartsPosix : public ChromeBrowserMainParts {
 public:
  ChromeBrowserMainPartsPosix(const content::MainFunctionParams& parameters,
                              StartupData* startup_data);

  // content::BrowserMainParts overrides.
  int PreEarlyInitialization() override;
  void PostMainMessageLoopStart() override;

  // ChromeBrowserMainParts overrides.
  void ShowMissingLocaleMessageBox() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainPartsPosix);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_POSIX_H_
