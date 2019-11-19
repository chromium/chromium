// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_finder.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window_targeter.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {

using WindowFinderTest = AshTestBase;

TEST_F(WindowFinderTest, RealTopmostCanBeNullptr) {
  std::unique_ptr<aura::Window> window1 =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));
  std::set<aura::Window*> ignore;

  EXPECT_EQ(window1.get(), GetTopmostWindowAtPoint(gfx::Point(10, 10), ignore));
}

TEST_F(WindowFinderTest, MultipleDisplays) {
  UpdateDisplay("200x200,300x300");

  std::unique_ptr<aura::Window> window1 =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(200, 0, 100, 100));
  ASSERT_NE(window1->GetRootWindow(), window2->GetRootWindow());

  std::set<aura::Window*> ignore;
  EXPECT_EQ(window1.get(), GetTopmostWindowAtPoint(gfx::Point(10, 10), ignore));
  EXPECT_EQ(window2.get(),
            GetTopmostWindowAtPoint(gfx::Point(210, 10), ignore));
  EXPECT_EQ(nullptr, GetTopmostWindowAtPoint(gfx::Point(10, 210), ignore));
}

TEST_F(WindowFinderTest, WindowTargeterWithHitTestRects) {
  std::unique_ptr<aura::Window> window1 =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));

  std::set<aura::Window*> ignore;

  EXPECT_EQ(window2.get(), GetTopmostWindowAtPoint(gfx::Point(10, 10), ignore));

  auto targeter = std::make_unique<aura::WindowTargeter>();
  targeter->SetInsets(gfx::Insets(0, 50, 0, 0));
  window2->SetEventTargeter(std::move(targeter));

  EXPECT_EQ(window1.get(), GetTopmostWindowAtPoint(gfx::Point(10, 10), ignore));
  EXPECT_EQ(window2.get(), GetTopmostWindowAtPoint(gfx::Point(60, 10), ignore));
}

// Tests that when overview is active, GetTopmostWindowAtPoint() will return
// the window in overview that contains the specified screen point, even though
// it might be a minimized window.
TEST_F(WindowFinderTest, TopmostWindowWithOverviewActive) {
  UpdateDisplay("400x400");
  std::unique_ptr<aura::Window> window1 =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Get |window1| and |window2|'s transformed bounds in overview.
  OverviewGrid* grid =
      overview_controller->overview_session()->GetGridWithRootWindow(
          window1->GetRootWindow());
  gfx::Rect bounds1 = gfx::ToEnclosedRect(
      grid->GetOverviewItemContaining(window1.get())->target_bounds());
  gfx::Rect bounds2 = gfx::ToEnclosedRect(
      grid->GetOverviewItemContaining(window2.get())->target_bounds());

  std::set<aura::Window*> ignore;
  EXPECT_EQ(window1.get(),
            GetTopmostWindowAtPoint(bounds1.CenterPoint(), ignore));
  EXPECT_EQ(window2.get(),
            GetTopmostWindowAtPoint(bounds2.CenterPoint(), ignore));

  WindowState::Get(window1.get())->Minimize();
  EXPECT_EQ(window1.get(),
            GetTopmostWindowAtPoint(bounds1.CenterPoint(), ignore));
}

}  // namespace ash
