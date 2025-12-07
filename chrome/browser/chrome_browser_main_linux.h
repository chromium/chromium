// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains functions used by BrowserMain() that are linux-specific.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_LINUX_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_LINUX_H_

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/config/linux/dbus/buildflags.h"
#include "chrome/browser/chrome_browser_main_posix.h"

#if BUILDFLAG(IS_CHROMEOS)
namespace metrics {
class StackSamplingRecorder;
}
#endif

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
#if BUILDFLAG(IS_LINUX)
  void PostMainMessageLoopRun() override;
#endif
  void PreProfileInit() override;
#if BUILDFLAG(USE_DBUS) && !BUILDFLAG(IS_CHROMEOS)
  // Only needed for native Linux, to set up the low-memory-monitor-based memory
  // monitoring (which depends on D-Bus).
  void PostBrowserStart() override;
#endif
  void PostDestroyThreads() override;

 private:
#if BUILDFLAG(IS_CHROMEOS)
  // Used by ChromeOS tast tests. This is in ChromeBrowserMainPartsLinux for
  // historical reasons and should be moved to ChromeBrowserMainPartsAsh.
  scoped_refptr<metrics::StackSamplingRecorder> stack_sampling_recorder_;
#endif
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_LINUX_H_
