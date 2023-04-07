// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mouse_cursor_event_filter.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/client/cursor_shape_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_windows.h"
#include "ui/base/cursor/cursor.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

class MouseCursorEventFilterTest : public AshTestBase {
 public:
  MouseCursorEventFilterTest() = default;

  MouseCursorEventFilterTest(const MouseCursorEventFilterTest&) = delete;
  MouseCursorEventFilterTest& operator=(const MouseCursorEventFilterTest&) =
      delete;

  ~MouseCursorEventFilterTest() override = default;

 protected:
  MouseCursorEventFilter* event_filter() {
    return Shell::Get()->mouse_cursor_filter();
  }

  bool TestIfMouseWarpsAt(const gfx::Point& point_in_screen) {
    return AshTestBase::TestIfMouseWarpsAt(GetEventGenerator(),
                                           point_in_screen);
  }
};

// Verifies if the mouse pointer correctly moves to another display when there
// are two displays.
TEST_F(MouseCursorEventFilterTest, WarpMouse) {
  UpdateDisplay("500x400,500x400");

  ASSERT_EQ(display::DisplayPlacement::RIGHT, Shell::Get()
                                                  ->display_manager()
                                                  ->GetCurrentDisplayLayout()
                                                  .placement_list[0]
                                                  .position);

  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(11, 11)));

  // Touch the right edge of the primary root window. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(499, 11)));
  EXPECT_EQ("501,11",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the secondary root window. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(500, 11)));
  EXPECT_EQ("498,11",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the primary root window.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(0, 11)));
  // Touch the top edge of the primary root window.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(11, 0)));
  // Touch the bottom edge of the primary root window.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(11, 399)));
  // Touch the right edge of the secondary root window.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(999, 11)));
  // Touch the top edge of the secondary root window.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(11, 0)));
  // Touch the bottom edge of the secondary root window.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(11, 399)));
}

// Verifies if the mouse pointer correctly moves to another display even when
// two displays are not the same size.
TEST_F(MouseCursorEventFilterTest, WarpMouseDifferentSizeDisplays) {
  UpdateDisplay("500x400,600x500");  // the second one is larger.

  ASSERT_EQ(display::DisplayPlacement::RIGHT, Shell::Get()
                                                  ->display_manager()
                                                  ->GetCurrentDisplayLayout()
                                                  .placement_list[0]
                                                  .position);

  // Touch the left edge of the secondary root window. Pointer should NOT warp
  // because 1px left of (0, 500) is outside the primary root window.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(501, 400)));
  EXPECT_EQ("501,400",
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the secondary root window. Pointer should warp
  // because 1px left of (0, 480) is inside the primary root window.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(500, 380)));
  EXPECT_EQ("498,380",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());
}

// Verifies if the mouse pointer correctly moves between displays with
// different scale factors. In native coords mode, there is no
// difference between drag and move.
TEST_F(MouseCursorEventFilterTest, WarpMouseDifferentScaleDisplaysInNative) {
  UpdateDisplay("500x400,600x500*2");

  ASSERT_EQ(display::DisplayPlacement::RIGHT, Shell::Get()
                                                  ->display_manager()
                                                  ->GetCurrentDisplayLayout()
                                                  .placement_list[0]
                                                  .position);

  aura::Env::GetInstance()->SetLastMouseLocation(gfx::Point(900, 123));

  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(499, 123)));
  EXPECT_EQ("500,123",
            aura::Env::GetInstance()->last_mouse_location().ToString());
  // Touch the edge of 2nd display again and make sure it warps to
  // 1st dislay.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(500, 123)));
  // TODO(oshima): Due to a bug in EventGenerator, the screen coordinates
  // is shrunk by dsf once. Fix this.
  EXPECT_EQ("498,61",
            aura::Env::GetInstance()->last_mouse_location().ToString());
}

// Verifies if MouseCursorEventFilter::set_mouse_warp_enabled() works as
// expected.
TEST_F(MouseCursorEventFilterTest, SetMouseWarpModeFlag) {
  UpdateDisplay("500x400,500x400");

  event_filter()->set_mouse_warp_enabled(false);
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(499, 11)));
  EXPECT_EQ("499,11",
            aura::Env::GetInstance()->last_mouse_location().ToString());

  event_filter()->set_mouse_warp_enabled(true);
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(499, 11)));
  EXPECT_EQ("501,11",
            aura::Env::GetInstance()->last_mouse_location().ToString());
}

// Verifies cursor's image scale factor is updated when a cursor has moved
// across root windows with different device scale factors
// (http://crbug.com/154183).
TEST_F(MouseCursorEventFilterTest, CursorDeviceScaleFactor) {
  UpdateDisplay("400x300,800x700*2");
  display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::RIGHT, 0));
  auto* cursor_manager = Shell::Get()->cursor_manager();
  const auto& cursor_shape_client = aura::client::GetCursorShapeClient();
  EXPECT_EQ(1.0f,
            cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                ->scale_factor);
  GetEventGenerator()->MoveMouseTo(401, 200);
  EXPECT_EQ(2.0f,
            cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                ->scale_factor);
  GetEventGenerator()->MoveMouseTo(399, 200);
  EXPECT_EQ(1.0f,
            cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                ->scale_factor);
}

// Verifies that pressing the key repeatedly will not hide the cursor.
// Otherwise, in one edge case, user may press one key repeatedly while moving
// the cursor and then the user interface looks weird.
// (http://crbug.com/855163).
TEST_F(MouseCursorEventFilterTest, CursorVisibilityWontFlip) {
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, 1234, gfx::Rect(5, 5, 100, 100)));
  window->Show();
  window->SetCapture();

  wm::CursorManager* manager = Shell::Get()->cursor_manager();

  // Cursor is visible at start
  EXPECT_TRUE(manager->IsCursorVisible());

  ui::test::EventGenerator* generator = GetEventGenerator();

  // Pressing key will hide the cursor
  generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(manager->IsCursorVisible());

  // Moving the mouse will show the cursor
  generator->MoveMouseTo(gfx::Point(10, 10));
  EXPECT_TRUE(manager->IsCursorVisible());

  // Pressing key repeatedly will not hide the cursor
  generator->PressKey(ui::VKEY_A, ui::EF_NONE | ui::EF_IS_REPEAT);
  EXPECT_TRUE(manager->IsCursorVisible());
}

}  // namespace ash
