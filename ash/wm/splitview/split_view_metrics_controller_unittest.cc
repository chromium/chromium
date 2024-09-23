// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_metrics_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/wm_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"

namespace ash {

class SplitViewMetricsControllerTest : public AshTestBase {
 public:
  SplitViewMetricsControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();
    window1_ = CreateAppWindow();
    window2_ = CreateAppWindow();
    window3_ = CreateAppWindow();
    window4_ = CreateAppWindow();
  }

  void TearDown() override {
    window1_.reset();
    window2_.reset();
    window3_.reset();
    window4_.reset();
    AshTestBase::TearDown();
  }

  void AdvanceClock(base::TimeDelta delta) {
    task_environment()->AdvanceClock(delta);
    task_environment()->RunUntilIdle();
  }

 protected:
  std::unique_ptr<aura::Window> window1_;
  std::unique_ptr<aura::Window> window2_;
  std::unique_ptr<aura::Window> window3_;
  std::unique_ptr<aura::Window> window4_;

  base::HistogramTester histogram_tester_;
};

// Tests that the metrics for recording the duration between one window getting
// snapped and another window getting snapped on the other side work correctly.
TEST_F(SplitViewMetricsControllerTest, RecordSnapTwoWindowsDuration) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);

  // Snap `window1_` to the left, wait 1 minute, then snap `window1_` to the
  // right. Test it doesn't record since it's the same window.
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  WindowState* window_state1 = WindowState::Get(window1_.get());
  window_state1->OnWMEvent(&snap_left);
  const WindowSnapWMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  window_state1->OnWMEvent(&snap_right);
  AdvanceClock(base::Minutes(1));
  histogram_tester_.ExpectTotalCount(kSnapTwoWindowsDurationHistogramName, 0);

  // Snap `window1_` to the left, wait 30 seconds, then snap `window2_` to the
  // right. Test that it records in the 0 minute bucket.
  window_state1->OnWMEvent(&snap_left);
  AdvanceClock(base::Seconds(30));
  WindowState* window_state2 = WindowState::Get(window2_.get());
  window_state2->OnWMEvent(&snap_right);
  histogram_tester_.ExpectTimeBucketCount(kSnapTwoWindowsDurationHistogramName,
                                          base::Seconds(30), 1);

  // Snap `window2_` to the left, wait 3 minutes, then snap `window1_` to the
  // right. Test that it records in the 3 minute bucket.
  window_state2->OnWMEvent(&snap_left);
  AdvanceClock(base::Minutes(3));
  window_state1->OnWMEvent(&snap_right);
  histogram_tester_.ExpectTimeBucketCount(kSnapTwoWindowsDurationHistogramName,
                                          base::Minutes(3), 1);

  // Snap `window1_` to the left, wait 3 minutes, open a new `window3_` and
  // close it to simulate real user sessions with multiple windows, then snap
  // `window2_` to the right. Test that it increments the 3 minute bucket.
  window_state1->OnWMEvent(&snap_left);
  AdvanceClock(base::Minutes(3));
  window3_.reset();
  window_state2->OnWMEvent(&snap_right);
  histogram_tester_.ExpectTimeBucketCount(kSnapTwoWindowsDurationHistogramName,
                                          base::Minutes(3), 2);

  // Snap `window1_` to the right, wait 3 minutes, then minimize it. Test that
  // it records in the max bucket, since no other window was snapped.
  window_state1->OnWMEvent(&snap_right);
  AdvanceClock(base::Minutes(3));
  window_state1->Minimize();
  histogram_tester_.ExpectTimeBucketCount(kSnapTwoWindowsDurationHistogramName,
                                          kSequentialSnapActionMaxTime, 1);

  // Snap a new `window4_` to the left, wait 3 minutes, then close it. Test that
  // it records in the max bucket, since no other window was snapped.
  WindowState::Get(window4_.get())->OnWMEvent(&snap_left);
  AdvanceClock(base::Minutes(3));
  window4_.reset();
  histogram_tester_.ExpectTimeBucketCount(kSnapTwoWindowsDurationHistogramName,
                                          kSequentialSnapActionMaxTime, 2);

  // Snap `window1_` to the left, wait 3 minutes, move it to a new desk, move
  // `window2_` to the same desk. Test it doesn't record anything.
  window_state1->OnWMEvent(&snap_left);
  AdvanceClock(base::Minutes(3));
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  desks_controller->desks()[0]->MoveWindowToDesk(
      window1_.get(), desks_controller->desks()[1].get(),
      window1_->GetRootWindow(), /*unminimize=*/true);
  desks_controller->desks()[0]->MoveWindowToDesk(
      window2_.get(), desks_controller->desks()[1].get(),
      window2_->GetRootWindow(), /*unminimize=*/true);
  histogram_tester_.ExpectTotalCount(kSnapTwoWindowsDurationHistogramName, 5);
}

