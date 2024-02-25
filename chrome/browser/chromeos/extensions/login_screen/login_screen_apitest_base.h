// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SCREEN_APITEST_BASE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SCREEN_APITEST_BASE_H_

#include <memory>
#include <string>

#include "chrome/browser/ash/policy/login/signin_profile_extensions_policy_test_base.h"
#include "components/version_info/channel.h"
#include "extensions/common/extension_id.h"

class ExtensionTestMessageListener;

namespace extensions {
class ResultCatcher;
}  // namespace extensions

namespace chromeos {

// This browser test uses a test extension to test certain API calls on the
// login screen. The extension is allowlisted to run as a force-installed "login
// screen extension" and is also allowlisted for the following APIs:
// * loginScreenUi
// * storage
// * login
// The extension's code can be found in
// chrome/test/data/extensions/api_test/login_screen_apis/
class LoginScreenApitestBase
    : public policy::SigninProfileExtensionsPolicyTestBase {
 public:
  explicit LoginScreenApitestBase(version_info::Channel channel);

  LoginScreenApitestBase(const LoginScreenApitestBase&) = delete;

  LoginScreenApitestBase& operator=(const LoginScreenApitestBase&) = delete;

  ~LoginScreenApitestBase() override;

  void SetUpTestListeners();

  void ClearTestListeners();

  void RunTest(const std::string& test_name);
  void RunTest(const std::string& test_name, bool assert_test_succeed);

  void SetUpLoginScreenExtensionAndRunTest(const std::string& test_name);
  void SetUpLoginScreenExtensionAndRunTest(const std::string& test_name,
                                           bool assert_test_succeed);

  extensions::ExtensionId extension_id() { return extension_id_; }

  std::string listener_message() { return listener_message_; }

 protected:
  const extensions::ExtensionId extension_id_;
  const std::string extension_update_manifest_path_;
  // The message |listener_| is listening for.
  const std::string listener_message_;
  std::unique_ptr<extensions::ResultCatcher> catcher_;
  std::unique_ptr<ExtensionTestMessageListener> listener_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SCREEN_APITEST_BASE_H_
