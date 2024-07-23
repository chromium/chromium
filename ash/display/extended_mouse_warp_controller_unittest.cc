// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/extended_mouse_warp_controller.h"

#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/screen_position_controller.h"
#include "ash/host/ash_window_tree_host_platform.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

class ExtendedMouseWarpControllerTest : public AshTestBase {
 public:
  ExtendedMouseWarpControllerTest() = default;

  ExtendedMouseWarpControllerTest(const ExtendedMouseWarpControllerTest&) =
      delete;
  ExtendedMouseWarpControllerTest& operator=(
      const ExtendedMouseWarpControllerTest&) = delete;

  ~ExtendedMouseWarpControllerTest() override = default;

 protected:
  MouseCursorEventFilter* event_filter() {
    return Shell::Get()->mouse_cursor_filter();
  }

  ExtendedMouseWarpController* mouse_warp_controller() {
    return static_cast<ExtendedMouseWarpController*>(
        event_filter()->mouse_warp_controller_for_test());
  }

  size_t GetWarpRegionsCount() {
    return mouse_warp_controller()->warp_regions_.size();
  }

  const ExtendedMouseWarpController::WarpRegion* GetWarpRegion(size_t index) {
    return mouse_warp_controller()->warp_regions_[index].get();
  }

  const gfx::Rect& GetIndicatorBounds(int64_t id) {
    return GetWarpRegion(0)->GetIndicatorBoundsForTest(id);
  }

  const gfx::Rect& GetIndicatorNativeBounds(int64_t id) {
    return GetWarpRegion(0)->GetIndicatorNativeBoundsForTest(id);
  }

  // Send mouse event with native event through AshWindowTreeHostPlatform.
  void DispatchMouseEventWithNative(AshWindowTreeHostPlatform* host,
                                    const gfx::Point& location_in_host_native,
                                    ui::EventType event_type,
                                    int event_flag1,
                                    int event_flag2) {
    ui::MouseEvent native_event(event_type, location_in_host_native,
                                location_in_host_native, ui::EventTimeForNow(),
                                event_flag1, event_flag2);
    ui::MouseEvent mouseev(&native_event);
    host->DispatchEvent(&mouseev);

    // The test relies on the last_mouse_location, which will be updated by
    // a synthesized event posted asynchronusly. Wait until the synthesized
    // event is handled and last mouse location is updated.
    base::RunLoop().RunUntilIdle();
  }
};

// Verifies if MouseCursorEventFilter's bounds calculation works correctly.
TEST_F(ExtendedMouseWarpControllerTest, IndicatorBoundsTestOnRight) {
  UpdateDisplay("360x350,800x700");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  int64_t display_0_id = display::Screen::GetScreen()
                             ->GetDisplayNearestWindow(root_windows[0])
                             .id();
  int64_t display_1_id = display::Screen::GetScreen()
                             ->GetDisplayNearestWindow(root_windows[1])
                             .id();

  std::unique_ptr<display::DisplayLayout> layout(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::RIGHT, 0));

  display_manager()->SetLayoutForCurrentDisplays(layout->Copy());
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);

  ASSERT_EQ(1U, GetWarpRegionsCount());
  EXPECT_EQ(gfx::Rect(359, 32, 1, 318), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(360, 0, 1, 350), GetIndicatorBounds(display_1_id));

  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ(gfx::Rect(359, 0, 1, 350), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(360, 32, 1, 318), GetIndicatorBounds(display_1_id));

  // Move 2nd display downwards a bit.
  layout->placement_list[0].offset = 5;
  display_manager()->SetLayoutForCurrentDisplays(layout->Copy());
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  // This is same as before because the 2nd display's y is above
  // the indicator's x.
  ASSERT_EQ(1U, GetWarpRegionsCount());
  EXPECT_EQ(gfx::Rect(359, 32, 1, 318), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(360, 5, 1, 345), GetIndicatorBounds(display_1_id));

  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ(gfx::Rect(359, 5, 1, 345), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(360, 37, 1, 313), GetIndicatorBounds(display_1_id));

  // Move it down further so that the shared edge is shorter than
  // minimum hole size (160).
  layout->placement_list[0].offset = 200;
  display_manager()->SetLayoutForCurrentDisplays(layout->Copy());
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  ASSERT_EQ(1U, GetWarpRegionsCount());
  EXPECT_EQ(gfx::Rect(359, 200, 1, 150), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(360, 200, 1, 150), GetIndicatorBounds(display_1_id));

  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  ASSERT_EQ(1U, GetWarpRegionsCount());
  EXPECT_EQ(gfx::Rect(359, 200, 1, 150), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(360, 200, 1, 150), GetIndicatorBounds(display_1_id));

  // Now move 2nd display upwards.
  layout->placement_list[0].offset = -5;
  display_manager()->SetLayoutForCurrentDisplays(layout->Copy());
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  ASSERT_EQ(1U, GetWarpRegionsCount());
  EXPECT_EQ(gfx::Rect(359, 32, 1, 318), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(360, 0, 1, 350), GetIndicatorBounds(display_1_id));
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  // 32 px are reserved on 2nd display from top, so y must be
  // (32 - 5) = 27.
  ASSERT_EQ(1U, GetWarpRegionsCount());
  EXPECT_EQ(gfx::Rect(359, 0, 1, 350), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(360, 27, 1, 323), GetIndicatorBounds(display_1_id));

  event_filter()->HideSharedEdgeIndicator();
}

