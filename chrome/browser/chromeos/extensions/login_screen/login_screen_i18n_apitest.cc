// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/values.h"
#include "chrome/browser/chromeos/extensions/login_screen/login_screen_apitest_base.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/test/test_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kI18nGetMessage[] = "I18nGetMessage";

struct I18nTestParams {
  std::string locale_name;
  std::string expected_message;
};

}  // namespace

namespace chromeos {

// A test suite which checks that the login screen extension returns the correct
// message from the i18n API based on the locale. The first param is the
// device's login screen locale and the second param is the expected message.
class LoginScreenI18nApitest
    : public LoginScreenApitestBase,
      public testing::WithParamInterface<I18nTestParams> {
 public:
  LoginScreenI18nApitest()
      : LoginScreenApitestBase(version_info::Channel::STABLE) {}

  LoginScreenI18nApitest(const LoginScreenI18nApitest&) = delete;

  LoginScreenI18nApitest& operator=(const LoginScreenI18nApitest&) = delete;

  ~LoginScreenI18nApitest() override = default;

  void SetExpectedMessage(const std::string custom_arg) {
    config_.Set("customArg", base::Value(custom_arg));
    extensions::TestGetConfigFunction::set_test_config_state(&config_);
  }

  void SetUpInProcessBrowserTestFixture() override {
    LoginScreenApitestBase::SetUpInProcessBrowserTestFixture();
    device_policy()
        ->payload()
        .mutable_login_screen_locales()
        ->add_login_screen_locales(GetParam().locale_name);
    RefreshDevicePolicy();
  }

 private:
  base::Value::Dict config_;
};

IN_PROC_BROWSER_TEST_P(LoginScreenI18nApitest, GetMessage) {
  SetExpectedMessage(GetParam().expected_message);
  SetUpLoginScreenExtensionAndRunTest(kI18nGetMessage);
}

INSTANTIATE_TEST_SUITE_P(
    LoginScreenExtension,
    LoginScreenI18nApitest,
    testing::ValuesIn({I18nTestParams{"en-US", "Hello World!"},
                       I18nTestParams{"de-DE", "Hallo Welt!"}}));

}  // namespace chromeos
