// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_notification_controller.h"

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/shell.h"
#include "ash/system/message_center/message_center_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/phonehub/fake_feature_status_provider.h"
#include "chromeos/components/phonehub/fake_notification_manager.h"
#include "chromeos/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/components/phonehub/mutable_phone_model.h"
#include "chromeos/components/phonehub/notification.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/notification_view_md.h"

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

const char kNotificationCustomViewType[] = "phonehub";

// Time to wait until we enable the reply button
constexpr base::TimeDelta kWaitForEnableButton =
    base::TimeDelta::FromSeconds(1);

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
  PhoneHubNotificationControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~PhoneHubNotificationControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(chromeos::features::kPhoneHub);
    AshTestBase::SetUp();

    feature_status_provider_ =
        phone_hub_manager_.fake_feature_status_provider();
    feature_status_provider_->SetStatus(
        chromeos::phonehub::FeatureStatus::kEnabledAndConnected);

    message_center_ = message_center::MessageCenter::Get();

    controller_ = Shell::Get()
                      ->message_center_controller()
                      ->phone_hub_notification_controller();
    controller_->SetManager(&phone_hub_manager_);
    notification_manager_ = phone_hub_manager_.fake_notification_manager();

    fake_notifications_.insert(CreateNotification(kPhoneHubNotificationId0));
    fake_notifications_.insert(CreateNotification(kPhoneHubNotificationId1));
    fake_notifications_.insert(CreateNotification(kPhoneHubNotificationId2));
  }

  message_center::Notification* FindNotification(const std::string& cros_id) {
    return message_center_->FindVisibleNotificationById(cros_id);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  message_center::MessageCenter* message_center_;
  chromeos::phonehub::FakePhoneHubManager phone_hub_manager_;
  chromeos::phonehub::FakeNotificationManager* notification_manager_;
  chromeos::phonehub::FakeFeatureStatusProvider* feature_status_provider_;
  PhoneHubNotificationController* controller_;
  base::flat_set<chromeos::phonehub::Notification> fake_notifications_;
};

TEST_F(PhoneHubNotificationControllerTest, AddNotifications) {
  EXPECT_FALSE(message_center_->NotificationCount());
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  EXPECT_EQ(3u, message_center_->NotificationCount());

  ASSERT_TRUE(FindNotification(kCrOSNotificationId0));
  ASSERT_TRUE(FindNotification(kCrOSNotificationId1));
  ASSERT_TRUE(FindNotification(kCrOSNotificationId2));

  auto* sample_notification = FindNotification(kCrOSNotificationId1);
  EXPECT_EQ(base::UTF8ToUTF16(kTitle), sample_notification->title());
  EXPECT_EQ(base::UTF8ToUTF16(kTextContent), sample_notification->message());
}

TEST_F(PhoneHubNotificationControllerTest, UpdateNotifications) {
  EXPECT_FALSE(message_center_->NotificationCount());
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  EXPECT_EQ(3u, message_center_->NotificationCount());

  auto* notification = FindNotification(kCrOSNotificationId1);
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

  notification_manager_->SetNotification(updated_notification);

  notification = FindNotification(kCrOSNotificationId1);
  EXPECT_EQ(base::UTF8ToUTF16(kNewTitle), notification->title());
  EXPECT_EQ(base::UTF8ToUTF16(kNewTextContent), notification->message());
}

TEST_F(PhoneHubNotificationControllerTest, RemoveNotifications) {
  EXPECT_FALSE(message_center_->NotificationCount());
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  EXPECT_EQ(3u, message_center_->NotificationCount());

  notification_manager_->RemoveNotification(kPhoneHubNotificationId0);
  EXPECT_EQ(2u, message_center_->NotificationCount());
  EXPECT_FALSE(FindNotification(kCrOSNotificationId0));

  notification_manager_->RemoveNotificationsInternal(base::flat_set<int64_t>(
      {kPhoneHubNotificationId1, kPhoneHubNotificationId2}));
  EXPECT_FALSE(message_center_->NotificationCount());

  // Attempt removing the same notifications again and expect nothing to happen.
  notification_manager_->RemoveNotificationsInternal(base::flat_set<int64_t>(
      {kPhoneHubNotificationId1, kPhoneHubNotificationId2}));
  EXPECT_FALSE(message_center_->NotificationCount());
}

TEST_F(PhoneHubNotificationControllerTest, CloseByUser) {
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  EXPECT_EQ(3u, message_center_->NotificationCount());

  message_center_->RemoveNotification(kCrOSNotificationId0, /*by_user=*/true);
  message_center_->RemoveNotification(kCrOSNotificationId1, /*by_user=*/true);
  message_center_->RemoveNotification(kCrOSNotificationId2, /*by_user=*/true);

  EXPECT_EQ(
      std::vector<int64_t>({kPhoneHubNotificationId0, kPhoneHubNotificationId1,
                            kPhoneHubNotificationId2}),
      notification_manager_->dismissed_notification_ids());
}

