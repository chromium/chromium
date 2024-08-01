// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_MAC_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_MAC_H_

#include <memory>

#include "chrome/browser/chrome_browser_main_posix.h"
#include "chrome/browser/mac/code_sign_clone_manager.h"

class PlatformAuthPolicyObserver;

namespace mac_metrics {
class Metrics;
}

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
  void PostMainMessageLoopRun() override;

  // Perform platform-specific work that needs to be done after the main event
  // loop has ended. The embedder must be sure to call this.
  static void DidEndMainMessageLoop();

 private:
  // Records mac related metrics. Some metrics are recorded on startup, some
  // are recorded later in response to an events.
  std::unique_ptr<mac_metrics::Metrics> metrics_;

  // Prevent code sign verification issues of the running instance of Chrome
  // when it has been updated on disk. CodeSignCloneManager does this by
  // creating a temporary clone of the on-disk app including a hard link of the
  // main executable during browser startup. The clone and hard link keep files
  // covered by the code signature reachable on the filesystem for dynamic and
  // static verification.
  code_sign_clone_manager::CodeSignCloneManager code_sign_clone_manager_;

  // Applies enterprise policies for platform auth SSO.
  std::unique_ptr<PlatformAuthPolicyObserver> platform_auth_policy_observer_;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_MAC_H_
