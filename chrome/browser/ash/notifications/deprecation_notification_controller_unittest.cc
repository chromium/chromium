// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/deprecation_notification_controller.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"

namespace ash {

namespace {

class DeprecationNotificationControllerTest : public AshTestBase {
 protected:
  DeprecationNotificationControllerTest() : controller_(&message_center_) {}
  DeprecationNotificationControllerTest(
      const DeprecationNotificationControllerTest&) = delete;
  DeprecationNotificationControllerTest& operator=(
      const DeprecationNotificationControllerTest&) = delete;
  ~DeprecationNotificationControllerTest() override = default;

  // |message_center_| must be declared before |controller_| since it is an
  // argument to it's constructor.
  message_center::FakeMessageCenter message_center_;
  DeprecationNotificationController controller_;
};

}  // namespace

// Test that all the notifications are displayed and only once each.
TEST_F(DeprecationNotificationControllerTest, AllNotificationsWorkAndNoDupes) {
  size_t expected_notification_count = 1;
  controller_.NotifyDeprecatedRightClickRewrite();
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);

  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_DELETE);
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_INSERT);
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_HOME);
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_END);
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_PRIOR);
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_NEXT);
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);

  // Clear the messages from the message center.
  message_center_.RemoveAllNotifications(
      /*by_user=*/false, message_center::FakeMessageCenter::RemoveType::ALL);

  // No additional notifications should be generated.
  controller_.NotifyDeprecatedRightClickRewrite();
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_DELETE);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_INSERT);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_HOME);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_END);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_PRIOR);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_NEXT);
  EXPECT_EQ(message_center_.NotificationCount(), 0u);
}

// Test that six pack notifications are not displayed when accelerators are
// blocked.
TEST_F(DeprecationNotificationControllerTest,
       SixPackNotificationsBlockedWithShortcuts) {
  // Block shortcuts and attempt to create notifications. Since shortcuts are
  // blocked, no notifications should be created.
  Shell::Get()->accelerator_controller()->SetPreventProcessingAccelerators(
      /*prevent_processing_accelerators=*/true);

  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_DELETE);
  EXPECT_EQ(message_center_.NotificationCount(), 0u);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_INSERT);
  EXPECT_EQ(message_center_.NotificationCount(), 0u);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_HOME);
  EXPECT_EQ(message_center_.NotificationCount(), 0u);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_END);
  EXPECT_EQ(message_center_.NotificationCount(), 0u);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_PRIOR);
  EXPECT_EQ(message_center_.NotificationCount(), 0u);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_NEXT);
  EXPECT_EQ(message_center_.NotificationCount(), 0u);

  // Clear the messages from the message center.
  message_center_.RemoveAllNotifications(
      /*by_user=*/false, message_center::FakeMessageCenter::RemoveType::ALL);

  // Unblock shortcuts and then publish notifications, notifications should now
  // publish as expected.
  Shell::Get()->accelerator_controller()->SetPreventProcessingAccelerators(
      /*prevent_processing_accelerators=*/false);

  size_t expected_notification_count = 1;
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_DELETE);
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_INSERT);
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_HOME);
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_END);
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_PRIOR);
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);
  controller_.NotifyDeprecatedSixPackKeyRewrite(ui::VKEY_NEXT);
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);
}

}  // namespace ash