TEST_F(PhoneHubNotificationControllerTest, InlineReply) {
  notification_manager_->SetNotificationsInternal(fake_notifications_);

  const base::string16 kInlineReply0 = base::UTF8ToUTF16("inline reply 0");
  const base::string16 kInlineReply1 = base::UTF8ToUTF16("inline reply 1");
  message_center_->ClickOnNotificationButtonWithReply(kCrOSNotificationId0, 0,
                                                      kInlineReply0);
  message_center_->ClickOnNotificationButtonWithReply(kCrOSNotificationId1, 0,
                                                      kInlineReply1);

  auto inline_replies = notification_manager_->inline_replies();
  EXPECT_EQ(kPhoneHubNotificationId0, inline_replies[0].notification_id);
  EXPECT_EQ(kInlineReply0, inline_replies[0].inline_reply_text);
  EXPECT_EQ(kPhoneHubNotificationId1, inline_replies[1].notification_id);
  EXPECT_EQ(kInlineReply1, inline_replies[1].inline_reply_text);
}

TEST_F(PhoneHubNotificationControllerTest, ClickSettings) {
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  EXPECT_TRUE(FindNotification(kCrOSNotificationId0));
  EXPECT_EQ(0, GetSystemTrayClient()->show_connected_devices_settings_count());

  message_center_->ClickOnSettingsButton(kCrOSNotificationId0);
  EXPECT_EQ(1, GetSystemTrayClient()->show_connected_devices_settings_count());
}

TEST_F(PhoneHubNotificationControllerTest, NotificationDataAndImages) {
  base::Time timestamp = base::Time::FromJsTime(12345);

  SkBitmap icon_bitmap;
  icon_bitmap.allocN32Pixels(32, 32);
  gfx::Image icon(gfx::ImageSkia::CreateFrom1xBitmap(icon_bitmap));

  SkBitmap contact_image_bitmap;
  contact_image_bitmap.allocN32Pixels(80, 80);
  gfx::Image contact_image(
      gfx::ImageSkia::CreateFrom1xBitmap(contact_image_bitmap));

  SkBitmap shared_image_bitmap;
  shared_image_bitmap.allocN32Pixels(400, 300);
  gfx::Image shared_image(
      gfx::ImageSkia::CreateFrom1xBitmap(shared_image_bitmap));

  chromeos::phonehub::Notification fake_notification(
      kPhoneHubNotificationId0,
      chromeos::phonehub::Notification::AppMetadata(base::UTF8ToUTF16(kAppName),
                                                    kPackageName, icon),
      timestamp, chromeos::phonehub::Notification::Importance::kHigh,
      /*inline_reply_id=*/0, base::UTF8ToUTF16(kTitle),
      base::UTF8ToUTF16(kTextContent), shared_image, contact_image);

  notification_manager_->SetNotification(fake_notification);

  auto* cros_notification = FindNotification(kCrOSNotificationId0);
  ASSERT_TRUE(cros_notification);
  EXPECT_EQ(timestamp, cros_notification->timestamp());
  EXPECT_EQ(message_center::MAX_PRIORITY, cros_notification->priority());
  EXPECT_EQ(kTitle, base::UTF16ToUTF8(cros_notification->title()));
  EXPECT_EQ(kAppName, base::UTF16ToUTF8(cros_notification->display_source()));

  // Note that there's a slight discrepancy between the PhoneHub and
  // notification image naming.
  EXPECT_EQ(contact_image, cros_notification->icon());
  EXPECT_EQ(icon, cros_notification->small_image());
  EXPECT_EQ(shared_image, cros_notification->image());
}

TEST_F(PhoneHubNotificationControllerTest, NotificationHasCustomViewType) {
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  auto* notification = FindNotification(kCrOSNotificationId0);

  // Notification should have a correct customize type.
  EXPECT_EQ(kNotificationCustomViewType, notification->custom_view_type());
}

TEST_F(PhoneHubNotificationControllerTest, NotificationHasPhoneName) {
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  auto* notification = FindNotification(kCrOSNotificationId0);

  const base::string16 expected_phone_name = base::UTF8ToUTF16("Phone name");
  phone_hub_manager_.mutable_phone_model()->SetPhoneName(expected_phone_name);

  auto notification_view =
      PhoneHubNotificationController::CreateCustomNotificationView(
          controller_->weak_ptr_factory_.GetWeakPtr(), *notification);
  auto* notification_view_md =
      static_cast<message_center::NotificationViewMD*>(notification_view.get());
  views::Label* summary_text_label =
      static_cast<views::Label*>(notification_view_md->GetViewByID(
          message_center::NotificationViewMD::kSummaryTextView));

  // Notification should contain phone name in the summary text.
  EXPECT_EQ(expected_phone_name, summary_text_label->GetText());
}

