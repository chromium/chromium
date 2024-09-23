// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_metrics.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/window_state.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_utils.h"

namespace ash {
namespace {

class OverviewMetricsTest : public AshTestBase {
 protected:
  OverviewMetricsTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void EnterOverview() {
    ASSERT_TRUE(AshTestBase::EnterOverview());
    WaitForOverviewEnterAnimation();
  }

  void ExitOverview() {
    ASSERT_TRUE(AshTestBase::ExitOverview());
    WaitForOverviewExitAnimation();
  }

  void WaitForDeskBarPresentationTimeMetricsRecording() {
    constexpr base::TimeDelta kLegacyPresentationTimeMaxLatency =
        base::Seconds(2);
    task_environment()->FastForwardBy(kLegacyPresentationTimeMaxLatency * 2);
  }

  void EnterAndExitOverview() {
    EnterOverview();
    ExitOverview();
    WaitForDeskBarPresentationTimeMetricsRecording();
  }

  base::HistogramTester histogram_tester_;
};

TEST_F(OverviewMetricsTest, GetPresentationTimeMetricNameWithDeskBar) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ASSERT_EQ(DesksController::Get()->desks().size(), 1u);
  std::unique_ptr<aura::Window> desk_1_window = CreateTestWindow();
  ASSERT_FALSE(WindowState::Get(desk_1_window.get())->IsMaximized());

  // With just 1 desk, the desk bar mini views should not be rendered, so the
  // metric should not be recorded.
  EnterAndExitOverview();
  histogram_tester_.ExpectTotalCount(
      "Ash.Overview.Enter.PresentationTime.WithDeskBarAndNumWindows1", 0);
  histogram_tester_.ExpectTotalCount(
      "Ash.Overview.Exit.PresentationTime.WithDeskBarAndNumWindows1", 0);

  NewDesk();

  // With 2 desks and a normal (not maximized) window, the desk bar mini views
  // should be rendered, but after the enter-overview animation completes. So
  // exit presentation time should be recorded, but enter should not.
  EnterOverview();
  // Desk bar should get rendered in the next frame (after the enter animation
  // has finished).
  ASSERT_TRUE(ui::WaitForNextFrameToBePresented(
      Shell::Get()->GetPrimaryRootWindow()->GetHost()->compositor()));
  ExitOverview();
  WaitForDeskBarPresentationTimeMetricsRecording();

  histogram_tester_.ExpectTotalCount(
      "Ash.Overview.Enter.PresentationTime.WithDeskBarAndNumWindows1", 0);
  histogram_tester_.ExpectTotalCount(
      "Ash.Overview.Exit.PresentationTime.WithDeskBarAndNumWindows1", 1);

  WindowState::Get(desk_1_window.get())->Maximize();

  // With 2 desks and a maximized window, the desk bar mini views should be
  // rendered immediately in the first frame, and both enter/exit metrics
  // should be recorded.
  EnterAndExitOverview();
  histogram_tester_.ExpectTotalCount(
      "Ash.Overview.Enter.PresentationTime.WithDeskBarAndNumWindows1", 1);
  histogram_tester_.ExpectTotalCount(
      "Ash.Overview.Exit.PresentationTime.WithDeskBarAndNumWindows1", 2);

  ActivateDesk(DesksController::Get()->GetDeskAtIndex(1));
  std::vector<std::unique_ptr<aura::Window>> desk_2_windows;
  for (int i = 2; i <= 10; ++i) {
    desk_2_windows.push_back(CreateTestWindow());
  }
  // Must maximize a window for the desk bar to be shown immediately and for the
  // enter presentation time to be recorded.
  WindowState::Get(desk_2_windows.back().get())->Maximize();
  EnterAndExitOverview();
  histogram_tester_.ExpectTotalCount(
      "Ash.Overview.Enter.PresentationTime.WithDeskBarAndNumWindows10", 1);
  histogram_tester_.ExpectTotalCount(
      "Ash.Overview.Exit.PresentationTime.WithDeskBarAndNumWindows10", 1);

  desk_2_windows.push_back(CreateTestWindow());
  EnterAndExitOverview();
  histogram_tester_.ExpectTotalCount(
      "Ash.Overview.Enter.PresentationTime.WithDeskBarAndNumWindowsMoreThan10",
      1);
  histogram_tester_.ExpectTotalCount(
      "Ash.Overview.Exit.PresentationTime.WithDeskBarAndNumWindowsMoreThan10",
      1);
}

}  // namespace
}  // namespace ash
