// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_app_notification_controller.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/webui/eche_app_ui/eche_alert_generator.h"
#include "base/check_deref.h"
#include "base/functional/callback_helpers.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/message_center/message_center.h"

namespace ash {
namespace eche_app {

namespace {

constexpr auto kAccountId =
    AccountId::Literal::FromUserEmailGaiaId("test@test",
                                            GaiaId::Literal("123456789"));

}  // namespace

class TestableNotificationController : public EcheAppNotificationController {
 public:
  TestableNotificationController(
      const AccountId& account_id,
      const base::RepeatingClosure& relaunch_callback)
      : EcheAppNotificationController(account_id, relaunch_callback) {}
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

class EcheAppNotificationControllerTest : public AshTestBase {
 protected:
  EcheAppNotificationControllerTest() = default;

  ~EcheAppNotificationControllerTest() override = default;
  EcheAppNotificationControllerTest(const EcheAppNotificationControllerTest&) =
      delete;
  EcheAppNotificationControllerTest& operator=(
      const EcheAppNotificationControllerTest&) = delete;

  void SetUp() override {
    AshTestBase::SetUp();

    ASSERT_TRUE(
        ash_test_helper()->user_session_test_environment().AddRegularUser(
            kAccountId));
    ash_test_helper()->user_session_test_environment().LogIn(kAccountId);

    notification_controller_ =
        std::make_unique<testing::StrictMock<TestableNotificationController>>(
            kAccountId, base::DoNothing());
  }

  const message_center::Notification* GetNotification(
      const std::string& notification_id) {
    const user_manager::User& user =
        CHECK_DEREF(user_manager::UserManager::Get()->FindUser(kAccountId));
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        CreateUserScopedNotificationId(notification_id, user.username_hash()));
  }

  std::unique_ptr<testing::StrictMock<TestableNotificationController>>
      notification_controller_;

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
  const message_center::Notification* notification =
      GetNotification(kEcheAppRetryConnectionNotifierId);
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
  notification = GetNotification(kEcheAppRetryConnectionNotifierId);
  ASSERT_TRUE(notification);
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
  notification = GetNotification(kEcheAppInactivityNotifierId);
  ASSERT_TRUE(notification);
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
  notification = GetNotification(kEcheAppNetworkSettingNotifierId);
  ASSERT_TRUE(notification);
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
  const message_center::Notification* notification =
      GetNotification(kEcheAppScreenLockNotifierId);
  ASSERT_TRUE(notification);
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
  const message_center::Notification* notification =
      GetNotification(kEcheAppScreenLockNotifierId);
  ASSERT_TRUE(notification);
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
  const message_center::Notification* notification =
      GetNotification(kEcheAppScreenLockNotifierId);
  ASSERT_FALSE(notification);

  std::optional<std::u16string> message = u"message";
  notification_controller_->ShowNotificationFromWebUI(
      title, message, mojom::WebNotificationType::CONNECTION_FAILED);
  notification_controller_->CloseNotification(
      kEcheAppRetryConnectionNotifierId);
  notification = GetNotification(kEcheAppRetryConnectionNotifierId);
  ASSERT_FALSE(notification);

  notification_controller_->ShowNotificationFromWebUI(
      title, message, mojom::WebNotificationType::DEVICE_IDLE);
  notification_controller_->CloseNotification(kEcheAppInactivityNotifierId);
  notification = GetNotification(kEcheAppInactivityNotifierId);
  ASSERT_FALSE(notification);

  notification_controller_->ShowNotificationFromWebUI(
      title, message, mojom::WebNotificationType::INVALID_NOTIFICATION);
  notification_controller_->CloseNotification(
      kEcheAppFromWebWithoutButtonNotifierId);
  notification = GetNotification(kEcheAppFromWebWithoutButtonNotifierId);
  ASSERT_FALSE(notification);

  notification_controller_->ShowNotificationFromWebUI(
      title, message, mojom::WebNotificationType::WIFI_NOT_READY);
  notification_controller_->CloseNotification(kEcheAppNetworkSettingNotifierId);
  notification = GetNotification(kEcheAppNetworkSettingNotifierId);
  ASSERT_FALSE(notification);
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

  const message_center::Notification* notification =
      GetNotification(kEcheAppScreenLockNotifierId);
  ASSERT_TRUE(notification);
  notification = GetNotification(kEcheAppRetryConnectionNotifierId);
  ASSERT_FALSE(notification);
  notification = GetNotification(kEcheAppInactivityNotifierId);
  ASSERT_FALSE(notification);
  notification = GetNotification(kEcheAppFromWebWithoutButtonNotifierId);
  ASSERT_FALSE(notification);
  notification = GetNotification(kEcheAppNetworkSettingNotifierId);
  ASSERT_FALSE(notification);
}

}  // namespace eche_app
}  // namespace ash
