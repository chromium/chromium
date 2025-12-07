// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/pcie_peripheral/pcie_peripheral_notification_controller.h"

#include <memory>
#include <optional>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

using message_center::MessageCenter;

namespace ash {

namespace {
const char kPciePeripheralLimitedPerformanceNotificationId[] =
    "cros_pcie_peripheral_limited_performance_notification_id";
const char kPciePeripheralLimitedPerformanceGuestModeNotificationId[] =
    "cros_pcie_peripheral_limited_performance_guest_mode_notification_id";
const char kPciePeripheralGuestModeNotSupportedNotificationId[] =
    "cros_pcie_peripheral_guest_mode_not_supported_notifcation_id";
const char kPciePeripheralDeviceBlockedNotificationId[] =
    "cros_pcie_peripheral_device_blocked_notifcation_id";
const char kPciePeripheralBillboardDeviceNotificationId[] =
    "cros_pcie_peripheral_billboard_device_notifcation_id";
const char kLearnMoreHelpUrl[] =
    "https://www.support.google.com/chromebook?p=connect_thblt_usb4_accy";

// A mock implementation of |NewWindowDelegate| for use in tests.
class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

}  // namespace

class PciePeripheralNotificationControllerTest : public AshTestBase {
 public:
  PciePeripheralNotificationControllerTest() = default;
  PciePeripheralNotificationControllerTest(
      const PciePeripheralNotificationControllerTest&) = delete;
  PciePeripheralNotificationControllerTest& operator=(
      const PciePeripheralNotificationControllerTest&) = delete;
  ~PciePeripheralNotificationControllerTest() override = default;

  PciePeripheralNotificationController* controller() {
    return Shell::Get()->pcie_peripheral_notification_controller();
  }

  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }

  message_center::Notification* GetLimitedPerformanceNotification() {
    return MessageCenter::Get()->FindVisibleNotificationById(
        kPciePeripheralLimitedPerformanceNotificationId);
  }

  message_center::Notification* GetLimitedPerformanceGuestModeNotification() {
    return MessageCenter::Get()->FindVisibleNotificationById(
        kPciePeripheralLimitedPerformanceGuestModeNotificationId);
  }

  message_center::Notification* GetGuestModeNotSupportedNotification() {
    return MessageCenter::Get()->FindVisibleNotificationById(
        kPciePeripheralGuestModeNotSupportedNotificationId);
  }

  message_center::Notification* GetPeripheralBlockedNotification() {
    return MessageCenter::Get()->FindVisibleNotificationById(
        kPciePeripheralDeviceBlockedNotificationId);
  }

  message_center::Notification* GetBillboardDeviceNotification() {
    return MessageCenter::Get()->FindVisibleNotificationById(
        kPciePeripheralBillboardDeviceNotificationId);
  }

  int GetNumOsPrivacySettingsOpened() {
    return GetSystemTrayClient()->show_os_settings_privacy_and_security_count();
  }

  int GetPrefNotificationCount() {
    PrefService* prefs =
        Shell::Get()->session_controller()->GetActivePrefService();
    return prefs->GetInteger(
        prefs::kPciePeripheralDisplayNotificationRemaining);
  }

  void ClickLimitedNotificationButton(std::optional<int> button_index) {
    // No button index means the notification body was clicked.
    if (!button_index.has_value()) {
      message_center::Notification* notification =
          MessageCenter::Get()->FindVisibleNotificationById(
              kPciePeripheralLimitedPerformanceNotificationId);
      notification->delegate()->Click(std::nullopt, std::nullopt);
      return;
    }

    message_center::Notification* notification =
        MessageCenter::Get()->FindVisibleNotificationById(
            kPciePeripheralLimitedPerformanceNotificationId);
    notification->delegate()->Click(button_index, std::nullopt);
  }

  void ClickGuestNotification(bool is_thunderbolt_only) {
    if (is_thunderbolt_only) {
      MessageCenter::Get()->ClickOnNotification(
          kPciePeripheralGuestModeNotSupportedNotificationId);
      return;
    }

    MessageCenter::Get()->ClickOnNotification(
        kPciePeripheralLimitedPerformanceGuestModeNotificationId);
  }

