// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/metrics_utils.h"

#include "ash/constants/ash_features.h"
#include "ash/system/message_center/ash_message_popup_collection.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/message_center/unified_message_center_view.h"
#include "ash/system/message_center/unified_message_list_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/views/widget/widget_utils.h"

using message_center::Notification;

namespace {

constexpr char kNotificationViewTypeHistogramName[] =
    "Ash.NotificationView.NotificationAdded.Type";

constexpr char kCountInOneGroupHistogramName[] =
    "Ash.Notification.CountOfNotificationsInOneGroup";

constexpr char kGroupNotificationAddedHistogramName[] =
    "Ash.Notification.GroupNotificationAdded";

const gfx::Image CreateTestImage() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(/*width=*/80, /*height=*/80);
  bitmap.eraseColor(SK_ColorGREEN);
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

void CheckNotificationViewTypeRecorded(
    std::unique_ptr<Notification> notification,
    ash::metrics_utils::NotificationViewType type) {
  base::HistogramTester histograms;

  // Add the notification. Expect that the corresponding notification type is
  // recorded.
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
  histograms.ExpectBucketCount(kNotificationViewTypeHistogramName, type, 1);
}

}  // namespace

namespace ash {

// This serves as an unit test class for all metrics recording in
// notification/message center.
class MessageCenterMetricsUtilsTest : public AshTestBase {
 public:
  MessageCenterMetricsUtilsTest() {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeature(features::kNotificationsRefresh);
  }
  MessageCenterMetricsUtilsTest(const MessageCenterMetricsUtilsTest&) = delete;
  MessageCenterMetricsUtilsTest& operator=(
      const MessageCenterMetricsUtilsTest&) = delete;
  ~MessageCenterMetricsUtilsTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    test_delegate_ =
        base::MakeRefCounted<message_center::NotificationDelegate>();
  }

  // Create a test notification. Noted that the notifications are using the same
  // url and profile id so that they are grouped together.
  std::unique_ptr<Notification> CreateTestNotification() {
    message_center::RichNotificationData data;
    data.settings_button_handler =
        message_center::SettingsButtonHandler::INLINE;
    message_center::NotifierId notifier_id;
    notifier_id.profile_id = "a@b.com";
    notifier_id.type = message_center::NotifierType::WEB_PAGE;
    return std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_BASE_FORMAT,
        base::NumberToString(current_id_++), u"title", u"message", gfx::Image(),
        u"display source", GURL(u"http://test-url.com"), notifier_id, data,
        /*delegate=*/nullptr);
  }

  // Get the notification view from message center associated with `id`.
  views::View* GetNotificationViewFromMessageCenter(const std::string& id) {
    return GetPrimaryUnifiedSystemTray()
        ->message_center_bubble()
        ->message_center_view()
        ->message_list_view()
        ->GetMessageViewForNotificationId(id);
  }

  // Get the popup notification view associated with `id`.
  views::View* GetPopupNotificationView(const std::string& id) {
    return GetPrimaryUnifiedSystemTray()
        ->GetMessagePopupCollection()
        ->GetMessageViewForNotificationId(id);
  }

  void ClickView(views::View* view) {
    ui::test::EventGenerator generator(GetRootWindow(view->GetWidget()));
    gfx::Point cursor_location = view->GetBoundsInScreen().CenterPoint();
    generator.MoveMouseTo(cursor_location);
    generator.ClickLeftButton();
  }

  void HoverOnView(views::View* view) {
    ui::test::EventGenerator generator(GetRootWindow(view->GetWidget()));
    gfx::Point cursor_location = view->GetBoundsInScreen().CenterPoint();
    generator.MoveMouseTo(cursor_location);
  }

  scoped_refptr<message_center::NotificationDelegate> test_delegate() {
    return test_delegate_;
  }

 private:
  scoped_refptr<message_center::NotificationDelegate> test_delegate_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;

  // Used to create test notification. This represents the current available
  // number that we can use to create the next test notification. This id will
  // be incremented whenever we create a new test notification.
  int current_id_ = 0;
};

