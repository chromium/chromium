// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/swipe_home_to_overview_controller.h"

#include <optional>
#include <tuple>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_metrics.h"
#include "ash/shelf/shelf_test_util.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ash {

namespace {

gfx::Rect GetShelfBounds() {
  return Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
}

gfx::RectF GetShelfBoundsInFloat() {
  return gfx::RectF(GetShelfBounds());
}

}  // namespace

class SwipeHomeToOverviewControllerTest : public AshTestBase {
 public:
  SwipeHomeToOverviewControllerTest() = default;

  SwipeHomeToOverviewControllerTest(const SwipeHomeToOverviewControllerTest&) =
      delete;
  SwipeHomeToOverviewControllerTest& operator=(
      const SwipeHomeToOverviewControllerTest&) = delete;

  ~SwipeHomeToOverviewControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay("1000x756");

    TabletModeControllerTestApi().EnterTabletMode();
    base::RunLoop().RunUntilIdle();

    // Advance tick clock by arbitrary non-zero amount.
    tick_clock_.Advance(base::Seconds(1000));
  }
  void TearDown() override {
    home_to_overview_controller_.reset();
    AshTestBase::TearDown();
  }

  void StartDrag() {
    home_to_overview_controller_ =
        std::make_unique<SwipeHomeToOverviewController>(
            GetPrimaryDisplay().id(), &tick_clock_);
  }

  void Drag(const gfx::PointF& location_in_screen,
            float scroll_x,
            float scroll_y) {
    home_to_overview_controller_->Drag(location_in_screen, scroll_x, scroll_y);
  }

  void EndDrag(const gfx::PointF& location_in_screen,
               std::optional<float> velocity_y) {
    home_to_overview_controller_->EndDrag(location_in_screen, velocity_y);
  }

  void CancelDrag() { home_to_overview_controller_->CancelDrag(); }

  AppListControllerImpl* app_list_controller() {
    return Shell::Get()->app_list_controller();
  }

  bool OverviewTransitionTimerRunning() const {
    return home_to_overview_controller_->overview_transition_timer_for_testing()
        ->IsRunning();
  }

  void FireOverviewTransitionTimer() {
    return home_to_overview_controller_->overview_transition_timer_for_testing()
        ->FireNow();
  }

  void WaitForHomeLauncherAnimationToFinish() {
    ui::LayerAnimationStoppedWaiter animation_waiter;
    ui::Layer* app_list_layer =
        GetAppListTestHelper()->GetAppListView()->GetWidget()->GetLayer();
    animation_waiter.Wait(app_list_layer);

    ui::Compositor* compositor = app_list_layer->GetCompositor();

    // Ensure there is one more frame presented after animation finishes
    // to allow animation throughput data is passed from cc to ui.
    std::ignore =
        ui::WaitForNextFrameToBePresented(compositor, base::Milliseconds(200));
  }

  void TapOnHomeLauncherSearchBox() {
    GetEventGenerator()->GestureTapAt(GetAppListTestHelper()
                                          ->GetAppListView()
                                          ->search_box_view()
                                          ->GetBoundsInScreen()
                                          .CenterPoint());
  }

  base::TimeTicks GetTimerDesiredRunTime() const {
    return home_to_overview_controller_->overview_transition_timer_for_testing()
        ->desired_run_time();
  }

  bool OverviewStarted() const {
    return Shell::Get()->overview_controller()->InOverviewSession();
  }

 protected:
  base::SimpleTestTickClock tick_clock_;

 private:
  std::unique_ptr<SwipeHomeToOverviewController> home_to_overview_controller_;
};

