// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_notification_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/shell.h"
#include "ash/system/notification_center/message_center_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/phonehub/fake_feature_status_provider.h"
#include "chromeos/ash/components/phonehub/fake_notification_manager.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/mutable_phone_model.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/views/controls/label.h"

namespace ash {

const int64_t kPhoneHubNotificationId0 = 0;
const int64_t kPhoneHubNotificationId1 = 1;
const int64_t kPhoneHubNotificationId2 = 2;
const int64_t kPhoneHubIncomingCallNotificationId = 3;

const int64_t kUserId = 0;

const char kCrOSNotificationId0[] = "chrome://phonehub-0";
const char kCrOSNotificationId1[] = "chrome://phonehub-1";
const char kCrOSNotificationId2[] = "chrome://phonehub-2";
const char kCrOSIncomingCallNotificationId[] = "chrome://phonehub-3";

const char16_t kAppName[] = u"Test App";
const char kPackageName[] = "com.google.testapp";

const char16_t kTitle[] = u"Test notification";
const char16_t kTextContent[] = u"This is a test notification";

const char kNotificationCustomViewType[] = "phonehub";

// Max notification age for it to be shown heads-up (marked as MAX_PRIORITY)
constexpr base::TimeDelta kMaxRecentNotificationAge = base::Seconds(15);

// Time to wait until we enable the reply button
constexpr base::TimeDelta kInlineReplyDisableTime = base::Seconds(1);

phonehub::Notification CreateNotification(int64_t id) {
  return phonehub::Notification(
      id,
      phonehub::Notification::AppMetadata(
          kAppName, kPackageName, /*color_icon=*/gfx::Image(),
          /*monochrome_icon_mask=*/std::nullopt,
          /*icon_color=*/std::nullopt, /*icon_is_monochrome=*/true, kUserId,
          phonehub::proto::AppStreamabilityStatus::STREAMABLE),
      base::Time::Now(), phonehub::Notification::Importance::kDefault,
      phonehub::Notification::Category::kConversation,
      {{phonehub::Notification::ActionType::kInlineReply,
        /*action_id=*/0}},
      phonehub::Notification::InteractionBehavior::kOpenable, kTitle,
      kTextContent);
}

phonehub::Notification CreateIncomingCallNotification(int64_t id) {
  return phonehub::Notification(
      id,
      phonehub::Notification::AppMetadata(
          kAppName, kPackageName,
          /*color_icon=*/gfx::Image(), /*monochrome_icon_mask=*/std::nullopt,
          /*icon_color=*/std::nullopt,
          /*icon_is_monochrome=*/true, kUserId,
          phonehub::proto::AppStreamabilityStatus::STREAMABLE),
      base::Time::Now(), phonehub::Notification::Importance::kDefault,
      phonehub::Notification::Category::kIncomingCall,
      {{phonehub::Notification::ActionType::kInlineReply,
        /*action_id=*/0}},
      phonehub::Notification::InteractionBehavior::kNone, kTitle, kTextContent);
}

class PhoneHubNotificationControllerTest : public AshTestBase {
 public:
  PhoneHubNotificationControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~PhoneHubNotificationControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kPhoneHub, features::kEcheSWA, features::kPhoneHubCameraRoll,
         features::kPhoneHubMonochromeNotificationIcons},
        {});
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);

    feature_status_provider_ =
        phone_hub_manager_.fake_feature_status_provider();
    feature_status_provider_->SetStatus(
        phonehub::FeatureStatus::kEnabledAndConnected);

    message_center_ = message_center::MessageCenter::Get();

    controller_ = Shell::Get()
                      ->message_center_controller()
                      ->phone_hub_notification_controller();
    controller_->SetManager(&phone_hub_manager_);
    notification_manager_ = phone_hub_manager_.fake_notification_manager();

    fake_notifications_.insert(CreateNotification(kPhoneHubNotificationId0));
    fake_notifications_.insert(CreateNotification(kPhoneHubNotificationId1));
    fake_notifications_.insert(CreateNotification(kPhoneHubNotificationId2));
    fake_notifications_.insert(
        CreateIncomingCallNotification(kPhoneHubIncomingCallNotificationId));
  }

  void TearDown() override {
    widget_.reset();
    AshTestBase::TearDown();
  }

  message_center::Notification* FindNotification(const std::string& cros_id) {
    return message_center_->FindVisibleNotificationById(cros_id);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<message_center::MessageCenter, DanglingUntriaged> message_center_;
  phonehub::FakePhoneHubManager phone_hub_manager_;
  raw_ptr<phonehub::FakeNotificationManager> notification_manager_;
  raw_ptr<phonehub::FakeFeatureStatusProvider> feature_status_provider_;
  raw_ptr<PhoneHubNotificationController, DanglingUntriaged> controller_;
  base::flat_set<phonehub::Notification> fake_notifications_;
};

