// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group_test_util.h"

#include "ash/root_window_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_item_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_test_util.h"
#include "chromeos/ui/base/window_state_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"

namespace ash {

SplitViewDivider* GetTopmostSnapGroupDivider() {
  auto* top_snap_group = SnapGroupController::Get()->GetTopmostSnapGroup();
  return top_snap_group ? top_snap_group->snap_group_divider() : nullptr;
}

gfx::Rect GetTopmostSnapGroupDividerBoundsInScreen() {
  return GetTopmostSnapGroupDivider()->GetDividerBoundsInScreen(
      /*is_dragging=*/false);
}

void ClickOverviewItem(ui::test::EventGenerator* event_generator,
                       aura::Window* window) {
  event_generator->MoveMouseTo(gfx::ToRoundedPoint(
      GetOverviewItemForWindow(window)->GetTransformedBounds().CenterPoint()));
  event_generator->ClickLeftButton();
}

void SnapTwoTestWindows(aura::Window* window1,
                        aura::Window* window2,
                        bool horizontal,
                        ui::test::EventGenerator* event_generator) {
  CHECK_NE(window1, window2);
  // Snap `window1` to trigger the overview session shown on the other side of
  // the screen.
  SnapOneTestWindow(window1,
                    /*state_type=*/chromeos::WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  WaitForOverviewEntered();
  VerifySplitViewOverviewSession(window1);

  // Snapping the first window makes it fill half the screen, either
  // vertically or horizontally (based on orientation).
  gfx::Rect work_area(GetWorkAreaBoundsForWindow(window1));
  gfx::Rect primary_bounds, secondary_bounds;
  if (horizontal) {
    work_area.SplitVertically(primary_bounds, secondary_bounds);
  } else {
    work_area.SplitHorizontally(primary_bounds, secondary_bounds);
  }

  EXPECT_EQ(primary_bounds, window1->GetBoundsInScreen());

  // The `window2` gets selected in the overview will be snapped to the
  // non-occupied snap position and the overview session will end.
  ClickOverviewItem(event_generator, window2);
  WaitForOverviewExitAnimation();
  EXPECT_EQ(WindowState::Get(window2)->GetStateType(),
            chromeos::WindowStateType::kSecondarySnapped);
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
  EXPECT_FALSE(
      RootWindowController::ForWindow(window1)->split_view_overview_session());

  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller);
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(window1, window2));

  // The snap group divider will show on two windows snapped.
  EXPECT_TRUE(GetTopmostSnapGroupDivider()->divider_widget());
  // There can be a slight rounding error when ChromeVox is on.
  EXPECT_NEAR(chromeos::kDefaultSnapRatio,
              *WindowState::Get(window1)->snap_ratio(), .01);
  EXPECT_NEAR(chromeos::kDefaultSnapRatio,
              *WindowState::Get(window2)->snap_ratio(), .01);

  gfx::Rect divider_bounds(GetTopmostSnapGroupDividerBoundsInScreen());
  EXPECT_EQ(work_area.CenterPoint().x(), divider_bounds.CenterPoint().x());
  UnionBoundsEqualToWorkAreaBounds(window1, window2,
                                   GetTopmostSnapGroupDivider());

  if (horizontal) {
    primary_bounds.set_width(primary_bounds.width() -
                             divider_bounds.width() / 2);
    secondary_bounds.set_x(secondary_bounds.x() + divider_bounds.width() / 2);
    secondary_bounds.set_width(secondary_bounds.width() -
                               divider_bounds.width() / 2);
    EXPECT_EQ(primary_bounds.width(), window1->GetBoundsInScreen().width());
    EXPECT_EQ(secondary_bounds.width(), window2->GetBoundsInScreen().width());
    EXPECT_EQ(primary_bounds.width() + secondary_bounds.width() +
                  divider_bounds.width(),
              work_area.width());
  } else {
    primary_bounds.set_height(primary_bounds.height() -
                              divider_bounds.height() / 2);
    secondary_bounds.set_y(secondary_bounds.y() + divider_bounds.height() / 2);
    secondary_bounds.set_height(secondary_bounds.height() -
                                divider_bounds.height() / 2);
    EXPECT_EQ(primary_bounds.height(), window1->GetBoundsInScreen().height());
    EXPECT_EQ(secondary_bounds.height(), window2->GetBoundsInScreen().height());
    EXPECT_EQ(primary_bounds.height() + secondary_bounds.height() +
                  divider_bounds.height(),
              work_area.height());
  }
}

}  // namespace ash
