// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system_notification_builder.h"

#include "base/functional/callback_helpers.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(SystemNotificationBuilderTest, TrivialSetters) {
  SystemNotificationBuilder builder;
  builder.SetId("test").SetCatalogName(
      NotificationCatalogName::kTestCatalogName);

  message_center::Notification notification = builder.Build(false);

  EXPECT_EQ(notification.type(),
            message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE);
  EXPECT_EQ(notification.title(), u"");
  EXPECT_EQ(notification.message(), u"");
  EXPECT_EQ(notification.display_source(), u"");
  EXPECT_FALSE(notification.origin_url().is_valid());
  EXPECT_EQ(notification.delegate(), nullptr);
  EXPECT_EQ(&notification.vector_small_image(), &gfx::kNoneIcon);
  EXPECT_EQ(notification.rich_notification_data().progress, 0);
  EXPECT_EQ(notification.system_notification_warning_level(),
            message_center::SystemNotificationWarningLevel::NORMAL);

  message_center::RichNotificationData optional_data;
  optional_data.progress = 1;
  notification =
      builder
          .SetType(message_center::NotificationType::NOTIFICATION_TYPE_PROGRESS)
          .SetTitle(u"title")
          .SetMessage(u"message")
          .SetDisplaySource(u"test")
          .SetOriginUrl(GURL("https://chromium.org"))
          .SetNotifierId(message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT, "notifier id",
              NotificationCatalogName::kTestCatalogName))
          .SetDelegate(base::MakeRefCounted<
                       message_center::HandleNotificationClickDelegate>(
              base::DoNothingAs<void()>()))
          .SetSmallImage(vector_icons::kSettingsIcon)
          .SetOptionalFields(optional_data)
          .SetWarningLevel(
              message_center::SystemNotificationWarningLevel::WARNING)
          .Build(false);

  EXPECT_EQ(notification.type(),
            message_center::NotificationType::NOTIFICATION_TYPE_PROGRESS);
  EXPECT_EQ(notification.title(), u"title");
  EXPECT_EQ(notification.message(), u"message");
  EXPECT_EQ(notification.display_source(), u"test");
  EXPECT_TRUE(notification.origin_url().is_valid());
  EXPECT_NE(notification.delegate(), nullptr);
  EXPECT_EQ(&notification.vector_small_image(), &vector_icons::kSettingsIcon);
  EXPECT_EQ(notification.rich_notification_data().progress, 1);
  EXPECT_EQ(notification.system_notification_warning_level(),
            message_center::SystemNotificationWarningLevel::WARNING);
}

TEST(SystemNotificationBuilderTest, NotifierId) {
  SystemNotificationBuilder builder;
  builder.SetId("test");

  message_center::NotifierId notifier_id;
  notifier_id.catalog_name = NotificationCatalogName::kTestCatalogName;
  notifier_id.id = "test";

  builder.SetCatalogName(NotificationCatalogName::kTestCatalogName);
  EXPECT_EQ(builder.Build(false).notifier_id(), notifier_id);

  notifier_id.id = "test";
  builder.SetNotifierId(notifier_id);
  EXPECT_EQ(builder.Build(false).notifier_id(), notifier_id);

  // Explicitly setting a non System NotifierId should still be possible.
  notifier_id = message_center::NotifierId();
  builder.SetNotifierId(notifier_id);
  EXPECT_EQ(builder.Build(false).notifier_id(), notifier_id);
}

}  // namespace ash
