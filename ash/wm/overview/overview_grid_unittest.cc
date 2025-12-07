// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_grid.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/window_occlusion_calculator.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_test_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class OverviewGridTest : public OverviewTestBase {
 public:
  OverviewGridTest() = default;
  OverviewGridTest(const OverviewGridTest&) = delete;
  OverviewGridTest& operator=(const OverviewGridTest&) = delete;
  ~OverviewGridTest() override = default;

  // Calculates `OverviewItemBase::should_animate_when_exiting_`. The reason we
  // do it like this is because this is normally called during shutdown, and
  // then the grid and items objects are destroyed. Note that this function
  // assumes one root window.
  void CalculateShouldAnimateWhenExiting(
      OverviewItemBase* selected_item = nullptr) {
    ASSERT_EQ(1u, Shell::GetAllRootWindows().size());

    OverviewGrid* grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
    ASSERT_TRUE(grid);
    grid->CalculateWindowListAnimationStates(selected_item,
                                             OverviewTransition::kExit,
                                             /*target_bounds=*/{});
  }

  // Checks expected against actual enter and exit animation values. Note that
  // this function assumes one root window.
  void VerifyAnimationStates(
      const std::vector<bool>& expected_enter_animations,
      const std::vector<bool>& expected_exit_animations) {
    ASSERT_EQ(1u, Shell::GetAllRootWindows().size());

    OverviewGrid* grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
    ASSERT_TRUE(grid);

    const std::vector<std::unique_ptr<OverviewItemBase>>& overview_items =
        grid->item_list();
    if (!expected_enter_animations.empty()) {
      ASSERT_EQ(overview_items.size(), expected_enter_animations.size());
      for (size_t i = 0; i < overview_items.size(); ++i) {
        EXPECT_EQ(expected_enter_animations[i],
                  overview_items[i]->should_animate_when_entering());
      }
    }
    if (!expected_exit_animations.empty()) {
      ASSERT_EQ(overview_items.size(), expected_exit_animations.size());
      for (size_t i = 0; i < overview_items.size(); ++i) {
        EXPECT_EQ(expected_exit_animations[i],
                  overview_items[i]->should_animate_when_exiting());
      }
    }
  }
};

// Tests that with only one window, we always animate.
TEST_F(OverviewGridTest, AnimateWithSingleWindow) {
  auto window = CreateAppWindow(gfx::Rect(100, 100));
  ToggleOverview();
  VerifyAnimationStates({true}, {});

  CalculateShouldAnimateWhenExiting();
  VerifyAnimationStates({}, {true});
}

// Tests that are animations if the destination bounds are shown.
TEST_F(OverviewGridTest, SourceHiddenDestinationShown) {
  auto window1 = CreateAppWindow(gfx::Rect(100, 100));
  auto window2 = CreateAppWindow(gfx::Rect(200, 200));

  ToggleOverview();
  VerifyAnimationStates({true, true}, {});

  CalculateShouldAnimateWhenExiting();
  VerifyAnimationStates({}, {true, true});
}

// Tests that are animations if the source bounds are shown.
TEST_F(OverviewGridTest, SourceShownDestinationHidden) {
  auto window1 = CreateAppWindow(gfx::Rect(100, 100));
  WindowState::Get(window1.get())->Maximize();

  auto window2 = CreateAppWindow(gfx::Rect(400, 400));

  ToggleOverview();
  VerifyAnimationStates({true, true}, {});

  CalculateShouldAnimateWhenExiting();
  VerifyAnimationStates({}, {true, true});
}

// Tests that a window that is in the union of two other windows, but is still
// shown will be animated.
TEST_F(OverviewGridTest, SourceShownButInTheUnionOfTwoOtherWindows) {
  // Create three windows, the union of the first two windows will be
  // gfx::Rect(0, 0, 200, 200). Window 3 will be in that union, but should still
  // animate since its not fully occluded.
  auto window3 = CreateAppWindow(gfx::Rect(50, 200));
  auto window2 = CreateAppWindow(gfx::Rect(50, 50, 150, 150));
  auto window1 = CreateAppWindow(gfx::Rect(100, 100));

  ToggleOverview();
  VerifyAnimationStates({true, true, true}, {});

  CalculateShouldAnimateWhenExiting();
  VerifyAnimationStates({}, {true, true, true});
}

