// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ui/ash/login/mock_login_display_host.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::ReturnNull;

// These values are only used to test the configuration.  They don't
// delay the test.
const int kAutoLoginDelay1 = 60000;
const int kAutoLoginDelay2 = 180000;

}  // namespace

class ExistingUserControllerAutoLoginTest : public ::testing::Test {
 protected:
  ExistingUserControllerAutoLoginTest()
      : local_state_(TestingBrowserProcess::GetGlobal()),
        fake_user_manager_(std::make_unique<FakeChromeUserManager>()) {
    auth_events_recorder_ = ash::AuthEventsRecorder::CreateForTesting();
  }

  void SetUp() override {
    existing_user_controller_ = std::make_unique<ExistingUserController>();
    mock_login_display_host_ = std::make_unique<MockLoginDisplayHost>();

    ON_CALL(*mock_login_display_host_, GetExistingUserController())
        .WillByDefault(Return(existing_user_controller_.get()));

    fake_user_manager_->AddPublicAccountUser(auto_login_account_id_);

    settings_helper_.ReplaceDeviceSettingsProviderWithStub();

    DeviceSettingsService::Get()->SetSessionManager(
        FakeSessionManagerClient::Get(), new ownership::MockOwnerKeyUtil());
    DeviceSettingsService::Get()->Load();

    base::Value::Dict account;
    account.Set(kAccountsPrefDeviceLocalAccountsKeyId, auto_login_user_id_);
    account.Set(
        kAccountsPrefDeviceLocalAccountsKeyType,
        static_cast<int>(policy::DeviceLocalAccountType::kPublicSession));
    base::Value::List accounts;
    accounts.Append(std::move(account));
    settings_helper_.Set(kAccountsPrefDeviceLocalAccounts,
                         base::Value(std::move(accounts)));

    // Prevent settings changes from auto-starting the timer.
    existing_user_controller_->local_account_auto_login_id_subscription_ = {};
    existing_user_controller_
        ->local_account_auto_login_delay_subscription_ = {};

    session_manager_.SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);
  }

  ExistingUserController* existing_user_controller() const {
    return ExistingUserController::current_controller();
  }

  void SetAutoLoginSettings(const std::string& user_id, int delay) {
    settings_helper_.SetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                               user_id);
    settings_helper_.SetInteger(kAccountsPrefDeviceLocalAccountAutoLoginDelay,
                                delay);
  }

  // ExistingUserController private member accessors.
  base::OneShotTimer* auto_login_timer() {
    return existing_user_controller()->auto_login_timer_.get();
  }

  const AccountId& auto_login_account_id() const {
    return existing_user_controller()->public_session_auto_login_account_id_;
  }
  void set_auto_login_account_id(const AccountId& account_id) {
    existing_user_controller()->public_session_auto_login_account_id_ =
        account_id;
  }

  int auto_login_delay() const {
    return existing_user_controller()->auto_login_delay_;
  }
  void set_auto_login_delay(int delay) {
    existing_user_controller()->auto_login_delay_ = delay;
  }

  bool is_login_in_progress() const {
    return existing_user_controller()->is_login_in_progress_;
  }
  void set_is_login_in_progress(bool is_login_in_progress) {
    existing_user_controller()->is_login_in_progress_ = is_login_in_progress;
  }

  void ConfigureAutoLogin() {
    existing_user_controller()->ConfigureAutoLogin();
  }

  const std::string auto_login_user_id_ =
      std::string("public_session_user@localhost");

  const AccountId auto_login_account_id_ =
      AccountId::FromUserEmail(policy::GenerateDeviceLocalAccountUserId(
          auto_login_user_id_,
          policy::DeviceLocalAccountType::kPublicSession));

 private:
  std::unique_ptr<MockLoginDisplayHost> mock_login_display_host_;
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;

  // Required by ExistingUserController:
  FakeSessionManagerClient fake_session_manager_client_;
  ScopedCrosSettingsTestHelper settings_helper_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;

  session_manager::SessionManager session_manager_;

  // `existing_user_controller_` must be destroyed before
  // `device_settings_test_helper_`.
  std::unique_ptr<ExistingUserController> existing_user_controller_;
  std::unique_ptr<ash::AuthEventsRecorder> auth_events_recorder_;
};

TEST_F(ExistingUserControllerAutoLoginTest, StartAutoLoginTimer) {
  set_auto_login_delay(kAutoLoginDelay2);

  // Timer shouldn't start if the policy isn't set.
  set_auto_login_account_id(EmptyAccountId());
  existing_user_controller()->StartAutoLoginTimer();
  EXPECT_FALSE(auto_login_timer());

  // Timer shouldn't fire in the middle of a login attempt.
  set_auto_login_account_id(auto_login_account_id_);
  set_is_login_in_progress(true);
  existing_user_controller()->StartAutoLoginTimer();
  EXPECT_FALSE(auto_login_timer());

  // Otherwise start.
  set_is_login_in_progress(false);
  existing_user_controller()->StartAutoLoginTimer();
  ASSERT_TRUE(auto_login_timer());
  EXPECT_TRUE(auto_login_timer()->IsRunning());
  EXPECT_EQ(auto_login_timer()->GetCurrentDelay().InMilliseconds(),
            kAutoLoginDelay2);
}

