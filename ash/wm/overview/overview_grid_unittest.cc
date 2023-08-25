// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_grid.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/strings/string_number_conversions.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class OverviewGridTest : public AshTestBase {
 public:
  OverviewGridTest() = default;

  OverviewGridTest(const OverviewGridTest&) = delete;
  OverviewGridTest& operator=(const OverviewGridTest&) = delete;

  ~OverviewGridTest() override = default;

  void InitializeGrid(const std::vector<aura::Window*>& windows) {
    aura::Window* root = Shell::GetPrimaryRootWindow();
    grid_ = std::make_unique<OverviewGrid>(root, windows, nullptr);
  }

  void CheckAnimationStates(
      const std::vector<aura::Window*>& windows,
      const std::vector<gfx::RectF>& target_bounds,
      const std::vector<bool>& expected_start_animations,
      const std::vector<bool>& expected_end_animations,
      absl::optional<size_t> selected_window_index = absl::nullopt) {
    ASSERT_EQ(windows.size(), target_bounds.size());
    ASSERT_EQ(windows.size(), expected_start_animations.size());
    ASSERT_EQ(windows.size(), expected_end_animations.size());

    InitializeGrid(windows);
    ASSERT_EQ(windows.size(), grid_->window_list().size());

    // The default values are to animate.
    for (const auto& item : grid_->window_list()) {
      SCOPED_TRACE("Initial values");
      EXPECT_TRUE(item->should_animate_when_entering());
      EXPECT_TRUE(item->should_animate_when_exiting());
    }

    grid_->CalculateWindowListAnimationStates(
        /*selected_item=*/nullptr, OverviewTransition::kEnter, target_bounds);
    for (size_t i = 0; i < grid_->window_list().size(); ++i) {
      SCOPED_TRACE("Enter animation, window " + base::NumberToString(i + 1));
      EXPECT_EQ(expected_start_animations[i],
                grid_->window_list()[i]->should_animate_when_entering());
    }

    for (size_t i = 0; i < grid_->window_list().size(); ++i) {
      grid_->window_list()[i]->set_target_bounds_for_testing(target_bounds[i]);
    }

    auto* selected_item =
        selected_window_index
            ? grid_->window_list()[*selected_window_index].get()
            : nullptr;
    grid_->CalculateWindowListAnimationStates(selected_item,
                                              OverviewTransition::kExit, {});
    for (size_t i = 0; i < grid_->window_list().size(); ++i) {
      SCOPED_TRACE("Exit animation, window " + base::NumberToString(i + 1));
      EXPECT_EQ(expected_end_animations[i],
                grid_->window_list()[i]->should_animate_when_exiting());
    }
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

  OverviewGrid* grid() { return grid_.get(); }

 private:
  std::unique_ptr<OverviewGrid> grid_;
};

// Tests that with only one window, we always animate.
TEST_F(OverviewGridTest, AnimateWithSingleWindow) {
  auto window = CreateTestWindow(gfx::Rect(100, 100));
  CheckAnimationStates({window.get()}, {gfx::RectF(100.f, 100.f)}, {true},
                       {true});
}

// Tests that if both the source and destination is hidden, there are no
// animations on the second window.
TEST_F(OverviewGridTest, SourceDestinationBothHidden) {
  auto window1 = CreateTestWindow(gfx::Rect(400, 400));
  auto window2 = CreateTestWindow(gfx::Rect(100, 100));
  std::vector<gfx::RectF> target_bounds = {gfx::RectF(100.f, 100.f),
                                           gfx::RectF(100.f, 100.f)};
  CheckAnimationStates({window1.get(), window2.get()}, target_bounds,
                       {true, false}, {true, false});
}

// Tests that are animations if the destination bounds are shown.
TEST_F(OverviewGridTest, SourceHiddenDestinationShown) {
  auto window1 = CreateTestWindow(gfx::Rect(400, 400));
  auto window2 = CreateTestWindow(gfx::Rect(100, 100));
  std::vector<gfx::RectF> target_bounds = {
      gfx::RectF(100.f, 100.f), gfx::RectF(400.f, 400.f, 100.f, 100.f)};
  CheckAnimationStates({window1.get(), window2.get()}, target_bounds,
                       {true, true}, {true, true});
}

