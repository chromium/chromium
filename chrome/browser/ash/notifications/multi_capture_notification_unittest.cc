// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/multi_capture_notification.h"

#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/origin.h"

namespace {
constexpr base::TimeDelta kMinimumNotificationPresenceTime = base::Seconds(6);
}  // namespace

namespace ash {

class MultiCaptureNotificationTest : public BrowserWithTestWindowTest {
 public:
  MultiCaptureNotificationTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~MultiCaptureNotificationTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    UserDataAuthClient::InitializeFake();

    auto user_manager = std::make_unique<user_manager::FakeUserManager>();
    user_manager_ = user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    tester_ = std::make_unique<NotificationDisplayServiceTester>(
        /*profile=*/nullptr);
    tester_->SetNotificationAddedClosure(
        base::BindRepeating(&MultiCaptureNotificationTest::OnNotificationAdded,
                            base::Unretained(this)));
    tester_->SetNotificationClosedClosure(base::BindRepeating(
        &MultiCaptureNotificationTest::OnNotificationRemoved,
        base::Unretained(this)));
    multi_capture_notification_ = std::make_unique<MultiCaptureNotification>();
    notification_count_ = 0u;
  }

  void TearDown() override {
    multi_capture_notification_.reset();
    UserDataAuthClient::Shutdown();
    BrowserWithTestWindowTest::TearDown();
  }

  absl::optional<message_center::Notification> GetNotification(
      const std::string& origin) {
    return tester_->GetNotification(base::StrCat({"multi_capture:", origin}));
  }

  void OnNotificationAdded() { notification_count_++; }
  void OnNotificationRemoved() { notification_count_--; }

  void CheckNotification(const std::u16string& origin) {
    absl::optional<message_center::Notification> notification =
        GetNotification(base::UTF16ToUTF8(origin));
    ASSERT_TRUE(notification);
    EXPECT_EQ(u"", notification->title());
    EXPECT_EQ(u"Your system administrator has allowed " + origin +
                  u" to record your screen",
              notification->message());
  }

 protected:
  user_manager::FakeUserManager* user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
  std::unique_ptr<MultiCaptureNotification> multi_capture_notification_;
  unsigned int notification_count_;
};

TEST_F(MultiCaptureNotificationTest,
       NotificationStartedAndStoppedAfterSixSeconds) {
  const url::Origin example_origin = url::Origin::CreateFromNormalizedTuple(
      /*scheme=*/"https", /*host=*/"example.com", /*port=*/443);
  multi_capture_notification_->MultiCaptureStarted(
      /*label=*/"test_label_1", example_origin);
  CheckNotification(u"example.com");
  EXPECT_EQ(1u, notification_count_);

  task_environment()->FastForwardBy(kMinimumNotificationPresenceTime +
                                    base::Milliseconds(1));
  multi_capture_notification_->MultiCaptureStopped(/*label=*/"test_label_1");
  EXPECT_EQ(0u, notification_count_);
}

TEST_F(MultiCaptureNotificationTest,
       NotificationsWithDifferentOriginsStartedAndStoppedAfterSixSeconds) {
  multi_capture_notification_->MultiCaptureStarted(
      /*label=*/"test_label_1",
      /*origin=*/url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"https", /*host=*/"example.com", /*port=*/443));
  multi_capture_notification_->MultiCaptureStarted(
      /*label=*/"test_label_2",
      /*origin=*/url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"https", /*host=*/"anotherexample.com", /*port=*/443));
  CheckNotification(u"example.com");
  CheckNotification(u"anotherexample.com");
  EXPECT_EQ(2u, notification_count_);

  task_environment()->FastForwardBy(kMinimumNotificationPresenceTime +
                                    base::Milliseconds(1));
  multi_capture_notification_->MultiCaptureStopped(/*label=*/"test_label_1");
  EXPECT_EQ(1u, notification_count_);
  EXPECT_FALSE(GetNotification("example.com").has_value());
  CheckNotification(u"anotherexample.com");

  multi_capture_notification_->MultiCaptureStopped(/*label=*/"test_label_2");
  EXPECT_EQ(0u, notification_count_);
  EXPECT_FALSE(GetNotification("example.com").has_value());
  EXPECT_FALSE(GetNotification("anotherexample.com").has_value());
}

TEST_F(MultiCaptureNotificationTest,
       FastNotificationStartedAndStoppedExpectedClosingDelay) {
  const url::Origin example_origin = url::Origin::CreateFromNormalizedTuple(
      /*scheme=*/"https", /*host=*/"example.com", /*port=*/443);
  multi_capture_notification_->MultiCaptureStarted(
      /*label=*/"test_label_1", example_origin);
  CheckNotification(u"example.com");
  EXPECT_EQ(1u, notification_count_);

  task_environment()->FastForwardBy(kMinimumNotificationPresenceTime -
                                    base::Milliseconds(1));
  multi_capture_notification_->MultiCaptureStopped(/*label=*/"test_label_1");
  EXPECT_TRUE(GetNotification("example.com").has_value());
  EXPECT_EQ(1u, notification_count_);

  task_environment()->FastForwardBy(base::Milliseconds(2));
  EXPECT_EQ(0u, notification_count_);
}

TEST_F(
    MultiCaptureNotificationTest,
    FastNotificationsWithDifferentOriginsStartedAndStoppedExpectedClosingDelay) {
  multi_capture_notification_->MultiCaptureStarted(
      /*label=*/"test_label_1",
      /*origin=*/url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"https", /*host=*/"example.com", /*port=*/443));
  multi_capture_notification_->MultiCaptureStarted(
      /*label=*/"test_label_2",
      /*origin=*/url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"https", /*host=*/"anotherexample.com", /*port=*/443));
  CheckNotification(u"example.com");
  CheckNotification(u"anotherexample.com");
  EXPECT_EQ(2u, notification_count_);

  task_environment()->FastForwardBy(kMinimumNotificationPresenceTime -
                                    base::Milliseconds(1));
  multi_capture_notification_->MultiCaptureStopped(/*label=*/"test_label_1");
  CheckNotification(u"example.com");
  CheckNotification(u"anotherexample.com");
  EXPECT_EQ(2u, notification_count_);

  multi_capture_notification_->MultiCaptureStopped(/*label=*/"test_label_2");
  EXPECT_EQ(2u, notification_count_);

  task_environment()->FastForwardBy(base::Milliseconds(2));
  EXPECT_EQ(0u, notification_count_);
}

}  // namespace ash