TEST_F(MessageCenterMetricsUtilsTest, RecordBadClicks) {
  base::HistogramTester histograms;
  auto notification = CreateTestNotification();

  // Add the notification and get its view in message center.
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(*notification));
  GetPrimaryUnifiedSystemTray()->ShowBubble();

  // A click to a notification without a delegate should record a bad click.
  ClickView(GetNotificationViewFromMessageCenter(notification->id()));
  histograms.ExpectTotalCount("Notifications.Cros.Actions.ClickedBody.BadClick",
                              1);
  histograms.ExpectTotalCount(
      "Notifications.Cros.Actions.ClickedBody.GoodClick", 0);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordGoodClicks) {
  base::HistogramTester histograms;
  auto notification = CreateTestNotification();
  notification->set_delegate(test_delegate());

  // Add the notification and get its view in message center.
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(*notification));
  GetPrimaryUnifiedSystemTray()->ShowBubble();

  // A click to a notification with a delegate should record a good click.
  ClickView(GetNotificationViewFromMessageCenter(notification->id()));
  histograms.ExpectTotalCount(
      "Notifications.Cros.Actions.ClickedBody.GoodClick", 1);
  histograms.ExpectTotalCount("Notifications.Cros.Actions.ClickedBody.BadClick",
                              0);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordHover) {
  base::HistogramTester histograms;
  auto notification = CreateTestNotification();

  // Add the notification and get its view in message center.
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(*notification));

  auto* popup = GetPopupNotificationView(notification->id());
  // Move the mouse hover on the popup notification view, expect hover action
  // recorded.
  HoverOnView(popup);
  histograms.ExpectTotalCount("Notifications.Cros.Actions.Popup.Hover", 1);

  GetPrimaryUnifiedSystemTray()->ShowBubble();
  auto* notification_view =
      GetNotificationViewFromMessageCenter(notification->id());

  // Move the mouse hover on the notification view, expect hover action
  // recorded.
  HoverOnView(notification_view);
  histograms.ExpectTotalCount("Notifications.Cros.Actions.Tray.Hover", 1);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordNotificationViewTypeSimple) {
  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);

  auto simple_notification = CreateTestNotification();
  CheckNotificationViewTypeRecorded(
      std::move(simple_notification),
      metrics_utils::NotificationViewType::SIMPLE);

  auto grouped_simple_notification = CreateTestNotification();
  CheckNotificationViewTypeRecorded(
      std::move(grouped_simple_notification),
      metrics_utils::NotificationViewType::GROUPED_SIMPLE);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordNotificationViewTypeImage) {
  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);

  auto image_notification = CreateTestNotification();
  image_notification->set_image(CreateTestImage());
  CheckNotificationViewTypeRecorded(
      std::move(image_notification),
      metrics_utils::NotificationViewType::HAS_IMAGE);

  auto grouped_image_notification = CreateTestNotification();
  grouped_image_notification->set_image(CreateTestImage());
  CheckNotificationViewTypeRecorded(
      std::move(grouped_image_notification),
      metrics_utils::NotificationViewType::GROUPED_HAS_IMAGE);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordNotificationViewTypeActionButtons) {
  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);

  auto notification = CreateTestNotification();
  notification->set_buttons({message_center::ButtonInfo(u"Test button")});
  CheckNotificationViewTypeRecorded(
      std::move(notification), metrics_utils::NotificationViewType::HAS_ACTION);

  auto grouped_notification = CreateTestNotification();
  grouped_notification->set_buttons(
      {message_center::ButtonInfo(u"Test button")});
  CheckNotificationViewTypeRecorded(
      std::move(grouped_notification),
      metrics_utils::NotificationViewType::GROUPED_HAS_ACTION);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordNotificationViewTypeInlineReply) {
  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);

  auto create_inline_reply_button = []() {
    message_center::ButtonInfo button(u"Test button");
    button.placeholder = std::u16string();
    return button;
  };
  auto notification = CreateTestNotification();
  notification->set_buttons({create_inline_reply_button()});

  CheckNotificationViewTypeRecorded(
      std::move(notification),
      metrics_utils::NotificationViewType::HAS_INLINE_REPLY);

  auto grouped_notification = CreateTestNotification();
  grouped_notification->set_buttons({create_inline_reply_button()});
  CheckNotificationViewTypeRecorded(
      std::move(grouped_notification),
      metrics_utils::NotificationViewType::GROUPED_HAS_INLINE_REPLY);
}

