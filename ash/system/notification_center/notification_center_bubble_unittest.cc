// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_bubble.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"

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

TEST_F(NotificationCenterBubbleTest, BubbleHeightConstrainedByDisplay) {
  const int display_height = 800;
  UpdateDisplay("1200x" + base::NumberToString(display_height));

  // Add a large number of notifications to overflow the scroll view in the
  // notification center.
  for (int i = 0; i < 100; i++)
    test_api()->AddNotification();

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
  for (int i = 0; i < 100; i++)
    test_api()->AddNotification();

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
  for (int i = 0; i < 100; i++)
    test_api()->AddNotification();

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

}  // namespace ash
