// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_toggle_fullscreen_event_handler.h"

#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"

namespace ash {

class TabletModeToggleFullscreenEventHandlerTest : public AshTestBase {
 public:
  TabletModeToggleFullscreenEventHandlerTest() = default;
  TabletModeToggleFullscreenEventHandlerTest(
      const TabletModeToggleFullscreenEventHandlerTest&) = delete;
  TabletModeToggleFullscreenEventHandlerTest& operator=(
      const TabletModeToggleFullscreenEventHandlerTest&) = delete;
  ~TabletModeToggleFullscreenEventHandlerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    UpdateDisplay("800x600");
    TabletModeControllerTestApi().EnterTabletMode();

    background_window_ = CreateTestWindow(gfx::Rect(200, 200));
    foreground_window_ = CreateTestWindow(gfx::Rect(200, 200));
    ToggleFullscreen(foreground_window_.get(), /*immersive=*/false);
    ToggleFullscreen(background_window_.get(), /*immersive=*/false);
  }

  void TearDown() override {
    background_window_.reset();
    foreground_window_.reset();
    AshTestBase::TearDown();
  }

  void ToggleFullscreen(aura::Window* window, bool immersive) {
    WMEvent toggle_fullscreen(WM_EVENT_TOGGLE_FULLSCREEN);
    WindowState::Get(window)->OnWMEvent(&toggle_fullscreen);
    window->SetProperty(chromeos::kImmersiveIsActive, immersive);
  }

  bool IsFullscreen(aura::Window* window) const {
    return WindowState::Get(window)->IsFullscreen();
  }

  void GenerateSwipe(int start_y, int end_y) {
    GetEventGenerator()->GestureScrollSequence(gfx::Point(100, start_y),
                                               gfx::Point(100, end_y),
                                               base::Milliseconds(100), 3);

    // Swiping down on the center reveals the tablet mode multitask menu. Ensure
    // our swipes do not reveal it, as it may eat following gestures.
    auto* multitask_menu = TabletModeControllerTestApi()
                               .tablet_mode_window_manager()
                               ->tablet_mode_multitask_menu_controller()
                               ->multitask_menu();
    ASSERT_FALSE(multitask_menu);
  }

  aura::Window* foreground_window() { return foreground_window_.get(); }
  aura::Window* background_window() { return background_window_.get(); }

 private:
  std::unique_ptr<aura::Window> foreground_window_;
  std::unique_ptr<aura::Window> background_window_;
};

TEST_F(TabletModeToggleFullscreenEventHandlerTest, SwipeFromTop) {
  ASSERT_TRUE(IsFullscreen(foreground_window()));
  ASSERT_TRUE(IsFullscreen(background_window()));

  // Try swiping from a point not on the edge. Verify that we do not exit
  // fullscreen.
  GenerateSwipe(100, 200);
  ASSERT_TRUE(IsFullscreen(foreground_window()));
  ASSERT_TRUE(IsFullscreen(background_window()));

  // Try a tiny swipe that is on the edge. Verify that we do not exit
  // fullscreen.
  GenerateSwipe(1, 5);
  ASSERT_TRUE(IsFullscreen(foreground_window()));
  ASSERT_TRUE(IsFullscreen(background_window()));

  // Test that a normal swipe on the edge will exit fullscreen on the active
  // window.
  GenerateSwipe(1, 50);
  EXPECT_FALSE(IsFullscreen(foreground_window()));
  EXPECT_TRUE(IsFullscreen(background_window()));

  // Test that a second swipe will not do anything..
  GenerateSwipe(1, 50);
  EXPECT_FALSE(IsFullscreen(foreground_window()));
  EXPECT_TRUE(IsFullscreen(background_window()));
}