// Tests that are animations if the source bounds are shown.
TEST_F(OverviewGridTest, SourceShownDestinationHidden) {
  auto window1 = CreateTestWindow(gfx::Rect(100, 100));
  auto window2 = CreateTestWindow(gfx::Rect(400, 400));
  std::vector<gfx::RectF> target_bounds = {gfx::RectF(100.f, 100.f),
                                           gfx::RectF(100.f, 100.f)};
  CheckAnimationStates({window1.get(), window2.get()}, target_bounds,
                       {true, true}, {true, true});
}

// Tests that a window that is in the union of two other windows, but is still
// shown will be animated.
TEST_F(OverviewGridTest, SourceShownButInTheUnionOfTwoOtherWindows) {
  // Create three windows, the union of the first two windows will be
  // gfx::Rect(0,0,200,200). Window 3 will be in that union, but should still
  // animate since its not fully occluded.
  auto window1 = CreateTestWindow(gfx::Rect(100, 100));
  auto window2 = CreateTestWindow(gfx::Rect(50, 50, 150, 150));
  auto window3 = CreateTestWindow(gfx::Rect(50, 200));
  std::vector<gfx::RectF> target_bounds = {gfx::RectF(100.f, 100.f),
                                           gfx::RectF(100.f, 100.f),
                                           gfx::RectF(100.f, 100.f)};
  CheckAnimationStates({window1.get(), window2.get(), window3.get()},
                       target_bounds, {true, true, true}, {true, true, true});
}

// Tests that an always on top window will take precedence over a normal
// window.
TEST_F(OverviewGridTest, AlwaysOnTopWindow) {
  // Create two windows, the second is always on top and covers the first
  // window. So the first window will not animate.
  auto window1 = CreateTestWindow(gfx::Rect(100, 100));
  auto window2 = CreateTestWindow(gfx::Rect(400, 400));
  window2->SetProperty(aura::client::kZOrderingKey,
                       ui::ZOrderLevel::kFloatingWindow);
  std::vector<gfx::RectF> target_bounds = {gfx::RectF(100.f, 100.f),
                                           gfx::RectF(100.f, 100.f)};
  CheckAnimationStates({window1.get(), window2.get()}, target_bounds,
                       {false, true}, {false, true});
}

// Tests that windows that are minimized are animated as expected.
TEST_F(OverviewGridTest, MinimizedWindows) {
  // Create 3 windows with the second and third windows being minimized. Both
  // the minimized window bounds are not occluded but only the third window is
  // animated because the target bounds for the first window is blocked.
  auto window1 = CreateTestWindow(gfx::Rect(100, 100));
  auto window2 = CreateTestWindow(gfx::Rect(400, 400));
  auto window3 = CreateTestWindow(gfx::Rect(400, 400));
  WindowState::Get(window2.get())->Minimize();
  WindowState::Get(window3.get())->Minimize();
  std::vector<gfx::RectF> target_bounds = {gfx::RectF(100.f, 100.f),
                                           gfx::RectF(100.f, 100.f),
                                           gfx::RectF(200.f, 200.f)};
  CheckAnimationStates({window1.get(), window2.get(), window3.get()},
                       target_bounds, {true, false, true}, {true, false, true});
}

TEST_F(OverviewGridTest, SelectedWindow) {
  // Create 3 windows with the third window being maximized. All windows are
  // visible on entering, so they should all be animated. On exit we select the
  // third window which is maximized, so the other two windows should not
  // animate.
  auto window1 = CreateTestWindow(gfx::Rect(100, 100));
  auto window2 = CreateTestWindow(gfx::Rect(400, 400));
  auto window3 = CreateTestWindow(gfx::Rect(400, 400));
  WindowState::Get(window3.get())->Maximize();
  std::vector<gfx::RectF> target_bounds = {gfx::RectF(100.f, 100.f),
                                           gfx::RectF(100.f, 100.f),
                                           gfx::RectF(100.f, 100.f)};
  CheckAnimationStates({window1.get(), window2.get(), window3.get()},
                       target_bounds, {true, true, true}, {false, false, true},
                       absl::make_optional(2u));
}

TEST_F(OverviewGridTest, WindowWithBackdrop) {
  // Create one non resizable window and one normal window and verify that the
  // backdrop shows over the non resizable window, and that normal window
  // becomes maximized upon entering tablet mode.
  auto window1 = CreateTestWindow(gfx::Rect(100, 100));
  auto window2 = CreateTestWindow(gfx::Rect(400, 400));
  window1->SetProperty(aura::client::kResizeBehaviorKey,
                       aura::client::kResizeBehaviorNone);
  wm::ActivateWindow(window1.get());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  BackdropController* backdrop_controller =
      GetWorkspaceControllerForContext(window1.get())
          ->layout_manager()
          ->backdrop_controller();
  EXPECT_EQ(window1.get(), backdrop_controller->GetTopmostWindowWithBackdrop());
  EXPECT_TRUE(backdrop_controller->backdrop_window());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsMaximized());

  // Tests that the second window despite being larger than the first window
  // does not animate as it is hidden behind the backdrop. On exit, it still
  // animates as the backdrop is not visible yet.
  std::vector<gfx::RectF> target_bounds = {gfx::RectF(100.f, 100.f),
                                           gfx::RectF(100.f, 100.f)};
  CheckAnimationStates({window1.get(), window2.get()}, target_bounds,
                       {true, false}, {true, true});
}

