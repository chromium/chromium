// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/powerwash_requirements_checker.h"

#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

using RebootOnSignOutPolicy =
    ::enterprise_management::DeviceRebootOnUserSignoutProto;
using ::testing::HasSubstr;

class PowerwashRequirementsCheckerTest : public BrowserWithTestWindowTest {
 public:
  PowerwashRequirementsCheckerTest()
      : fake_user_manager_(new chromeos::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(fake_user_manager_)) {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    chromeos::CryptohomeClient::InitializeFake();
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
  }

  void TearDown() override {
    BrowserWithTestWindowTest::TearDown();
    chromeos::CryptohomeClient::Shutdown();
  }

  void SetupUserWithAffiliation(bool is_affiliated) {
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "1234567890"));
    fake_user_manager_->AddUserWithAffiliation(account_id, is_affiliated);
    fake_user_manager_->LoginUser(account_id);
  }

  void SetDeviceRebootOnUserSignoutPolicy(int value) {
    settings_helper_.SetInteger(chromeos::kDeviceRebootOnUserSignout, value);
  }

  void SetupCryptohomeRequiresPowerwash(bool requires_powerwash) {
    chromeos::FakeCryptohomeClient::Get()->set_requires_powerwash(
        requires_powerwash);
    PowerwashRequirementsChecker::InitializeSynchronouslyForTesting();
  }

  PowerwashRequirementsChecker::State GetStateForArc() const {
    return PowerwashRequirementsChecker(
               PowerwashRequirementsChecker::Context::kArc, profile())
        .GetState();
  }

  PowerwashRequirementsChecker::State GetStateForCrostini() const {
    return PowerwashRequirementsChecker(
               PowerwashRequirementsChecker::Context::kCrostini, profile())
        .GetState();
  }

 private:
  chromeos::ScopedCrosSettingsTestHelper settings_helper_{
      /* create_settings_service=*/false};
  chromeos::FakeChromeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
};

TEST_F(PowerwashRequirementsCheckerTest, IsNotRequiredWhenPolicyIsNotSet) {
  SetupUserWithAffiliation(false);
  SetupCryptohomeRequiresPowerwash(true);

  EXPECT_EQ(PowerwashRequirementsChecker::State::kNotRequired,
            GetStateForArc());
  EXPECT_EQ(PowerwashRequirementsChecker::State::kNotRequired,
            GetStateForCrostini());
}

TEST_F(PowerwashRequirementsCheckerTest, IsNotRequiredWhenPolicyIsNever) {
  SetupUserWithAffiliation(false);
  SetDeviceRebootOnUserSignoutPolicy(RebootOnSignOutPolicy::NEVER);
  SetupCryptohomeRequiresPowerwash(true);

  EXPECT_EQ(PowerwashRequirementsChecker::State::kNotRequired,
            GetStateForArc());
  EXPECT_EQ(PowerwashRequirementsChecker::State::kNotRequired,
            GetStateForCrostini());
}

TEST_F(PowerwashRequirementsCheckerTest, IsNotRequiredWhenUserIsAffiliated) {
  SetupUserWithAffiliation(true);
  SetDeviceRebootOnUserSignoutPolicy(RebootOnSignOutPolicy::ALWAYS);
  SetupCryptohomeRequiresPowerwash(true);

  EXPECT_EQ(PowerwashRequirementsChecker::State::kNotRequired,
            GetStateForArc());
  EXPECT_EQ(PowerwashRequirementsChecker::State::kNotRequired,
            GetStateForCrostini());
}

TEST_F(PowerwashRequirementsCheckerTest,
       IsNotRequiredWhenCryptohomeDoesNotRequire) {
  SetupUserWithAffiliation(false);
  SetDeviceRebootOnUserSignoutPolicy(RebootOnSignOutPolicy::ALWAYS);
  SetupCryptohomeRequiresPowerwash(false);

  EXPECT_EQ(PowerwashRequirementsChecker::State::kNotRequired,
            GetStateForArc());
  EXPECT_EQ(PowerwashRequirementsChecker::State::kNotRequired,
            GetStateForCrostini());
}