  void RemoveAllNotifications() {
    MessageCenter::Get()->RemoveAllNotifications(
        /*by_user=*/false, message_center::MessageCenter::RemoveType::ALL);
  }

 private:
  MockNewWindowDelegate new_window_delegate_;
};

TEST_F(PciePeripheralNotificationControllerTest, GuestNotificationTbtOnly) {
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  controller()->NotifyGuestModeNotification(/*is_thunderbolt_only=*/true);
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());

  message_center::Notification* notification =
      GetGuestModeNotSupportedNotification();
  ASSERT_TRUE(notification);

  // This notification has no buttons.
  EXPECT_EQ(0u, notification->buttons().size());

  controller()->NotifyGuestModeNotification(/*is_thunderbolt_only=*/true);
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());

  // Click on the notification and expect the Learn More page to appear.
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL(kLearnMoreHelpUrl),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));
  ClickGuestNotification(/*is_thunderbolt_only=*/true);
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
}

TEST_F(PciePeripheralNotificationControllerTest, GuestNotificationTbtAltMode) {
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  controller()->NotifyGuestModeNotification(/*is_thunderbolt_only=*/false);
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());

  message_center::Notification* notification =
      GetLimitedPerformanceGuestModeNotification();
  ASSERT_TRUE(notification);

  // This notification has no buttons.
  EXPECT_EQ(0u, notification->buttons().size());

  controller()->NotifyGuestModeNotification(/*is_thunderbolt_only=*/false);
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());

  // Click on the notification and expect the Learn More page to appear.
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL(kLearnMoreHelpUrl),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));
  ClickGuestNotification(/*is_thunderbolt_only=*/false);
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
}

TEST_F(PciePeripheralNotificationControllerTest,
       PeripheralBlockedNotification) {
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  controller()->NotifyPeripheralBlockedNotification();
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());

  message_center::Notification* notification =
      GetPeripheralBlockedNotification();
  ASSERT_TRUE(notification);

  // This notification has no buttons.
  EXPECT_EQ(0u, notification->buttons().size());

  // Click on the notification and expect the Learn More page to appear.
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL(kLearnMoreHelpUrl),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));
  MessageCenter::Get()->ClickOnNotification(
      kPciePeripheralDeviceBlockedNotificationId);
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
}

TEST_F(PciePeripheralNotificationControllerTest, BillboardDeviceNotification) {
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
  EXPECT_EQ(3, GetPrefNotificationCount());

  controller()->NotifyBillboardDevice();
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());

  message_center::Notification* notification = GetBillboardDeviceNotification();
  ASSERT_TRUE(notification);

  // This notification has no buttons.
  EXPECT_EQ(0u, notification->buttons().size());

  controller()->NotifyBillboardDevice();
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());

  // Click on the notification and expect the Learn More page to appear.
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL(kLearnMoreHelpUrl),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));
  MessageCenter::Get()->ClickOnNotification(
      kPciePeripheralBillboardDeviceNotificationId);
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
  EXPECT_EQ(3, GetPrefNotificationCount());
}

TEST_F(PciePeripheralNotificationControllerTest,
       LimitedPerformanceNotificationLearnMoreClick) {
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
  EXPECT_EQ(3, GetPrefNotificationCount());

  controller()->NotifyLimitedPerformance();
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());

  message_center::Notification* notification =
      GetLimitedPerformanceNotification();
  ASSERT_TRUE(notification);

  // Ensure this notification has the two correct buttons.
  EXPECT_EQ(2u, notification->buttons().size());

  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL(kLearnMoreHelpUrl),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));
  // Click the learn more link.
  ClickLimitedNotificationButton(/*button_index=*/1);
  EXPECT_EQ(2, GetPrefNotificationCount());
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL(kLearnMoreHelpUrl),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));
  controller()->NotifyLimitedPerformance();
  ClickLimitedNotificationButton(/*button_index=*/1);
  EXPECT_EQ(1, GetPrefNotificationCount());
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL(kLearnMoreHelpUrl),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));
  controller()->NotifyLimitedPerformance();
  ClickLimitedNotificationButton(/*button_index=*/1);
  EXPECT_EQ(0, GetPrefNotificationCount());
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  controller()->NotifyLimitedPerformance();
  // Pref is currently at 0, so no new notifications should appear.
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
}

