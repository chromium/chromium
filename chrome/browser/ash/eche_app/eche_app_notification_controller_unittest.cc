// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/eche_app/eche_app_notification_controller.h"

#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace eche_app {

class TestableNotificationController : public EcheAppNotificationController {
 public:
  explicit TestableNotificationController(Profile* profile)
      : EcheAppNotificationController(profile) {}
  ~TestableNotificationController() override = default;
  TestableNotificationController(const TestableNotificationController&) =
      delete;
  TestableNotificationController& operator=(
      const TestableNotificationController&) = delete;

  // EcheAppNotificationController:
  MOCK_METHOD0(LaunchSettings, void());
  MOCK_METHOD0(LaunchLearnMore, void());
  MOCK_METHOD0(LaunchTryAgain, void());
  MOCK_METHOD0(LaunchHelp, void());
};

class EcheAppNotificationControllerTest : public BrowserWithTestWindowTest {
 protected:
  EcheAppNotificationControllerTest() = default;
  ~EcheAppNotificationControllerTest() override = default;
  EcheAppNotificationControllerTest(const EcheAppNotificationControllerTest&) =
      delete;
  EcheAppNotificationControllerTest& operator=(
      const EcheAppNotificationControllerTest&) = delete;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
    notification_controller_ =
        std::make_unique<testing::StrictMock<TestableNotificationController>>(
            profile());
  }

  std::unique_ptr<testing::StrictMock<TestableNotificationController>>
      notification_controller_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;

  void Initialize(mojom::WebNotificationType type) {
    absl::optional<std::u16string> title = u"title";
    absl::optional<std::u16string> message = u"message";
    notification_controller_->ShowNotificationFromWebUI(title, message, type);
  }

  void VerifyNotificationHasAction(
      absl::optional<message_center::Notification>& notification) {
    ASSERT_TRUE(notification);
    ASSERT_EQ(2u, notification->buttons().size());
    EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());

    // Clicking the first notification button should launch try again.
    EXPECT_CALL(*notification_controller_, LaunchTryAgain());
    notification->delegate()->Click(0, absl::nullopt);

    // Clicking the second notification button should launch help.
    EXPECT_CALL(*notification_controller_, LaunchHelp());
    notification->delegate()->Click(1, absl::nullopt);
  }
};

TEST_F(EcheAppNotificationControllerTest, ShowScreenLockNotification) {
  absl::optional<std::u16string> title = u"title";
  notification_controller_->ShowScreenLockNotification(title);
  absl::optional<message_center::Notification> notification =
      display_service_->GetNotification(kEcheAppScreenLockNotifierId);
  ASSERT_TRUE(notification);
  ASSERT_EQ(2u, notification->buttons().size());
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());

  // Clicking the first notification button should launch settings.
  EXPECT_CALL(*notification_controller_, LaunchSettings());
  notification->delegate()->Click(0, absl::nullopt);

  // Clicking the second notification button should launch learn more.
  EXPECT_CALL(*notification_controller_, LaunchLearnMore());
  notification->delegate()->Click(1, absl::nullopt);
}

TEST_F(EcheAppNotificationControllerTest, ShowDisabledByPhoneNotification) {
  absl::optional<std::u16string> title = u"title";
  notification_controller_->ShowDisabledByPhoneNotification(title);
  absl::optional<message_center::Notification> notification =
      display_service_->GetNotification(kEcheAppDisabledByPhoneNotifierId);
  ASSERT_TRUE(notification);
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());
}

}  // namespace eche_app
}  // namespace ash
