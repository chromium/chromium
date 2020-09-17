// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_notification_controller.h"

#include "ash/shell.h"
#include "ash/system/message_center/message_center_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/phonehub/fake_notification_manager.h"
#include "chromeos/components/phonehub/notification.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/events/event.h"
#include "ui/message_center/message_center.h"

namespace ash {

const int64_t kPhoneHubNotificationId0 = 0;
const int64_t kPhoneHubNotificationId1 = 1;
const int64_t kPhoneHubNotificationId2 = 2;

const char kCrOSNotificationId0[] = "chrome://phonehub-0";
const char kCrOSNotificationId1[] = "chrome://phonehub-1";
const char kCrOSNotificationId2[] = "chrome://phonehub-2";

const char kAppName[] = "Test App";
const char kPackageName[] = "com.google.testapp";

const char kTitle[] = "Test notification";
const char kTextContent[] = "This is a test notification";

chromeos::phonehub::Notification CreateNotification(int64_t id) {
  return chromeos::phonehub::Notification(
      id,
      chromeos::phonehub::Notification::AppMetadata(base::UTF8ToUTF16(kAppName),
                                                    kPackageName,
                                                    /*icon=*/gfx::Image()),
      base::Time::Now(), chromeos::phonehub::Notification::Importance::kDefault,
      /*inline_reply_id=*/0, base::UTF8ToUTF16(kTitle),
      base::UTF8ToUTF16(kTextContent));
}

class PhoneHubNotificationControllerTest : public AshTestBase {
 public:
  PhoneHubNotificationControllerTest() = default;
  ~PhoneHubNotificationControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(chromeos::features::kPhoneHub);
    AshTestBase::SetUp();

    message_center_ = message_center::MessageCenter::Get();

    controller_ = Shell::Get()
                      ->message_center_controller()
                      ->phone_hub_notification_controller();
    controller_->SetManager(&notification_manager_);

    fake_notifications_.insert(CreateNotification(kPhoneHubNotificationId0));
    fake_notifications_.insert(CreateNotification(kPhoneHubNotificationId1));
    fake_notifications_.insert(CreateNotification(kPhoneHubNotificationId2));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  message_center::MessageCenter* message_center_;
  chromeos::phonehub::FakeNotificationManager notification_manager_;
  PhoneHubNotificationController* controller_;
  base::flat_set<chromeos::phonehub::Notification> fake_notifications_;
};

TEST_F(PhoneHubNotificationControllerTest, AddNotifications) {
  EXPECT_FALSE(message_center_->NotificationCount());
  notification_manager_.SetNotifications(fake_notifications_);
  EXPECT_EQ(3u, message_center_->NotificationCount());

  ASSERT_TRUE(
      message_center_->FindVisibleNotificationById(kCrOSNotificationId0));
  ASSERT_TRUE(
      message_center_->FindVisibleNotificationById(kCrOSNotificationId1));
  ASSERT_TRUE(
      message_center_->FindVisibleNotificationById(kCrOSNotificationId2));

  auto* sample_notification =
      message_center_->FindVisibleNotificationById(kCrOSNotificationId1);
  EXPECT_EQ(base::UTF8ToUTF16(kTitle), sample_notification->title());
  EXPECT_EQ(base::UTF8ToUTF16(kTextContent), sample_notification->message());
}

TEST_F(PhoneHubNotificationControllerTest, UpdateNotifications) {
  EXPECT_FALSE(message_center_->NotificationCount());
  notification_manager_.SetNotifications(fake_notifications_);
  EXPECT_EQ(3u, message_center_->NotificationCount());

  auto* notification =
      message_center_->FindVisibleNotificationById(kCrOSNotificationId1);
  EXPECT_EQ(base::UTF8ToUTF16(kTitle), notification->title());
  EXPECT_EQ(base::UTF8ToUTF16(kTextContent), notification->message());

  std::string kNewTitle = "New title";
  std::string kNewTextContent = "New text content";
  chromeos::phonehub::Notification updated_notification(
      kPhoneHubNotificationId1,
      chromeos::phonehub::Notification::AppMetadata(base::UTF8ToUTF16(kAppName),
                                                    kPackageName,
                                                    /*icon=*/gfx::Image()),
      base::Time::Now(), chromeos::phonehub::Notification::Importance::kDefault,
      /*inline_reply_id=*/0, base::UTF8ToUTF16(kNewTitle),
      base::UTF8ToUTF16(kNewTextContent));

  notification_manager_.SetNotification(updated_notification);

  notification =
      message_center_->FindVisibleNotificationById(kCrOSNotificationId1);
  EXPECT_EQ(base::UTF8ToUTF16(kNewTitle), notification->title());
  EXPECT_EQ(base::UTF8ToUTF16(kNewTextContent), notification->message());
}

TEST_F(PhoneHubNotificationControllerTest, RemoveNotifications) {
  EXPECT_FALSE(message_center_->NotificationCount());
  notification_manager_.SetNotifications(fake_notifications_);
  EXPECT_EQ(3u, message_center_->NotificationCount());

  notification_manager_.RemoveNotification(kPhoneHubNotificationId0);
  EXPECT_EQ(2u, message_center_->NotificationCount());
  EXPECT_FALSE(
      message_center_->FindVisibleNotificationById(kCrOSNotificationId0));

  notification_manager_.RemoveNotifications(base::flat_set<int64_t>(
      {kPhoneHubNotificationId1, kPhoneHubNotificationId2}));
  EXPECT_FALSE(message_center_->NotificationCount());
}

TEST_F(PhoneHubNotificationControllerTest, CloseByUser) {
  notification_manager_.SetNotifications(fake_notifications_);
  EXPECT_EQ(3u, message_center_->NotificationCount());

  message_center_->RemoveNotification(kCrOSNotificationId0, /*by_user=*/true);
  message_center_->RemoveNotification(kCrOSNotificationId1, /*by_user=*/true);
  message_center_->RemoveNotification(kCrOSNotificationId2, /*by_user=*/true);

  EXPECT_EQ(
      std::vector<int64_t>({kPhoneHubNotificationId0, kPhoneHubNotificationId1,
                            kPhoneHubNotificationId2}),
      notification_manager_.dismissed_notification_ids());
}

TEST_F(PhoneHubNotificationControllerTest, InlineReply) {
  notification_manager_.SetNotifications(fake_notifications_);

  const base::string16 kInlineReply0 = base::UTF8ToUTF16("inline reply 0");
  const base::string16 kInlineReply1 = base::UTF8ToUTF16("inline reply 1");
  message_center_->ClickOnNotificationButtonWithReply(kCrOSNotificationId0, 0,
                                                      kInlineReply0);
  message_center_->ClickOnNotificationButtonWithReply(kCrOSNotificationId1, 0,
                                                      kInlineReply1);

  auto inline_replies = notification_manager_.inline_replies();
  EXPECT_EQ(kPhoneHubNotificationId0, inline_replies[0].notification_id);
  EXPECT_EQ(kInlineReply0, inline_replies[0].inline_reply_text);
  EXPECT_EQ(kPhoneHubNotificationId1, inline_replies[1].notification_id);
  EXPECT_EQ(kInlineReply1, inline_replies[1].inline_reply_text);
}

TEST_F(PhoneHubNotificationControllerTest, ClickSettings) {
  // TODO(tengs): Test this case once it is implemented.
}

}  // namespace ash
