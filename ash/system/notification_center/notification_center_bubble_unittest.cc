// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_bubble.h"

#include <cstdint>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/message_center/ash_notification_view.h"
#include "ash/system/notification_center/notification_center_bubble.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/notification_center/notification_center_view.h"
#include "ash/system/notification_center/notification_list_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/message_center/views/message_view.h"

namespace ash {

class NotificationCenterBubbleTest : public AshTestBase {
 public:
  NotificationCenterBubbleTest() = default;
  NotificationCenterBubbleTest(const NotificationCenterBubbleTest&) = delete;
  NotificationCenterBubbleTest& operator=(const NotificationCenterBubbleTest&) =
      delete;
  ~NotificationCenterBubbleTest() override = default;

  void SetUp() override {
    // Enable quick settings revamp feature.
    scoped_feature_list_.InitAndEnableFeature(features::kQsRevamp);

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

// Tests that the notification bubble does not get cut off by the top of the
// screen on the launcher homescreen in tablet mode; see b/278471988.
TEST_F(NotificationCenterBubbleTest,
       TopOfBubbleConstrainedByTopOfDisplayInTabletModeHomescreen) {
  // Set the display to some known size.
  UpdateDisplay("1200x800");

  // Switch to tablet mode and verify we're on the launcher homescreen.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  ASSERT_EQ(ShelfBackgroundType::kHomeLauncher,
            GetPrimaryShelf()->GetBackgroundType());

  // Add a large number of notifications to overflow the scroll view in the
  // notification center.
  for (int i = 0; i < 100; i++) {
    test_api()->AddNotification();
  }

  // Show the notification center bubble.
  test_api()->ToggleBubble();

  // Verify that the top of the notification center bubble window is not beyond
  // the top of the display.
  EXPECT_GE(
      test_api()->GetBubble()->GetBubbleWidget()->GetWindowBoundsInScreen().y(),
      0);
}

TEST_F(NotificationCenterBubbleTest, BubbleHeightConstrainedByDisplay) {
  const int display_height = 800;
  UpdateDisplay("1200x" + base::NumberToString(display_height));

  // Add a large number of notifications to overflow the scroll view in the
  // notification center.
  for (int i = 0; i < 100; i++) {
    test_api()->AddNotification();
  }

  // Show notification center bubble.
  test_api()->ToggleBubble();

  // The height of the notification center should not exceed the
  // display height.
  EXPECT_LT(test_api()->GetNotificationCenterView()->bounds().height(),
            display_height);
}

TEST_F(NotificationCenterBubbleTest, BubbleHeightUpdatedByDisplaySizeChange) {
  UpdateDisplay("800x600");

  // Add a large number of notifications to overflow the scroll view in the
  // notification center.
  for (int i = 0; i < 100; i++) {
    test_api()->AddNotification();
  }

  // Show notification center bubble.
  test_api()->ToggleBubble();

  gfx::Rect previous_bounds = test_api()->GetNotificationCenterView()->bounds();

  UpdateDisplay("1600x800");

  gfx::Rect current_bounds = test_api()->GetNotificationCenterView()->bounds();

  // The height of the notification center should increase as the display height
  // has increased. However, the width should stay constant.
  EXPECT_GT(current_bounds.height(), previous_bounds.height());
  EXPECT_EQ(current_bounds.width(), previous_bounds.width());
}

TEST_F(NotificationCenterBubbleTest, BubbleHeightUpdatedByDisplayRotation) {
  const int display_width = 1000;
  const int display_height = 600;
  UpdateDisplay(base::NumberToString(display_width) + "x" +
                base::NumberToString(display_height));

  // Add a large number of notifications to overflow the scroll view in the
  // notification center.
  for (int i = 0; i < 100; i++) {
    test_api()->AddNotification();
  }

  // Show notification center bubble.
  test_api()->ToggleBubble();

  // Rotate the display to portrait mode.
  auto* display_manager = Shell::Get()->display_manager();
  display::Display display = GetPrimaryDisplay();
  display_manager->SetDisplayRotation(display.id(), display::Display::ROTATE_90,
                                      display::Display::RotationSource::ACTIVE);

  auto* notification_center_view = test_api()->GetNotificationCenterView();

  // In portrait mode the notification center's height should be constrained by
  // the original `display_width`.
  EXPECT_GT(notification_center_view->bounds().height(), display_height);
  EXPECT_LT(notification_center_view->bounds().height(), display_width);

  // Rotate back to landscape mode.
  display_manager->SetDisplayRotation(display.id(), display::Display::ROTATE_0,
                                      display::Display::RotationSource::ACTIVE);

  // In landspace mode the height constraint should be back to `display_height`.
  EXPECT_LT(notification_center_view->bounds().height(), display_height);
}

// Tests that notifications from a single notifier id are grouped in a single
// parent notification view.
TEST_F(NotificationCenterBubbleTest, NotificationsGroupingBasic) {
  const std::string source_url = "http://test-url.com";

  std::string id0, id1;
  id0 = test_api()->AddNotificationWithSourceUrl(source_url);
  id1 = test_api()->AddNotificationWithSourceUrl(source_url);

  // Get the notification id for the parent notification. Parent notifications
  // are created by copying the oldest notification for a given notifier_id.
  const std::string parent_id =
      test_api()->NotificationIdToParentNotificationId(id0);

  test_api()->ToggleBubble();

  auto* parent_notification_view =
      test_api()->GetNotificationViewForId(parent_id);

  // Ensure id0, id1 exist as child notifications inside the
  // `parent_notification_view`.
  EXPECT_TRUE(parent_notification_view->FindGroupNotificationView(id0));
  EXPECT_TRUE(parent_notification_view->FindGroupNotificationView(id1));
}

TEST_F(NotificationCenterBubbleTest,
       NotificationCollapseStatePreservedFromPopup) {
  std::string id0 = test_api()->AddNotification();

  // Manually collapse the notification.
  static_cast<AshNotificationView*>(
      test_api()->GetPopupViewForId(id0)->message_view())
      ->ToggleExpand();

  // Expect `id0` to be collapsed despite the default state for it to be
  // expanded.
  test_api()->ToggleBubble();
  EXPECT_FALSE(test_api()->GetNotificationViewForId(id0)->IsExpanded());
}

TEST_F(NotificationCenterBubbleTest,
       NotificationExpandStatePreservedAcrossDisplays) {
  UpdateDisplay("600x500,600x500");

  std::string id0, id1;
  id0 = test_api()->AddNotification();
  id1 = test_api()->AddNotification();

  const int64_t secondary_display_id = display_manager()->GetDisplayAt(1).id();

  test_api()->ToggleBubbleOnDisplay(secondary_display_id);

  auto* notification_view0 = static_cast<AshNotificationView*>(
      test_api()->GetNotificationViewForIdOnDisplay(id0, secondary_display_id));

  auto* notification_view1 = static_cast<AshNotificationView*>(
      test_api()->GetNotificationViewForIdOnDisplay(id1, secondary_display_id));

  // The newest notification is expected to be expanded by default while other
  // notifications are expected to be collapsed by default.
  EXPECT_TRUE(notification_view1->IsExpanded());
  EXPECT_FALSE(notification_view0->IsExpanded());

  // Toggle the expand states for both notifications so they are opposite of the
  // default state.
  notification_view0->ToggleExpand();
  notification_view1->ToggleExpand();

  const int64_t primary_display_id = display_manager()->GetDisplayAt(0).id();
  test_api()->ToggleBubbleOnDisplay(primary_display_id);

  // Expect the expanded states to be preserved on the primary display after
  // they were changed by the user on the secondary display.
  EXPECT_FALSE(test_api()
                   ->GetNotificationViewForIdOnDisplay(id1, primary_display_id)
                   ->IsExpanded());
  EXPECT_TRUE(test_api()
                  ->GetNotificationViewForIdOnDisplay(id0, primary_display_id)
                  ->IsExpanded());
}

TEST_F(NotificationCenterBubbleTest, LockScreenNotificationVisibility) {
  std::string system_id, id;
  system_id = test_api()->AddSystemNotification();
  id = test_api()->AddNotification();

  GetSessionControllerClient()->LockScreen();

  test_api()->ToggleBubble();

  EXPECT_FALSE(test_api()->GetNotificationViewForId(id));
  EXPECT_TRUE(test_api()->GetNotificationViewForId(system_id));
  EXPECT_TRUE(test_api()->GetNotificationViewForId(system_id)->GetVisible());
}

TEST_F(NotificationCenterBubbleTest, LargeNotificationExpand) {
  const std::string url = "http://test-url.com/";
  std::string id0 = test_api()->AddNotificationWithSourceUrl(url);

  // Create a large grouped notification by adding a bunch of notifications with
  // the same url.
  for (int i = 0; i < 20; i++) {
    test_api()->AddNotificationWithSourceUrl(url);
  }

  test_api()->ToggleBubble();

  std::string parent_id =
      id0 + message_center::kIdSuffixForGroupContainerNotification;

  auto* parent_notification_view = static_cast<AshNotificationView*>(
      test_api()->GetNotificationViewForId(parent_id));

  EXPECT_FALSE(parent_notification_view->IsExpanded());

  // Expand the grouped notification to overflow the bubble. Then, make sure the
  // `NotificationListView` is sized correctly relative to the notification and
  // the bubble's widget.
  parent_notification_view->ToggleExpand();
  EXPECT_TRUE(parent_notification_view->IsExpanded());
  test_api()->CompleteNotificationListAnimation();

  EXPECT_EQ(parent_notification_view->height(),
            test_api()->GetNotificationListView()->height());

  EXPECT_LT(test_api()->GetWidget()->GetWindowBoundsInScreen().height(),
            test_api()->GetNotificationListView()->height());
}

}  // namespace ash
