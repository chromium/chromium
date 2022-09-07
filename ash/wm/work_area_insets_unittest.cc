// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/work_area_insets.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

using WorkAreaInsetsTest = AshTestBase;

TEST_F(WorkAreaInsetsTest, WorkAreaBoundsForTwoScreens) {
  UpdateDisplay("800x600,800x600");

  // The work area for the default configuration is simply the window bounds
  // minus the shelf bounds.
  gfx::Rect shelf_bounds =
      Shell::GetPrimaryRootWindowController()->shelf()->GetIdealBounds();
  gfx::Size expected_work_area_size =
      gfx::Size(800, 600 - shelf_bounds.height());

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  gfx::Rect work_area1 =
      WorkAreaInsets::ForWindow(root_windows[0])->user_work_area_bounds();
  EXPECT_EQ(gfx::Point(0, 0), work_area1.origin());
  EXPECT_EQ(expected_work_area_size, work_area1.size());

  gfx::Rect work_area2 =
      WorkAreaInsets::ForWindow(root_windows[1])->user_work_area_bounds();
  EXPECT_EQ(gfx::Point(800, 0), work_area2.origin());
  EXPECT_EQ(expected_work_area_size, work_area2.size());
}

TEST_F(WorkAreaInsetsTest, ComputeStableWorkAreaForTwoScreens) {
  UpdateDisplay("800x600,800x600");

  // The work area for the default configuration is simply the window bounds
  // minus the shelf bounds.
  gfx::Rect shelf_bounds =
      Shell::GetPrimaryRootWindowController()->shelf()->GetIdealBounds();
  gfx::Size expected_work_area_size =
      gfx::Size(800, 600 - shelf_bounds.height());

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  gfx::Rect stable_work_area1 =
      WorkAreaInsets::ForWindow(root_windows[0])->ComputeStableWorkArea();
  EXPECT_EQ(gfx::Point(0, 0), stable_work_area1.origin());
  EXPECT_EQ(expected_work_area_size, stable_work_area1.size());

  gfx::Rect stable_work_area2 =
      WorkAreaInsets::ForWindow(root_windows[1])->ComputeStableWorkArea();
  EXPECT_EQ(gfx::Point(800, 0), stable_work_area2.origin());
  EXPECT_EQ(expected_work_area_size, stable_work_area2.size());
}

}  // namespace ash
