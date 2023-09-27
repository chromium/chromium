// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_metrics_controller.h"

#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"

namespace ash {

class SplitViewMetricsControllerTest : public AshTestBase {
 public:
  SplitViewMetricsControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void AdvanceClock(base::TimeDelta delta) {
    task_environment()->AdvanceClock(delta);
    task_environment()->RunUntilIdle();
  }
};

// Tests that the metrics for recording the duration between one window getting
// snapped and another window getting snapped on the other side work correctly.
TEST_F(SplitViewMetricsControllerTest, RecordSnapTwoWindowsDuration) {
  base::HistogramTester histogram_tester;
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  std::unique_ptr<aura::Window> window1(CreateAppWindow());
  std::unique_ptr<aura::Window> window2(CreateAppWindow());
  WindowState* window_state1 = WindowState::Get(window1.get());
  WindowState* window_state2 = WindowState::Get(window2.get());

  // Snap `window1` to the left, wait 1 minute, then snap `window1` to the
  // right. Test it doesn't record since it's the same window.
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  window_state1->OnWMEvent(&snap_left);
  const WindowSnapWMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  window_state1->OnWMEvent(&snap_right);
  AdvanceClock(base::Minutes(1));
  histogram_tester.ExpectTotalCount(kSnapTwoWindowsDurationHistogramName, 0);

  // Snap `window1` to the left, wait 30 seconds, then snap `window2` to the
  // right. Test that it records in the 0 minute bucket.
  window_state1->OnWMEvent(&snap_left);
  AdvanceClock(base::Seconds(30));
  window_state2->OnWMEvent(&snap_right);
  histogram_tester.ExpectTimeBucketCount(kSnapTwoWindowsDurationHistogramName,
                                         base::Seconds(30), 1);

  // Snap `window2` to the left, wait 3 minutes, then snap `window1` to the
  // right. Test that it records in the 3 minute bucket.
  window_state2->OnWMEvent(&snap_left);
  AdvanceClock(base::Minutes(3));
  window_state1->OnWMEvent(&snap_right);
  histogram_tester.ExpectTimeBucketCount(kSnapTwoWindowsDurationHistogramName,
                                         base::Minutes(3), 1);

  // Snap `window1` to the left, wait 3 minutes, open a new `window3` and close
  // it to simulate real user sessions with multiple windows, then snap
  // `window2` to the right. Test that it increments the 3 minute bucket.
  window_state1->OnWMEvent(&snap_left);
  AdvanceClock(base::Minutes(3));
  std::unique_ptr<aura::Window> window3(CreateAppWindow());
  window3.reset();
  window_state2->OnWMEvent(&snap_right);
  histogram_tester.ExpectTimeBucketCount(kSnapTwoWindowsDurationHistogramName,
                                         base::Minutes(3), 2);

  // Snap `window1` to the right, wait 3 minutes, then minimize it. Test that it
  // records in the max bucket, since no other window was snapped.
  window_state1->OnWMEvent(&snap_right);
  AdvanceClock(base::Minutes(3));
  window_state1->Minimize();
  histogram_tester.ExpectTimeBucketCount(kSnapTwoWindowsDurationHistogramName,
                                         kSequentialSnapActionMaxTime, 1);

  // Snap a new `window4` to the left, wait 3 minutes, then close it. Test that
  // it records in the max bucket, since no other window was snapped.
  std::unique_ptr<aura::Window> window4(CreateAppWindow());
  WindowState* window_state4 = WindowState::Get(window4.get());
  window_state4->OnWMEvent(&snap_left);
  AdvanceClock(base::Minutes(3));
  window4.reset();
  histogram_tester.ExpectTimeBucketCount(kSnapTwoWindowsDurationHistogramName,
                                         kSequentialSnapActionMaxTime, 2);

  // Snap `window1` to the left, wait 3 minutes, move it to a new desk, move
  // `window2` to the same desk. Test it doesn't record anything.
  window_state1->OnWMEvent(&snap_left);
  AdvanceClock(base::Minutes(3));
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  desks_controller->desks()[0]->MoveWindowToDesk(
      window1.get(), desks_controller->desks()[1].get(),
      window1->GetRootWindow(), /*unminimize=*/true);
  desks_controller->desks()[0]->MoveWindowToDesk(
      window2.get(), desks_controller->desks()[1].get(),
      window2->GetRootWindow(), /*unminimize=*/true);
  histogram_tester.ExpectTotalCount(kSnapTwoWindowsDurationHistogramName, 5);
}

// Tests the metrics for the elapsed time between the first snapped window
// getting minimized and the second snapped window getting minimized.
TEST_F(SplitViewMetricsControllerTest, RecordMinimizeTwoWindowsDuration) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<aura::Window> window1(CreateAppWindow());
  std::unique_ptr<aura::Window> window2(CreateAppWindow());
  WindowState* window_state1 = WindowState::Get(window1.get());
  WindowState* window_state2 = WindowState::Get(window2.get());

  // Snap `window1` and `window2`.
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  window_state1->OnWMEvent(&snap_left);
  const WindowSnapWMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  window_state2->OnWMEvent(&snap_right);
  histogram_tester.ExpectTotalCount(kSnapTwoWindowsDurationHistogramName, 1);

  // Minimize `window1`, wait 3 minutes, then minimize `window2`.
  window_state1->Minimize();
  AdvanceClock(base::Minutes(3));
  window_state2->Minimize();
  histogram_tester.ExpectTimeBucketCount(
      kMinimizeTwoWindowsDurationHistogramName, base::Minutes(3), 1);

  // Minimize `window1`, wait 1 minute, then maximize `window1`. Test it records
  // in the maximum bucket.
  window_state1->Restore();
  window_state2->Restore();
  EXPECT_TRUE(window_state1->IsSnapped());
  EXPECT_TRUE(window_state2->IsSnapped());
  window_state1->Minimize();
  AdvanceClock(base::Minutes(3));
  window_state1->Maximize();
  histogram_tester.ExpectTimeBucketCount(
      kMinimizeTwoWindowsDurationHistogramName, kSequentialSnapActionMaxTime,
      1);

  // Minimize `window1`, wait 2 minutes, restore it to snapped state, then
  // minimize it again. Test we don't record anything.
  window_state1->OnWMEvent(&snap_left);
  window_state1->Minimize();
  AdvanceClock(base::Minutes(3));
  window_state1->Restore();
  EXPECT_TRUE(window_state1->IsSnapped());
  window_state1->Minimize();
  histogram_tester.ExpectTotalCount(kMinimizeTwoWindowsDurationHistogramName,
                                    3);

  // Minimize `window2`, wait 1 minute, then close `window2`. Test it records
  // in the maximum bucket.
  window_state1->OnWMEvent(&snap_left);
  window_state2->OnWMEvent(&snap_right);
  EXPECT_TRUE(window_state1->IsSnapped());
  EXPECT_TRUE(window_state2->IsSnapped());
  window_state2->Minimize();
  AdvanceClock(base::Minutes(3));
  window2.reset();
  histogram_tester.ExpectTimeBucketCount(
      kMinimizeTwoWindowsDurationHistogramName, kSequentialSnapActionMaxTime,
      2);
  histogram_tester.ExpectTotalCount(kMinimizeTwoWindowsDurationHistogramName,
                                    4);
}

}  // namespace ash
