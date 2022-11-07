// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_tray.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/notification_center/notification_center_bubble.h"
#include "ash/system/notification_center/notification_center_view.h"
#include "ash/system/notification_center/stacked_notification_bar.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

class NotificationCenterTrayTest : public AshTestBase {
 public:
  NotificationCenterTrayTest() = default;
  NotificationCenterTrayTest(const NotificationCenterTrayTest&) = delete;
  NotificationCenterTrayTest& operator=(const NotificationCenterTrayTest&) =
      delete;
  ~NotificationCenterTrayTest() override = default;

  void SetUp() override {
    // Enable quick settings revamp feature.
    scoped_feature_list_.InitWithFeatures({ash::features::kQsRevamp}, {});

    AshTestBase::SetUp();

    notification_tray_ = StatusAreaWidgetTestHelper::GetStatusAreaWidget()
                             ->notification_center_tray();
  }

  void TearDown() override {
    notification_tray_ = nullptr;
    AshTestBase::TearDown();
  }

  std::unique_ptr<message_center::Notification> CreateNotification(
      const std::string& id,
      const std::string& title = "test_title") {
    return std::make_unique<message_center::Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, id, base::UTF8ToUTF16(title),
        u"test message", ui::ImageModel(),
        /*display_source=*/std::u16string(), GURL(),
        message_center::NotifierId(), message_center::RichNotificationData(),
        new message_center::NotificationDelegate());
  }

  std::string AddNotification() {
    std::string id = base::NumberToString(id_++);
    message_center::MessageCenter::Get()->AddNotification(
        CreateNotification(id));
    return id;
  }

  NotificationCenterTray* GetNotificationCenterTray() {
    return notification_tray_;
  }

  views::View* GetClearAllButton() {
    DCHECK(notification_tray_->bubble_);
    return notification_tray_->bubble_->notification_center_view_
        ->notification_bar_->clear_all_button_;
  }

 private:
  int id_ = 0;

  base::test::ScopedFeatureList scoped_feature_list_;

  // Owned by `StatusAreaWidget`.
  NotificationCenterTray* notification_tray_ = nullptr;
};

// Test the initial state.
TEST_F(NotificationCenterTrayTest, ShowTrayButtonOnNotificationAvailability) {
  EXPECT_FALSE(GetNotificationCenterTray()->GetVisible());

  std::string id = AddNotification();
  EXPECT_TRUE(GetNotificationCenterTray()->GetVisible());

  message_center::MessageCenter::Get()->RemoveNotification(id, true);

  EXPECT_FALSE(GetNotificationCenterTray()->GetVisible());
}

// Bubble creation and destruction.
TEST_F(NotificationCenterTrayTest, ShowAndHideBubbleOnUserInteraction) {
  AddNotification();

  auto* tray = GetNotificationCenterTray();

  // Clicking on the tray button should show  the bubble.
  LeftClickOn(tray);
  EXPECT_TRUE(tray->GetBubbleWidget()->IsVisible());
  EXPECT_TRUE(tray->is_active());

  // Clicking a second time should destroy the bubble.
  LeftClickOn(GetNotificationCenterTray());
  EXPECT_FALSE(tray->GetBubbleWidget());
  EXPECT_FALSE(tray->is_active());
}

// Hitting escape after opening the bubble should destroy the bubble
// gracefully.
TEST_F(NotificationCenterTrayTest, EscapeClosesBubble) {
  auto* tray = GetNotificationCenterTray();
  LeftClickOn(tray);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tray->GetBubbleWidget());
  EXPECT_FALSE(tray->is_active());
}
// Removing all notifications by hitting the `clear_all_button_` should result
// in the bubble being destroyed and the tray bubble going invisible.
TEST_F(NotificationCenterTrayTest,
       ClearAllNotificationsDestroysBubbleAndHidesTray) {
  AddNotification();
  AddNotification();
  AddNotification();

  auto* tray = GetNotificationCenterTray();

  LeftClickOn(tray);
  LeftClickOn(GetClearAllButton());
  EXPECT_FALSE(tray->GetBubbleWidget());
  EXPECT_FALSE(tray->is_active());
  EXPECT_FALSE(tray->GetVisible());
}

// The last notification being removed directly by the
// `message_center::MessageCenter` API should result in the bubble being
// destroyed and tray visibilty being updated.
TEST_F(NotificationCenterTrayTest, NotificationsRemovedByMessageCenterApi) {
  std::string id = AddNotification();
  message_center::MessageCenter::Get()->RemoveNotification(id,
                                                           /*by_user=*/true);

  auto* tray = GetNotificationCenterTray();
  EXPECT_FALSE(tray->GetBubbleWidget());
  EXPECT_FALSE(tray->is_active());
  EXPECT_FALSE(tray->GetVisible());
}

// TODO(b/252875025):
// Add following test cases as we add relevant functionality:
// - Focus Change dismissing bubble
// - Popup notifications are dismissed when the bubble appears.
// - New popups are not created when the bubble exists.
// - Display removed while the bubble is shown.
// - Tablet mode transition with the bubble open.
// - Open/Close bubble by keyboard shortcut.

}  // namespace ash