TEST_F(ExtendedMouseWarpControllerTest, IndicatorBoundsTestOnLeft) {
  UpdateDisplay("360x350,800x700");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  int64_t display_0_id = display::Screen::GetScreen()
                             ->GetDisplayNearestWindow(root_windows[0])
                             .id();
  int64_t display_1_id = display::Screen::GetScreen()
                             ->GetDisplayNearestWindow(root_windows[1])
                             .id();

  std::unique_ptr<display::DisplayLayout> layout(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::LEFT, 0));
  display_manager()->SetLayoutForCurrentDisplays(layout->Copy());

  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  ASSERT_EQ(1U, GetWarpRegionsCount());
  EXPECT_EQ(gfx::Rect(0, 32, 1, 318), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(-1, 0, 1, 350), GetIndicatorBounds(display_1_id));

  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  ASSERT_EQ(1U, GetWarpRegionsCount());
  EXPECT_EQ(gfx::Rect(0, 0, 1, 350), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(-1, 32, 1, 318), GetIndicatorBounds(display_1_id));

  layout->placement_list[0].offset = 250;
  display_manager()->SetLayoutForCurrentDisplays(layout->Copy());
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  ASSERT_EQ(1U, GetWarpRegionsCount());
  EXPECT_EQ(gfx::Rect(0, 250, 1, 100), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(-1, 250, 1, 100), GetIndicatorBounds(display_1_id));

  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  ASSERT_EQ(1U, GetWarpRegionsCount());
  EXPECT_EQ(gfx::Rect(0, 250, 1, 100), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(-1, 250, 1, 100), GetIndicatorBounds(display_1_id));

  event_filter()->HideSharedEdgeIndicator();
}

TEST_F(ExtendedMouseWarpControllerTest, IndicatorBoundsTestOnTopBottom) {
  UpdateDisplay("360x350,800x700");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  int64_t display_0_id = display::Screen::GetScreen()
                             ->GetDisplayNearestWindow(root_windows[0])
                             .id();
  int64_t display_1_id = display::Screen::GetScreen()
                             ->GetDisplayNearestWindow(root_windows[1])
                             .id();

  std::unique_ptr<display::DisplayLayout> layout(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::TOP, 0));
  display_manager()->SetLayoutForCurrentDisplays(layout->Copy());
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  ASSERT_EQ(1U, GetWarpRegionsCount());
  EXPECT_EQ(gfx::Rect(0, 0, 360, 1), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(0, -1, 360, 1), GetIndicatorBounds(display_1_id));

  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  ASSERT_EQ(1U, GetWarpRegionsCount());
  EXPECT_EQ(gfx::Rect(0, 0, 360, 1), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(0, -1, 360, 1), GetIndicatorBounds(display_1_id));

  layout->placement_list[0].offset = 250;
  display_manager()->SetLayoutForCurrentDisplays(layout->Copy());
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  ASSERT_EQ(1U, GetWarpRegionsCount());
  EXPECT_EQ(gfx::Rect(250, 0, 110, 1), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(250, -1, 110, 1), GetIndicatorBounds(display_1_id));

  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  ASSERT_EQ(1U, GetWarpRegionsCount());
  EXPECT_EQ(gfx::Rect(250, 0, 110, 1), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(250, -1, 110, 1), GetIndicatorBounds(display_1_id));

  layout->placement_list[0].position = display::DisplayPlacement::BOTTOM;
  layout->placement_list[0].offset = 0;
  display_manager()->SetLayoutForCurrentDisplays(layout->Copy());
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  ASSERT_EQ(1U, GetWarpRegionsCount());
  EXPECT_EQ(gfx::Rect(0, 349, 360, 1), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(0, 350, 360, 1), GetIndicatorBounds(display_1_id));

  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  ASSERT_EQ(1U, GetWarpRegionsCount());
  EXPECT_EQ(gfx::Rect(0, 349, 360, 1), GetIndicatorBounds(display_0_id));
  EXPECT_EQ(gfx::Rect(0, 350, 360, 1), GetIndicatorBounds(display_1_id));

  event_filter()->HideSharedEdgeIndicator();
}