TEST_F(ExistingUserControllerAutoLoginTest, StopAutoLoginTimer) {
  set_auto_login_account_id(auto_login_account_id_);
  set_auto_login_delay(kAutoLoginDelay2);

  existing_user_controller()->StartAutoLoginTimer();
  ASSERT_TRUE(auto_login_timer());
  EXPECT_TRUE(auto_login_timer()->IsRunning());

  existing_user_controller()->StopAutoLoginTimer();
  ASSERT_TRUE(auto_login_timer());
  EXPECT_FALSE(auto_login_timer()->IsRunning());
}

TEST_F(ExistingUserControllerAutoLoginTest, ResetAutoLoginTimer) {
  set_auto_login_account_id(auto_login_account_id_);

  // Timer starts off not running.
  EXPECT_FALSE(auto_login_timer());

  // When the timer isn't running, nothing should happen.
  existing_user_controller()->OnUserActivity(/*event=*/nullptr);
  EXPECT_FALSE(auto_login_timer());

  // Start the timer.
  set_auto_login_delay(kAutoLoginDelay2);
  existing_user_controller()->StartAutoLoginTimer();
  ASSERT_TRUE(auto_login_timer());
  EXPECT_TRUE(auto_login_timer()->IsRunning());
  EXPECT_EQ(auto_login_timer()->GetCurrentDelay().InMilliseconds(),
            kAutoLoginDelay2);

  // User activity should restart the timer, so check to see that the
  // timer delay was modified.
  set_auto_login_delay(kAutoLoginDelay1);
  existing_user_controller()->OnUserActivity(/*event=*/nullptr);
  ASSERT_TRUE(auto_login_timer());
  EXPECT_TRUE(auto_login_timer()->IsRunning());
  EXPECT_EQ(auto_login_timer()->GetCurrentDelay().InMilliseconds(),
            kAutoLoginDelay1);
}

TEST_F(ExistingUserControllerAutoLoginTest, ConfigureAutoLogin) {
  // Timer shouldn't start when the policy is disabled.
  ConfigureAutoLogin();
  EXPECT_FALSE(auto_login_timer());
  EXPECT_EQ(auto_login_delay(), 0);
  EXPECT_EQ(auto_login_account_id(), EmptyAccountId());

  // Timer shouldn't start when the delay alone is set.
  SetAutoLoginSettings("", kAutoLoginDelay1);
  ConfigureAutoLogin();
  EXPECT_FALSE(auto_login_timer());
  EXPECT_EQ(auto_login_delay(), kAutoLoginDelay1);
  EXPECT_EQ(auto_login_account_id(), EmptyAccountId());

  // Timer should start when the account ID is set.
  SetAutoLoginSettings(auto_login_user_id_, kAutoLoginDelay1);
  ConfigureAutoLogin();
  ASSERT_TRUE(auto_login_timer());
  EXPECT_TRUE(auto_login_timer()->IsRunning());
  EXPECT_EQ(auto_login_timer()->GetCurrentDelay().InMilliseconds(),
            kAutoLoginDelay1);
  EXPECT_EQ(auto_login_delay(), kAutoLoginDelay1);
  EXPECT_EQ(auto_login_account_id(), auto_login_account_id_);

  // Timer should restart when the delay is changed.
  SetAutoLoginSettings(auto_login_user_id_, kAutoLoginDelay2);
  ConfigureAutoLogin();
  ASSERT_TRUE(auto_login_timer());
  EXPECT_TRUE(auto_login_timer()->IsRunning());
  EXPECT_EQ(auto_login_timer()->GetCurrentDelay().InMilliseconds(),
            kAutoLoginDelay2);
  EXPECT_EQ(auto_login_delay(), kAutoLoginDelay2);
  EXPECT_EQ(auto_login_account_id(), auto_login_account_id_);

  // Timer should stop when the account ID is unset.
  SetAutoLoginSettings("", kAutoLoginDelay2);
  ConfigureAutoLogin();
  ASSERT_TRUE(auto_login_timer());
  EXPECT_FALSE(auto_login_timer()->IsRunning());
  EXPECT_EQ(auto_login_timer()->GetCurrentDelay().InMilliseconds(),
            kAutoLoginDelay2);
  EXPECT_EQ(auto_login_account_id(), EmptyAccountId());
  EXPECT_EQ(auto_login_delay(), kAutoLoginDelay2);
}

}  // namespace ash
