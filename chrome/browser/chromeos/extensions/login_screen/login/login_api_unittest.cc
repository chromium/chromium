// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/login_api.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "chrome/browser/chromeos/login/ui/mock_login_display_host.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "extensions/browser/api_unittest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace {

class MockExistingUserController : public chromeos::ExistingUserController {
 public:
  MockExistingUserController() = default;
  ~MockExistingUserController() override = default;

  MOCK_METHOD2(Login,
               void(const chromeos::UserContext&,
                    const chromeos::SigninSpecifics&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockExistingUserController);
};

}  // namespace

namespace extensions {

class LoginApiUnittest : public ExtensionApiUnittest {
 public:
  LoginApiUnittest() = default;
  ~LoginApiUnittest() override = default;

  void SetUp() override {
    ExtensionApiUnittest::SetUp();

    fake_chrome_user_manager_ = new chromeos::FakeChromeUserManager();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::unique_ptr<chromeos::FakeChromeUserManager>(
            fake_chrome_user_manager_));
    mock_login_display_host_ =
        std::make_unique<chromeos::MockLoginDisplayHost>();
    mock_existing_user_controller_ =
        std::make_unique<MockExistingUserController>();

    // Run pending async tasks resulting from profile construction to ensure
    // these are complete before the test begins.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    mock_existing_user_controller_.reset();
    mock_login_display_host_.reset();
    scoped_user_manager_.reset();

    ExtensionApiUnittest::TearDown();
  }

 protected:
  chromeos::FakeChromeUserManager* fake_chrome_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<chromeos::MockLoginDisplayHost> mock_login_display_host_;
  std::unique_ptr<MockExistingUserController> mock_existing_user_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginApiUnittest);
};

MATCHER_P(MatchSigninSpecifics, expected, "") {
  return expected.guest_mode_url == arg.guest_mode_url &&
         expected.guest_mode_url_append_locale ==
             arg.guest_mode_url_append_locale &&
         expected.kiosk_diagnostic_mode == arg.kiosk_diagnostic_mode &&
         expected.is_auto_login == arg.is_auto_login;
}

// Test that calling |login.launchManagedGuestSession()| calls the corresponding
// method from the |ExistingUserController|.
TEST_F(LoginApiUnittest, LaunchManagedGuestSession) {
  AccountId test_account_id =
      AccountId::FromUserEmail("publicaccount@test.com");

  fake_chrome_user_manager_->AddPublicAccountUser(test_account_id);
  EXPECT_CALL(*mock_login_display_host_, GetExistingUserController())
      .WillOnce(Return(mock_existing_user_controller_.get()));

  chromeos::SigninSpecifics signin_specifics;
  chromeos::UserContext userContext(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                                    test_account_id);
  EXPECT_CALL(*mock_existing_user_controller_,
              Login(userContext, MatchSigninSpecifics(signin_specifics)))
      .Times(1);

  RunFunction(new LoginLaunchManagedGuestSessionFunction(), "[]");
}

// Test that calling |login.launchManagedGuestSession()| returns an error when
// there are no managed guest session accounts.
TEST_F(LoginApiUnittest, LaunchManagedGuestSessionNoAccounts) {
  ASSERT_EQ("No managed guest session accounts",
            RunFunctionAndReturnError(
                new LoginLaunchManagedGuestSessionFunction(), "[]"));
}

// Test that calling |login.exitCurrentSession()| with data for the next login
// attempt sets the |kLoginExtensionApiDataForNextLoginAttempt| pref to the
// given data.
TEST_F(LoginApiUnittest, ExitCurrentSessionWithData) {
  const std::string data_for_next_login_attempt = "hello world";

  RunFunction(
      new LoginExitCurrentSessionFunction(),
      base::StringPrintf(R"(["%s"])", data_for_next_login_attempt.c_str()));

  PrefService* local_state = g_browser_process->local_state();
  ASSERT_EQ(
      data_for_next_login_attempt,
      local_state->GetString(prefs::kLoginExtensionApiDataForNextLoginAttempt));
}

// Test that calling |login.exitCurrentSession()| with no data clears the
// |kLoginExtensionApiDataForNextLoginAttempt| pref.
TEST_F(LoginApiUnittest, ExitCurrentSessionWithNoData) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                         "hello world");

  RunFunction(new LoginExitCurrentSessionFunction(), "[]");

  ASSERT_EQ("", local_state->GetString(
                    prefs::kLoginExtensionApiDataForNextLoginAttempt));
}

// Test that calling |login.fetchDataForNextLoginAttempt()| function returns the
// value stored in the |kLoginExtensionsApiDataForNextLoginAttempt| pref and
// clears the pref.
TEST_F(LoginApiUnittest, FetchDataForNextLoginAttemptClearsPref) {
  const std::string data_for_next_login_attempt = "hello world";

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                         data_for_next_login_attempt);

  std::unique_ptr<base::Value> value(RunFunctionAndReturnValue(
      new LoginFetchDataForNextLoginAttemptFunction(), "[]"));
  ASSERT_EQ(data_for_next_login_attempt, value->GetString());

  ASSERT_EQ("", local_state->GetString(
                    prefs::kLoginExtensionApiDataForNextLoginAttempt));
}

}  // namespace extensions
