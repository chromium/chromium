// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_MAC_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_MAC_H_

#include "chrome/browser/chrome_browser_main_posix.h"

class ChromeBrowserMainPartsMac : public ChromeBrowserMainPartsPosix {
 public:
  ChromeBrowserMainPartsMac(bool is_integration_test,
                            StartupData* startup_data);

  ChromeBrowserMainPartsMac(const ChromeBrowserMainPartsMac&) = delete;
  ChromeBrowserMainPartsMac& operator=(const ChromeBrowserMainPartsMac&) =
      delete;

  ~ChromeBrowserMainPartsMac() override;

  // BrowserParts overrides.
  int PreEarlyInitialization() override;
  void PreCreateMainMessageLoop() override;
  void PostCreateMainMessageLoop() override;
  void PreProfileInit() override;
  void PostProfileInit(Profile* profile, bool is_initial_profile) override;

  // Perform platform-specific work that needs to be done after the main event
  // loop has ended. The embedder must be sure to call this.
  static void DidEndMainMessageLoop();
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_MAC_H_