// Verify indicators show up as expected with 3+ displays.
TEST_F(ExtendedMouseWarpControllerTest, IndicatorBoundsTestThreeDisplays) {
  UpdateDisplay("360x350,800x700,1000x900");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  display::Screen* screen = display::Screen::GetScreen();
  int64_t display_0_id = screen->GetDisplayNearestWindow(root_windows[0]).id();
  int64_t display_1_id = screen->GetDisplayNearestWindow(root_windows[1]).id();
  int64_t display_2_id = screen->GetDisplayNearestWindow(root_windows[2]).id();

  // Drag from left most display
  event_filter()->ShowSharedEdgeIndicator(root_windows[0]);
  ASSERT_EQ(2U, GetWarpRegionsCount());
  const ExtendedMouseWarpController::WarpRegion* region_0 = GetWarpRegion(0);
  const ExtendedMouseWarpController::WarpRegion* region_1 = GetWarpRegion(1);
  EXPECT_EQ(gfx::Rect(359, 32, 1, 318),
            region_1->GetIndicatorBoundsForTest(display_0_id));
  EXPECT_EQ(gfx::Rect(360, 0, 1, 350),
            region_1->GetIndicatorBoundsForTest(display_1_id));
  EXPECT_EQ(gfx::Rect(1159, 0, 1, 700),
            region_0->GetIndicatorBoundsForTest(display_1_id));
  EXPECT_EQ(gfx::Rect(1160, 0, 1, 700),
            region_0->GetIndicatorBoundsForTest(display_2_id));

  // Drag from middle display
  event_filter()->ShowSharedEdgeIndicator(root_windows[1]);
  ASSERT_EQ(2U, mouse_warp_controller()->warp_regions_.size());
  region_0 = GetWarpRegion(0);
  region_1 = GetWarpRegion(1);
  EXPECT_EQ(gfx::Rect(359, 0, 1, 350),
            region_1->GetIndicatorBoundsForTest(display_0_id));
  EXPECT_EQ(gfx::Rect(360, 32, 1, 318),
            region_1->GetIndicatorBoundsForTest(display_1_id));
  EXPECT_EQ(gfx::Rect(1159, 32, 1, 668),
            region_0->GetIndicatorBoundsForTest(display_1_id));
  EXPECT_EQ(gfx::Rect(1160, 0, 1, 700),
            region_0->GetIndicatorBoundsForTest(display_2_id));

  // Right most display
  event_filter()->ShowSharedEdgeIndicator(root_windows[2]);
  ASSERT_EQ(2U, mouse_warp_controller()->warp_regions_.size());
  region_0 = GetWarpRegion(0);
  region_1 = GetWarpRegion(1);
  EXPECT_EQ(gfx::Rect(359, 0, 1, 350),
            region_1->GetIndicatorBoundsForTest(display_0_id));
  EXPECT_EQ(gfx::Rect(360, 0, 1, 350),
            region_1->GetIndicatorBoundsForTest(display_1_id));
  EXPECT_EQ(gfx::Rect(1159, 0, 1, 700),
            region_0->GetIndicatorBoundsForTest(display_1_id));
  EXPECT_EQ(gfx::Rect(1160, 32, 1, 668),
            region_0->GetIndicatorBoundsForTest(display_2_id));
  event_filter()->HideSharedEdgeIndicator();
  // TODO(oshima): Add test cases primary swap.
}