TEST_F(PowerwashRequirementsCheckerTest,
       IsRequiredForArcWhenPolicyIsSetForArc) {
  SetupUserWithAffiliation(false);
  SetDeviceRebootOnUserSignoutPolicy(RebootOnSignOutPolicy::ARC_SESSION);
  SetupCryptohomeRequiresPowerwash(true);

  EXPECT_EQ(PowerwashRequirementsChecker::State::kRequired, GetStateForArc());
  EXPECT_EQ(PowerwashRequirementsChecker::State::kNotRequired,
            GetStateForCrostini());
}

TEST_F(PowerwashRequirementsCheckerTest,
       IsRequiredWhenUserNotAffiliatedPolicyIsSetAndCryptohomeRequires) {
  for (auto policy : {RebootOnSignOutPolicy::ALWAYS,
                      RebootOnSignOutPolicy::VM_STARTED_OR_ARC_SESSION}) {
    SetupUserWithAffiliation(false);
    SetDeviceRebootOnUserSignoutPolicy(policy);
    SetupCryptohomeRequiresPowerwash(true);

    EXPECT_EQ(PowerwashRequirementsChecker::State::kRequired, GetStateForArc());
    EXPECT_EQ(PowerwashRequirementsChecker::State::kRequired,
              GetStateForCrostini());
  }
}