// Verify that the metrics of home launcher animation are recorded correctly
// when entering/exiting overview mode.
TEST_F(SwipeHomeToOverviewControllerTest, VerifyHomeLauncherMetrics) {
  // Set non-zero animation duration to report animation metrics.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  base::HistogramTester histogram_tester;

  // Enter overview mode by gesture swipe on shelf.
  {
    const gfx::Point gesture_start_point =
        Shelf::ForWindow(Shell::GetPrimaryRootWindow())
            ->GetShelfViewForTesting()
            ->GetBoundsInScreen()
            .CenterPoint();

    // Calculate the suitable gesture end location to trigger the overview mode
    // through the gesture scroll.
    // Note that we cannot access `SwipeHomeToOverviewController`'s non-static
    // members here since the class instance has not be created yet.
    const int extra_distance = 15;
    const gfx::Point gesture_end_point(
        gesture_start_point.x(),
        GetPrimaryDisplay().bounds().bottom() -
            ShelfConfig::Get()->shelf_size() -
            SwipeHomeToOverviewController::
                kVerticalThresholdForOverviewTransition -
            extra_distance);

    // Scroll should be slow enough to trigger the overview mode.
    constexpr int steps = 12;
    int update_count = 0;
    GetEventGenerator()->GestureScrollSequenceWithCallback(
        gesture_start_point, gesture_end_point, base::Milliseconds(100),
        /*steps=*/steps,
        base::BindRepeating(
            [](int* update_count, ui::EventType event_type,
               const gfx::Vector2dF& delta) {
              if (event_type != ui::EventType::kGestureScrollUpdate) {
                return;
              }

              *update_count = *update_count + 1;
              if (*update_count == steps) {
                // Wait until overview animation finishes. If the gesture scroll
                // ends too early, we may not be able to enter the overview mode
                WaitForOverviewAnimation(/*enter=*/true);
              }
            },
            &update_count));
  }

  // Collect metrics data. Verify that the animation to hide the home launcher
  // is recorded.
  WaitForHomeLauncherAnimationToFinish();
  histogram_tester.ExpectTotalCount(
      "Apps.HomeLauncherTransition.AnimationSmoothness.FadeInOverview", 1);
  histogram_tester.ExpectTotalCount(
      "Apps.HomeLauncherTransition.AnimationSmoothness.FadeOutOverview", 0);

  // Exit overview mode by gesture tap.
  GetEventGenerator()->GestureTapAt(
      GetContext()->GetBoundsInScreen().top_center());

  // Wait until overview animation finishes.
  WaitForOverviewAnimation(/*enter=*/false);

  // Verify that the animation to show the home launcher is recorded.
  WaitForHomeLauncherAnimationToFinish();
  histogram_tester.ExpectTotalCount(
      "Apps.HomeLauncherTransition.AnimationSmoothness.FadeInOverview", 1);
  histogram_tester.ExpectTotalCount(
      "Apps.HomeLauncherTransition.AnimationSmoothness.FadeOutOverview", 1);
}

TEST_F(SwipeHomeToOverviewControllerTest, BasicFlow) {
  const gfx::RectF shelf_bounds = GetShelfBoundsInFloat();

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kEnterOverviewHistogramName, EnterOverviewFromHomeLauncher::kOverview, 0);

  StartDrag();
  // Drag to a point within shelf bounds - verify that app list has not been
  // scaled, and the transition to overview transition timer has not started.
  Drag(shelf_bounds.CenterPoint(), 0.f, 1.f);

  aura::Window* home_screen_window =
      app_list_controller()->GetHomeScreenWindow();
  ASSERT_TRUE(home_screen_window);

  EXPECT_EQ(gfx::Transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_FALSE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());
  histogram_tester.ExpectBucketCount(
      kEnterOverviewHistogramName, EnterOverviewFromHomeLauncher::kOverview, 0);

  const int transition_threshold =
      SwipeHomeToOverviewController::kVerticalThresholdForOverviewTransition;

  // Move above the shelf but not far enough to trigger transition to overview.
  // The home window is expected to be scaled at this point, but the overview
  // transition timer to should not yet be running.
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold / 2),
       0.f, 1.f);

  EXPECT_EQ(home_screen_window->transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_TRUE(home_screen_window->transform().IsScaleOrTranslation());
  EXPECT_FALSE(home_screen_window->transform().IsIdentityOrTranslation());
  EXPECT_EQ(1.f, home_screen_window->layer()->opacity());
  histogram_tester.ExpectBucketCount(
      kEnterOverviewHistogramName, EnterOverviewFromHomeLauncher::kOverview, 0);

  // Move above the transition threshold - verify the overview transition timer
  // has started.
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold + 10),
       0.f, 1.f);

  EXPECT_EQ(home_screen_window->transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_TRUE(home_screen_window->transform().IsScaleOrTranslation());
  EXPECT_FALSE(home_screen_window->transform().IsIdentityOrTranslation());

  EXPECT_TRUE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());
  histogram_tester.ExpectBucketCount(
      kEnterOverviewHistogramName, EnterOverviewFromHomeLauncher::kOverview, 0);

  // Fire overview transition timer, and verify the overview has started.
  FireOverviewTransitionTimer();

  EXPECT_TRUE(OverviewStarted());
  histogram_tester.ExpectBucketCount(
      kEnterOverviewHistogramName, EnterOverviewFromHomeLauncher::kOverview, 1);

  // Home screen is still scaled down, and not visible.
  EXPECT_EQ(home_screen_window->transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_TRUE(home_screen_window->transform().IsScaleOrTranslation());
  EXPECT_FALSE(home_screen_window->transform().IsIdentityOrTranslation());
  EXPECT_EQ(0.f, home_screen_window->layer()->opacity());

  // The user ending drag after this point should be no-op.
  EndDrag(
      shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold + 10),
      1.f);

  EXPECT_TRUE(OverviewStarted());
  histogram_tester.ExpectBucketCount(
      kEnterOverviewHistogramName, EnterOverviewFromHomeLauncher::kOverview, 1);

  // Home screen is still scaled down, and not visible.
  EXPECT_EQ(home_screen_window->transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_TRUE(home_screen_window->transform().IsScaleOrTranslation());
  EXPECT_FALSE(home_screen_window->transform().IsIdentityOrTranslation());
  EXPECT_EQ(0.f, home_screen_window->layer()->opacity());
}

