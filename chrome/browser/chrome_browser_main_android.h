// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_ANDROID_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_ANDROID_H_

#include "base/macros.h"
#include "chrome/browser/android/chrome_backup_watcher.h"
#include "chrome/browser/chrome_browser_main.h"

class ChromeBrowserMainPartsAndroid : public ChromeBrowserMainParts {
 public:
  ChromeBrowserMainPartsAndroid(
      const content::MainFunctionParams& parameters,
      ChromeFeatureListCreator* chrome_feature_list_creator);
  ~ChromeBrowserMainPartsAndroid() override;

  // content::BrowserMainParts overrides.
  int PreCreateThreads() override;
  void PostProfileInit() override;
  int PreEarlyInitialization() override;

  // ChromeBrowserMainParts overrides.
  void PostBrowserStart() override;
  void ShowMissingLocaleMessageBox() override;

 private:
  std::unique_ptr<base::MessageLoop> main_message_loop_;
  std::unique_ptr<android::ChromeBackupWatcher> backup_watcher_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainPartsAndroid);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_ANDROID_H_
