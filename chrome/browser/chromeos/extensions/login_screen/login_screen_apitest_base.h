// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SCREEN_APITEST_BASE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SCREEN_APITEST_BASE_H_

#include <string>

#include "chrome/browser/chromeos/policy/signin_profile_extensions_policy_test_base.h"
#include "components/version_info/version_info.h"

namespace chromeos {

// This browser test uses a test extension to test certain API calls on the
// login screen. The extension is whitelisted to run as a force-installed "login
// screen extension" and is also whitelisted for the following APIs:
// * loginScreenUi
// * storage
// * login
// The extension's code can be found in
// chrome/test/data/extensions/api_test/login_screen_apis/
class LoginScreenApitestBase
    : public policy::SigninProfileExtensionsPolicyTestBase {
 public:
  explicit LoginScreenApitestBase(version_info::Channel channel);
  ~LoginScreenApitestBase() override;

  void SetUpExtensionAndRunTest(const std::string& testName);

  void SetUpExtensionAndRunTest(const std::string& testName,
                                bool assert_test_succeed);

 protected:
  const std::string extension_id_;
  const std::string extension_update_manifest_path_;

  DISALLOW_COPY_AND_ASSIGN(LoginScreenApitestBase);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SCREEN_APITEST_BASE_H_