TEST_F(TabletModeToggleFullscreenEventHandlerTest, SwipeFromBottom) {
  ASSERT_TRUE(IsFullscreen(foreground_window()));
  ASSERT_TRUE(IsFullscreen(background_window()));

  // Try swiping from a point not on the edge. Verify that we do not exit
  // fullscreen.
  GenerateSwipe(500, 400);
  ASSERT_TRUE(IsFullscreen(foreground_window()));
  ASSERT_TRUE(IsFullscreen(background_window()));

  // Try a tiny swipe that is on the edge. Verify that we do not exit
  // fullscreen.
  GenerateSwipe(599, 594);
  ASSERT_TRUE(IsFullscreen(foreground_window()));
  ASSERT_TRUE(IsFullscreen(background_window()));

  // Test that a normal swipe on the edge will exit fullscreen on the active
  // window.
  GenerateSwipe(599, 549);
  EXPECT_FALSE(IsFullscreen(foreground_window()));
  EXPECT_TRUE(IsFullscreen(background_window()));
}

// Tests that tapping on the edge does not exit fullscreen.
TEST_F(TabletModeToggleFullscreenEventHandlerTest, TapOnEdge) {
  ASSERT_TRUE(IsFullscreen(foreground_window()));

  // Tap on the top edge.
  GetEventGenerator()->set_current_screen_location(gfx::Point(400, 1));
  GetEventGenerator()->PressTouch();
  GetEventGenerator()->ReleaseTouch();
  EXPECT_TRUE(IsFullscreen(foreground_window()));

  // Tap on the bottom edge.
  GetEventGenerator()->set_current_screen_location(gfx::Point(400, 50));
  GetEventGenerator()->PressTouch();
  GetEventGenerator()->ReleaseTouch();
  EXPECT_TRUE(IsFullscreen(foreground_window()));
}

TEST_F(TabletModeToggleFullscreenEventHandlerTest,
       SwipeImmersiveFullscreenWindow) {
  // Switch from non-immersive fullscreen to immersive fullscreen mode.
  ToggleFullscreen(foreground_window(), /*immersive=*/true);
  ToggleFullscreen(foreground_window(), /*immersive=*/true);
  ASSERT_TRUE(IsFullscreen(foreground_window()));

  // Test that a normal swipe on the top edge will not exit immersive
  // fullscreen.
  GenerateSwipe(1, 50);
  EXPECT_TRUE(IsFullscreen(foreground_window()));
  EXPECT_TRUE(IsFullscreen(background_window()));

  // Test that a normal swipe on the top edge will not exit immersive
  // fullscreen.
  GenerateSwipe(599, 549);
  EXPECT_TRUE(IsFullscreen(foreground_window()));
  EXPECT_TRUE(IsFullscreen(background_window()));
}

// Tests that if a window is un-fullscreened during a drag, it remains
// un-fullscreened on touch release.
TEST_F(TabletModeToggleFullscreenEventHandlerTest, ToggleFullscreenDuringDrag) {
  ASSERT_TRUE(IsFullscreen(foreground_window()));

  GetEventGenerator()->set_current_screen_location(gfx::Point(400, 1));
  GetEventGenerator()->PressTouch();
  ToggleFullscreen(foreground_window(), /*immersive=*/false);
  EXPECT_FALSE(IsFullscreen(foreground_window()));

  GetEventGenerator()->set_current_screen_location(gfx::Point(400, 50));
  GetEventGenerator()->ReleaseTouch();
  EXPECT_FALSE(IsFullscreen(foreground_window()));
}

// Tests that we do not have a crash when using this feature with multiple
// fingers. Regression test for b/270155382.
TEST_F(TabletModeToggleFullscreenEventHandlerTest, SwipingWithMultipleFingers) {
  ASSERT_TRUE(IsFullscreen(foreground_window()));

  const int kTouchId1 = 1;
  const int kTouchId2 = 2;

  GetEventGenerator()->PressTouchId(kTouchId1, gfx::Point(400, 1));
  GetEventGenerator()->MoveTouchIdBy(kTouchId1, 0, 48);

  // Swipe with a second finger while the first one is still touching the
  // screen. There should be no crash.
  GetEventGenerator()->PressTouchId(kTouchId2, gfx::Point(400, 1));
  GetEventGenerator()->MoveTouchIdBy(kTouchId2, 0, 48);
  GetEventGenerator()->ReleaseTouchId(kTouchId2);

  // Release the first finger and verify the window is un-fullscreened.
  GetEventGenerator()->ReleaseTouchId(kTouchId1);
  EXPECT_FALSE(IsFullscreen(foreground_window()));
}

}  // namespace ash
