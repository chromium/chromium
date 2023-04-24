// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/multi_capture_login_notification.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/origin.h"

namespace ash {

class MultiCaptureLoginNotificationTest : public AshTestBase {
 public:
  MultiCaptureLoginNotificationTest()
      : ash::AshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>())),
        active_user_account_id_(
            AccountId::FromUserEmail(/*user_email=*/"sample_user@gmail.com")) {}
  ~MultiCaptureLoginNotificationTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    std::unique_ptr<FakeChromeUserManager> user_manager =
        std::make_unique<FakeChromeUserManager>();
    user_manager_ = user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
    user_manager_->AddUser(active_user_account_id_);

    CreateUserSessions(1);

    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_NONE,
                                        LoginState::LOGGED_IN_USER_NONE);
    multi_capture_login_notification_ =
        std::make_unique<MultiCaptureLoginNotification>();
    notification_count_ = 0u;
    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    tester_ = std::make_unique<NotificationDisplayServiceTester>(
        /*profile=*/nullptr);
    tester_->SetNotificationAddedClosure(base::BindRepeating(
        &MultiCaptureLoginNotificationTest::OnNotificationAdded,
        base::Unretained(this)));
  }

  void TearDown() override {
    multi_capture_login_notification_.reset();
    AshTestBase::TearDown();
  }

  absl::optional<message_center::Notification> GetNotification() {
    return tester_->GetNotification("multi_capture_on_login");
  }

  void OnNotificationAdded() { notification_count_++; }

 protected:
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
  std::unique_ptr<MultiCaptureLoginNotification>
      multi_capture_login_notification_;
  AccountId active_user_account_id_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  raw_ptr<FakeChromeUserManager, ExperimentalAsh> user_manager_;

  unsigned int notification_count_;
};

TEST_F(MultiCaptureLoginNotificationTest, NotificationTriggeredOnLogin) {
  LoginState* login_state = LoginState::Get();
  SetIsMultiCaptureAllowedCallbackForTesting(
      /*is_multi_capture_allowed=*/true);
  EXPECT_EQ(0u, notification_count_);

  user_manager_->SwitchActiveUser(active_user_account_id_);
  user_manager_->SimulateUserProfileLoad(active_user_account_id_);
  login_state->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR);

  absl::optional<message_center::Notification> notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(u"Your screen might be recorded", notification->title());
  EXPECT_EQ(
      u"You'll see a notification if recording starts on this managed device",
      notification->message());
  EXPECT_EQ(1u, notification_count_);
}

TEST_F(MultiCaptureLoginNotificationTest,
       FeatureDisabledNotificationNotTriggeredOnLogin) {
  LoginState* login_state = LoginState::Get();
  SetIsMultiCaptureAllowedCallbackForTesting(
      /*is_multi_capture_allowed=*/false);
  EXPECT_EQ(0u, notification_count_);

  user_manager_->SwitchActiveUser(active_user_account_id_);
  user_manager_->SimulateUserProfileLoad(active_user_account_id_);
  login_state->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR);

  absl::optional<message_center::Notification> notification = GetNotification();
  ASSERT_FALSE(notification);
  EXPECT_EQ(0u, notification_count_);
}

TEST_F(MultiCaptureLoginNotificationTest, NotLoggedInNoNotification) {
  LoginState* login_state = LoginState::Get();
  SetIsMultiCaptureAllowedCallbackForTesting(
      /*is_multi_capture_allowed=*/true);
  EXPECT_EQ(0u, notification_count_);

  user_manager_->SwitchActiveUser(active_user_account_id_);
  user_manager_->SimulateUserProfileLoad(active_user_account_id_);
  login_state->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_NONE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_NONE);

  absl::optional<message_center::Notification> notification = GetNotification();
  ASSERT_FALSE(notification);
  EXPECT_EQ(0u, notification_count_);
}

}  // namespace ash
