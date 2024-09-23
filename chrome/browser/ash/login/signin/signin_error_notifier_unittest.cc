// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/login/signin/signin_error_notifier.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/signin/signin_error_notifier_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {
namespace {

constexpr char kTestEmail[] = "email@example.com";
constexpr char kTestSecondaryEmail[] = "email2@example.com";

constexpr char kTokenHandle[] = "test_token_handle";

// Notification ID corresponding to kProfileSigninNotificationId +
// kTestAccountId.
constexpr char kPrimaryAccountErrorNotificationId[] =
    "chrome://settings/signin/testing_profile@test";
constexpr char kSecondaryAccountErrorNotificationId[] =
    "chrome://settings/signin/testing_profile@test/secondary-account";
}  // namespace

class SigninErrorNotifierTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    // Required to initialize TokenHandleUtil.
    ash::UserDataAuthClient::InitializeFake();

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

    ash::UserDataAuthClient::Shutdown();
    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  void SetAuthError(const CoreAccountId& account_id,
                    const GoogleServiceAuthError& error) {
    signin::UpdatePersistentErrorOfRefreshTokenForAccount(
        identity_test_env()->identity_manager(), account_id, error);
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

 protected:
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
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
  CoreAccountId account_id =
      identity_test_env()->MakeAccountAvailable(kTestEmail).account_id;
  identity_test_env()->SetPrimaryAccount(kTestEmail,
                                         signin::ConsentLevel::kSync);

  // Mark signout required.
  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(profile());
  service->set_signout_required_after_supervision_enabled();

  SetAuthError(
      identity_test_env()->identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSync),
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  EXPECT_FALSE(
      display_service_->GetNotification(kPrimaryAccountErrorNotificationId));
}

