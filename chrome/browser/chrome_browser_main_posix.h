// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_POSIX_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_POSIX_H_

#include "chrome/browser/chrome_browser_main.h"

class ChromeBrowserMainPartsPosix : public ChromeBrowserMainParts {
 public:
  ChromeBrowserMainPartsPosix(bool is_integration_test,
                              StartupData* startup_data);

  ChromeBrowserMainPartsPosix(const ChromeBrowserMainPartsPosix&) = delete;
  ChromeBrowserMainPartsPosix& operator=(const ChromeBrowserMainPartsPosix&) =
      delete;

  // content::BrowserMainParts overrides.
  int PreEarlyInitialization() override;
  void PostCreateMainMessageLoop() override;

  // ChromeBrowserMainParts overrides.
  void ShowMissingLocaleMessageBox() override;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_POSIX_H_