// Tests that an always on top window will still be animated even if its source
// and destination bounds are covered.
TEST_F(OverviewGridTest, AlwaysOnTopWindow) {
  UpdateDisplay("800x600");

  // Create two windows, even if `window1` is maximized, `window2` will still
  // animate since it is always on top.
  auto window2 = CreateAppWindow(gfx::Rect(100, 100));
  window2->SetProperty(aura::client::kZOrderingKey,
                       ui::ZOrderLevel::kFloatingWindow);
  auto window1 = CreateAppWindow(gfx::Rect(100, 100));
  WindowState::Get(window1.get())->Maximize();

  ToggleOverview();
  VerifyAnimationStates({true, true}, {});

  CalculateShouldAnimateWhenExiting();
  VerifyAnimationStates({}, {true, true});
}

// Tests that windows that are minimized are animated as expected.
TEST_F(OverviewGridTest, MinimizedWindows) {
  UpdateDisplay("800x600");

  auto window3 = CreateAppWindow(gfx::Rect(800, 600));
  WindowState::Get(window3.get())->Minimize();
  auto window2 = CreateAppWindow(gfx::Rect(800, 600));
  WindowState::Get(window2.get())->Minimize();
  auto window1 = CreateAppWindow(gfx::Rect(10, 10, 780, 580));

  // The minimized windows do not animate since their source is hidden, and
  // their destination is blocked by the near maximized window.
  ToggleOverview();
  VerifyAnimationStates({true, false, false}, {});

  CalculateShouldAnimateWhenExiting();
  VerifyAnimationStates({}, {true, false, false});
}

TEST_F(OverviewGridTest, SelectedWindow) {
  // Create 3 windows with the third window being maximized. All windows are
  // visible on entering, so they should all be animated. On exit we select the
  // third window which is maximized, so the other two windows should not
  // animate.
  auto window3 = CreateAppWindow(gfx::Rect(400, 400));
  WindowState::Get(window3.get())->Maximize();
  auto window2 = CreateAppWindow(gfx::Rect(400, 400));
  auto window1 = CreateAppWindow(gfx::Rect(100, 100));

  ToggleOverview();
  VerifyAnimationStates({true, true, true}, {});

  OverviewItemBase* selected_item = GetOverviewItemForWindow(window3.get());
  CalculateShouldAnimateWhenExiting(selected_item);
  VerifyAnimationStates({}, {false, false, true});
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
  ToggleOverview();
  VerifyAnimationStates({true, false}, {});

  CalculateShouldAnimateWhenExiting();
  VerifyAnimationStates({}, {true, true});
}

TEST_F(OverviewGridTest, SourcePartiallyOffscreenWindow) {
  UpdateDisplay("500x400");

  // Create `window2` to be partially offscreen.
  auto window2 = CreateAppWindow(gfx::Rect(450, 100, 100, 100));
  auto window1 = CreateAppWindow(gfx::Rect(100, 100));

  // Tests that it still animates because the onscreen portion is not occluded
  // by `window1`.
  ToggleOverview();
  VerifyAnimationStates({true, true}, {});

  CalculateShouldAnimateWhenExiting();
  VerifyAnimationStates({}, {true, true});
  ToggleOverview();

  // Maximize `window1`. `window2` should no longer animate since the parts of
  // it that are onscreen are fully occluded.
  WindowState::Get(window1.get())->Maximize();
  ToggleOverview();
  VerifyAnimationStates({true, false}, {});
}

