// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/multi_capture_notifications.h"

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/origin.h"

namespace {
constexpr base::TimeDelta kMinimumNotificationPresenceTime = base::Seconds(6);
}  // namespace

namespace ash {

class MultiCaptureNotificationsTest : public AshTestBase {
 public:
  MultiCaptureNotificationsTest()
      : AshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::TimeSource::MOCK_TIME))),
        active_user_account_id_(
            AccountId::FromUserEmail(/*user_email=*/"sample_user@gmail.com")) {}
  ~MultiCaptureNotificationsTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    UserDataAuthClient::InitializeFake();

    std::unique_ptr<FakeChromeUserManager> user_manager =
        std::make_unique<FakeChromeUserManager>();
    user_manager_ = user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
    user_manager_->AddUser(active_user_account_id_);

    CreateUserSessions(1);

    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_NONE,
                                        LoginState::LOGGED_IN_USER_NONE);
    multi_capture_notifications_ =
        std::make_unique<MultiCaptureNotifications>();
    notification_count_ = 0u;
    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    tester_ = std::make_unique<NotificationDisplayServiceTester>(
        /*profile=*/nullptr);
    tester_->SetNotificationAddedClosure(
        base::BindRepeating(&MultiCaptureNotificationsTest::OnNotificationAdded,
                            base::Unretained(this)));
    tester_->SetNotificationClosedClosure(base::BindRepeating(
        &MultiCaptureNotificationsTest::OnNotificationRemoved,
        base::Unretained(this)));
    notification_count_ = 0u;
  }

  void TearDown() override {
    multi_capture_notifications_.reset();
    UserDataAuthClient::Shutdown();
    AshTestBase::TearDown();
  }

  absl::optional<message_center::Notification> GetLoginNotification() {
    return tester_->GetNotification("multi_capture_on_login");
  }

  absl::optional<message_center::Notification> GetCaptureNotification(
      const std::string& origin) {
    return tester_->GetNotification(base::StrCat({"multi_capture:", origin}));
  }

  void CheckCaptureNotification(const std::u16string& origin) {
    absl::optional<message_center::Notification> notification =
        GetCaptureNotification(base::UTF16ToUTF8(origin));
    ASSERT_TRUE(notification);
    EXPECT_EQ(origin + u" is recording your screen", notification->title());
    EXPECT_EQ(u"Your system administrator has allowed " + origin +
                  u" to record your screen",
              notification->message());
  }

  void OnNotificationAdded() { notification_count_++; }
  void OnNotificationRemoved() { notification_count_--; }

 protected:
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
  std::unique_ptr<MultiCaptureNotifications> multi_capture_notifications_;
  AccountId active_user_account_id_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  raw_ptr<FakeChromeUserManager> user_manager_ = nullptr;

  unsigned int notification_count_;
};

TEST_F(MultiCaptureNotificationsTest, LoginNotificationTriggeredOnLogin) {
  LoginState* login_state = LoginState::Get();
  SetIsMultiCaptureAllowedForTesting(
      /*is_multi_capture_allowed=*/true);
  EXPECT_EQ(0u, notification_count_);

  user_manager_->SwitchActiveUser(active_user_account_id_);
  user_manager_->SimulateUserProfileLoad(active_user_account_id_);
  login_state->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR);

  absl::optional<message_center::Notification> notification =
      GetLoginNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(u"Your screen might be recorded", notification->title());
  EXPECT_EQ(
      u"You'll see a notification if recording starts on this managed device",
      notification->message());
  EXPECT_EQ(1u, notification_count_);
}

TEST_F(MultiCaptureNotificationsTest,
       LoginFeatureDisabledNotificationNotTriggeredOnLogin) {
  LoginState* login_state = LoginState::Get();
  SetIsMultiCaptureAllowedForTesting(
      /*is_multi_capture_allowed=*/false);
  EXPECT_EQ(0u, notification_count_);

  user_manager_->SwitchActiveUser(active_user_account_id_);
  user_manager_->SimulateUserProfileLoad(active_user_account_id_);
  login_state->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR);

  absl::optional<message_center::Notification> notification =
      GetLoginNotification();
  ASSERT_FALSE(notification);
  EXPECT_EQ(0u, notification_count_);
}

TEST_F(MultiCaptureNotificationsTest, LoginNotLoggedInNoNotification) {
  LoginState* login_state = LoginState::Get();
  SetIsMultiCaptureAllowedForTesting(
      /*is_multi_capture_allowed=*/true);
  EXPECT_EQ(0u, notification_count_);

  user_manager_->SwitchActiveUser(active_user_account_id_);
  user_manager_->SimulateUserProfileLoad(active_user_account_id_);
  login_state->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_NONE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_NONE);

  absl::optional<message_center::Notification> notification =
      GetLoginNotification();
  ASSERT_FALSE(notification);
  EXPECT_EQ(0u, notification_count_);
}