TEST_F(MessageCenterMetricsUtilsTest,
       RecordNotificationViewTypeImageActionButtons) {
  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);

  auto notification = CreateTestNotification();
  notification->set_image(CreateTestImage());
  notification->set_buttons({message_center::ButtonInfo(u"Test button")});
  CheckNotificationViewTypeRecorded(
      std::move(notification),
      metrics_utils::NotificationViewType::HAS_IMAGE_AND_ACTION);

  auto grouped_notification = CreateTestNotification();
  grouped_notification->set_image(CreateTestImage());
  grouped_notification->set_buttons(
      {message_center::ButtonInfo(u"Test button")});
  CheckNotificationViewTypeRecorded(
      std::move(grouped_notification),
      metrics_utils::NotificationViewType::GROUPED_HAS_IMAGE_AND_ACTION);
}

TEST_F(MessageCenterMetricsUtilsTest,
       RecordNotificationViewTypeImageInlineReply) {
  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);

  auto create_inline_reply_button = []() {
    message_center::ButtonInfo button(u"Test button");
    button.placeholder = std::u16string();
    return button;
  };
  auto notification = CreateTestNotification();
  notification->set_image(CreateTestImage());
  notification->set_buttons({create_inline_reply_button()});

  CheckNotificationViewTypeRecorded(
      std::move(notification),
      metrics_utils::NotificationViewType::HAS_IMAGE_AND_INLINE_REPLY);

  auto grouped_notification = CreateTestNotification();
  grouped_notification->set_image(CreateTestImage());
  grouped_notification->set_buttons({create_inline_reply_button()});
  CheckNotificationViewTypeRecorded(
      std::move(grouped_notification),
      metrics_utils::NotificationViewType::GROUPED_HAS_IMAGE_AND_INLINE_REPLY);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordCountOfNotificationsInOneGroup) {
  base::HistogramTester histograms;

  auto notification1 = CreateTestNotification();
  std::string id1 = notification1->id();
  auto notification2 = CreateTestNotification();
  std::string id2 = notification2->id();
  auto notification3 = CreateTestNotification();

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification1));
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification2));

  histograms.ExpectBucketCount(kCountInOneGroupHistogramName, 2, 1);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification3));
  histograms.ExpectBucketCount(kCountInOneGroupHistogramName, 3, 1);

  message_center::MessageCenter::Get()->RemoveNotification(id1,
                                                           /*by_user=*/true);
  histograms.ExpectBucketCount(kCountInOneGroupHistogramName, 2, 2);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordGroupNotificationAddedType) {
  base::HistogramTester histograms;

  auto notification1 = CreateTestNotification();
  auto notification2 = CreateTestNotification();
  auto notification3 = CreateTestNotification();

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification1));
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification2));

  // There should be 1 group parent that contains 2 group child notifications.
  histograms.ExpectBucketCount(
      kGroupNotificationAddedHistogramName,
      metrics_utils::GroupNotificationType::GROUP_PARENT, 1);
  histograms.ExpectBucketCount(
      kGroupNotificationAddedHistogramName,
      metrics_utils::GroupNotificationType::GROUP_CHILD, 2);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification3));
  histograms.ExpectBucketCount(
      kGroupNotificationAddedHistogramName,
      metrics_utils::GroupNotificationType::GROUP_PARENT, 1);
  histograms.ExpectBucketCount(
      kGroupNotificationAddedHistogramName,
      metrics_utils::GroupNotificationType::GROUP_CHILD, 3);
}

}  // namespace ash