TEST_F(
    PowerwashRequirementsCheckerTest,
    IsUndefinedWhenUserNotAffiliatedPolicyIsSetAndCryptohomeRespondsWithError) {
  SetupUserWithAffiliation(false);
  SetDeviceRebootOnUserSignoutPolicy(RebootOnSignOutPolicy::ALWAYS);
  PowerwashRequirementsChecker::ResetForTesting();
  chromeos::FakeCryptohomeClient::Get()->set_cryptohome_error(
      cryptohome::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
  SetupCryptohomeRequiresPowerwash(false);

  EXPECT_EQ(PowerwashRequirementsChecker::State::kUndefined, GetStateForArc());
  EXPECT_EQ(PowerwashRequirementsChecker::State::kUndefined,
            GetStateForCrostini());
}

TEST_F(PowerwashRequirementsCheckerTest,
       IsUndefinedWhenUserNotAffiliatedPolicyIsSetAndCryptohomeNotResponding) {
  SetupUserWithAffiliation(false);
  SetDeviceRebootOnUserSignoutPolicy(RebootOnSignOutPolicy::ALWAYS);
  PowerwashRequirementsChecker::ResetForTesting();
  chromeos::FakeCryptohomeClient::Get()->SetServiceIsAvailable(false);
  // Cryptohome will never response and initialization callbacks will never be
  // called.
  PowerwashRequirementsChecker::Initialize();

  EXPECT_EQ(PowerwashRequirementsChecker::State::kUndefined, GetStateForArc());
  EXPECT_EQ(PowerwashRequirementsChecker::State::kUndefined,
            GetStateForCrostini());
  chromeos::FakeCryptohomeClient::Get()->ReportServiceIsNotAvailable();
}

TEST_F(
    PowerwashRequirementsCheckerTest,
    IsUndefinedWhenUserNotAffiliatedPolicyIsSetAndCryptohomeRespondsNotAvailable) {
  SetupUserWithAffiliation(false);
  SetDeviceRebootOnUserSignoutPolicy(RebootOnSignOutPolicy::ALWAYS);
  PowerwashRequirementsChecker::ResetForTesting();
  chromeos::FakeCryptohomeClient::Get()->SetServiceIsAvailable(false);
  PowerwashRequirementsChecker::Initialize();
  chromeos::FakeCryptohomeClient::Get()->ReportServiceIsNotAvailable();

  EXPECT_EQ(PowerwashRequirementsChecker::State::kUndefined, GetStateForArc());
  EXPECT_EQ(PowerwashRequirementsChecker::State::kUndefined,
            GetStateForCrostini());
}

TEST_F(PowerwashRequirementsCheckerTest, ShowsCorrectNotificationsForArc) {
  SetupUserWithAffiliation(false);
  SetDeviceRebootOnUserSignoutPolicy(RebootOnSignOutPolicy::ALWAYS);
  PowerwashRequirementsChecker::ResetForTesting();

  NotificationDisplayServiceTester notification_service(profile());

  PowerwashRequirementsChecker pw_checker(
      PowerwashRequirementsChecker::Context::kArc, profile());
  EXPECT_EQ(PowerwashRequirementsChecker::State::kUndefined,
            pw_checker.GetState());
  pw_checker.ShowNotification();

  {
    // Cryptohome state is unknown. Error notification is shown instead of
    // normal one.
    auto error_notification = notification_service.GetNotification(
        "arc_powerwash_request_cryptohome_error");
    EXPECT_NE(base::nullopt, error_notification);
    EXPECT_THAT(base::UTF16ToUTF8(error_notification->message()),
                HasSubstr("Google Play"));

    auto notification = notification_service.GetNotification(
        "arc_powerwash_request_instead_of_run");
    EXPECT_EQ(base::nullopt, notification);
  }

  SetupCryptohomeRequiresPowerwash(true);
  EXPECT_EQ(PowerwashRequirementsChecker::State::kRequired,
            pw_checker.GetState());
  pw_checker.ShowNotification();

  {
    // Cryptohome state is available. Show normal notification.
    auto error_notification = notification_service.GetNotification(
        "arc_powerwash_request_cryptohome_error");
    EXPECT_EQ(base::nullopt, error_notification);

    auto notification = notification_service.GetNotification(
        "arc_powerwash_request_instead_of_run");
    EXPECT_NE(base::nullopt, notification);
    EXPECT_THAT(base::UTF16ToUTF8(notification->message()),
                HasSubstr("Google Play"));
  }
}

TEST_F(PowerwashRequirementsCheckerTest, ShowsCorrectNotificationForCrostini) {
  SetupUserWithAffiliation(false);
  SetDeviceRebootOnUserSignoutPolicy(RebootOnSignOutPolicy::ALWAYS);
  PowerwashRequirementsChecker::ResetForTesting();

  NotificationDisplayServiceTester notification_service(profile());

  PowerwashRequirementsChecker pw_checker(
      PowerwashRequirementsChecker::Context::kCrostini, profile());
  EXPECT_EQ(PowerwashRequirementsChecker::State::kUndefined,
            pw_checker.GetState());
  pw_checker.ShowNotification();

  {
    // Cryptohome state is unknown. Error notification is shown instead of
    // normal one.
    auto error_notification = notification_service.GetNotification(
        "crostini_powerwash_request_cryptohome_error");
    EXPECT_NE(base::nullopt, error_notification);
    EXPECT_THAT(base::UTF16ToUTF8(error_notification->message()),
                HasSubstr("Linux"));

    auto notification = notification_service.GetNotification(
        "crostini_powerwash_request_instead_of_run");
    EXPECT_EQ(base::nullopt, notification);
  }

  SetupCryptohomeRequiresPowerwash(true);
  EXPECT_EQ(PowerwashRequirementsChecker::State::kRequired,
            pw_checker.GetState());
  pw_checker.ShowNotification();

  {
    // Cryptohome state is available. Show normal notification.
    auto error_notification = notification_service.GetNotification(
        "crostini_powerwash_request_cryptohome_error");
    EXPECT_EQ(base::nullopt, error_notification);

    auto notification = notification_service.GetNotification(
        "crostini_powerwash_request_instead_of_run");
    EXPECT_NE(base::nullopt, notification);
    EXPECT_THAT(base::UTF16ToUTF8(notification->message()), HasSubstr("Linux"));
  }
}

}  // namespace policy