TEST_F(PhoneHubNotificationControllerTest, AddNotifications) {
  EXPECT_FALSE(message_center_->NotificationCount());
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  EXPECT_EQ(4u, message_center_->NotificationCount());

  ASSERT_TRUE(FindNotification(kCrOSNotificationId0));
  ASSERT_TRUE(FindNotification(kCrOSNotificationId1));
  ASSERT_TRUE(FindNotification(kCrOSNotificationId2));
  ASSERT_TRUE(FindNotification(kCrOSIncomingCallNotificationId));

  auto* sample_notification = FindNotification(kCrOSNotificationId1);
  EXPECT_EQ(kTitle, sample_notification->title());
  EXPECT_EQ(kTextContent, sample_notification->message());
}

TEST_F(PhoneHubNotificationControllerTest, UpdateNotifications) {
  EXPECT_FALSE(message_center_->NotificationCount());
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  EXPECT_EQ(4u, message_center_->NotificationCount());

  auto* notification = FindNotification(kCrOSNotificationId1);
  EXPECT_EQ(kTitle, notification->title());
  EXPECT_EQ(kTextContent, notification->message());

  std::u16string kNewTitle = u"New title";
  std::u16string kNewTextContent = u"New text content";
  phonehub::Notification updated_notification(
      kPhoneHubNotificationId1,
      phonehub::Notification::AppMetadata(
          kAppName, kPackageName,
          /*color_icon=*/gfx::Image(), /*monochrome_icon_mask=*/std::nullopt,
          /*icon_color=*/std::nullopt,
          /*icon_is_monochrome=*/true, kUserId,
          phonehub::proto::AppStreamabilityStatus::STREAMABLE),
      base::Time::Now(), phonehub::Notification::Importance::kDefault,
      phonehub::Notification::Category::kConversation,
      {{phonehub::Notification::ActionType::kInlineReply, 0}},
      phonehub::Notification::InteractionBehavior::kNone, kNewTitle,
      kNewTextContent);

  notification_manager_->SetNotification(updated_notification);

  notification = FindNotification(kCrOSNotificationId1);
  EXPECT_EQ(kNewTitle, notification->title());
  EXPECT_EQ(kNewTextContent, notification->message());
  EXPECT_TRUE(notification->rich_notification_data()
                  .should_make_spoken_feedback_for_popup_updates);
}

