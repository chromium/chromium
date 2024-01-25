// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/message_center/metadata_utils.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/message_center/arc_notification_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using arc::mojom::ArcNotificationData;

namespace ash {

class MetadataUtilsTest : public AshTestBase {
 public:
  MetadataUtilsTest() = default;

  MetadataUtilsTest(const MetadataUtilsTest&) = delete;
  MetadataUtilsTest& operator=(const MetadataUtilsTest&) = delete;

  ~MetadataUtilsTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kRenderArcNotificationsByChrome);
    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

 protected:
  const std::string kDefaultNotificationKey = "notification_id";
  const std::string kDefaultNotificationId =
      kArcNotificationIdPrefix + kDefaultNotificationKey;
  const message_center::NotifierId notifier_id =
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "ARC_NOTIFICATION");
};

TEST_F(MetadataUtilsTest, CreateProgressNotification) {
  // Create test data
  const message_center::NotificationType notification_type =
      message_center::NOTIFICATION_TYPE_PROGRESS;
  message_center::RichNotificationData rich_data =
      message_center::RichNotificationData();
  scoped_refptr<message_center::NotificationDelegate> delegate = nullptr;
  arc::mojom::ArcNotificationData arc_notification_data;
  arc_notification_data.title = "title";
  arc_notification_data.message = "message";
  arc_notification_data.render_on_chrome = true;
  arc_notification_data.indeterminate_progress = true;

  std::unique_ptr<message_center::Notification>
      indeterminate_progress_notification =
          CreateNotificationFromArcNotificationData(
              notification_type, kDefaultNotificationId, &arc_notification_data,
              notifier_id, rich_data, delegate);

  // Assert that indeterminate progress notification exists and has correct
  // fields.
  ASSERT_TRUE(indeterminate_progress_notification != nullptr);
  EXPECT_EQ(indeterminate_progress_notification->type(), notification_type);
  EXPECT_EQ(indeterminate_progress_notification->id(), kDefaultNotificationId);
  EXPECT_EQ(indeterminate_progress_notification->progress(), -1);

  arc_notification_data.indeterminate_progress = false;
  arc_notification_data.progress_current = 30;
  arc_notification_data.progress_max = 100;

  std::unique_ptr<message_center::Notification> progress_notification =
      CreateNotificationFromArcNotificationData(
          notification_type, kDefaultNotificationId, &arc_notification_data,
          notifier_id, rich_data, delegate);

  // Assert that regular progress notification exists and has correct fields.
  ASSERT_TRUE(progress_notification != nullptr);
  EXPECT_EQ(progress_notification->type(), notification_type);
  EXPECT_EQ(progress_notification->id(), kDefaultNotificationId);
  EXPECT_EQ(progress_notification->progress(), 30);
}

TEST_F(MetadataUtilsTest, CreateNotificationWithButtons) {
  // Create test data
  const message_center::NotificationType notification_type =
      message_center::NOTIFICATION_TYPE_SIMPLE;
  message_center::RichNotificationData rich_data =
      message_center::RichNotificationData();
  scoped_refptr<message_center::NotificationDelegate> delegate = nullptr;

  arc::mojom::ArcNotificationData arc_notification_data;
  arc_notification_data.title = "title";
  arc_notification_data.message = "message";
  arc_notification_data.render_on_chrome = true;
  arc_notification_data.reply_button_index = 1;
  arc_notification_data.buttons.emplace();
  arc_notification_data.buttons->push_back(
      arc::mojom::ArcNotificationButton::New("Done"));
  arc_notification_data.buttons->push_back(
      arc::mojom::ArcNotificationButton::New("Reply", "placeholder"));

  std::unique_ptr<message_center::Notification> notification =
      CreateNotificationFromArcNotificationData(
          notification_type, kDefaultNotificationId, &arc_notification_data,
          notifier_id, rich_data, delegate);

  // Assert that notification exists and has correct fields and buttons.
  ASSERT_TRUE(notification != nullptr);
  EXPECT_EQ(notification->type(), notification_type);
  EXPECT_EQ(notification->id(), kDefaultNotificationId);
  EXPECT_EQ(u"title", notification->title());
  EXPECT_EQ(u"message", notification->message());
  ASSERT_EQ(2u, notification->buttons().size());
  EXPECT_EQ(u"Done", notification->buttons()[0].title);
  EXPECT_EQ(u"Reply", notification->buttons()[1].title);
  EXPECT_EQ(u"placeholder", notification->buttons()[1].placeholder.value());
}

TEST_F(MetadataUtilsTest, CreateListNotification) {
  // Create test data
  const message_center::NotificationType notification_type =
      message_center::NOTIFICATION_TYPE_SIMPLE;
  message_center::RichNotificationData rich_data =
      message_center::RichNotificationData();
  scoped_refptr<message_center::NotificationDelegate> delegate = nullptr;
  arc::mojom::ArcNotificationData arc_notification_data;
  arc_notification_data.title = "title";
  arc_notification_data.message = "message";
  arc_notification_data.render_on_chrome = true;
  arc_notification_data.texts = {"text 1", "text 2", "text 3",
                                 "text 4", "text 5", "text 6"};
  const size_t texts_size = 6;
  const size_t items_num =
      std::min(texts_size, message_center::kNotificationMaximumItems);

  std::unique_ptr<message_center::Notification> notification =
      CreateNotificationFromArcNotificationData(
          notification_type, kDefaultNotificationId, &arc_notification_data,
          notifier_id, rich_data, delegate);

  // Assert that notification exists and has correct fields.
  ASSERT_TRUE(notification != nullptr);
  EXPECT_EQ(notification->type(), notification_type);
  EXPECT_EQ(notification->id(), kDefaultNotificationId);
  ASSERT_EQ(items_num, notification->items().size());
  EXPECT_EQ(notification->items()[0].message(), u"text 1");
  EXPECT_EQ(notification->items()[1].message(), u"text 2");
  EXPECT_EQ(notification->items()[2].message(), u"text 3");
  EXPECT_EQ(notification->items()[3].message(), u"text 4");
  EXPECT_EQ(notification->items()[4].message(), u"\u2026");
  EXPECT_TRUE(notification->items()[0].title().empty());
  EXPECT_TRUE(notification->items()[1].title().empty());
  EXPECT_TRUE(notification->items()[2].title().empty());
  EXPECT_TRUE(notification->items()[3].title().empty());
  EXPECT_TRUE(notification->items()[4].title().empty());
}

}  // namespace ash
