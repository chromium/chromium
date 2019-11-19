// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_error_notifier_ash.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_error_notifier_factory_ash.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

const char kTestEmail[] = "email@example.com";

// Notification ID corresponding to kProfileSigninNotificationId +
// kTestAccountId.
const char kPrimaryAccountErrorNotificationId[] =
    "chrome://settings/signin/testing_profile";
const char kSecondaryAccountErrorNotificationId[] =
    "chrome://settings/signin/testing_profile/secondary-account";

class SigninErrorNotifierTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    mock_user_manager_ = new chromeos::MockUserManager();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(mock_user_manager_));

    SigninErrorNotifierFactory::GetForProfile(GetProfile());
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());

    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(GetProfile());
  }

  void TearDown() override {
    // Need to be destroyed before the profile associated to this test, which
    // will be destroyed as part of the TearDown() process.
    identity_test_env_profile_adaptor_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  void SetAuthError(const std::string& account_id,
                    const GoogleServiceAuthError& error) {
    signin::UpdatePersistentErrorOfRefreshTokenForAccount(
        identity_test_env()->identity_manager(), account_id, error);
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

 protected:
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  chromeos::MockUserManager* mock_user_manager_;  // Not owned.
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
};

TEST_F(SigninErrorNotifierTest, NoNotification) {
  EXPECT_FALSE(
      display_service_->GetNotification(kPrimaryAccountErrorNotificationId));
  EXPECT_FALSE(
      display_service_->GetNotification(kSecondaryAccountErrorNotificationId));
}

// Verify that if Supervision has just been added for the current user
// the notification isn't shown.  This is because the Add Supervision
// flow itself will prompt the user to sign out, so the notification
// is unnecessary.
TEST_F(SigninErrorNotifierTest, NoNotificationAfterAddSupervisionEnabled) {
  std::string account_id =
      identity_test_env()->MakeAccountAvailable(kTestEmail).account_id;
  identity_test_env()->SetPrimaryAccount(kTestEmail);

  // Mark signout required.
  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(profile());
  service->set_signout_required_after_supervision_enabled();

  SetAuthError(
      identity_test_env()->identity_manager()->GetPrimaryAccountId(),
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  EXPECT_FALSE(
      display_service_->GetNotification(kPrimaryAccountErrorNotificationId));
}

TEST_F(SigninErrorNotifierTest, ErrorResetForPrimaryAccount) {
  EXPECT_FALSE(
      display_service_->GetNotification(kPrimaryAccountErrorNotificationId));

  std::string account_id =
      identity_test_env()->MakePrimaryAccountAvailable(kTestEmail).account_id;
  SetAuthError(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_TRUE(
      display_service_->GetNotification(kPrimaryAccountErrorNotificationId));

  SetAuthError(account_id, GoogleServiceAuthError::AuthErrorNone());
  EXPECT_FALSE(
      display_service_->GetNotification(kPrimaryAccountErrorNotificationId));
}

TEST_F(SigninErrorNotifierTest, ErrorResetForSecondaryAccount) {
  EXPECT_FALSE(
      display_service_->GetNotification(kSecondaryAccountErrorNotificationId));

  std::string account_id =
      identity_test_env()->MakeAccountAvailable(kTestEmail).account_id;
  SetAuthError(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  // Uses the run loop from |BrowserTaskEnvironment|.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      display_service_->GetNotification(kSecondaryAccountErrorNotificationId));

  SetAuthError(account_id, GoogleServiceAuthError::AuthErrorNone());
  EXPECT_FALSE(
      display_service_->GetNotification(kSecondaryAccountErrorNotificationId));
}

TEST_F(SigninErrorNotifierTest, ErrorTransitionForPrimaryAccount) {
  std::string account_id =
      identity_test_env()->MakePrimaryAccountAvailable(kTestEmail).account_id;
  SetAuthError(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  base::Optional<message_center::Notification> notification =
      display_service_->GetNotification(kPrimaryAccountErrorNotificationId);
  ASSERT_TRUE(notification);
  base::string16 message = notification->message();
  EXPECT_FALSE(message.empty());

  // Now set another auth error.
  SetAuthError(account_id,
               GoogleServiceAuthError(
                   GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE));

  notification =
      display_service_->GetNotification(kPrimaryAccountErrorNotificationId);
  ASSERT_TRUE(notification);
  base::string16 new_message = notification->message();
  EXPECT_FALSE(new_message.empty());

  ASSERT_NE(new_message, message);
}

// Verify that SigninErrorNotifier ignores certain errors.
TEST_F(SigninErrorNotifierTest, AuthStatusEnumerateAllErrors) {
  typedef struct {
    GoogleServiceAuthError::State error_state;
    bool is_error;
  } ErrorTableEntry;

  ErrorTableEntry table[] = {
      {GoogleServiceAuthError::NONE, false},
      {GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS, true},
      {GoogleServiceAuthError::USER_NOT_SIGNED_UP, true},
      {GoogleServiceAuthError::CONNECTION_FAILED, false},
      {GoogleServiceAuthError::SERVICE_UNAVAILABLE, false},
      {GoogleServiceAuthError::REQUEST_CANCELED, false},
      {GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE, true},
      {GoogleServiceAuthError::SERVICE_ERROR, true},
  };
  static_assert(
      base::size(table) == GoogleServiceAuthError::NUM_STATES -
                               GoogleServiceAuthError::kDeprecatedStateCount,
      "table size should match number of auth error types");
  std::string account_id =
      identity_test_env()->MakePrimaryAccountAvailable(kTestEmail).account_id;

  for (size_t i = 0; i < base::size(table); ++i) {
    SetAuthError(account_id, GoogleServiceAuthError(table[i].error_state));
    base::Optional<message_center::Notification> notification =
        display_service_->GetNotification(kPrimaryAccountErrorNotificationId);
    ASSERT_EQ(table[i].is_error, !!notification) << "Failed case #" << i;
    if (table[i].is_error) {
      EXPECT_FALSE(notification->title().empty());
      EXPECT_FALSE(notification->message().empty());
      EXPECT_EQ((size_t)1, notification->buttons().size());
    }
    SetAuthError(account_id, GoogleServiceAuthError::AuthErrorNone());
  }
}

}  // namespace
