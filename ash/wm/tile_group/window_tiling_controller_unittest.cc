// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tile_group/window_tiling_controller.h"

#include "ash/accelerators/accelerator_commands.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/wm_metrics.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

class WindowTilingControllerTest : public AshTestBase {
 public:
  WindowTilingControllerTest()
      : scoped_features_(features::kTilingWindowResize) {}
  WindowTilingControllerTest(const WindowTilingControllerTest&) = delete;
  WindowTilingControllerTest& operator=(const WindowTilingControllerTest&) =
      delete;
  ~WindowTilingControllerTest() override = default;

 protected:
  WindowTilingController* controller() {
    return Shell::Get()->window_tiling_controller();
  }

  std::unique_ptr<aura::Window> CreateToplevelTestWindow(
      const gfx::Rect& screen_bounds,
      const display::Display& display = display::Display()) {
    auto window = AshTestBase::CreateToplevelTestWindow(screen_bounds);
    if (display.is_valid()) {
      // If most of the initial window bounds is off screen, the window may be
      // shifted to show more of it when created, so we set it again.
      window->SetBoundsInScreen(screen_bounds, display);
    }
    EXPECT_EQ(window->GetBoundsInScreen(), screen_bounds);
    return window;
  }

  aura::test::TestWindowDelegate* GetTestDelegate(aura::Window* window) {
    return static_cast<aura::test::TestWindowDelegate*>(window->delegate());
  }

  gfx::Rect TopHalf(gfx::Rect bounds) {
    bounds.set_height(base::ClampRound(bounds.height() / 2.0));
    return bounds;
  }
  gfx::Rect BottomHalf(gfx::Rect bounds) {
    bounds.Subtract(TopHalf(bounds));
    return bounds;
  }
  gfx::Rect LeftHalf(gfx::Rect bounds) {
    bounds.set_width(base::ClampRound(bounds.width() / 2.0));
    return bounds;
  }
  gfx::Rect RightHalf(gfx::Rect bounds) {
    bounds.Subtract(LeftHalf(bounds));
    return bounds;
  }

  base::test::ScopedFeatureList scoped_features_;
};

}  // namespace

TEST_F(WindowTilingControllerTest, CanTilingResizeNormalWindow) {
  auto window = CreateToplevelTestWindow(gfx::Rect(10, 20, 450, 350));
  ASSERT_TRUE(WindowState::Get(window.get())->IsNormalStateType());

  EXPECT_TRUE(controller()->CanTilingResize(window.get()));
}

TEST_F(WindowTilingControllerTest, CanTilingResizeSnappedWindow) {
  auto window = CreateToplevelTestWindow(gfx::Rect(10, 20, 450, 350));

  const WindowSnapWMEvent event(
      WM_EVENT_SNAP_SECONDARY, WindowSnapActionSource::kDragWindowToEdgeToSnap);
  WindowState::Get(window.get())->OnWMEvent(&event);
  ASSERT_TRUE(WindowState::Get(window.get())->IsSnapped());

  EXPECT_TRUE(controller()->CanTilingResize(window.get()));

  const gfx::Rect work_area = GetPrimaryDisplay().work_area();
  gfx::Rect window_bounds = window->GetBoundsInScreen();
  controller()->OnTilingResizeUp(window.get());
  window_bounds.set_height(base::ClampRound(work_area.height() * 3 / 4.0));
  EXPECT_EQ(window->GetBoundsInScreen(), window_bounds);
}

TEST_F(WindowTilingControllerTest, CanTilingResizeMaximizedWindow) {
  auto window = CreateToplevelTestWindow(gfx::Rect(10, 20, 450, 350));

  WindowState::Get(window.get())->Maximize();
  ASSERT_TRUE(WindowState::Get(window.get())->IsMaximized());

  EXPECT_TRUE(controller()->CanTilingResize(window.get()));

  const gfx::Rect work_area = GetPrimaryDisplay().work_area();
  gfx::Rect window_bounds = window->GetBoundsInScreen();
  controller()->OnTilingResizeLeft(window.get());
  window_bounds.set_width(base::ClampRound(work_area.width() * 3 / 4.0));
  EXPECT_EQ(window->GetBoundsInScreen(), window_bounds);
}

TEST_F(WindowTilingControllerTest, CannotTilingResizeFullscreenWindow) {
  auto window = CreateToplevelTestWindow(gfx::Rect(10, 20, 450, 350));

  const WMEvent fullscreen(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(window.get())->OnWMEvent(&fullscreen);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFullscreen());

  EXPECT_FALSE(controller()->CanTilingResize(window.get()));
}

