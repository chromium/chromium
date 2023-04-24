// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/authpolicy/authpolicy_credentials_manager.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/authpolicy/fake_authpolicy_client.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kProfileSigninNotificationId[] = "chrome://settings/signin/";
constexpr char kProfileEmail[] = "user@example.com";
constexpr char kDisplayName[] = "DisplayName";
constexpr char16_t kDisplayName16[] = u"DisplayName";
constexpr char kGivenName[] = "Given Name";
constexpr char16_t kGivenName16[] = u"Given Name";

}  // namespace

class AuthPolicyCredentialsManagerTest : public testing::Test {
 public:
  AuthPolicyCredentialsManagerTest()
      : local_state_(TestingBrowserProcess::GetGlobal()),
        user_manager_enabler_(std::make_unique<FakeChromeUserManager>()) {}

  AuthPolicyCredentialsManagerTest(const AuthPolicyCredentialsManagerTest&) =
      delete;
  AuthPolicyCredentialsManagerTest& operator=(
      const AuthPolicyCredentialsManagerTest&) = delete;

  ~AuthPolicyCredentialsManagerTest() override = default;

  void SetUp() override {
    AuthPolicyClient::InitializeFake();
    fake_authpolicy_client()->DisableOperationDelayForTesting();

    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(kProfileEmail);
    account_id_ =
        AccountId::AdFromUserEmailObjGuid(kProfileEmail, "1234567890");
    auto* user = fake_user_manager()->AddUser(account_id_);

    base::RunLoop run_loop;
    fake_authpolicy_client()->set_on_get_status_closure(run_loop.QuitClosure());

    profile_ = profile_builder.Build();
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());

    authpolicy_credentials_manager_ =
        static_cast<AuthPolicyCredentialsManager*>(
            AuthPolicyCredentialsManagerFactory::GetInstance()
                ->GetServiceForBrowserContext(profile(), false /* create */));
    EXPECT_TRUE(authpolicy_credentials_manager_);

    run_loop.Run();
    EXPECT_FALSE(user->force_online_signin());
  }

  void TearDown() override {
    profile_.reset();
    AuthPolicyClient::Shutdown();
  }

 protected:
  AccountId& account_id() { return account_id_; }
  TestingProfile* profile() { return profile_.get(); }
  AuthPolicyCredentialsManager* authpolicy_credentials_manager() {
    return authpolicy_credentials_manager_;
  }
  FakeAuthPolicyClient* fake_authpolicy_client() const {
    return FakeAuthPolicyClient::Get();
  }

  FakeChromeUserManager* fake_user_manager() {
    return static_cast<FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  int GetNumberOfNotifications() {
    return display_service_
        ->GetDisplayedNotificationsForType(NotificationHandler::Type::TRANSIENT)
        .size();
  }

  void CancelNotificationById(int message_id) {
    const std::string notification_id = kProfileSigninNotificationId +
                                        profile()->GetProfileUserName() +
                                        base::NumberToString(message_id);
    EXPECT_TRUE(display_service_->GetNotification(notification_id));
    display_service_->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                         notification_id, false);
  }

  void CallGetUserStatusAndWait() {
    base::RunLoop run_loop;
    fake_authpolicy_client()->set_on_get_status_closure(run_loop.QuitClosure());
    authpolicy_credentials_manager()->GetUserStatus();
    run_loop.Run();
  }

  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;

  NetworkHandlerTestHelper network_handler_test_helper_;
  AccountId account_id_;
  std::unique_ptr<TestingProfile> profile_;

  // Owned by AuthPolicyCredentialsManagerFactory.
  raw_ptr<AuthPolicyCredentialsManager, ExperimentalAsh>
      authpolicy_credentials_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
};

// Tests saving display and given name into user manager. No error means no
// notifications are shown.
TEST_F(AuthPolicyCredentialsManagerTest, SaveNames) {
  fake_authpolicy_client()->set_display_name(kDisplayName);
  fake_authpolicy_client()->set_given_name(kGivenName);

  CallGetUserStatusAndWait();
  EXPECT_EQ(0, GetNumberOfNotifications());
  const auto* user = fake_user_manager()->FindUser(account_id());
  ASSERT_TRUE(user);
  EXPECT_EQ(kDisplayName16, user->display_name());
  EXPECT_EQ(kGivenName16, user->GetGivenName());
}

// Tests notification is shown at most once for the same error.
TEST_F(AuthPolicyCredentialsManagerTest, ShowSameNotificationOnce) {
  // In case of expired password save to force online signin and show
  // notification.
  fake_authpolicy_client()->set_password_status(
      authpolicy::ActiveDirectoryUserStatus::PASSWORD_EXPIRED);
  CallGetUserStatusAndWait();
  EXPECT_EQ(1, GetNumberOfNotifications());
  EXPECT_TRUE(
      fake_user_manager()->FindUser(account_id())->force_online_signin());
  CancelNotificationById(IDS_ACTIVE_DIRECTORY_PASSWORD_EXPIRED);

  // Do not show the same notification twice.
  CallGetUserStatusAndWait();
  EXPECT_EQ(0, GetNumberOfNotifications());
  EXPECT_TRUE(
      fake_user_manager()->FindUser(account_id())->force_online_signin());
}

// Tests both notifications are shown if different errors occurs.
TEST_F(AuthPolicyCredentialsManagerTest, ShowDifferentNotifications) {
  // In case of expired password save to force online signin and show
  // notification.
  fake_authpolicy_client()->set_password_status(
      authpolicy::ActiveDirectoryUserStatus::PASSWORD_CHANGED);
  fake_authpolicy_client()->set_tgt_status(
      authpolicy::ActiveDirectoryUserStatus::TGT_EXPIRED);
  CallGetUserStatusAndWait();
  EXPECT_EQ(2, GetNumberOfNotifications());
  EXPECT_TRUE(
      fake_user_manager()->FindUser(account_id())->force_online_signin());
  CancelNotificationById(IDS_ACTIVE_DIRECTORY_PASSWORD_CHANGED);
  CancelNotificationById(IDS_ACTIVE_DIRECTORY_REFRESH_AUTH_TOKEN);
  EXPECT_EQ(0, GetNumberOfNotifications());
}

// Tests invalid TGT status does not force online signin but still shows
// a notification.
TEST_F(AuthPolicyCredentialsManagerTest, InvalidTGTDoesntForceOnlineSignin) {
  fake_authpolicy_client()->set_tgt_status(
      authpolicy::ActiveDirectoryUserStatus::TGT_EXPIRED);
  CallGetUserStatusAndWait();
  EXPECT_FALSE(
      fake_user_manager()->FindUser(account_id())->force_online_signin());
  EXPECT_EQ(1, GetNumberOfNotifications());
  CancelNotificationById(IDS_ACTIVE_DIRECTORY_REFRESH_AUTH_TOKEN);
  EXPECT_EQ(0, GetNumberOfNotifications());
}

// Tests successfull case does not show any notification and does not force
// online signin.
TEST_F(AuthPolicyCredentialsManagerTest, Success_NoNotifications) {
  CallGetUserStatusAndWait();
  EXPECT_EQ(0, GetNumberOfNotifications());
  EXPECT_FALSE(
      fake_user_manager()->FindUser(account_id())->force_online_signin());
}

}  // namespace ash