TEST_F(PhoneHubNotificationControllerTest, ReplyBrieflyDisabled) {
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  auto* notification = FindNotification(kCrOSNotificationId0);

  auto notification_view =
      PhoneHubNotificationController::CreateCustomNotificationView(
          controller_->weak_ptr_factory_.GetWeakPtr(), *notification);
  auto* notification_view_md =
      static_cast<message_center::NotificationViewMD*>(notification_view.get());
  views::View* action_buttons_row = notification_view_md->GetViewByID(
      message_center::NotificationViewMD::kActionButtonsRow);
  views::View* reply_button = action_buttons_row->children()[0];

  // Initially, reply button should be disabled after replied.
  const base::string16 kInlineReply0 = base::UTF8ToUTF16("inline reply 0");
  notification_view_md->OnNotificationInputSubmit(0, kInlineReply0);
  EXPECT_FALSE(reply_button->GetEnabled());

  // After a brief moment, it should be enabled.
  task_environment()->FastForwardBy(kWaitForEnableButton);
  EXPECT_TRUE(reply_button->GetEnabled());
}

TEST_F(PhoneHubNotificationControllerTest, DoNotReshowPopupNotification) {
  chromeos::phonehub::Notification fake_notification(
      kPhoneHubNotificationId0,
      chromeos::phonehub::Notification::AppMetadata(base::UTF8ToUTF16(kAppName),
                                                    kPackageName,
                                                    /*icon=*/gfx::Image()),
      base::Time::Now(), chromeos::phonehub::Notification::Importance::kHigh,
      /*inline_reply_id=*/0, base::UTF8ToUTF16(kTitle),
      base::UTF8ToUTF16(kTextContent));

  // Adding the notification for the first time shows a pop-up (MAX_PRIORITY).
  notification_manager_->SetNotification(fake_notification);
  auto* cros_notification = FindNotification(kCrOSNotificationId0);
  ASSERT_TRUE(cros_notification);
  EXPECT_EQ(message_center::MAX_PRIORITY, cros_notification->priority());

  feature_status_provider_->SetStatus(
      chromeos::phonehub::FeatureStatus::kEnabledButDisconnected);
  feature_status_provider_->SetStatus(
      chromeos::phonehub::FeatureStatus::kEnabledAndConnecting);
  feature_status_provider_->SetStatus(
      chromeos::phonehub::FeatureStatus::kUnavailableBluetoothOff);
  feature_status_provider_->SetStatus(
      chromeos::phonehub::FeatureStatus::kLockOrSuspended);
  feature_status_provider_->SetStatus(
      chromeos::phonehub::FeatureStatus::kEnabledAndConnected);

  // Removing and readding the notification (e.g. across disconnects) should
  // downgrade the priority so it doesn't pop-up again.
  notification_manager_->RemoveNotification(kPhoneHubNotificationId0);
  ASSERT_FALSE(FindNotification(kCrOSNotificationId0));
  notification_manager_->SetNotification(fake_notification);
  cros_notification = FindNotification(kCrOSNotificationId0);
  ASSERT_TRUE(cros_notification);
  EXPECT_EQ(message_center::LOW_PRIORITY, cros_notification->priority());

  // Disable the feature.
  feature_status_provider_->SetStatus(
      chromeos::phonehub::FeatureStatus::kDisabled);
  notification_manager_->RemoveNotification(kPhoneHubNotificationId0);
  ASSERT_FALSE(FindNotification(kCrOSNotificationId0));

  // Reconnect and notification should be reshown as a pop-up.
  feature_status_provider_->SetStatus(
      chromeos::phonehub::FeatureStatus::kEnabledAndConnected);
  notification_manager_->SetNotification(fake_notification);
  cros_notification = FindNotification(kCrOSNotificationId0);
  ASSERT_TRUE(cros_notification);
  EXPECT_EQ(message_center::MAX_PRIORITY, cros_notification->priority());

  // Update the notification with some new text, but keep the notification ID
  // the same.
  chromeos::phonehub::Notification modified_fake_notification(
      kPhoneHubNotificationId0,
      chromeos::phonehub::Notification::AppMetadata(base::UTF8ToUTF16(kAppName),
                                                    kPackageName,
                                                    /*icon=*/gfx::Image()),
      base::Time::Now(), chromeos::phonehub::Notification::Importance::kHigh,
      /*inline_reply_id=*/0, base::UTF8ToUTF16(kTitle),
      base::UTF8ToUTF16("New text"));

  // Update the existingt notification; the priority should be MAX_PRIORITY, and
  // renotify should be true.
  notification_manager_->SetNotification(modified_fake_notification);
  cros_notification = FindNotification(kCrOSNotificationId0);
  ASSERT_TRUE(cros_notification);
  EXPECT_EQ(message_center::MAX_PRIORITY, cros_notification->priority());
  EXPECT_TRUE(cros_notification->renotify());
}

}  // namespace ash