TEST_F(SigninErrorNotifierTest, ErrorResetForPrimaryAccount) {
  EXPECT_FALSE(
      display_service_->GetNotification(kPrimaryAccountErrorNotificationId));

  CoreAccountId account_id =
      identity_test_env()
          ->MakePrimaryAccountAvailable(kTestEmail, signin::ConsentLevel::kSync)
          .account_id;
  SetAuthError(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_TRUE(
      display_service_->GetNotification(kPrimaryAccountErrorNotificationId));

  SetAuthError(account_id, GoogleServiceAuthError::AuthErrorNone());
  EXPECT_FALSE(
      display_service_->GetNotification(kPrimaryAccountErrorNotificationId));
}

TEST_F(SigninErrorNotifierTest, ErrorShownForUnconsentedPrimaryAccount) {
  EXPECT_FALSE(
      display_service_->GetNotification(kPrimaryAccountErrorNotificationId));

  CoreAccountId account_id = identity_test_env()
                                 ->MakePrimaryAccountAvailable(
                                     kTestEmail, signin::ConsentLevel::kSignin)
                                 .account_id;
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

  CoreAccountId account_id =
      identity_test_env()->MakeAccountAvailable(kTestEmail).account_id;
  SetAuthError(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  // Uses the run loop from `BrowserTaskEnvironment`.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      display_service_->GetNotification(kSecondaryAccountErrorNotificationId));

  SetAuthError(account_id, GoogleServiceAuthError::AuthErrorNone());
  EXPECT_FALSE(
      display_service_->GetNotification(kSecondaryAccountErrorNotificationId));
}

TEST_F(SigninErrorNotifierTest, ErrorTransitionForPrimaryAccount) {
  CoreAccountId account_id =
      identity_test_env()
          ->MakePrimaryAccountAvailable(kTestEmail, signin::ConsentLevel::kSync)
          .account_id;
  SetAuthError(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(kPrimaryAccountErrorNotificationId);
  ASSERT_TRUE(notification);
  std::u16string message = notification->message();
  EXPECT_FALSE(message.empty());

  // Now set another auth error.
  SetAuthError(account_id,
               GoogleServiceAuthError(
                   GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE));

  notification =
      display_service_->GetNotification(kPrimaryAccountErrorNotificationId);
  ASSERT_TRUE(notification);
  std::u16string new_message = notification->message();
  EXPECT_FALSE(new_message.empty());

  ASSERT_NE(new_message, message);
}

// Verify that SigninErrorNotifier ignores certain errors.
TEST_F(SigninErrorNotifierTest, AuthStatusEnumerateAllErrors) {
  GoogleServiceAuthError::State table[] = {
      GoogleServiceAuthError::NONE,
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
      GoogleServiceAuthError::USER_NOT_SIGNED_UP,
      GoogleServiceAuthError::CONNECTION_FAILED,
      GoogleServiceAuthError::SERVICE_UNAVAILABLE,
      GoogleServiceAuthError::REQUEST_CANCELED,
      GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE,
      GoogleServiceAuthError::SERVICE_ERROR,
      GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR,
      GoogleServiceAuthError::CHALLENGE_RESPONSE_REQUIRED,
  };
  static_assert(
      std::size(table) == GoogleServiceAuthError::NUM_STATES -
                              GoogleServiceAuthError::kDeprecatedStateCount,
      "table size should match number of auth error types");
  CoreAccountId account_id =
      identity_test_env()
          ->MakePrimaryAccountAvailable(kTestEmail, signin::ConsentLevel::kSync)
          .account_id;

  for (size_t i = 0; i < std::size(table); ++i) {
    GoogleServiceAuthError error(table[i]);
    SetAuthError(account_id, error);
    std::optional<message_center::Notification> notification =
        display_service_->GetNotification(kPrimaryAccountErrorNotificationId);

    // Only non scope persistent errors are reported.
    bool expect_notification =
        error.IsPersistentError() && !error.IsScopePersistentError();
    ASSERT_EQ(expect_notification, !!notification) << "Failed case #" << i;
    if (!expect_notification)
      continue;

    ASSERT_TRUE(notification.has_value()) << "Failed case #" << i;
    EXPECT_FALSE(notification->title().empty());
    EXPECT_FALSE(notification->message().empty());
    EXPECT_EQ((size_t)1, notification->buttons().size());
    SetAuthError(account_id, GoogleServiceAuthError::AuthErrorNone());
  }
}

TEST_F(SigninErrorNotifierTest, ChildSecondaryAccountMigrationTest) {
  CoreAccountId primary_account =
      identity_test_env()
          ->MakePrimaryAccountAvailable(kTestEmail, signin::ConsentLevel::kSync)
          .account_id;
  CoreAccountId secondary_account =
      identity_test_env()->MakeAccountAvailable(kTestSecondaryEmail).account_id;

  // Mark the profile as a child user.
  GetProfile()->SetIsSupervisedProfile();
  base::RunLoop().RunUntilIdle();

  // Invalidate the secondary account.
  SetAuthError(
      secondary_account,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // Expect that there is a notification, accounts didn't migrate yet.
  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(kSecondaryAccountErrorNotificationId);
  ASSERT_TRUE(notification);
  std::u16string message = notification->message();
  EXPECT_FALSE(message.empty());

  // Clear error.
  SetAuthError(secondary_account, GoogleServiceAuthError::AuthErrorNone());
  EXPECT_FALSE(
      display_service_->GetNotification(kSecondaryAccountErrorNotificationId));

  // Mark secondary account as migrated, message should be different.
  profile()->GetPrefs()->SetBoolean(prefs::kEduCoexistenceArcMigrationCompleted,
                                    true);

  // Invalidate the secondary account.
  SetAuthError(
      secondary_account,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  notification =
      display_service_->GetNotification(kSecondaryAccountErrorNotificationId);
  ASSERT_TRUE(notification);
  std::u16string new_message = notification->message();
  EXPECT_NE(new_message, message);
}

// Tests that token handle errors display the expected error message.
TEST_F(SigninErrorNotifierTest, TokenHandleTest) {
  // Setup.
  const CoreAccountId core_account_id =
      identity_test_env()
          ->MakePrimaryAccountAvailable(kTestEmail, signin::ConsentLevel::kSync)
          .account_id;
  const AccountId account_id = AccountId::FromUserEmailGaiaId(
      /*user_email=*/kTestEmail, /*gaia_id=*/core_account_id.ToString());
  TokenHandleUtil::StoreTokenHandle(account_id, kTokenHandle);
  TokenHandleUtil::SetInvalidTokenForTesting(kTokenHandle);
  SigninErrorNotifier* signin_error_notifier =
      SigninErrorNotifierFactory::GetForProfile(GetProfile());
  signin_error_notifier->OnTokenHandleCheck(account_id, kTokenHandle,
                                            /*reauth_required=*/true);

  // Test.
  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(kPrimaryAccountErrorNotificationId);
  ASSERT_TRUE(notification);
  const std::u16string& message = notification->message();
  EXPECT_EQ(message, l10n_util::GetStringUTF16(
                         IDS_SYNC_TOKEN_HANDLE_ERROR_BUBBLE_VIEW_MESSAGE));
}

TEST_F(SigninErrorNotifierTest,
       TokenHandleErrorsDoNotDisplaySecondaryAccountErrors) {
  // Setup Secondary Account Error.
  CoreAccountId secondary_account =
      identity_test_env()->MakeAccountAvailable(kTestSecondaryEmail).account_id;
  SetAuthError(
      secondary_account,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // Setup Device Account.
  const CoreAccountId core_account_id =
      identity_test_env()
          ->MakePrimaryAccountAvailable(kTestEmail, signin::ConsentLevel::kSync)
          .account_id;
  const AccountId account_id = AccountId::FromUserEmailGaiaId(
      /*user_email=*/kTestEmail, /*gaia_id=*/core_account_id.ToString());
  TokenHandleUtil::StoreTokenHandle(account_id, kTokenHandle);
  TokenHandleUtil::SetInvalidTokenForTesting(kTokenHandle);
  SigninErrorNotifier* signin_error_notifier =
      SigninErrorNotifierFactory::GetForProfile(GetProfile());
  signin_error_notifier->OnTokenHandleCheck(account_id, kTokenHandle,
                                            /*reauth_required=*/true);

  // Test.
  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(kPrimaryAccountErrorNotificationId);
  ASSERT_TRUE(notification);
  const std::u16string& message = notification->message();
  EXPECT_EQ(message, l10n_util::GetStringUTF16(
                         IDS_SYNC_TOKEN_HANDLE_ERROR_BUBBLE_VIEW_MESSAGE));
}

}  // namespace ash
