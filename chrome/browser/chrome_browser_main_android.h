// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_ANDROID_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_ANDROID_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/chrome_browser_main.h"

namespace crash_reporter {
class ChildExitObserver;
}

class ChromeBrowserMainPartsAndroid : public ChromeBrowserMainParts {
 public:
  ChromeBrowserMainPartsAndroid(bool is_integration_test,
                                StartupData* startup_data);

  ChromeBrowserMainPartsAndroid(const ChromeBrowserMainPartsAndroid&) = delete;
  ChromeBrowserMainPartsAndroid& operator=(
      const ChromeBrowserMainPartsAndroid&) = delete;

  ~ChromeBrowserMainPartsAndroid() override;

  // content::BrowserMainParts overrides.
  int PreCreateThreads() override;
  void PostProfileInit(Profile* profile, bool is_initial_profile) override;
  int PreEarlyInitialization() override;

  // ChromeBrowserMainParts overrides.
  void PostBrowserStart() override;
  void ShowMissingLocaleMessageBox() override;

 private:
  std::unique_ptr<crash_reporter::ChildExitObserver> child_exit_observer_;
  // Owns the Java ChromeBackupWatcher object and invokes destroy() on
  // destruction.
  base::ScopedClosureRunner backup_watcher_runner_;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_ANDROID_H_