TEST_F(MultiCaptureNotificationsTest,
       CaptureNotificationStartedAndStoppedAfterSixSeconds) {
  const url::Origin example_origin = url::Origin::CreateFromNormalizedTuple(
      /*scheme=*/"https", /*host=*/"example.com", /*port=*/443);
  multi_capture_notifications_->MultiCaptureStarted(
      /*label=*/"test_label_1", example_origin);
  CheckCaptureNotification(u"example.com");
  EXPECT_EQ(1u, notification_count_);

  task_environment()->FastForwardBy(kMinimumNotificationPresenceTime +
                                    base::Milliseconds(1));
  multi_capture_notifications_->MultiCaptureStopped(/*label=*/"test_label_1");
  EXPECT_EQ(0u, notification_count_);
}

TEST_F(
    MultiCaptureNotificationsTest,
    CaptureNotificationsWithDifferentOriginsStartedAndStoppedAfterSixSeconds) {
  multi_capture_notifications_->MultiCaptureStarted(
      /*label=*/"test_label_1",
      /*origin=*/url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"https", /*host=*/"example.com", /*port=*/443));
  multi_capture_notifications_->MultiCaptureStarted(
      /*label=*/"test_label_2",
      /*origin=*/url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"https", /*host=*/"anotherexample.com", /*port=*/443));
  CheckCaptureNotification(u"example.com");
  CheckCaptureNotification(u"anotherexample.com");
  EXPECT_EQ(2u, notification_count_);

  task_environment()->FastForwardBy(kMinimumNotificationPresenceTime +
                                    base::Milliseconds(1));
  multi_capture_notifications_->MultiCaptureStopped(/*label=*/"test_label_1");
  EXPECT_EQ(1u, notification_count_);
  EXPECT_FALSE(GetCaptureNotification("example.com").has_value());
  CheckCaptureNotification(u"anotherexample.com");

  multi_capture_notifications_->MultiCaptureStopped(/*label=*/"test_label_2");
  EXPECT_EQ(0u, notification_count_);
  EXPECT_FALSE(GetCaptureNotification("example.com").has_value());
  EXPECT_FALSE(GetCaptureNotification("anotherexample.com").has_value());
}

TEST_F(MultiCaptureNotificationsTest,
       CaptureFastNotificationStartedAndStoppedExpectedClosingDelay) {
  const url::Origin example_origin = url::Origin::CreateFromNormalizedTuple(
      /*scheme=*/"https", /*host=*/"example.com", /*port=*/443);
  multi_capture_notifications_->MultiCaptureStarted(
      /*label=*/"test_label_1", example_origin);
  CheckCaptureNotification(u"example.com");
  EXPECT_EQ(1u, notification_count_);

  task_environment()->FastForwardBy(kMinimumNotificationPresenceTime -
                                    base::Milliseconds(1));
  multi_capture_notifications_->MultiCaptureStopped(/*label=*/"test_label_1");
  EXPECT_TRUE(GetCaptureNotification("example.com").has_value());
  EXPECT_EQ(1u, notification_count_);

  task_environment()->FastForwardBy(base::Milliseconds(2));
  EXPECT_EQ(0u, notification_count_);
}

TEST_F(
    MultiCaptureNotificationsTest,
    CaptureFastNotificationsWithDifferentOriginsStartedAndStoppedExpectedClosingDelay) {
  multi_capture_notifications_->MultiCaptureStarted(
      /*label=*/"test_label_1",
      /*origin=*/url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"https", /*host=*/"example.com", /*port=*/443));
  multi_capture_notifications_->MultiCaptureStarted(
      /*label=*/"test_label_2",
      /*origin=*/url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"https", /*host=*/"anotherexample.com", /*port=*/443));
  CheckCaptureNotification(u"example.com");
  CheckCaptureNotification(u"anotherexample.com");
  EXPECT_EQ(2u, notification_count_);

  task_environment()->FastForwardBy(kMinimumNotificationPresenceTime -
                                    base::Milliseconds(1));
  multi_capture_notifications_->MultiCaptureStopped(/*label=*/"test_label_1");
  CheckCaptureNotification(u"example.com");
  CheckCaptureNotification(u"anotherexample.com");
  EXPECT_EQ(2u, notification_count_);

  multi_capture_notifications_->MultiCaptureStopped(/*label=*/"test_label_2");
  EXPECT_EQ(2u, notification_count_);

  task_environment()->FastForwardBy(base::Milliseconds(2));
  EXPECT_EQ(0u, notification_count_);
}

}  // namespace ash
