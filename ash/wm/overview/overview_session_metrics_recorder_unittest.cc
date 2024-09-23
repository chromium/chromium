// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_session_metrics_recorder.h"

#include "ash/public/cpp/overview_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_utils.h"

namespace ash {

class OverviewSessionMetricsRecorderTest : public AshTestBase {
 protected:
  void WaitForNextFramePresentation() {
    ASSERT_TRUE(ui::WaitForNextFrameToBePresented(
        Shell::Get()->GetPrimaryRootWindow()->GetHost()->compositor()));
  }

  void EnterOverviewAndWaitForAnimation() {
    ASSERT_TRUE(EnterOverview());
    // Required for presentation time to be recorded.
    WaitForNextFramePresentation();
    WaitForOverviewEnterAnimation();
  }

  void ExitOverviewAndWaitForAnimation() {
    ASSERT_TRUE(ExitOverview());
    WaitForNextFramePresentation();
    WaitForOverviewExitAnimation();
  }

  base::HistogramTester histogram_tester_;
};

TEST_F(OverviewSessionMetricsRecorderTest,
       RecordsAllCoreMetricsInBasicSession) {
  // Ensure the metrics work across multiple sessions.
  constexpr int kNumOverviewSessionsToTest = 2;
  for (int i = 0; i < kNumOverviewSessionsToTest; ++i) {
    EnterOverviewAndWaitForAnimation();
    ExitOverviewAndWaitForAnimation();
  }

  histogram_tester_.ExpectUniqueSample("Ash.Overview.StartAction",
                                       OverviewStartAction::kTests,
                                       kNumOverviewSessionsToTest);
  histogram_tester_.ExpectUniqueSample("Ash.Overview.EndAction",
                                       OverviewEndAction::kTests,
                                       kNumOverviewSessionsToTest);
  histogram_tester_.ExpectUniqueSample("Ash.Overview.DeskCount", 1,
                                       kNumOverviewSessionsToTest);
  histogram_tester_.ExpectTotalCount(kEnterOverviewPresentationHistogram,
                                     kNumOverviewSessionsToTest);
  histogram_tester_.ExpectTotalCount(kExitOverviewPresentationHistogram,
                                     kNumOverviewSessionsToTest);
}

TEST_F(OverviewSessionMetricsRecorderTest, DeskBarVisibilityShownImmediately) {
  ASSERT_TRUE(EnterOverview());
  // Required for presentation time to be recorded.
  WaitForNextFramePresentation();
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid->desks_bar_view());
  WaitForOverviewEnterAnimation();

  ExitOverviewAndWaitForAnimation();

  histogram_tester_.ExpectUniqueSample("Ash.Overview.DeskBarVisibility",
                                       DeskBarVisibility::kShownImmediately, 1);
}

TEST_F(OverviewSessionMetricsRecorderTest,
       DeskBarVisibilityShownAfterFirstFrame) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  // With 1 normal (not maximized) window, the desk bar should be rendered after
  // the overview enter animation completes.
  constexpr gfx::Rect kBounds(0, 0, 10, 10);
  std::unique_ptr<aura::Window> window1(CreateAppWindow(kBounds));
  std::unique_ptr<aura::Window> window2(CreateAppWindow(kBounds));
  ASSERT_FALSE(WindowState::Get(window1.get())->IsMaximized());
  ASSERT_FALSE(WindowState::Get(window2.get())->IsMaximized());

  ASSERT_TRUE(EnterOverview());
  WaitForNextFramePresentation();
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_FALSE(overview_grid->desks_bar_view());

  WaitForOverviewEnterAnimation();
  WaitForNextFramePresentation();
  ASSERT_TRUE(overview_grid->desks_bar_view());

  ExitOverviewAndWaitForAnimation();

  histogram_tester_.ExpectUniqueSample("Ash.Overview.DeskBarVisibility",
                                       DeskBarVisibility::kShownAfterFirstFrame,
                                       1);
}

TEST_F(OverviewSessionMetricsRecorderTest, DeskBarVisibilityNotShown) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  EnterOverviewAndWaitForAnimation();
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_FALSE(overview_grid->desks_bar_view());

  ExitOverviewAndWaitForAnimation();

  histogram_tester_.ExpectUniqueSample("Ash.Overview.DeskBarVisibility",
                                       DeskBarVisibility::kNotShown, 1);
}

TEST_F(OverviewSessionMetricsRecorderTest,
       EnterPresentationTimeSegmentedByReason) {
  OverviewController* const controller = OverviewController::Get();
  ASSERT_TRUE(controller->StartOverview(OverviewStartAction::kOverviewButton));
  WaitForNextFramePresentation();
  ExitOverviewAndWaitForAnimation();

  histogram_tester_.ExpectTotalCount(
      "Ash.Overview.Enter.PresentationTime2.UserInitiatedClamshell", 1);

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  ASSERT_TRUE(
      controller->StartOverview(OverviewStartAction::kDragWindowFromShelf));
  WaitForNextFramePresentation();
  ExitOverviewAndWaitForAnimation();

  histogram_tester_.ExpectTotalCount(
      "Ash.Overview.Enter.PresentationTime2.UserInitiatedTablet", 1);

  ASSERT_TRUE(controller->StartOverview(OverviewStartAction::kTests));
  WaitForNextFramePresentation();
  ASSERT_TRUE(ExitOverview());

  histogram_tester_.ExpectTotalCount(
      "Ash.Overview.Enter.PresentationTime2.Other", 1);

  ASSERT_TRUE(controller->StartOverview(OverviewStartAction::kPine));
  WaitForNextFramePresentation();
  ASSERT_TRUE(ExitOverview());

  histogram_tester_.ExpectTotalCount(
      "Ash.Overview.Enter.PresentationTime2.InformedRestore", 1);
}

}  // namespace ash
