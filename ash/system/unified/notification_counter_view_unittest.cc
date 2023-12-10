// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_counter_view.h"

#include "ash/shelf/shelf.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/unified/notification_icons_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

namespace ash {

namespace {

void AddNotification(const std::string& notification_id,
                     bool is_pinned = false) {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.pinned = is_pinned;
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          u"test_title", u"test message", ui::ImageModel(),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                     "app"),
          rich_notification_data, new message_center::NotificationDelegate()));
}

}  // namespace

class NotificationCounterViewTest : public AshTestBase {
 public:
  NotificationCounterViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  NotificationCounterViewTest(const NotificationCounterViewTest&) = delete;
  NotificationCounterViewTest& operator=(const NotificationCounterViewTest&) =
      delete;
  ~NotificationCounterViewTest() override = default;

 protected:
  NotificationCounterView* GetNotificationCounterView() {
    auto* status_area_widget = GetPrimaryShelf()->status_area_widget();
    return status_area_widget->notification_center_tray()
        ->notification_icons_controller_->notification_counter_view();
  }

  QuietModeView* GetDoNotDisturbIconView() {
    auto* status_area_widget = GetPrimaryShelf()->status_area_widget();
    return status_area_widget->notification_center_tray()
        ->notification_icons_controller_->quiet_mode_view();
  }
};

TEST_F(NotificationCounterViewTest, CountForDisplay) {
  // Not visible when count == 0.
  GetNotificationCounterView()->Update();
  EXPECT_EQ(0, GetNotificationCounterView()->count_for_display_for_testing());
  EXPECT_FALSE(GetNotificationCounterView()->GetVisible());

  // Count is visible and updates between 1..max+1.
  int max = static_cast<int>(kTrayNotificationMaxCount);
  for (int i = 1; i <= max + 1; i++) {
    AddNotification(base::NumberToString(i));
    GetNotificationCounterView()->Update();
    EXPECT_EQ(i, GetNotificationCounterView()->count_for_display_for_testing());
    EXPECT_TRUE(GetNotificationCounterView()->GetVisible());
  }

  // Count does not change after max+1.
  AddNotification(base::NumberToString(max + 2));
  GetNotificationCounterView()->Update();
  EXPECT_EQ(max + 1,
            GetNotificationCounterView()->count_for_display_for_testing());
  EXPECT_TRUE(GetNotificationCounterView()->GetVisible());
}

TEST_F(NotificationCounterViewTest, HiddenNotificationCount) {
  // Not visible when count == 0.
  GetNotificationCounterView()->Update();
  EXPECT_EQ(0, GetNotificationCounterView()->count_for_display_for_testing());
  EXPECT_FALSE(GetNotificationCounterView()->GetVisible());

  // Added a pinned notification, counter should not be visible.
  AddNotification("1", true /* is_pinned */);
  GetNotificationCounterView()->Update();
  EXPECT_TRUE(!GetNotificationCounterView()->GetVisible());

  // Added a normal notification.
  AddNotification("2");
  GetNotificationCounterView()->Update();
  EXPECT_TRUE(GetNotificationCounterView()->GetVisible());
  EXPECT_EQ(1, GetNotificationCounterView()->count_for_display_for_testing());

  // Added another pinned.
  AddNotification("3", true /* is_pinned */);
  GetNotificationCounterView()->Update();
  EXPECT_TRUE(GetNotificationCounterView()->GetVisible());
  EXPECT_EQ(1, GetNotificationCounterView()->count_for_display_for_testing());

  message_center::MessageCenter::Get()->RemoveNotification("1",
                                                           false /* by_user */);
  message_center::MessageCenter::Get()->RemoveNotification("3",
                                                           false /* by_user */);
  GetNotificationCounterView()->Update();
  EXPECT_EQ(1, GetNotificationCounterView()->count_for_display_for_testing());
}

TEST_F(NotificationCounterViewTest, DisplayChanged) {
  AddNotification("1", true /* is_pinned */);
  GetNotificationCounterView()->Update();

  // In medium size screen, the counter should not be displayed since pinned
  // notification icon is shown (if the feature is enabled).
  UpdateDisplay("800x700");
  EXPECT_TRUE(!GetNotificationCounterView()->GetVisible());

  // The counter should not be shown when we remove the pinned notification.
  message_center::MessageCenter::Get()->RemoveNotification("1",
                                                           false /* by_user */);
  GetNotificationCounterView()->Update();
  EXPECT_FALSE(GetNotificationCounterView()->GetVisible());

  AddNotification("1", true /* is_pinned */);
  GetNotificationCounterView()->Update();

  // In small display, the counter show be shown with pinned notification.
  UpdateDisplay("600x500");
  EXPECT_TRUE(GetNotificationCounterView()->GetVisible());

  // In large screen size, expected the same behavior like medium screen size.
  UpdateDisplay("1680x800");
  EXPECT_TRUE(!GetNotificationCounterView()->GetVisible());

  message_center::MessageCenter::Get()->RemoveNotification("1",
                                                           false /* by_user */);
  GetNotificationCounterView()->Update();
  EXPECT_FALSE(GetNotificationCounterView()->GetVisible());
}

TEST_F(NotificationCounterViewTest, DoNotDisturbIconVisibility) {
  ASSERT_FALSE(GetDoNotDisturbIconView()->GetVisible());

  // Turn on Do not disturb mode.
  message_center::MessageCenter::Get()->SetQuietMode(true);
  EXPECT_TRUE(GetDoNotDisturbIconView()->GetVisible());

  // Show the lock screen.
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  EXPECT_FALSE(GetDoNotDisturbIconView()->GetVisible());

  // Log in.
  UnblockUserSession();
  EXPECT_TRUE(GetDoNotDisturbIconView()->GetVisible());
}

TEST_F(NotificationCounterViewTest, LockScreenCounter) {
  for (size_t i = 0; i < kTrayNotificationMaxCount; i++) {
    AddNotification(base::NumberToString(i));
  }

  // Make sure we show the full count of notifications on the lock screen.
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  EXPECT_TRUE(GetNotificationCounterView()->GetVisible());
  EXPECT_EQ(static_cast<int>(kTrayNotificationMaxCount),
            GetNotificationCounterView()->count_for_display_for_testing());
}

TEST_F(NotificationCounterViewTest, LockScreenCounterInDoNotDisturbMode) {
  for (size_t i = 0; i < kTrayNotificationMaxCount; i++) {
    AddNotification(base::NumberToString(i));
  }

  // Turn on Do not disturb mode.
  message_center::MessageCenter::Get()->SetQuietMode(true);

  // Counter not shown when Do not disturb mode is enabled.
  EXPECT_FALSE(GetNotificationCounterView()->GetVisible());

  // Counter should become visible if the screen is locked.
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  EXPECT_TRUE(GetNotificationCounterView()->GetVisible());
}

}  // namespace ash