TEST_F(SwipeHomeToOverviewControllerTest, EndDragBeforeTimeout) {
  const gfx::RectF shelf_bounds = GetShelfBoundsInFloat();

  StartDrag();

  aura::Window* home_screen_window =
      app_list_controller()->GetHomeScreenWindow();
  ASSERT_TRUE(home_screen_window);

  const int transition_threshold =
      SwipeHomeToOverviewController::kVerticalThresholdForOverviewTransition;

  // Move above the transition threshold - verify the overview transition timer
  // has started.
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold / 2),
       0.f, 1.f);
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold + 10),
       0.f, 1.f);

  EXPECT_EQ(home_screen_window->transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_TRUE(home_screen_window->transform().IsScaleOrTranslation());
  EXPECT_FALSE(home_screen_window->transform().IsIdentityOrTranslation());

  EXPECT_TRUE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());

  // The user ending drag should reset the home view state.
  EndDrag(
      shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold + 10),
      1.f);

  EXPECT_EQ(home_screen_window->transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_EQ(gfx::Transform(), home_screen_window->transform());
  EXPECT_EQ(1.f, home_screen_window->layer()->opacity());

  EXPECT_FALSE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());
}

TEST_F(SwipeHomeToOverviewControllerTest, GoBackOnHomeLauncher) {
  // Show home screen search results page.
  GetAppListTestHelper()->CheckVisibility(true);
  TapOnHomeLauncherSearchBox();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  const gfx::RectF shelf_bounds = GetShelfBoundsInFloat();

  StartDrag();

  aura::Window* home_screen_window =
      app_list_controller()->GetHomeScreenWindow();
  ASSERT_TRUE(home_screen_window);

  const int transition_threshold =
      SwipeHomeToOverviewController::kVerticalThresholdForOverviewTransition;

  // Move above the transition threshold - verify the overview transition timer
  // has started.
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold / 2),
       0.f, 1.f);
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold + 10),
       0.f, 1.f);

  EXPECT_EQ(home_screen_window->transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_TRUE(home_screen_window->transform().IsScaleOrTranslation());
  EXPECT_FALSE(home_screen_window->transform().IsIdentityOrTranslation());

  EXPECT_TRUE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());

  // The user ending drag with a fling should move home to the initial state -
  // fullscreen all apps.
  EndDrag(
      shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold + 10),
      -1500.f);

  EXPECT_EQ(home_screen_window->transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_EQ(gfx::Transform(), home_screen_window->transform());
  EXPECT_EQ(1.f, home_screen_window->layer()->opacity());

  EXPECT_FALSE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

TEST_F(SwipeHomeToOverviewControllerTest, FlingOnAppsPage) {
  // Show home screen search results page.
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  const gfx::RectF shelf_bounds = GetShelfBoundsInFloat();

  StartDrag();

  aura::Window* home_screen_window =
      app_list_controller()->GetHomeScreenWindow();
  ASSERT_TRUE(home_screen_window);

  const int transition_threshold =
      SwipeHomeToOverviewController::kVerticalThresholdForOverviewTransition;

  // Move above the transition threshold - verify the overview transition timer
  // has started.
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold / 2),
       0.f, 1.f);
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold + 10),
       0.f, 1.f);

  EXPECT_EQ(home_screen_window->transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_TRUE(home_screen_window->transform().IsScaleOrTranslation());
  EXPECT_FALSE(home_screen_window->transform().IsIdentityOrTranslation());

  EXPECT_TRUE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());

  // The user ending drag with a fling should move home to the initial state -
  // fullscreen all apps.
  EndDrag(
      shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold + 10),
      -1500.f);

  EXPECT_EQ(home_screen_window->transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_EQ(gfx::Transform(), home_screen_window->transform());
  EXPECT_EQ(1.f, home_screen_window->layer()->opacity());

  EXPECT_FALSE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());

  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