TEST_F(PciePeripheralNotificationControllerTest,
       LimitedPerformanceNotificationBodyClick) {
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
  EXPECT_EQ(3, GetPrefNotificationCount());

  controller()->NotifyLimitedPerformance();
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());
  // New notifications will decrement the counter.
  EXPECT_EQ(2, GetPrefNotificationCount());

  message_center::Notification* notification =
      GetLimitedPerformanceNotification();
  ASSERT_TRUE(notification);

  // Ensure this notification has the two correct buttons.
  EXPECT_EQ(2u, notification->buttons().size());

  // Click the notification body.
  ClickLimitedNotificationButton(std::nullopt);
  EXPECT_EQ(0, GetPrefNotificationCount());
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
  EXPECT_EQ(1, GetNumOsPrivacySettingsOpened());

  // No new notifications can appear.
  controller()->NotifyLimitedPerformance();
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
}

TEST_F(PciePeripheralNotificationControllerTest,
       LimitedPerformanceNotificationSettingsButtonClick) {
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
  EXPECT_EQ(3, GetPrefNotificationCount());

  controller()->NotifyLimitedPerformance();
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());
  // New notifications will decrement the counter.
  EXPECT_EQ(2, GetPrefNotificationCount());

  message_center::Notification* notification =
      GetLimitedPerformanceNotification();
  ASSERT_TRUE(notification);

  // Ensure this notification has the two correct buttons.
  EXPECT_EQ(2u, notification->buttons().size());

  // Click the Settings button.
  ClickLimitedNotificationButton(/*button_index=*/0);
  EXPECT_EQ(0, GetPrefNotificationCount());
  EXPECT_EQ(1, GetNumOsPrivacySettingsOpened());
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  // No new notifications can appear.
  controller()->NotifyLimitedPerformance();
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
}

TEST_F(PciePeripheralNotificationControllerTest,
       ClickGuestNotificationTbtOnly) {
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
  EXPECT_EQ(3, GetPrefNotificationCount());

  controller()->NotifyGuestModeNotification(/*is_thunderbolt_only=*/true);
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());

  message_center::Notification* notification =
      GetGuestModeNotSupportedNotification();
  ASSERT_TRUE(notification);

  // This notification has no buttons.
  EXPECT_EQ(0u, notification->buttons().size());

  // We will always show guest notifications, expect that the pref did not
  // decrement.
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL(kLearnMoreHelpUrl),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));
  ClickGuestNotification(/*is_thunderbolt_only=*/true);
  EXPECT_EQ(3, GetPrefNotificationCount());
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  controller()->NotifyGuestModeNotification(/*is_thunderbolt_only=*/true);
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());
}

TEST_F(PciePeripheralNotificationControllerTest,
       ClickGuestNotificationTbtAltMode) {
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
  EXPECT_EQ(3, GetPrefNotificationCount());

  controller()->NotifyGuestModeNotification(/*is_thunderbolt_only=*/false);
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());

  message_center::Notification* notification =
      GetLimitedPerformanceGuestModeNotification();
  ASSERT_TRUE(notification);

  // This notification has no buttons.
  EXPECT_EQ(0u, notification->buttons().size());

  // We will always show guest notifications, expect that the pref did not
  // decrement.
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL(kLearnMoreHelpUrl),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));
  ClickGuestNotification(/*is_thunderbolt_only=*/false);
  EXPECT_EQ(3, GetPrefNotificationCount());
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  controller()->NotifyGuestModeNotification(/*is_thunderbolt_only=*/false);
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());
}

}  // namespace ash