TEST_F(ExtendedMouseWarpControllerTest,
       IndicatorBoundsTestThreeDisplaysWithLayout) {
  UpdateDisplay("700x500,600x500,1000x900");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  display::Screen* screen = display::Screen::GetScreen();
  int64_t display_0_id = screen->GetDisplayNearestWindow(root_windows[0]).id();
  int64_t display_1_id = screen->GetDisplayNearestWindow(root_windows[1]).id();
  int64_t display_2_id = screen->GetDisplayNearestWindow(root_windows[2]).id();

  // Layout so that all displays touches togter like this:
  //  +-----+---+
  //  |  0  | 1 |
  //  +-+---+--++
  //    |  2   |
  //    +------+
  display::DisplayLayoutBuilder builder(display_0_id);
  builder.AddDisplayPlacement(display_1_id, display_0_id,
                              display::DisplayPlacement::RIGHT, 0);
  builder.AddDisplayPlacement(display_2_id, display_0_id,
                              display::DisplayPlacement::BOTTOM, 100);

  display_manager()->SetLayoutForCurrentDisplays(builder.Build());
  ASSERT_EQ(3U, GetWarpRegionsCount());

  // Drag from 0.
  event_filter()->ShowSharedEdgeIndicator(root_windows[0]);
  ASSERT_EQ(3U, GetWarpRegionsCount());
  const ExtendedMouseWarpController::WarpRegion* region_0 = GetWarpRegion(0);
  const ExtendedMouseWarpController::WarpRegion* region_1 = GetWarpRegion(1);
  const ExtendedMouseWarpController::WarpRegion* region_2 = GetWarpRegion(2);
  // between 2 and 0
  EXPECT_EQ(gfx::Rect(100, 499, 600, 1),
            region_0->GetIndicatorBoundsForTest(display_0_id));
  EXPECT_EQ(gfx::Rect(100, 500, 600, 1),
            region_0->GetIndicatorBoundsForTest(display_2_id));
  // between 2 and 1
  EXPECT_EQ(gfx::Rect(700, 499, 400, 1),
            region_1->GetIndicatorBoundsForTest(display_1_id));
  EXPECT_EQ(gfx::Rect(700, 500, 400, 1),
            region_1->GetIndicatorBoundsForTest(display_2_id));
  // between 1 and 0
  EXPECT_EQ(gfx::Rect(699, 32, 1, 468),
            region_2->GetIndicatorBoundsForTest(display_0_id));
  EXPECT_EQ(gfx::Rect(700, 0, 1, 500),
            region_2->GetIndicatorBoundsForTest(display_1_id));
  event_filter()->HideSharedEdgeIndicator();
}

TEST_F(ExtendedMouseWarpControllerTest,
       IndicatorBoundsTestThreeDisplaysWithLayout2) {
  UpdateDisplay("700x500,600x500,1000x900");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  display::Screen* screen = display::Screen::GetScreen();
  int64_t display_0_id = screen->GetDisplayNearestWindow(root_windows[0]).id();
  int64_t display_1_id = screen->GetDisplayNearestWindow(root_windows[1]).id();
  int64_t display_2_id = screen->GetDisplayNearestWindow(root_windows[2]).id();

  // Layout so that 0 and 1 displays are disconnected.
  //  +-----+ +---+
  //  |  0  | |1 |
  //  +-+---+-+++
  //    |  2   |
  //    +------+
  display::DisplayLayoutBuilder builder(display_0_id);
  builder.AddDisplayPlacement(display_2_id, display_0_id,
                              display::DisplayPlacement::BOTTOM, 100);
  builder.AddDisplayPlacement(display_1_id, display_2_id,
                              display::DisplayPlacement::TOP, 800);

  display_manager()->SetLayoutForCurrentDisplays(builder.Build());
  ASSERT_EQ(2U, GetWarpRegionsCount());

  // Drag from 0.
  event_filter()->ShowSharedEdgeIndicator(root_windows[0]);
  ASSERT_EQ(2U, GetWarpRegionsCount());
  const ExtendedMouseWarpController::WarpRegion* region_0 = GetWarpRegion(0);
  const ExtendedMouseWarpController::WarpRegion* region_1 = GetWarpRegion(1);
  // between 2 and 0
  EXPECT_EQ(gfx::Rect(100, 499, 600, 1),
            region_0->GetIndicatorBoundsForTest(display_0_id));
  EXPECT_EQ(gfx::Rect(100, 500, 600, 1),
            region_0->GetIndicatorBoundsForTest(display_2_id));
  // between 2 and 1
  EXPECT_EQ(gfx::Rect(900, 499, 200, 1),
            region_1->GetIndicatorBoundsForTest(display_1_id));
  EXPECT_EQ(gfx::Rect(900, 500, 200, 1),
            region_1->GetIndicatorBoundsForTest(display_2_id));
  event_filter()->HideSharedEdgeIndicator();
}

