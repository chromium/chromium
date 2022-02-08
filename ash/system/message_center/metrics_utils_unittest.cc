// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/metrics_utils.h"

#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/message_center/unified_message_center_view.h"
#include "ash/system/message_center/unified_message_list_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/widget/widget_utils.h"

using message_center::Notification;

namespace ash {

class MessageCenterMetricsUtilsTest : public AshTestBase {
 public:
  MessageCenterMetricsUtilsTest() = default;
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

  // Create a test notification that is used in the view.
  std::unique_ptr<Notification> CreateTestNotification() {
    message_center::RichNotificationData data;
    data.settings_button_handler =
        message_center::SettingsButtonHandler::INLINE;

    return std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_BASE_FORMAT,
        base::NumberToString(current_id_++), u"title", u"message", gfx::Image(),
        u"display source", GURL(),
        message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                   "extension_id"),
        data, /*delegate=*/nullptr);
  }

  // Get the tested notification view from message center. This is used in
  // checking smoothness metrics: The check requires the use of the compositor,
  // which we don't have in the customed made `notification_view_`.
  views::View* GetNotificationViewFromMessageCenter(const std::string& id) {
    return GetPrimaryUnifiedSystemTray()
        ->message_center_bubble()
        ->message_center_view()
        ->message_list_view()
        ->GetMessageViewForNotificationId(id);
  }

  void ClickView(views::View* view) {
    ui::test::EventGenerator generator(GetRootWindow(view->GetWidget()));
    gfx::Point cursor_location = view->GetBoundsInScreen().CenterPoint();
    generator.MoveMouseTo(cursor_location);
    generator.ClickLeftButton();
  }

  scoped_refptr<message_center::NotificationDelegate> test_delegate() {
    return test_delegate_;
  }

 private:
  scoped_refptr<message_center::NotificationDelegate> test_delegate_;

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

}  // namespace ash