TEST_F(SwipeHomeToOverviewControllerTest, CancelDragBeforeTimeout) {
  // Show home screen search results page.
  GetAppListTestHelper()->CheckVisibility(true);
  TapOnHomeLauncherSearchBox();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  const gfx::RectF shelf_bounds = GetShelfBoundsInFloat();

  StartDrag();

  aura::Window* home_screen_window =
      app_list_controller()->GetHomeScreenWindow();
  ASSERT_TRUE(home_screen_window);

  const int transition_threshold =
      SwipeHomeToOverviewController::kVerticalThresholdForOverviewTransition;

  // Move above the transition threshold - verify the overview transition timer
  // has started.
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold / 2),
       0.f, 1.f);
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold + 10),
       0.f, 1.f);

  EXPECT_EQ(home_screen_window->transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_TRUE(home_screen_window->transform().IsScaleOrTranslation());
  EXPECT_FALSE(home_screen_window->transform().IsIdentityOrTranslation());

  EXPECT_TRUE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());

  // Drag gesture getting canceled should reset the home view state.
  CancelDrag();

  EXPECT_EQ(home_screen_window->transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_EQ(gfx::Transform(), home_screen_window->transform());
  EXPECT_EQ(1.f, home_screen_window->layer()->opacity());

  EXPECT_FALSE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());

  // The gesture was not a fling - the home screen should have stayed in the
  // fullscreen search state.
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);
}

TEST_F(SwipeHomeToOverviewControllerTest, DragMovementRestartsTimeout) {
  const gfx::RectF shelf_bounds = GetShelfBoundsInFloat();

  StartDrag();

  const int transition_threshold =
      SwipeHomeToOverviewController::kVerticalThresholdForOverviewTransition;

  // Move above the transition threshold - verify the overview transition timer
  // has started.
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold / 2),
       0.f, 1.f);
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold), 0.f,
       1.f);

  EXPECT_TRUE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());

  const base::TimeDelta delay =
      GetTimerDesiredRunTime() - tick_clock_.NowTicks();
  EXPECT_GT(delay, base::TimeDelta());

  const float max_allowed_velocity =
      SwipeHomeToOverviewController::kMovementVelocityThreshold;
  // Advance clock, and simulate another drag whose speed is above the max
  // allowed.
  tick_clock_.Advance(base::Milliseconds(1));
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, 2 * transition_threshold),
       0.f, max_allowed_velocity + 10);

  // Verify the timer was stopped.
  EXPECT_FALSE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());

  tick_clock_.Advance(base::Milliseconds(1));

  // Another slow drag should restart the timer.
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, 2 * transition_threshold),
       0.f, max_allowed_velocity / 2);

  EXPECT_TRUE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());

  EXPECT_EQ(delay, GetTimerDesiredRunTime() - tick_clock_.NowTicks());
}

TEST_F(SwipeHomeToOverviewControllerTest,
       SmallDragMovementDoesNotRestartTimeout) {
  const gfx::RectF shelf_bounds = GetShelfBoundsInFloat();

  StartDrag();

  const int transition_threshold =
      SwipeHomeToOverviewController::kVerticalThresholdForOverviewTransition;

  // Move just below the transition threshold - verify overview transition timer
  // has not started.
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold - 1),
       0.f, 1.f);

  EXPECT_FALSE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());

  // Move a little to reach the transition threshold - the timer should start at
  // this point.
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold), 0.f,
       1.f);

  EXPECT_TRUE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());

  const base::TimeDelta delay =
      GetTimerDesiredRunTime() - tick_clock_.NowTicks();
  EXPECT_GT(delay, base::TimeDelta());

  const float movement_threshold =
      SwipeHomeToOverviewController::kMovementVelocityThreshold;

  // Advance clock, and simulate another drag, for an amount below the movement
  // threshold.
  tick_clock_.Advance(base::Milliseconds(1));
  Drag(shelf_bounds.top_center() -
           gfx::Vector2d(0, transition_threshold + movement_threshold - 1),
       0.f, movement_threshold / 2);

  // Verify the expected timer run time was not updated.
  EXPECT_TRUE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());

  EXPECT_EQ(delay - base::Milliseconds(1),
            GetTimerDesiredRunTime() - tick_clock_.NowTicks());

  // Movement with velocity above the allowed threshold restarts the timer.
  Drag(shelf_bounds.top_center() -
           gfx::Vector2d(0, transition_threshold + movement_threshold - 1),
       0.f, movement_threshold + 1);

  EXPECT_FALSE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());
}