// Tests the metrics for the elapsed time between the first snapped window
// getting minimized and the second snapped window getting minimized.
TEST_F(SplitViewMetricsControllerTest, RecordMinimizeTwoWindowsDuration) {
  // Snap `window1_` and `window2_`.
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  WindowState* window_state1 = WindowState::Get(window1_.get());
  window_state1->OnWMEvent(&snap_left);
  const WindowSnapWMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  WindowState* window_state2 = WindowState::Get(window2_.get());
  window_state2->OnWMEvent(&snap_right);
  histogram_tester_.ExpectTotalCount(kSnapTwoWindowsDurationHistogramName, 1);

  // Minimize `window1_`, wait 3 minutes, then minimize `window2_`.
  window_state1->Minimize();
  AdvanceClock(base::Minutes(3));
  window_state2->Minimize();
  histogram_tester_.ExpectTimeBucketCount(
      kMinimizeTwoWindowsDurationHistogramName, base::Minutes(3), 1);

  // Minimize `window1_`, wait 1 minute, then maximize `window1_`. Test it
  // records in the maximum bucket.
  window_state1->Restore();
  window_state2->Restore();
  EXPECT_TRUE(window_state1->IsSnapped());
  EXPECT_TRUE(window_state2->IsSnapped());
  window_state1->Minimize();
  AdvanceClock(base::Minutes(3));
  window_state1->Maximize();
  histogram_tester_.ExpectTimeBucketCount(
      kMinimizeTwoWindowsDurationHistogramName, kSequentialSnapActionMaxTime,
      1);

  // Minimize `window1_`, wait 2 minutes, restore it to snapped state, then
  // minimize it again. Test we don't record anything.
  window_state1->OnWMEvent(&snap_left);
  window_state1->Minimize();
  AdvanceClock(base::Minutes(3));
  window_state1->Restore();
  EXPECT_TRUE(window_state1->IsSnapped());
  window_state1->Minimize();
  histogram_tester_.ExpectTotalCount(kMinimizeTwoWindowsDurationHistogramName,
                                     3);

  // Minimize `window2_`, wait 1 minute, then close `window2_`. Test it records
  // in the maximum bucket.
  window_state1->OnWMEvent(&snap_left);
  window_state2->OnWMEvent(&snap_right);
  EXPECT_TRUE(window_state1->IsSnapped());
  EXPECT_TRUE(window_state2->IsSnapped());
  window_state2->Minimize();
  AdvanceClock(base::Minutes(3));
  window2_.reset();
  histogram_tester_.ExpectTimeBucketCount(
      kMinimizeTwoWindowsDurationHistogramName, kSequentialSnapActionMaxTime,
      2);
  histogram_tester_.ExpectTotalCount(kMinimizeTwoWindowsDurationHistogramName,
                                     4);
}

