// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/eche_app/eche_app_notification_controller.h"

#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/webui/eche_app_ui/eche_alert_generator.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace eche_app {

namespace {

void RelaunchEcheApp(Profile* profile) {}

}  // namespace

class TestableNotificationController : public EcheAppNotificationController {
 public:
  explicit TestableNotificationController(
      Profile* profile,
      const base::RepeatingCallback<void(Profile*)>& relaunch_callback)
      : EcheAppNotificationController(profile, relaunch_callback) {}
  ~TestableNotificationController() override = default;
  TestableNotificationController(const TestableNotificationController&) =
      delete;
  TestableNotificationController& operator=(
      const TestableNotificationController&) = delete;

  // EcheAppNotificationController:
  MOCK_METHOD0(LaunchSettings, void());
  MOCK_METHOD0(LaunchTryAgain, void());
  MOCK_METHOD0(LaunchNetworkSettings, void());
};

class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
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
            profile(), base::BindRepeating(&RelaunchEcheApp));
  }

  std::unique_ptr<testing::StrictMock<TestableNotificationController>>
      notification_controller_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;

  void Initialize(mojom::WebNotificationType type) {
    std::optional<std::u16string> title = u"title";
    std::optional<std::u16string> message = u"message";
    notification_controller_->ShowNotificationFromWebUI(title, message, type);
  }

  void VerifyNotificationHasAction(
      std::optional<message_center::Notification>& notification) {
    ASSERT_TRUE(notification);
    ASSERT_EQ(2u, notification->buttons().size());
    EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());

    // Clicking the notification button should launch try again.
    EXPECT_CALL(*notification_controller_, LaunchTryAgain());
    notification->delegate()->Click(0, std::nullopt);
  }

 private:
  MockNewWindowDelegate new_window_delegate_;
};

TEST_F(EcheAppNotificationControllerTest, ShowNotificationFromWebUI) {
  std::optional<std::u16string> title = u"Connection Fail Title";
  std::optional<std::u16string> message = u"Connection Fail Message";
  notification_controller_->ShowNotificationFromWebUI(
      title, message, mojom::WebNotificationType::CONNECTION_FAILED);
  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(kEcheAppRetryConnectionNotifierId);
  ASSERT_TRUE(notification);
  ASSERT_EQ(1u, notification->buttons().size());
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());
  EXPECT_EQ(notification->title(), title);
  EXPECT_EQ(notification->message(), message);

  // Clicking the notification button should relaunch again.
  EXPECT_CALL(*notification_controller_, LaunchTryAgain());
  notification->delegate()->Click(std::nullopt, std::nullopt);

  title = u"Connection Lost Title";
  message = u"Connection Lost Message";
  notification_controller_->ShowNotificationFromWebUI(
      title, message, mojom::WebNotificationType::CONNECTION_LOST);
  notification =
      display_service_->GetNotification(kEcheAppRetryConnectionNotifierId);
  ASSERT_TRUE(notification.has_value());
  ASSERT_EQ(1u, notification->buttons().size());
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());
  EXPECT_EQ(notification->title(), title);
  EXPECT_EQ(notification->message(), message);

  // Clicking the notification button should relaunch again.
  EXPECT_CALL(*notification_controller_, LaunchTryAgain());
  notification->delegate()->Click(std::nullopt, std::nullopt);

  title = u"Inactivity Title";
  message = u"Inactivity Message";
  notification_controller_->ShowNotificationFromWebUI(
      title, message, mojom::WebNotificationType::DEVICE_IDLE);
  notification =
      display_service_->GetNotification(kEcheAppInactivityNotifierId);
  ASSERT_TRUE(notification.has_value());
  ASSERT_EQ(1u, notification->buttons().size());
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());
  EXPECT_EQ(notification->title(), title);
  EXPECT_EQ(notification->message(), message);

  // Clicking the first notification button should relaunch again.
  EXPECT_CALL(*notification_controller_, LaunchTryAgain());
  notification->delegate()->Click(std::nullopt, std::nullopt);

  title = u"Check WIFI Title";
  message = u"Check WIFI Message";
  notification_controller_->ShowNotificationFromWebUI(
      title, message, mojom::WebNotificationType::WIFI_NOT_READY);
  notification =
      display_service_->GetNotification(kEcheAppNetworkSettingNotifierId);
  ASSERT_TRUE(notification.has_value());
  ASSERT_EQ(1u, notification->buttons().size());
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());
  EXPECT_EQ(notification->title(), title);
  EXPECT_EQ(notification->message(), message);

  // Clicking the notification button should launch network settings.
  EXPECT_CALL(*notification_controller_, LaunchNetworkSettings());
  notification->delegate()->Click(std::nullopt, std::nullopt);
}