TEST_F(SwipeHomeToOverviewControllerTest, DragBellowThresholdStopsTimer) {
  const gfx::RectF shelf_bounds = GetShelfBoundsInFloat();

  StartDrag();
  Drag(shelf_bounds.CenterPoint(), 0.f, 1.f);

  const int transition_threshold =
      SwipeHomeToOverviewController::kVerticalThresholdForOverviewTransition;

  // Move above the transition threshold - verify the overview transition timer
  // has started.
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold / 2),
       0.f, 1.f);
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold + 10),
       0.f, 1.f);

  aura::Window* home_screen_window =
      app_list_controller()->GetHomeScreenWindow();
  ASSERT_TRUE(home_screen_window);

  EXPECT_EQ(home_screen_window->transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_TRUE(home_screen_window->transform().IsScaleOrTranslation());
  EXPECT_FALSE(home_screen_window->transform().IsIdentityOrTranslation());

  EXPECT_TRUE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());

  // Move bellow threshold, verify the timer has stopped.
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold - 10),
       0.f, 1.f);

  EXPECT_EQ(home_screen_window->transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_TRUE(home_screen_window->transform().IsScaleOrTranslation());
  EXPECT_FALSE(home_screen_window->transform().IsIdentityOrTranslation());

  EXPECT_FALSE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());

  // Move further down, under the shelf.
  Drag(shelf_bounds.CenterPoint(), 0.f, 1.f);

  EXPECT_EQ(home_screen_window->transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_EQ(gfx::Transform(), home_screen_window->transform());
  EXPECT_FALSE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());

  // Move above the transition threshold again, the timer should be restarted..
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold + 10),
       0.f, 1.f);

  EXPECT_EQ(home_screen_window->transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_TRUE(home_screen_window->transform().IsScaleOrTranslation());
  EXPECT_FALSE(home_screen_window->transform().IsIdentityOrTranslation());

  EXPECT_TRUE(OverviewTransitionTimerRunning());
  EXPECT_FALSE(OverviewStarted());

  // Fire overview transition timer, and verify the overview has started.
  FireOverviewTransitionTimer();

  EXPECT_FALSE(OverviewTransitionTimerRunning());
  EXPECT_TRUE(OverviewStarted());

  // Home screen is still scaled down, and not visible.
  EXPECT_EQ(home_screen_window->transform(),
            home_screen_window->layer()->GetTargetTransform());
  EXPECT_TRUE(home_screen_window->transform().IsScaleOrTranslation());
  EXPECT_FALSE(home_screen_window->transform().IsIdentityOrTranslation());
  EXPECT_EQ(0.f, home_screen_window->layer()->opacity());
}

TEST_F(SwipeHomeToOverviewControllerTest, ScaleChangesDuringDrag) {
  const gfx::RectF shelf_bounds = GetShelfBoundsInFloat();

  StartDrag();
  Drag(shelf_bounds.CenterPoint(), 0.f, 1.f);

  aura::Window* home_screen_window =
      app_list_controller()->GetHomeScreenWindow();
  ASSERT_TRUE(home_screen_window);
  const gfx::RectF original_home_bounds(home_screen_window->bounds());

  const int transition_threshold =
      SwipeHomeToOverviewController::kVerticalThresholdForOverviewTransition;
  // Moving up should shrink home bounds.
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold - 50),
       0.f, 1.f);

  gfx::RectF last_home_bounds =
      home_screen_window->transform().MapRect(original_home_bounds);
  EXPECT_GT(original_home_bounds.width(), last_home_bounds.width());

  // Moving up should shrink home bounds further.
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold + 10),
       0.f, 1.f);

  gfx::RectF current_home_bounds =
      home_screen_window->transform().MapRect(original_home_bounds);
  EXPECT_GT(last_home_bounds.width(), current_home_bounds.width());
  last_home_bounds = current_home_bounds;

  // Moving down should expand bounds.
  Drag(shelf_bounds.top_center() - gfx::Vector2d(0, transition_threshold - 40),
       0.f, 1.f);

  current_home_bounds =
      home_screen_window->transform().MapRect(original_home_bounds);
  EXPECT_LT(last_home_bounds.width(), current_home_bounds.width());
  last_home_bounds = current_home_bounds;

  // Horizontal movement should not change bounds.
  Drag(shelf_bounds.top_center() - gfx::Vector2d(50, transition_threshold - 40),
       1.f, 0.f);
  current_home_bounds =
      home_screen_window->transform().MapRect(original_home_bounds);
  EXPECT_EQ(last_home_bounds, current_home_bounds);

  // At shelf top the home window should have no transform.
  Drag(shelf_bounds.top_center(), 0.f, 1.f);
  EXPECT_EQ(gfx::Transform(), home_screen_window->transform());
}

}  // namespace ash
