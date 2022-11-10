// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_tray.h"

#include "ash/constants/ash_features.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"

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
    scoped_feature_list_.InitWithFeatures(
        {features::kQsRevamp, features::kQsRevampWip}, {});

    AshTestBase::SetUp();

    test_api_ = std::make_unique<NotificationCenterTestApi>(
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()
            ->notification_center_tray());
  }

  NotificationCenterTestApi* test_api() { return test_api_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<NotificationCenterTestApi> test_api_;
};

// Test the initial state.
TEST_F(NotificationCenterTrayTest, ShowTrayButtonOnNotificationAvailability) {
  EXPECT_FALSE(test_api()->GetTray()->GetVisible());

  std::string id = test_api()->AddNotification();
  EXPECT_TRUE(test_api()->GetTray()->GetVisible());

  message_center::MessageCenter::Get()->RemoveNotification(id, true);

  EXPECT_FALSE(test_api()->GetTray()->GetVisible());
}

// Bubble creation and destruction.
TEST_F(NotificationCenterTrayTest, ShowAndHideBubbleOnUserInteraction) {
  test_api()->AddNotification();

  auto* tray = test_api()->GetTray();

  // Clicking on the tray button should show  the bubble.
  LeftClickOn(tray);
  EXPECT_TRUE(test_api()->IsBubbleShown());

  // Clicking a second time should destroy the bubble.
  LeftClickOn(test_api()->GetTray());
  EXPECT_FALSE(test_api()->IsBubbleShown());
}

// Hitting escape after opening the bubble should destroy the bubble
// gracefully.
TEST_F(NotificationCenterTrayTest, EscapeClosesBubble) {
  auto* tray = test_api()->GetTray();
  LeftClickOn(tray);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_api()->IsBubbleShown());
}

// Removing all notifications by hitting the `clear_all_button_` should result
// in the bubble being destroyed and the tray bubble going invisible.
TEST_F(NotificationCenterTrayTest,
       ClearAllNotificationsDestroysBubbleAndHidesTray) {
  test_api()->AddNotification();
  test_api()->AddNotification();
  test_api()->AddNotification();

  auto* tray = test_api()->GetTray();

  LeftClickOn(tray);
  LeftClickOn(test_api()->GetClearAllButton());
  EXPECT_FALSE(test_api()->IsBubbleShown());
  EXPECT_FALSE(test_api()->IsTrayShown());
}

// The last notification being removed directly by the
// `message_center::MessageCenter` API should result in the bubble being
// destroyed and tray visibilty being updated.
TEST_F(NotificationCenterTrayTest, NotificationsRemovedByMessageCenterApi) {
  std::string id = test_api()->AddNotification();
  test_api()->RemoveNotification(id);

  EXPECT_FALSE(test_api()->IsBubbleShown());
  EXPECT_FALSE(test_api()->IsTrayShown());
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