TEST_F(OverviewGridTest, DestinationPartiallyOffscreenWindow) {
  UpdateDisplay("500x400");
  auto window1 = CreateTestWindow(gfx::Rect(100, 100));
  auto window2 = CreateTestWindow(gfx::Rect(100, 100));

  // Position |window2|'s destination to be partially offscreen. Tests that it
  // still animates because the onscreen portion is not occluded by |window1|.
  std::vector<gfx::RectF> target_bounds = {
      gfx::RectF(100.f, 100.f), gfx::RectF(350.f, 100.f, 100.f, 100.f)};
  CheckAnimationStates({window1.get(), window2.get()}, target_bounds,
                       {true, true}, {true, true});

  // Maximize |window1|. |window2| should no longer animate since the parts of
  // it that are onscreen are fully occluded.
  WindowState::Get(window1.get())->Maximize();
  CheckAnimationStates({window1.get(), window2.get()}, target_bounds,
                       {true, false}, {true, false});
}

TEST_F(OverviewGridTest, SourcePartiallyOffscreenWindow) {
  UpdateDisplay("500x400");
  auto window1 = CreateTestWindow(gfx::Rect(100, 100));
  // Create |window2| to be partially offscreen.
  auto window2 = CreateTestWindow(gfx::Rect(450, 100, 100, 100));

  // Tests that it still animates because the onscreen portion is not occluded
  // by |window1|.
  std::vector<gfx::RectF> target_bounds = {gfx::RectF(100.f, 100.f),
                                           gfx::RectF(200.f, 200.f)};
  CheckAnimationStates({window1.get(), window2.get()}, target_bounds,
                       {true, true}, {true, true});

  // Maximize |window1|. |window2| should no longer animate since the parts of
  // it that are onscreen are fully occluded.
  WindowState::Get(window1.get())->Maximize();
  CheckAnimationStates({window1.get(), window2.get()}, target_bounds,
                       {true, false}, {true, false});
}

// Tests that windows whose destination is fully offscreen never animate.
TEST_F(OverviewGridTest, FullyOffscreenWindow) {
  UpdateDisplay("500x400");
  auto window1 = CreateTestWindow(gfx::Rect(100, 100));
  auto window2 = CreateTestWindow(gfx::Rect(100, 100));

  std::vector<gfx::RectF> target_bounds = {
      gfx::RectF(100.f, 100.f), gfx::RectF(450.f, 450.f, 100.f, 100.f)};
  CheckAnimationStates({window1.get(), window2.get()}, target_bounds,
                       {true, false}, {true, false});

  WindowState::Get(window1.get())->Maximize();
  CheckAnimationStates({window1.get(), window2.get()}, target_bounds,
                       {true, false}, {true, false});
}

// Tests that only one window animates when entering overview from splitview
// double snapped.
TEST_F(OverviewGridTest, SnappedWindow) {
  auto window1 = CreateTestWindow(gfx::Rect(100, 100));
  auto window2 = CreateTestWindow(gfx::Rect(100, 100));
  auto window3 = CreateTestWindow(gfx::Rect(100, 100));
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window2.get());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);

  // Snap |window2| and check that |window3| is maximized.
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_TRUE(WindowState::Get(window3.get())->IsMaximized());

  // We cannot create a grid object like in the other tests because creating a
  // grid calls |GetGridBoundsInScreen| with split view state both snapped which
  // is an unnatural state.
  EnterOverview();

  // Tests that |window3| is not animated even though its bounds are larger than
  // |window2| because it is fully occluded by |window1| + |window2| and the
  // split view divider.
  auto* item2 = GetOverviewItemForWindow(window2.get());
  auto* item3 = GetOverviewItemForWindow(window3.get());
  EXPECT_TRUE(item2->should_animate_when_entering());
  EXPECT_FALSE(item3->should_animate_when_entering());
}

}  // namespace ash