TEST_F(WindowTilingControllerTest, OnTilingResizeLeftThenRight) {
  UpdateDisplay("800x600");
  const gfx::Rect work_area = GetPrimaryDisplay().work_area();

  gfx::Rect expected_bounds(120, 80, 350, 250);
  auto window = CreateToplevelTestWindow(expected_bounds);
  GetTestDelegate(window.get())
      ->set_minimum_size(gfx::Size(work_area.width() / 4.0 + 3, 200));
  ASSERT_TRUE(controller()->CanTilingResize(window.get()));

  controller()->OnTilingResizeLeft(window.get());
  expected_bounds = LeftHalf(work_area);
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Should work at different heights.
  controller()->OnTilingResizeDown(window.get());
  expected_bounds.set_y(base::ClampRound(work_area.height() / 4.0));
  expected_bounds.set_height(work_area.height() - expected_bounds.y());
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Keep shrinking left.
  controller()->OnTilingResizeLeft(window.get());
  expected_bounds.set_width(base::ClampRound(work_area.width() / 3.0));
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Doesn't resize smaller than minimum size.
  controller()->OnTilingResizeLeft(window.get());
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Remove minimum size and resize again.
  GetTestDelegate(window.get())->set_minimum_size(gfx::Size());
  controller()->OnTilingResizeLeft(window.get());
  expected_bounds.set_width(base::ClampRound(work_area.width() / 4.0));
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Doesn't resize smaller than 1/4 work area width.
  controller()->OnTilingResizeLeft(window.get());
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Expand to the right.
  for (float ratio : {1.0 / 3, 1.0 / 2, 2.0 / 3, 3.0 / 4, 1.0}) {
    SCOPED_TRACE(base::StringPrintf(
        "Expanding right bound rightward to ratio=%.3f", ratio));
    controller()->OnTilingResizeRight(window.get());
    expected_bounds.set_width(base::ClampRound(work_area.width() * ratio));
    EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);
  }

  // Shrink to the right.
  for (float ratio : {1.0 / 4, 1.0 / 3, 1.0 / 2}) {
    SCOPED_TRACE(base::StringPrintf(
        "Shrinking left bound rightward to ratio=%.3f", ratio));
    controller()->OnTilingResizeRight(window.get());
    expected_bounds.set_x(base::ClampRound(work_area.width() * ratio));
    expected_bounds.set_width(work_area.width() - expected_bounds.x());
    EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);
  }
}

TEST_F(WindowTilingControllerTest, OnTilingResizeRightThenLeft) {
  UpdateDisplay("800x600");
  const gfx::Rect work_area = GetPrimaryDisplay().work_area();

  gfx::Rect expected_bounds(120, 80, 350, 250);
  auto window = CreateToplevelTestWindow(expected_bounds);
  GetTestDelegate(window.get())
      ->set_minimum_size(gfx::Size(work_area.width() / 4.0 + 3, 200));
  ASSERT_TRUE(controller()->CanTilingResize(window.get()));

  controller()->OnTilingResizeRight(window.get());
  expected_bounds = RightHalf(work_area);
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Should work at different heights.
  controller()->OnTilingResizeUp(window.get());
  expected_bounds.set_height(base::ClampRound(work_area.height() * 3 / 4.0));
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Keep shrinking right.
  controller()->OnTilingResizeRight(window.get());
  expected_bounds.set_x(base::ClampRound(work_area.width() * 2 / 3.0));
  expected_bounds.set_width(work_area.width() - expected_bounds.x());
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Doesn't resize smaller than minimum size.
  controller()->OnTilingResizeRight(window.get());
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Remove minimum size and resize again.
  GetTestDelegate(window.get())->set_minimum_size(gfx::Size());
  controller()->OnTilingResizeRight(window.get());
  expected_bounds.set_x(base::ClampRound(work_area.width() * 3 / 4.0));
  expected_bounds.set_width(work_area.width() - expected_bounds.x());
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Doesn't resize smaller than 1/4 work area width.
  controller()->OnTilingResizeRight(window.get());
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Expand to the left.
  for (float ratio : {2.0 / 3, 1.0 / 2, 1.0 / 3, 1.0 / 4, 0.0}) {
    SCOPED_TRACE(base::StringPrintf(
        "Expanding left bound leftward to ratio=%.3f", ratio));
    controller()->OnTilingResizeLeft(window.get());
    expected_bounds.set_x(base::ClampRound(work_area.width() * ratio));
    expected_bounds.set_width(work_area.width() - expected_bounds.x());
    EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);
  }

  // Shrink to the left.
  for (float ratio : {3.0 / 4, 2.0 / 3, 1.0 / 2}) {
    SCOPED_TRACE(base::StringPrintf(
        "Shrinking right bound leftward to ratio=%.3f", ratio));
    controller()->OnTilingResizeLeft(window.get());
    expected_bounds.set_width(base::ClampRound(work_area.width() * ratio));
    EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);
  }
}

