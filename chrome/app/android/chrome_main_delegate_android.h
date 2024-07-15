// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_ANDROID_CHROME_MAIN_DELEGATE_ANDROID_H_
#define CHROME_APP_ANDROID_CHROME_MAIN_DELEGATE_ANDROID_H_

#include <memory>
#include <optional>

#include "chrome/app/chrome_main_delegate.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/browser_main_runner.h"

// Android override of ChromeMainDelegate
class ChromeMainDelegateAndroid : public ChromeMainDelegate {
 public:
  static void SecureDataDirectory();  // visible for testing

  ChromeMainDelegateAndroid();

  ChromeMainDelegateAndroid(const ChromeMainDelegateAndroid&) = delete;
  ChromeMainDelegateAndroid& operator=(const ChromeMainDelegateAndroid&) =
      delete;

  ~ChromeMainDelegateAndroid() override;

  std::optional<int> BasicStartupComplete() override;
  void PreSandboxStartup() override;
  absl::variant<int, content::MainFunctionParams> RunProcess(
      const std::string& process_type,
      content::MainFunctionParams main_function_params) override;
  void ProcessExiting(const std::string& process_type) override {}

 private:
  std::unique_ptr<content::BrowserMainRunner> browser_runner_;
};

#endif  // CHROME_APP_ANDROID_CHROME_MAIN_DELEGATE_ANDROID_H_