// Check that the point in the rotated secondary display's warp region is
// converted correctly from native host coordinates to screen DIP coordinates.
// (see https://crbug.com/905035)
// Flaky. https://crbug.com/1217187.
TEST_F(ExtendedMouseWarpControllerTest,
       DISABLED_CheckHostPointToScreenInMouseWarpRegion) {
  // Zoom factor is needed to trigger rounding error which occured in previous
  // code.
  UpdateDisplay("50+50-300x200@0.8,50+300-300x100/r");

  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();

  // Check the primary display's size and scale.
  display::Display primary_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[0]);
  ASSERT_EQ("250x250", primary_display.size().ToString());
  ASSERT_EQ(0.8f, primary_display.device_scale_factor());

  // Create a window to be dragged in primary display.
  std::unique_ptr<aura::test::TestWindowDelegate> test_window_delegate =
      std::make_unique<aura::test::TestWindowDelegate>();
  test_window_delegate->set_window_component(HTCAPTION);
  const gfx::Size initial_window_size(100, 100);
  std::unique_ptr<aura::Window> test_window(
      CreateTestWindowInShellWithDelegateAndType(
          test_window_delegate.get(), aura::client::WINDOW_TYPE_NORMAL, 0,
          gfx::Rect(initial_window_size)));
  ASSERT_EQ(root_windows[0], test_window->GetRootWindow());
  ASSERT_FALSE(test_window->HasCapture());

  AshWindowTreeHostPlatform* window_host =
      static_cast<AshWindowTreeHostPlatform*>(root_windows[0]->GetHost());

  // Move mouse cursor and capture the window.
  gfx::Point location_in_host_native(0, 0);
  DispatchMouseEventWithNative(window_host, location_in_host_native,
                               ui::EventType::kMouseMoved, ui::EF_NONE,
                               ui::EF_NONE);
  DispatchMouseEventWithNative(
      window_host, location_in_host_native, ui::EventType::kMousePressed,
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);

  // Window should be captured.
  ASSERT_TRUE(test_window->HasCapture());

  int64_t display_0_id = primary_display.id();
  const gfx::Rect indicator_in_primary_display =
      GetIndicatorNativeBounds(display_0_id);
  int64_t display_1_id = display::Screen::GetScreen()
                             ->GetDisplayNearestWindow(root_windows[1])
                             .id();
  const gfx::Rect indicator_in_secondary_display =
      GetIndicatorNativeBounds(display_1_id);

  gfx::Point location_in_screen_native, location_in_screen_dip;

  // Move mouse cursor to the warp region of first display.
  location_in_screen_native = indicator_in_primary_display.CenterPoint();
  location_in_host_native =
      location_in_screen_native -
      root_windows[0]->GetHost()->GetBoundsInPixels().OffsetFromOrigin();
  DispatchMouseEventWithNative(window_host, location_in_host_native,
                               ui::EventType::kMouseDragged,
                               ui::EF_LEFT_MOUSE_BUTTON, 0);

  // Mouse cursor should be warped into secondary display.
  location_in_screen_dip = aura::Env::GetInstance()->last_mouse_location();
  EXPECT_TRUE(
      root_windows[1]->GetBoundsInScreen().Contains(location_in_screen_dip));

  // Move mouse cursor to the warp region of secondary display.
  location_in_screen_native = indicator_in_secondary_display.CenterPoint();
  location_in_host_native =
      location_in_screen_native -
      root_windows[0]->GetHost()->GetBoundsInPixels().OffsetFromOrigin();
  DispatchMouseEventWithNative(window_host, location_in_host_native,
                               ui::EventType::kMouseDragged,
                               ui::EF_LEFT_MOUSE_BUTTON, 0);

  // Mouse cursor should be warped into first display.
  location_in_screen_dip = aura::Env::GetInstance()->last_mouse_location();
  EXPECT_TRUE(
      root_windows[0]->GetBoundsInScreen().Contains(location_in_screen_dip));

  // After mouse warping, x-coordinate of mouse location in native coordinates
  // should be 2 px away from end. Primary display has zoom factor of 0.8. So
  // the offset in screen coordinates should be 2/0.8, which is 2.5. The end of
  // primary display in screen coordinates is 250. So x-coordinate of mouse
  // cursor in screen coordinates should be 247.
  EXPECT_EQ(247, aura::Env::GetInstance()->last_mouse_location().x());

  // Get cursor's location in host native coordinates.
  gfx::Point location_in_host_dip;
  location_in_screen_dip = aura::Env::GetInstance()->last_mouse_location();
  location_in_host_dip = location_in_screen_dip;
  ::wm::ConvertPointFromScreen(root_windows[0], &location_in_host_dip);
  location_in_host_native = location_in_host_dip;
  root_windows[0]->GetHost()->ConvertDIPToPixels(&location_in_host_native);

  // Release mouse button.
  DispatchMouseEventWithNative(
      window_host, location_in_host_native, ui::EventType::kMouseReleased,
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
}

}  // namespace ash
