// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains functions used by BrowserMain() that are linux-specific.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_LINUX_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_LINUX_H_

#include "build/build_config.h"
#include "chrome/browser/chrome_browser_main_posix.h"

class ChromeBrowserMainPartsLinux : public ChromeBrowserMainPartsPosix {
 public:
  ChromeBrowserMainPartsLinux(bool is_integration_test,
                              StartupData* startup_data);

  ChromeBrowserMainPartsLinux(const ChromeBrowserMainPartsLinux&) = delete;
  ChromeBrowserMainPartsLinux& operator=(const ChromeBrowserMainPartsLinux&) =
      delete;

  ~ChromeBrowserMainPartsLinux() override;

  // ChromeBrowserMainPartsPosix overrides.
  void PostCreateMainMessageLoop() override;
  void PreProfileInit() override;
#if defined(USE_DBUS) && !BUILDFLAG(IS_CHROMEOS)
  // Only needed for native Linux, to set up the low-memory-monitor-based memory
  // monitoring (which depends on D-Bus).
  void PostBrowserStart() override;
#endif
  void PostDestroyThreads() override;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_LINUX_H_
