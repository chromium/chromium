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
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/notification_center_bubble.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/notification_center/views/ash_notification_view.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ash/system/notification_center/views/notification_list_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/message_center/views/message_view.h"

namespace ash {

class NotificationCenterBubbleTestBase : public AshTestBase {
 public:
  NotificationCenterBubbleTestBase(bool enable_ongoing_processes)
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        enable_ongoing_processes_(enable_ongoing_processes) {
    scoped_feature_list_.InitWithFeatureState(features::kOngoingProcesses,
                                              AreOngoingProcessesEnabled());
  }

  void SetUp() override {
    AshTestBase::SetUp();
    test_api_ = std::make_unique<NotificationCenterTestApi>();
  }

  NotificationCenterTestApi* test_api() { return test_api_.get(); }

  bool AreOngoingProcessesEnabled() const { return enable_ongoing_processes_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<NotificationCenterTestApi> test_api_;
  bool enable_ongoing_processes_ = false;
};

class NotificationCenterBubbleTest : public NotificationCenterBubbleTestBase,
                                     public testing::WithParamInterface<
                                         /*enable_ongoing_processes=*/bool> {
 public:
  NotificationCenterBubbleTest()
      : NotificationCenterBubbleTestBase(
            /*enable_ongoing_processes=*/GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         NotificationCenterBubbleTest,
                         /*enable_ongoing_processes=*/testing::Bool());

// Tests that the notification bubble does not get cut off by the top of the
// screen on the launcher homescreen in tablet mode; see b/278471988.
TEST_P(NotificationCenterBubbleTest,
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

TEST_P(NotificationCenterBubbleTest, BubbleHeightConstrainedByDisplay) {
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

TEST_P(NotificationCenterBubbleTest, BubbleHeightUpdatedByDisplaySizeChange) {
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

TEST_P(NotificationCenterBubbleTest, BubbleHeightUpdatedByDisplayRotation) {
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
TEST_P(NotificationCenterBubbleTest, NotificationsGroupingBasic) {
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

TEST_P(NotificationCenterBubbleTest,
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

TEST_P(NotificationCenterBubbleTest,
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

TEST_P(NotificationCenterBubbleTest, LockScreenNotificationVisibility) {
  std::string system_id, id;
  system_id = test_api()->AddSystemNotification();
  id = test_api()->AddNotification();

  GetSessionControllerClient()->LockScreen();

  test_api()->ToggleBubble();

  EXPECT_FALSE(test_api()->GetNotificationViewForId(id));
  EXPECT_TRUE(test_api()->GetNotificationViewForId(system_id));
  EXPECT_TRUE(test_api()->GetNotificationViewForId(system_id)->GetVisible());
}

TEST_P(NotificationCenterBubbleTest, BubbleActivationWithGestureTap) {
  test_api()->AddNotification();

  test_api()->ToggleBubble();
  auto* widget = test_api()->GetWidget();
  EXPECT_FALSE(widget->IsActive());

  GestureTapOn(test_api()->GetNotificationCenterView());
  EXPECT_TRUE(widget->IsActive());
}

TEST_P(NotificationCenterBubbleTest, BubbleActivationWithTouchPress) {
  test_api()->AddNotification();

  test_api()->ToggleBubble();
  auto* widget = test_api()->GetWidget();
  EXPECT_FALSE(widget->IsActive());

  GetEventGenerator()->PressTouch(test_api()
                                      ->GetNotificationCenterView()
                                      ->GetBoundsInScreen()
                                      .CenterPoint());
  EXPECT_TRUE(widget->IsActive());
}

TEST_P(NotificationCenterBubbleTest, BubbleActivationWithMouseClick) {
  test_api()->AddNotification();

  test_api()->ToggleBubble();
  auto* widget = test_api()->GetWidget();
  EXPECT_FALSE(widget->IsActive());

  LeftClickOn(test_api()->GetNotificationCenterView());
  EXPECT_TRUE(widget->IsActive());
}

// Tests that unlocking the device automatically closes the notification bubble.
// See b/287622547.
// TODO(b/347817687): Re-enable test by fixing dangling ptr check.
TEST_P(NotificationCenterBubbleTest, DISABLED_UnlockClosesBubble) {
  // Add a notification so that the notification tray will be visible on the
  // lock screen.
  test_api()->AddNotification();
  ASSERT_GE(1u, test_api()->GetNotificationCount());

  // Show the lock screen.
  GetSessionControllerClient()->LockScreen();
  ASSERT_GE(1u, test_api()->GetNotificationCount());

  // Make the notification bubble visible on the lock screen.
  test_api()->ToggleBubble();
  ASSERT_TRUE(test_api()->IsBubbleShown());

  // Unlock the device without explicitly closing the notification bubble first.
  GetSessionControllerClient()->UnlockScreen();

  // Verify that the notification bubble was automatically closed.
  EXPECT_FALSE(test_api()->IsBubbleShown());
}

TEST_P(NotificationCenterBubbleTest, LargeNotificationExpand) {
  const std::string url = "http://test-url.com/";
  std::string id0 = test_api()->AddNotificationWithSourceUrl(url);

  // Create a large grouped notification by adding a bunch of notifications with
  // the same url.
  for (int i = 0; i < 20; i++) {
    test_api()->AddNotificationWithSourceUrl(url);
  }

  test_api()->ToggleBubble();

  std::string parent_id =
      id0 + message_center_utils::GenerateGroupParentNotificationIdSuffix(
                message_center::MessageCenter::Get()
                    ->FindNotificationById(id0)
                    ->notifier_id());

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

class NotificationCenterBubbleMultiDisplayTest
    : public NotificationCenterBubbleTestBase,
      public testing::WithParamInterface<
          std::tuple</* Primary display height */ int,
                     /* Secondary display height */ int,
                     /* enable_ongoing_processes */ bool>> {
 public:
  NotificationCenterBubbleMultiDisplayTest()
      : NotificationCenterBubbleTestBase(
            /*enable_ongoing_processes=*/std::get<2>(GetParam())) {}

 protected:
  int GetPrimaryDisplayHeight() { return std::get<0>(GetParam()); }
  int GetSecondaryDisplayHeight() { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(DisplayHeight,
                         NotificationCenterBubbleMultiDisplayTest,
                         testing::Values(
                             // Short primary display, tall secondary display
                             std::make_tuple(600, 1600, false),
                             std::make_tuple(600, 1600, true),
                             // Tall primary display, short secondary display
                             std::make_tuple(1600, 600, false),
                             std::make_tuple(1600, 600, true),
                             // Same primary and secondary display heights
                             std::make_tuple(600, 600, false),
                             std::make_tuple(600, 600, true)));

// Tests that the height of the bubble is constrained according to the
// parameters of the display it is being shown on.
TEST_P(NotificationCenterBubbleMultiDisplayTest,
       BubbleHeightConstrainedByDisplay) {
  UpdateDisplay("800x" + base::NumberToString(GetPrimaryDisplayHeight()) +
                ",800x" + base::NumberToString(GetSecondaryDisplayHeight()));
  const int64_t secondary_display_id = display_manager()->GetDisplayAt(1).id();

  // Add a large number of notifications to overflow the scroll view in the
  // notification center.
  for (int i = 0; i < 100; i++) {
    test_api()->AddNotification();
  }

  // Show the primary display's notification center.
  test_api()->ToggleBubble();

  // The height of the primary display's notification center should not exceed
  // the primary display's height.
  const int bubble1_height =
      test_api()->GetNotificationCenterView()->bounds().height();
  EXPECT_LT(bubble1_height, GetPrimaryDisplayHeight());

  // Show the secondary display's notification center.
  test_api()->ToggleBubbleOnDisplay(secondary_display_id);

  // The height of the secondary display's notification center should not exceed
  // the secondary display's height.
  const int bubble2_height =
      test_api()
          ->GetNotificationCenterViewOnDisplay(secondary_display_id)
          ->bounds()
          .height();
  EXPECT_LT(bubble2_height, GetSecondaryDisplayHeight());

  // The height of the notification center on the taller display should be
  // larger than the height of the notification center on the shorter display,
  // or the heights should be equal if the displays' heights are equal.
  if (GetPrimaryDisplayHeight() > GetSecondaryDisplayHeight()) {
    EXPECT_GT(bubble1_height, bubble2_height);
  } else if (GetPrimaryDisplayHeight() < GetSecondaryDisplayHeight()) {
    EXPECT_LT(bubble1_height, bubble2_height);
  } else {
    EXPECT_EQ(bubble1_height, bubble2_height);
  }
}

}  // namespace ash