TEST_F(EcheAppNotificationControllerTest, ShowScreenLockNotification) {
  std::u16string title = u"title";
  notification_controller_->ShowScreenLockNotification(title);
  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(kEcheAppScreenLockNotifierId);
  ASSERT_TRUE(notification.has_value());
  ASSERT_TRUE(notification->title().size() > 0);
  ASSERT_TRUE(notification->message().size() > 0);
  ASSERT_EQ(1u, notification->buttons().size());
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());

  // Clicking the notification button should launch settings.
  EXPECT_CALL(*notification_controller_, LaunchSettings());
  notification->delegate()->Click(std::nullopt, std::nullopt);
}

TEST_F(EcheAppNotificationControllerTest,
       ShowScreenLockNotificationWithNullValue) {
  // Null value for title should still show a degraded message.
  std::u16string title;
  notification_controller_->ShowScreenLockNotification(title);
  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(kEcheAppScreenLockNotifierId);
  ASSERT_TRUE(notification.has_value());
  ASSERT_TRUE(notification->title().size() > 0);
  ASSERT_TRUE(notification->message().size() > 0);
  ASSERT_EQ(1u, notification->buttons().size());
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());

  // Clicking the notification button should launch settings.
  EXPECT_CALL(*notification_controller_, LaunchSettings());
  notification->delegate()->Click(std::nullopt, std::nullopt);
}

TEST_F(EcheAppNotificationControllerTest, CloseNotification) {
  std::u16string title = u"title";
  notification_controller_->ShowScreenLockNotification(title);
  notification_controller_->CloseNotification(kEcheAppScreenLockNotifierId);
  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(kEcheAppScreenLockNotifierId);
  ASSERT_FALSE(notification.has_value());

  std::optional<std::u16string> message = u"message";
  notification_controller_->ShowNotificationFromWebUI(
      title, message, mojom::WebNotificationType::CONNECTION_FAILED);
  notification_controller_->CloseNotification(
      kEcheAppRetryConnectionNotifierId);
  notification =
      display_service_->GetNotification(kEcheAppRetryConnectionNotifierId);
  ASSERT_FALSE(notification.has_value());

  notification_controller_->ShowNotificationFromWebUI(
      title, message, mojom::WebNotificationType::DEVICE_IDLE);
  notification_controller_->CloseNotification(kEcheAppInactivityNotifierId);
  notification =
      display_service_->GetNotification(kEcheAppInactivityNotifierId);
  ASSERT_FALSE(notification.has_value());

  notification_controller_->ShowNotificationFromWebUI(
      title, message, mojom::WebNotificationType::INVALID_NOTIFICATION);
  notification_controller_->CloseNotification(
      kEcheAppFromWebWithoutButtonNotifierId);
  notification =
      display_service_->GetNotification(kEcheAppFromWebWithoutButtonNotifierId);
  ASSERT_FALSE(notification.has_value());

  notification_controller_->ShowNotificationFromWebUI(
      title, message, mojom::WebNotificationType::WIFI_NOT_READY);
  notification_controller_->CloseNotification(
      kEcheAppFromWebWithoutButtonNotifierId);
  notification =
      display_service_->GetNotification(kEcheAppFromWebWithoutButtonNotifierId);
  ASSERT_FALSE(notification.has_value());
}

TEST_F(EcheAppNotificationControllerTest,
       CloseConnectionOrLaunchErrorNotifications) {
  std::u16string title = u"title";
  std::optional<std::u16string> message = u"message";
  notification_controller_->ShowScreenLockNotification(title);
  notification_controller_->ShowNotificationFromWebUI(
      title, message, mojom::WebNotificationType::CONNECTION_FAILED);
  notification_controller_->ShowNotificationFromWebUI(
      title, message, mojom::WebNotificationType::DEVICE_IDLE);
  notification_controller_->ShowNotificationFromWebUI(
      title, message, mojom::WebNotificationType::INVALID_NOTIFICATION);
  notification_controller_->ShowNotificationFromWebUI(
      title, message, mojom::WebNotificationType::WIFI_NOT_READY);
  notification_controller_->CloseConnectionOrLaunchErrorNotifications();

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(kEcheAppScreenLockNotifierId);
  ASSERT_TRUE(notification.has_value());
  notification =
      display_service_->GetNotification(kEcheAppRetryConnectionNotifierId);
  ASSERT_FALSE(notification.has_value());
  notification =
      display_service_->GetNotification(kEcheAppInactivityNotifierId);
  ASSERT_FALSE(notification.has_value());
  notification =
      display_service_->GetNotification(kEcheAppFromWebWithoutButtonNotifierId);
  ASSERT_FALSE(notification.has_value());
  notification =
      display_service_->GetNotification(kEcheAppNetworkSettingNotifierId);
  ASSERT_FALSE(notification.has_value());
}

}  // namespace eche_app
}  // namespace ash
