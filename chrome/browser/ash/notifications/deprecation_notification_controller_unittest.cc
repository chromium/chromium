// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/deprecation_notification_controller.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"

namespace ash {

namespace {

class DeprecationNotificationControllerTest : public testing::Test {
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

  controller_.NotifyDeprecatedAltBasedKeyRewrite(ui::VKEY_DELETE);
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);
  controller_.NotifyDeprecatedAltBasedKeyRewrite(ui::VKEY_HOME);
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);
  controller_.NotifyDeprecatedAltBasedKeyRewrite(ui::VKEY_END);
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);
  controller_.NotifyDeprecatedAltBasedKeyRewrite(ui::VKEY_PRIOR);
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);
  controller_.NotifyDeprecatedAltBasedKeyRewrite(ui::VKEY_NEXT);
  EXPECT_EQ(message_center_.NotificationCount(), expected_notification_count++);

  // Clear the messages from the message center.
  message_center_.RemoveAllNotifications(
      /*by_user=*/false, message_center::FakeMessageCenter::RemoveType::ALL);

  // No additional notifications should be generated.
  controller_.NotifyDeprecatedRightClickRewrite();
  controller_.NotifyDeprecatedAltBasedKeyRewrite(ui::VKEY_DELETE);
  controller_.NotifyDeprecatedAltBasedKeyRewrite(ui::VKEY_HOME);
  controller_.NotifyDeprecatedAltBasedKeyRewrite(ui::VKEY_END);
  controller_.NotifyDeprecatedAltBasedKeyRewrite(ui::VKEY_PRIOR);
  controller_.NotifyDeprecatedAltBasedKeyRewrite(ui::VKEY_NEXT);
  EXPECT_EQ(message_center_.NotificationCount(), 0);
}

// Only one notification is shown no matter which F-Key is triggered.
TEST_F(DeprecationNotificationControllerTest, NoDuplicateFKeyNotifications) {
  // First F-Key generates a notification.
  controller_.NotifyDeprecatedFKeyRewrite();
  EXPECT_EQ(message_center_.NotificationCount(), 1);

  // Clear the messages from the message center.
  message_center_.RemoveAllNotifications(
      /*by_user=*/false, message_center::FakeMessageCenter::RemoveType::ALL);

  // Subsequent times don't generate an additional notification.
  controller_.NotifyDeprecatedFKeyRewrite();
  EXPECT_EQ(message_center_.NotificationCount(), 0);
}

}  // namespace ash