TEST_F(PhoneHubNotificationControllerTest, UpdateNotificationsNewIconType) {
  EXPECT_FALSE(message_center_->NotificationCount());
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  EXPECT_EQ(4u, message_center_->NotificationCount());

  auto rich_notification_data =
      FindNotification(kCrOSNotificationId1)->rich_notification_data();
  EXPECT_FALSE(rich_notification_data.accent_color.has_value());
  EXPECT_TRUE(rich_notification_data.ignore_accent_color_for_small_image);
  EXPECT_FALSE(rich_notification_data.ignore_accent_color_for_text);
  EXPECT_TRUE(rich_notification_data.small_image_needs_additional_masking);

  SkColor iconColor = SkColorSetRGB(0x12, 0x34, 0x56);
  phonehub::Notification updated_notification(
      kPhoneHubNotificationId1,
      phonehub::Notification::AppMetadata(
          kAppName, kPackageName, /*color_icon=*/gfx::Image(),
          /*monochrome_icon_mask=*/std::nullopt, iconColor,
          /*icon_is_monochrome=*/true, kUserId,
          phonehub::proto::AppStreamabilityStatus::STREAMABLE),
      base::Time::Now(), phonehub::Notification::Importance::kDefault,
      phonehub::Notification::Category::kConversation,
      {{phonehub::Notification::ActionType::kInlineReply, 0}},
      phonehub::Notification::InteractionBehavior::kNone, kTitle, kTextContent);
  notification_manager_->SetNotification(updated_notification);

  rich_notification_data =
      FindNotification(kCrOSNotificationId1)->rich_notification_data();
  EXPECT_TRUE(rich_notification_data.accent_color.has_value());
  EXPECT_EQ(iconColor, rich_notification_data.accent_color);
  EXPECT_TRUE(rich_notification_data.ignore_accent_color_for_small_image);
  EXPECT_FALSE(rich_notification_data.ignore_accent_color_for_text);
  EXPECT_TRUE(rich_notification_data.small_image_needs_additional_masking);

  updated_notification = phonehub::Notification(
      kPhoneHubNotificationId1,
      phonehub::Notification::AppMetadata(
          kAppName, kPackageName,
          /*color_icon=*/gfx::Image(), /*monochrome_icon_mask=*/std::nullopt,
          /*icon_color=*/std::nullopt,
          /*icon_is_monochrome=*/false, kUserId,
          phonehub::proto::AppStreamabilityStatus::STREAMABLE),
      base::Time::Now(), phonehub::Notification::Importance::kDefault,
      phonehub::Notification::Category::kConversation,
      {{phonehub::Notification::ActionType::kInlineReply, 0}},
      phonehub::Notification::InteractionBehavior::kNone, kTitle, kTextContent);
  notification_manager_->SetNotification(updated_notification);

  rich_notification_data =
      FindNotification(kCrOSNotificationId1)->rich_notification_data();
  EXPECT_FALSE(rich_notification_data.accent_color.has_value());
  EXPECT_TRUE(rich_notification_data.ignore_accent_color_for_small_image);
  EXPECT_FALSE(rich_notification_data.ignore_accent_color_for_text);
  EXPECT_FALSE(rich_notification_data.small_image_needs_additional_masking);
}

TEST_F(PhoneHubNotificationControllerTest, RemoveNotifications) {
  EXPECT_FALSE(message_center_->NotificationCount());
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  EXPECT_EQ(4u, message_center_->NotificationCount());

  notification_manager_->RemoveNotification(kPhoneHubNotificationId0);
  EXPECT_EQ(3u, message_center_->NotificationCount());
  EXPECT_FALSE(FindNotification(kCrOSNotificationId0));

  notification_manager_->RemoveNotificationsInternal(base::flat_set<int64_t>(
      {kPhoneHubNotificationId1, kPhoneHubNotificationId2,
       kPhoneHubIncomingCallNotificationId}));
  EXPECT_FALSE(message_center_->NotificationCount());

  // Attempt removing the same notifications again and expect nothing to happen.
  notification_manager_->RemoveNotificationsInternal(base::flat_set<int64_t>(
      {kPhoneHubNotificationId1, kPhoneHubNotificationId2,
       kPhoneHubIncomingCallNotificationId}));
  EXPECT_FALSE(message_center_->NotificationCount());
}