// Tests that the metrics for recording the duration between closing a snapped
// window and closing another snapped window on the opposite side work
// correctly.
TEST_F(SplitViewMetricsControllerTest, CloseSnapTwoWindowsDuration) {
  // Snap `window1_` and `window2_`.
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  WindowState* window_state1 = WindowState::Get(window1_.get());
  window_state1->OnWMEvent(&snap_left);
  WindowState* window_state2 = WindowState::Get(window2_.get());
  const WindowSnapWMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  window_state2->OnWMEvent(&snap_right);

  // Close `window1_`, wait 3 minutes, then close `window2_`. Test it records in
  // the 3 minute bucket.
  window1_.reset();
  AdvanceClock(base::Minutes(3));
  window2_.reset();
  histogram_tester_.ExpectTimeBucketCount(kCloseTwoWindowsDurationHistogramName,
                                          base::Minutes(3), 1);

  // Snap `window3_` and `window4_`.
  WindowState* window_state3 = WindowState::Get(window3_.get());
  WindowState* window_state4 = WindowState::Get(window4_.get());
  window_state3->OnWMEvent(&snap_left);
  window_state4->OnWMEvent(&snap_right);

  // Close `window3_`, wait 5 minutes, then maximize `window4_`. Test it records
  // in the max bucket.
  window3_.reset();
  AdvanceClock(base::Minutes(5));
  window_state4->Maximize();
  histogram_tester_.ExpectTimeBucketCount(kCloseTwoWindowsDurationHistogramName,
                                          kSequentialSnapActionMaxTime, 1);
}

// Tests that snapping then toggling float between 2 windows doesn't crash
// (b/314823042).
TEST_F(SplitViewMetricsControllerTest, FloatToSnap) {
  // Float `window1`.
  WindowState* window_state1 = WindowState::Get(window1_.get());
  const WindowFloatWMEvent float_event(
      chromeos::FloatStartLocation::kBottomRight);
  window_state1->OnWMEvent(&float_event);

  // Snap then float `window2`.
  WindowState* window_state2 = WindowState::Get(window2_.get());
  const WindowSnapWMEvent snap_right(WM_EVENT_SNAP_SECONDARY,
                                     chromeos::kOneThirdSnapRatio);
  window_state2->OnWMEvent(&snap_right);
  window_state2->OnWMEvent(&float_event);

  // Snap then float `window1`.
  window_state1->OnWMEvent(&snap_right);
  window_state1->OnWMEvent(&float_event);
}

// Tests that we record the pref value whenever a window is snapped.
TEST_F(SplitViewMetricsControllerTest, SnapWindowSuggestions) {
  // The pref is enabled by default.
  PrefService* pref =
      Shell::Get()->session_controller()->GetActivePrefService();
  ASSERT_TRUE(pref->GetBoolean(prefs::kSnapWindowSuggestions));

  const auto snap_action_sources = {
      WindowSnapActionSource::kSnapByWindowLayoutMenu,
      WindowSnapActionSource::kDragWindowToEdgeToSnap,
      WindowSnapActionSource::kLongPressCaptionButtonToSnap,
      WindowSnapActionSource::kLacrosSnapButtonOrWindowLayoutMenu,
      WindowSnapActionSource::kKeyboardShortcutToSnap,
      WindowSnapActionSource::kSnapByWindowStateRestore,
      WindowSnapActionSource::kSnapByFullRestoreOrDeskTemplateOrSavedDesk,
  };
  for (const auto snap_action_source : snap_action_sources) {
    // Verify initial histogram values.
    std::string histogram_name(
        BuildSnapWindowSuggestionsHistogramName(snap_action_source));
    histogram_tester_.ExpectTotalCount(histogram_name,
                                       /*expected_count=*/0);

    // We only increment the histogram if the source can start split view.
    bool increment =
        CanSnapActionSourceStartFasterSplitView(snap_action_source);

    // Enable the pref. Test it increments the `true` bucket.
    pref->SetBoolean(prefs::kSnapWindowSuggestions, true);
    const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY,
                                      snap_action_source);
    WindowState* window_state1 = WindowState::Get(window1_.get());
    window_state1->OnWMEvent(&snap_left);
    histogram_tester_.ExpectBucketCount(histogram_name,
                                        /*sample=*/true,
                                        /*expected_count=*/increment ? 1 : 0);

    // Disable the pref. Test it increments the `false` bucket.
    pref->SetBoolean(prefs::kSnapWindowSuggestions, false);
    const WindowSnapWMEvent snap_right(WM_EVENT_SNAP_SECONDARY,
                                       snap_action_source);
    window_state1->OnWMEvent(&snap_right);
    histogram_tester_.ExpectBucketCount(histogram_name,
                                        /*sample=*/false,
                                        /*expected_count=*/increment ? 1 : 0);
  }
}

}  // namespace ash
