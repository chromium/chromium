// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_chromeos.h"

#include "base/functional/bind.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

// Regression test for https://crbug.com/840105
TEST(NotificationPlatformBridgeChromeOsTest, Update) {
  message_center::MessageCenter::Initialize();
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  NotificationPlatformBridgeChromeOs bridge;

  // Create and display a notification and make sure clicking it gets back to
  // the delegate.
  const std::string id("test_id");
  int initial_delegate_clicks = 0;
  auto initial_delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              [](int* clicks, std::optional<int> button_index) { ++*clicks; },
              &initial_delegate_clicks));
  message_center::Notification initial_notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, std::u16string(),
      std::u16string(), ui::ImageModel(), std::u16string(), GURL(),
      message_center::NotifierId(), {}, initial_delegate);
  bridge.Display(NotificationHandler::Type::TRANSIENT, &profile,
                 initial_notification, nullptr);
  EXPECT_EQ(0, initial_delegate_clicks);
  ProfileNotification permuted_notification(&profile, initial_notification);
  bridge.HandleNotificationClicked(permuted_notification.notification().id());
  EXPECT_EQ(1, initial_delegate_clicks);

  // Display a second notification with the same ID which replaces the original
  // and make sure clicking it gets back to the correct delegate.
  int updated_delegate_clicks = 0;
  auto updated_delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              [](int* clicks, std::optional<int> button_index) { ++*clicks; },
              &updated_delegate_clicks));
  message_center::Notification updated_notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, std::u16string(),
      std::u16string(), ui::ImageModel(), std::u16string(), GURL(),
      message_center::NotifierId(), {}, updated_delegate);
  bridge.Display(NotificationHandler::Type::TRANSIENT, &profile,
                 updated_notification, nullptr);
  EXPECT_EQ(1, initial_delegate_clicks);
  EXPECT_EQ(0, updated_delegate_clicks);
  bridge.HandleNotificationClicked(permuted_notification.notification().id());
  EXPECT_EQ(1, initial_delegate_clicks);
  EXPECT_EQ(1, updated_delegate_clicks);

  message_center::MessageCenter::Shutdown();
}