TEST_F(PhoneHubNotificationControllerTest, CloseByUser) {
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  EXPECT_EQ(4u, message_center_->NotificationCount());

  message_center_->RemoveNotification(kCrOSNotificationId0, /*by_user=*/true);
  message_center_->RemoveNotification(kCrOSNotificationId1, /*by_user=*/true);
  message_center_->RemoveNotification(kCrOSNotificationId2, /*by_user=*/true);

  EXPECT_EQ(
      std::vector<int64_t>({kPhoneHubNotificationId0, kPhoneHubNotificationId1,
                            kPhoneHubNotificationId2}),
      notification_manager_->dismissed_notification_ids());
}

TEST_F(PhoneHubNotificationControllerTest, UserCanNotCloseCallNotification) {
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  EXPECT_EQ(4u, message_center_->NotificationCount());

  message_center_->RemoveNotification(kCrOSNotificationId0, /*by_user=*/true);
  message_center_->RemoveNotification(kCrOSIncomingCallNotificationId,
                                      /*by_user=*/true);

  EXPECT_EQ(std::vector<int64_t>({kPhoneHubNotificationId0}),
            notification_manager_->dismissed_notification_ids());
}

TEST_F(PhoneHubNotificationControllerTest, InlineReply) {
  notification_manager_->SetNotificationsInternal(fake_notifications_);

  const std::u16string kInlineReply0 = u"inline reply 0";
  const std::u16string kInlineReply1 = u"inline reply 1";
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

TEST_F(PhoneHubNotificationControllerTest, HandleNotificationClick) {
  phonehub::FakeNotificationInteractionHandler* handler =
      phone_hub_manager_.fake_notification_interaction_handler();
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  message_center_->ClickOnNotification(kCrOSNotificationId0);
  EXPECT_EQ(1u, handler->handled_notification_count());
  message_center_->ClickOnNotification(kCrOSNotificationId1);
  EXPECT_EQ(2u, handler->handled_notification_count());
}

TEST_F(PhoneHubNotificationControllerTest, ClickSettings) {
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  EXPECT_TRUE(FindNotification(kCrOSNotificationId0));
  EXPECT_EQ(0, GetSystemTrayClient()->show_connected_devices_settings_count());

  message_center_->ClickOnSettingsButton(kCrOSNotificationId0);
  EXPECT_EQ(1, GetSystemTrayClient()->show_connected_devices_settings_count());
}

TEST_F(PhoneHubNotificationControllerTest, NotificationDataAndImages) {
  base::Time timestamp = base::Time::Now();

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

  const std::u16string expected_phone_name = u"Phone name";
  phone_hub_manager_.mutable_phone_model()->SetPhoneName(expected_phone_name);

  phonehub::Notification fake_notification(
      kPhoneHubNotificationId0,
      phonehub::Notification::AppMetadata(
          kAppName, kPackageName, /*color_icon=*/icon,
          /*monochrome_icon_mask=*/std::nullopt,
          /*icon_color=*/std::nullopt,
          /*icon_is_monochrome=*/true, kUserId,
          phonehub::proto::AppStreamabilityStatus::STREAMABLE),
      timestamp, phonehub::Notification::Importance::kHigh,
      phonehub::Notification::Category::kConversation,
      {{phonehub::Notification::ActionType::kInlineReply, 0}},
      phonehub::Notification::InteractionBehavior::kNone, kTitle, kTextContent,
      shared_image, contact_image);

  notification_manager_->SetNotification(fake_notification);

  const std::u16string expected_accessible_name =
      base::StrCat({kAppName, u", ", kTitle, u": ", kTextContent, u", ",
                    expected_phone_name});

  auto* cros_notification = FindNotification(kCrOSNotificationId0);
  ASSERT_TRUE(cros_notification);
  EXPECT_EQ(timestamp, cros_notification->timestamp());
  EXPECT_EQ(message_center::MAX_PRIORITY, cros_notification->priority());
  EXPECT_EQ(kTitle, cros_notification->title());
  EXPECT_EQ(kAppName, cros_notification->display_source());
  EXPECT_EQ(expected_accessible_name, cros_notification->accessible_name());

  // Note that there's a slight discrepancy between the PhoneHub and
  // notification image naming.
  EXPECT_TRUE(contact_image.AsImageSkia().BackedBySameObjectAs(
      cros_notification->icon().Rasterize(nullptr)));
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

  const std::u16string expected_phone_name = u"Phone name";
  phone_hub_manager_.mutable_phone_model()->SetPhoneName(expected_phone_name);

  auto phonehub_notification_view =
      PhoneHubNotificationController::CreateCustomNotificationView(
          controller_->weak_ptr_factory_.GetWeakPtr(), *notification,
          /*shown_in_popup=*/true);
  auto* notification_view = static_cast<message_center::NotificationView*>(
      phonehub_notification_view.get());
  views::Label* summary_text_label =
      static_cast<views::Label*>(notification_view->GetViewByID(
          message_center::NotificationView::kSummaryTextView));

  // Notification should contain phone name in the summary text.
  EXPECT_EQ(expected_phone_name, summary_text_label->GetText());
}

TEST_F(PhoneHubNotificationControllerTest, ReplyBrieflyDisabled) {
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  auto* notification = FindNotification(kCrOSNotificationId0);

  auto phonehub_notification_view =
      PhoneHubNotificationController::CreateCustomNotificationView(
          controller_->weak_ptr_factory_.GetWeakPtr(), *notification,
          /*shown_in_popup=*/true);
  auto* notification_view = static_cast<message_center::NotificationView*>(
      phonehub_notification_view.get());
  views::View* action_buttons_row = notification_view->GetViewByID(
      message_center::NotificationView::kActionButtonsRow);
  views::View* reply_button = action_buttons_row->children()[0];

  auto* view = widget_->SetContentsView(std::move(phonehub_notification_view));
  auto* focus_manager = widget_->GetFocusManager();

  // Initially, reply button should be disabled after replied.
  const std::u16string kInlineReply0 = u"inline reply 0";
  notification_view->OnNotificationInputSubmit(0, kInlineReply0);

  EXPECT_EQ(view, focus_manager->GetFocusedView());
  EXPECT_FALSE(reply_button->GetEnabled());

  // After a brief moment, it should be enabled.
  task_environment()->FastForwardBy(kInlineReplyDisableTime);
  EXPECT_TRUE(reply_button->GetEnabled());
}

TEST_F(PhoneHubNotificationControllerTest, CustomActionRowExpanded) {
  notification_manager_->SetNotificationsInternal(fake_notifications_);
  auto* notification = FindNotification(kCrOSIncomingCallNotificationId);

  auto phonehub_notification_view =
      PhoneHubNotificationController::CreateCustomActionNotificationView(
          controller_->weak_ptr_factory_.GetWeakPtr(), *notification,
          /*shown_in_popup=*/true);
  auto* notification_view = static_cast<message_center::NotificationView*>(
      phonehub_notification_view.get());

  // Initially, action button row should be expanded in incoming call
  // notification.
  EXPECT_TRUE(notification_view->IsManuallyExpandedOrCollapsed());
}

TEST_F(PhoneHubNotificationControllerTest, DoNotShowOldNotification) {
  // Subtract a few extra seconds as a preemptive measure against test flakiness
  base::Time old_timestamp =
      (base::Time::Now() - kMaxRecentNotificationAge) - base::Seconds(5);
  phonehub::Notification fake_notification(
      kPhoneHubNotificationId0,
      phonehub::Notification::AppMetadata(
          kAppName, kPackageName,
          /*color_icon=*/gfx::Image(), /*monochrome_icon_mask=*/std::nullopt,
          /*icon_color=*/std::nullopt,
          /*icon_is_monochrome=*/true, kUserId,
          phonehub::proto::AppStreamabilityStatus::STREAMABLE),
      old_timestamp, phonehub::Notification::Importance::kHigh,
      phonehub::Notification::Category::kConversation,
      {{phonehub::Notification::ActionType::kInlineReply, 0}},
      phonehub::Notification::InteractionBehavior::kNone, kTitle, kTextContent);

  // Adding an old notification does not show a pop-up (LOW_PRIORITY).
  notification_manager_->SetNotification(fake_notification);
  auto* cros_notification = FindNotification(kCrOSNotificationId0);
  ASSERT_TRUE(cros_notification);
  EXPECT_EQ(message_center::LOW_PRIORITY, cros_notification->priority());

  // Removing and readding the old notification (e.g. across disconnects) should
  // not show a pop-up either.
  notification_manager_->RemoveNotification(kPhoneHubNotificationId0);
  ASSERT_FALSE(FindNotification(kCrOSNotificationId0));
  notification_manager_->SetNotification(fake_notification);
  cros_notification = FindNotification(kCrOSNotificationId0);
  ASSERT_TRUE(cros_notification);
  EXPECT_EQ(message_center::LOW_PRIORITY, cros_notification->priority());

  // Update the notification with some new text and a recent timestamp, but keep
  // the notification ID the same. Add a few extra seconds as a preemptive
  // measure against test flakiness.
  phonehub::Notification modified_fake_notification(
      kPhoneHubNotificationId0,
      phonehub::Notification::AppMetadata(
          kAppName, kPackageName,
          /*color_icon=*/gfx::Image(), /*monochrome_icon_mask=*/std::nullopt,
          /*icon_color=*/std::nullopt,
          /*icon_is_monochrome=*/true, kUserId,
          phonehub::proto::AppStreamabilityStatus::STREAMABLE),
      base::Time::Now(), phonehub::Notification::Importance::kHigh,
      phonehub::Notification::Category::kConversation,
      {{phonehub::Notification::ActionType::kInlineReply, 0}},
      phonehub::Notification::InteractionBehavior::kNone, kTitle, u"New text");

  // Update the existing notification; the priority should be MAX_PRIORITY, and
  // renotify should be true.
  notification_manager_->SetNotification(modified_fake_notification);
  cros_notification = FindNotification(kCrOSNotificationId0);
  ASSERT_TRUE(cros_notification);
  EXPECT_EQ(message_center::MAX_PRIORITY, cros_notification->priority());
  EXPECT_TRUE(cros_notification->renotify());

  // Removing and readding the same recent notification (e.g. across
  // disconnects) should still show a pop-up.
  notification_manager_->RemoveNotification(kPhoneHubNotificationId0);
  ASSERT_FALSE(FindNotification(kCrOSNotificationId0));
  notification_manager_->SetNotification(modified_fake_notification);
  cros_notification = FindNotification(kCrOSNotificationId0);
  ASSERT_TRUE(cros_notification);
  EXPECT_EQ(message_center::MAX_PRIORITY, cros_notification->priority());
}

// Regression test for https://crbug.com/1165646.
TEST_F(PhoneHubNotificationControllerTest, MinPriorityNotification) {
  phonehub::Notification fake_notification(
      kPhoneHubNotificationId0,
      phonehub::Notification::AppMetadata(
          kAppName, kPackageName,
          /*color_icon=*/gfx::Image(), /*monochrome_icon_mask=*/std::nullopt,
          /*icon_color=*/std::nullopt,
          /*icon_is_monochrome=*/true, kUserId,
          phonehub::proto::AppStreamabilityStatus::STREAMABLE),
      base::Time::Now(), phonehub::Notification::Importance::kMin,
      phonehub::Notification::Category::kConversation,
      {{phonehub::Notification::ActionType::kInlineReply, 0}},
      phonehub::Notification::InteractionBehavior::kNone, kTitle, kTextContent);

  // Adding the notification for the first time shows a pop-up (MAX_PRIORITY),
  // even though the notification itself is Importance::kMin.
  notification_manager_->SetNotification(fake_notification);
  auto* cros_notification = FindNotification(kCrOSNotificationId0);
  ASSERT_TRUE(cros_notification);
  EXPECT_EQ(message_center::MAX_PRIORITY, cros_notification->priority());
}

}  // namespace ash