TEST_F(WindowTilingControllerTest, OnTilingResizeUpThenDown) {
  UpdateDisplay("800x600");
  const gfx::Rect work_area = GetPrimaryDisplay().work_area();

  gfx::Rect expected_bounds(120, 80, 350, 250);
  auto window = CreateToplevelTestWindow(expected_bounds);
  GetTestDelegate(window.get())
      ->set_minimum_size(gfx::Size(200, work_area.height() / 4.0 + 3));
  ASSERT_TRUE(controller()->CanTilingResize(window.get()));

  controller()->OnTilingResizeUp(window.get());
  expected_bounds = TopHalf(work_area);
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Should work at different widths.
  controller()->OnTilingResizeLeft(window.get());
  expected_bounds.set_width(base::ClampRound(work_area.width() * 3 / 4.0));
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Keep shrinking upward.
  controller()->OnTilingResizeUp(window.get());
  expected_bounds.set_height(base::ClampRound(work_area.height() / 3.0));
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Doesn't resize smaller than minimum size.
  controller()->OnTilingResizeUp(window.get());
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Remove minimum size and resize again.
  GetTestDelegate(window.get())->set_minimum_size(gfx::Size());
  controller()->OnTilingResizeUp(window.get());
  expected_bounds.set_height(base::ClampRound(work_area.height() / 4.0));
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Doesn't resize smaller than 1/4 work area height.
  controller()->OnTilingResizeUp(window.get());
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Expand downward.
  for (float ratio : {1.0 / 3, 1.0 / 2, 2.0 / 3, 3.0 / 4, 1.0}) {
    SCOPED_TRACE(base::StringPrintf(
        "Expanding bottom bound downward to ratio=%.3f", ratio));
    controller()->OnTilingResizeDown(window.get());
    expected_bounds.set_height(base::ClampRound(work_area.height() * ratio));
    EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);
  }

  // Shrink downward.
  for (float ratio : {1.0 / 4, 1.0 / 3, 1.0 / 2}) {
    SCOPED_TRACE(base::StringPrintf(
        "Shrinking top bound downward to ratio=%.3f", ratio));
    controller()->OnTilingResizeDown(window.get());
    expected_bounds.set_y(base::ClampRound(work_area.height() * ratio));
    expected_bounds.set_height(work_area.height() - expected_bounds.y());
    EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);
  }
}

TEST_F(WindowTilingControllerTest, OnTilingResizeDownThenUp) {
  UpdateDisplay("800x600");
  const gfx::Rect work_area = GetPrimaryDisplay().work_area();

  gfx::Rect expected_bounds(120, 80, 350, 250);
  auto window = CreateToplevelTestWindow(expected_bounds);
  GetTestDelegate(window.get())
      ->set_minimum_size(gfx::Size(200, work_area.height() / 4.0 + 3));
  ASSERT_TRUE(controller()->CanTilingResize(window.get()));

  controller()->OnTilingResizeDown(window.get());
  expected_bounds = BottomHalf(work_area);
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Should work at different widths.
  controller()->OnTilingResizeRight(window.get());
  expected_bounds.set_x(base::ClampRound(work_area.width() / 4.0));
  expected_bounds.set_width(work_area.width() - expected_bounds.x());
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Keep shrinking downward.
  controller()->OnTilingResizeDown(window.get());
  expected_bounds.set_y(base::ClampRound(work_area.height() * 2 / 3.0));
  expected_bounds.set_height(work_area.height() - expected_bounds.y());
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Doesn't resize smaller than minimum size.
  controller()->OnTilingResizeDown(window.get());
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Remove minimum size and resize again.
  GetTestDelegate(window.get())->set_minimum_size(gfx::Size());
  controller()->OnTilingResizeDown(window.get());
  expected_bounds.set_y(base::ClampRound(work_area.height() * 3 / 4.0));
  expected_bounds.set_height(work_area.height() - expected_bounds.y());
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Doesn't resize smaller than 1/4 work area height.
  controller()->OnTilingResizeDown(window.get());
  EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);

  // Expand upward.
  for (float ratio : {2.0 / 3, 1.0 / 2, 1.0 / 3, 1.0 / 4, 0.0}) {
    SCOPED_TRACE(
        base::StringPrintf("Expanding top bound upward to ratio=%.3f", ratio));
    controller()->OnTilingResizeUp(window.get());
    expected_bounds.set_y(base::ClampRound(work_area.height() * ratio));
    expected_bounds.set_height(work_area.height() - expected_bounds.y());
    EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);
  }

  // Shrink upward.
  for (float ratio : {3.0 / 4, 2.0 / 3, 1.0 / 2}) {
    SCOPED_TRACE(base::StringPrintf(
        "Shrinking bottom bound upward to ratio=%.3f", ratio));
    controller()->OnTilingResizeUp(window.get());
    expected_bounds.set_height(base::ClampRound(work_area.height() * ratio));
    EXPECT_EQ(window->GetBoundsInScreen(), expected_bounds);
  }
}

}  // namespace ash