// Tests that windows whose destination is partially or fully offscreen never
// animate.
TEST_F(OverviewGridTest, PartialAndFullOffscreenWindow) {
  UpdateDisplay("800x600");

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // With this display size, the 9th and 10th windows will be partially
  // offscreen and the 11th and 12th windows will be fully offscreen once we
  // enter overview. Since the earlier created windows are lower on the MRU
  // order, this equals the 3rd and 4th windows and the 1st and 2nd window
  // respectively.
  std::vector<std::unique_ptr<aura::Window>> windows;
  for (int i = 0; i < 12; ++i) {
    windows.push_back(CreateAppWindow(gfx::Rect(100, 100)));
  }

  // Enter overview and assert that we have one partially offscreen overview
  // item and one fully offscreen overview item.
  ToggleOverview();
  OverviewItemBase* partially_offscreen_item =
      GetOverviewItemForWindow(windows[2].get());
  OverviewItemBase* fully_offscreen_item =
      GetOverviewItemForWindow(windows[0].get());
  ASSERT_FALSE(gfx::RectF(800.f, 600.f)
                   .Contains(partially_offscreen_item->target_bounds()));
  ASSERT_TRUE(gfx::RectF(800.f, 600.f)
                  .Intersects(partially_offscreen_item->target_bounds()));
  ASSERT_FALSE(
      gfx::RectF(800.f, 600.f).Contains(fully_offscreen_item->target_bounds()));
  ASSERT_FALSE(gfx::RectF(800.f, 600.f)
                   .Intersects(fully_offscreen_item->target_bounds()));
  EXPECT_FALSE(partially_offscreen_item->should_animate_when_entering());
  EXPECT_FALSE(fully_offscreen_item->should_animate_when_entering());

  CalculateShouldAnimateWhenExiting();
  EXPECT_FALSE(partially_offscreen_item->should_animate_when_exiting());
  EXPECT_FALSE(fully_offscreen_item->should_animate_when_exiting());
}

// Tests that only one window animates when entering overview from splitview
// double snapped.
TEST_F(OverviewGridTest, SnappedWindow) {
  auto window3 = CreateAppWindow(gfx::Rect(100, 100));
  auto window2 = CreateAppWindow(gfx::Rect(100, 100));
  auto window1 = CreateAppWindow(gfx::Rect(100, 100));

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(window1.get(), SnapPosition::kPrimary);

  // Snap `window2` and check that `window3` is maximized.
  split_view_controller->SnapWindow(window2.get(), SnapPosition::kSecondary);
  EXPECT_TRUE(WindowState::Get(window3.get())->IsMaximized());

  // Tests that `window3` is not animated even though its bounds are larger than
  // `window2` because it is fully occluded by `window1` + `window2` and the
  // split view divider.
  ToggleOverview();
  VerifyAnimationStates({true, false}, {});
}

TEST_F(OverviewGridTest, RecordsDelayedDeskBarPresentationMetric) {
  // Use SLOW_DURATION to ensure the enter animation doesn't complete too
  // quickly (within one frame), which causes the metric to be recorded earlier
  // than expected by this test's intermediate check.
  gfx::ScopedAnimationDurationScaleMode animation_scale(
      gfx::ScopedAnimationDurationScaleMode::SLOW_DURATION);

  // Since the windows are not maximized, the desk bar should open after
  // the overview animation is complete, causing
  // `kOverviewDelayedDeskBarPresentationHistogram` to be recorded.
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());

  ui::Compositor* const compositor = window1->GetHost()->compositor();
  base::HistogramTester histogram_tester;
  ToggleOverview();
  ASSERT_TRUE(ui::WaitForNextFrameToBePresented(compositor));
  histogram_tester.ExpectTotalCount(
      kOverviewDelayedDeskBarPresentationHistogram, 0);
  WaitForOverviewEnterAnimation();
  ASSERT_TRUE(ui::WaitForNextFrameToBePresented(compositor));
  histogram_tester.ExpectTotalCount(
      kOverviewDelayedDeskBarPresentationHistogram, 1);
}

TEST_F(OverviewGridTest, DoesNotRecordDelayedDeskBarPresentationMetric) {
  gfx::ScopedAnimationDurationScaleMode animation_scale(
      gfx::ScopedAnimationDurationScaleMode::FAST_DURATION);

  // Since the windows are maximized, the desk bar should open immediately when
  // we enter overview and `kOverviewDelayedDeskBarPresentationHistogram` should
  // not be recorded.
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  WindowState::Get(window1.get())->Maximize();
  WindowState::Get(window2.get())->Maximize();

  ui::Compositor* const compositor = window1->GetHost()->compositor();
  base::HistogramTester histogram_tester;
  ToggleOverview();
  ASSERT_TRUE(ui::WaitForNextFrameToBePresented(compositor));
  WaitForOverviewEnterAnimation();
  ASSERT_TRUE(ui::WaitForNextFrameToBePresented(compositor));
  histogram_tester.ExpectTotalCount(
      kOverviewDelayedDeskBarPresentationHistogram, 0);
}

}  // namespace ash
