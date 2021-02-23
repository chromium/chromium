// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/pcie_peripheral/pcie_peripheral_notification_controller.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/optional.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/message_center/fake_message_center.h"
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
const char kLearnMoreHelpUrl[] =
    "https://www.support.google.com/chromebook?p=connect_thblt_usb4_accy";

// A mock implementation of |NewWindowDelegate| for use in tests.
class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              NewTabWithUrl,
              (const GURL& url, bool from_user_interaction),
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

  int GetNumOsPrivacySettingsOpened() {
    return GetSystemTrayClient()->show_os_settings_privacy_and_security_count();
  }

  void ClickLimitedNotificationButton(base::Optional<int> button_index) {
    // No button index means the notification body was clicked.
    if (!button_index.has_value()) {
      message_center::Notification* notification =
          MessageCenter::Get()->FindVisibleNotificationById(
              kPciePeripheralLimitedPerformanceNotificationId);
      notification->delegate()->Click(base::nullopt, base::nullopt);
      return;
    }

    message_center::Notification* notification =
        MessageCenter::Get()->FindVisibleNotificationById(
            kPciePeripheralLimitedPerformanceNotificationId);
    notification->delegate()->Click(button_index, base::nullopt);
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

 private:
  MockNewWindowDelegate new_window_delegate_;
};

TEST_F(PciePeripheralNotificationControllerTest,
       LimitedPerformanceNotification) {
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  controller()->NotifyLimitedPerformance();
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());

  message_center::Notification* notification =
      GetLimitedPerformanceNotification();

  EXPECT_TRUE(notification);

  // Ensure this notification has the two correct buttons.
  EXPECT_EQ(2u, notification->buttons().size());

  EXPECT_EQ(0, GetNumOsPrivacySettingsOpened());
  // Click on the Settings button and expect it to reach the OS Settings privacy
  // page.
  ClickLimitedNotificationButton(/*button_index=*/0);
  EXPECT_EQ(1, GetNumOsPrivacySettingsOpened());
  // Clicking on the notification will close it.
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  // Open new notification and click on its body.
  controller()->NotifyLimitedPerformance();
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());
  ClickLimitedNotificationButton(base::nullopt);
  EXPECT_EQ(2, GetNumOsPrivacySettingsOpened());
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  // Open new notification and click on the Learn More button.
  controller()->NotifyLimitedPerformance();
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());
  EXPECT_CALL(new_window_delegate(), NewTabWithUrl)
      .WillOnce([](const GURL& url, bool from_user_interaction) {
        EXPECT_EQ(GURL(kLearnMoreHelpUrl), url);
        EXPECT_TRUE(from_user_interaction);
      });
  ClickLimitedNotificationButton(/*button_index=*/1);
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
}

TEST_F(PciePeripheralNotificationControllerTest, GuestNotificationTbtOnly) {
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  controller()->NotifyGuestModeNotification(/*is_thunderbolt_only=*/true);

  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());

  message_center::Notification* notification =
      GetGuestModeNotSupportedNotification();

  EXPECT_TRUE(notification);

  // This notification has no buttons.
  EXPECT_EQ(0u, notification->buttons().size());

  controller()->NotifyGuestModeNotification(/*is_thunderbolt_only=*/true);
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());

  // Click on the notification and expect the Learn More page to page to appear.
  EXPECT_CALL(new_window_delegate(), NewTabWithUrl)
      .WillOnce([](const GURL& url, bool from_user_interaction) {
        EXPECT_EQ(GURL(kLearnMoreHelpUrl), url);
        EXPECT_TRUE(from_user_interaction);
      });
  ClickGuestNotification(/*is_thunderbolt_only=*/true);
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
}

TEST_F(PciePeripheralNotificationControllerTest, GuestNotificationTbtAltMode) {
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  controller()->NotifyGuestModeNotification(/*is_thunderbolt_only=*/false);
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());

  message_center::Notification* notification =
      GetLimitedPerformanceGuestModeNotification();

  EXPECT_TRUE(notification);

  // This notification has no buttons.
  EXPECT_EQ(0u, notification->buttons().size());

  controller()->NotifyGuestModeNotification(/*is_thunderbolt_only=*/false);
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());

  // Click on the notification and expect the Learn More page to page to appear.
  EXPECT_CALL(new_window_delegate(), NewTabWithUrl)
      .WillOnce([](const GURL& url, bool from_user_interaction) {
        EXPECT_EQ(GURL(kLearnMoreHelpUrl), url);
        EXPECT_TRUE(from_user_interaction);
      });
  ClickGuestNotification(/*is_thunderbolt_only=*/false);
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
}

}  // namespace ash
