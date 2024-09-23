// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/home_to_overview_nudge_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/controls/contextual_nudge.h"
#include "ash/controls/contextual_tooltip.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/swipe_home_to_overview_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class WidgetCloseObserver {
 public:
  explicit WidgetCloseObserver(views::Widget* widget)
      : widget_(widget->GetWeakPtr()) {}

  ~WidgetCloseObserver() = default;

  bool WidgetClosed() const { return !widget_ || widget_->IsClosed(); }

 private:
  base::WeakPtr<views::Widget> widget_;
};

class HomeToOverviewNudgeControllerWithNudgesDisabledTest : public AshTestBase {
 public:
  HomeToOverviewNudgeControllerWithNudgesDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kHideShelfControlsInTabletMode);
  }
  ~HomeToOverviewNudgeControllerWithNudgesDisabledTest() override = default;

  HomeToOverviewNudgeControllerWithNudgesDisabledTest(
      const HomeToOverviewNudgeControllerWithNudgesDisabledTest& other) =
      delete;
  HomeToOverviewNudgeControllerWithNudgesDisabledTest& operator=(
      const HomeToOverviewNudgeControllerWithNudgesDisabledTest& other) =
      delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class HomeToOverviewNudgeControllerTest : public AshTestBase {
 public:
  HomeToOverviewNudgeControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kHideShelfControlsInTabletMode);
  }
  ~HomeToOverviewNudgeControllerTest() override = default;

  HomeToOverviewNudgeControllerTest(
      const HomeToOverviewNudgeControllerTest& other) = delete;
  HomeToOverviewNudgeControllerTest& operator=(
      const HomeToOverviewNudgeControllerTest& other) = delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);
    test_clock_.Advance(base::Hours(2));
    contextual_tooltip::OverrideClockForTesting(&test_clock_);
  }
  void TearDown() override {
    contextual_tooltip::ClearClockOverrideForTesting();
    AshTestBase::TearDown();
  }

  HomeToOverviewNudgeController* GetNudgeController() {
    return GetPrimaryShelf()
        ->shelf_layout_manager()
        ->home_to_overview_nudge_controller_for_testing();
  }

  views::Widget* GetNudgeWidget() {
    if (!GetNudgeController()->nudge_for_testing())
      return nullptr;
    return GetNudgeController()->nudge_for_testing()->GetWidget();
  }

  HotseatWidget* GetHotseatWidget() {
    return GetPrimaryShelf()->shelf_widget()->hotseat_widget();
  }

  // Helper that creates and minimzes |count| number of windows.
  using ScopedWindowList = std::vector<std::unique_ptr<aura::Window>>;
  ScopedWindowList CreateAndMinimizeWindows(int count) {
    ScopedWindowList windows;

    for (int i = 0; i < count; ++i) {
      std::unique_ptr<aura::Window> window =
          CreateTestWindow(gfx::Rect(0, 0, 400, 400));
      WindowState::Get(window.get())->Minimize();
      windows.push_back(std::move(window));
    }

    return windows;
  }

  void SanityCheckNudgeBounds() {
    views::Widget* const nudge_widget = GetNudgeWidget();
    ASSERT_TRUE(nudge_widget);
    EXPECT_TRUE(nudge_widget->IsVisible());

    const gfx::Rect nudge_bounds =
        nudge_widget->GetLayer()->transform().MapRect(
            nudge_widget->GetNativeWindow()->GetTargetBounds());

    HotseatWidget* const hotseat = GetHotseatWidget();
    const gfx::Rect hotseat_bounds =
        hotseat->GetLayerForNudgeAnimation()->transform().MapRect(
            hotseat->GetNativeWindow()->GetTargetBounds());

    // Nudge and hotseat should have the same transform.
    EXPECT_EQ(hotseat->GetLayerForNudgeAnimation()->transform(),
              nudge_widget->GetLayer()->transform());

    // Nudge should be under the hotseat.
    EXPECT_LE(hotseat_bounds.bottom(), nudge_bounds.y());

    const gfx::Rect display_bounds = GetPrimaryDisplay().bounds();
    EXPECT_TRUE(display_bounds.Contains(nudge_bounds))
        << display_bounds.ToString() << " contains " << nudge_bounds.ToString();

    // Verify that the nudge is centered within the display bounds.
    EXPECT_LE((nudge_bounds.x() - display_bounds.x()) -
                  (display_bounds.right() - nudge_bounds.right()),
              1)
        << nudge_bounds.ToString() << " within " << display_bounds.ToString();

    // Verify that the nudge label is visible
    EXPECT_EQ(
        1.0f,
        GetNudgeController()->nudge_for_testing()->label()->layer()->opacity());
  }

  base::SimpleTestClock test_clock_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class HomeToOverviewNudgeControllerTestWithA11yPrefs
    : public HomeToOverviewNudgeControllerTest,
      public ::testing::WithParamInterface<std::string> {};

INSTANTIATE_TEST_SUITE_P(
    all,
    HomeToOverviewNudgeControllerTestWithA11yPrefs,
    testing::Values(prefs::kAccessibilityAutoclickEnabled,
                    prefs::kAccessibilitySpokenFeedbackEnabled,
                    prefs::kAccessibilitySwitchAccessEnabled));

// Tests that home to overview gesture nudge is not shown if contextual nudges
// are disabled.
TEST_F(HomeToOverviewNudgeControllerWithNudgesDisabledTest,
       NoNudgeOnHomeScreen) {
  EXPECT_FALSE(GetPrimaryShelf()
                   ->shelf_layout_manager()
                   ->home_to_overview_nudge_controller_for_testing());
  TabletModeControllerTestApi().EnterTabletMode();
  CreateUserSessions(1);

  std::unique_ptr<aura::Window> window_1 =
      CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  WindowState::Get(window_1.get())->Minimize();
  std::unique_ptr<aura::Window> window_2 =
      CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  WindowState::Get(window_2.get())->Minimize();

  EXPECT_FALSE(GetPrimaryShelf()
                   ->shelf_layout_manager()
                   ->home_to_overview_nudge_controller_for_testing());
}

// Tests that home to overview nudge is not shown before user logs in.
TEST_F(HomeToOverviewNudgeControllerTest, NoNudgeBeforeLogin) {
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_FALSE(GetNudgeController());

  CreateUserSessions(1);
  EXPECT_TRUE(GetNudgeController());
}

// Test the flow for showing the home to overview gesture nudge - when shown the
// first time, nudge should remain visible until the hotseat state changes. On
// subsequent shows, the nudge should be hidden after a timeout.
TEST_F(HomeToOverviewNudgeControllerTest, ShownOnHomeScreen) {
  base::HistogramTester histogram_tester;
  CreateUserSessions(1);

  // The nudge should not be shown in clamshell.
  EXPECT_FALSE(GetNudgeController());

  // In tablet mode, the nudge should be shown after at least 2 windows are
  // minimized.
  TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(GetNudgeController());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());

  EXPECT_FALSE(GetNudgeController()->HasShowTimerForTesting());

  ScopedWindowList window_1 = CreateAndMinimizeWindows(1);
  EXPECT_FALSE(GetNudgeController()->HasShowTimerForTesting());

  ScopedWindowList window_2 = CreateAndMinimizeWindows(1);
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
  GetNudgeController()->FireShowTimerForTesting();

  ASSERT_TRUE(GetNudgeController()->nudge_for_testing());
  {
    SCOPED_TRACE("First nudge");
    SanityCheckNudgeBounds();
  }
  EXPECT_FALSE(GetNudgeController()->HasHideTimerForTesting());

  // Transitioning to overview should hide the nudge.
  EnterOverview();

  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());

  // Ending overview, and transitioning to the home screen again should not show
  // the nudge.
  ExitOverview();
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_EQ(gfx::Transform(),
            GetHotseatWidget()->GetLayerForNudgeAnimation()->transform());

  // Advance time for more than a day (which should enable the nudge again).
  test_clock_.Advance(base::Hours(25));

  // The nudge should not show up unless the user actually transitions to home.
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());

  // Create and minimize another test window to force a transition to home.
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  WindowState::Get(window.get())->Minimize();

  // Nudge should be shown again.
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
  GetNudgeController()->FireShowTimerForTesting();
  ASSERT_TRUE(GetNudgeController()->nudge_for_testing());
  {
    SCOPED_TRACE("Second nudge");
    SanityCheckNudgeBounds();
  }

  // The second time, the nudge should be hidden after a timeout.
  ASSERT_TRUE(GetNudgeController()->HasHideTimerForTesting());
  GetNudgeController()->FireHideTimerForTesting();
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_EQ(gfx::Transform(),
            GetHotseatWidget()->GetLayerForNudgeAnimation()->transform());
}

// Tests that the nudge eventually stops showing.
TEST_F(HomeToOverviewNudgeControllerTest, ShownLimitedNumberOfTimes) {
  TabletModeControllerTestApi().EnterTabletMode();
  CreateUserSessions(1);
  ScopedWindowList extra_windows = CreateAndMinimizeWindows(2);
  ASSERT_TRUE(GetNudgeController());

  // Show the nudge kNotificationLimit amount of time.
  for (int i = 0; i < contextual_tooltip::kNotificationLimit; ++i) {
    SCOPED_TRACE(testing::Message() << "Attempt " << i);
    EXPECT_FALSE(GetNudgeController()->nudge_for_testing());

    ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
    GetNudgeController()->FireShowTimerForTesting();
    ASSERT_TRUE(GetNudgeController()->nudge_for_testing());

    std::unique_ptr<aura::Window> window =
        CreateTestWindow(gfx::Rect(0, 0, 400, 400));
    wm::ActivateWindow(window.get());
    test_clock_.Advance(base::Hours(25));
    WindowState::Get(window.get())->Minimize();
  }

  // At this point, given the nudge was shown the intended number of times
  // already, the nudge should not show up, even though the device is on home
  // screen.
  EXPECT_FALSE(GetNudgeController()->HasShowTimerForTesting());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
}

// Tests that the nudge is hidden when tablet mode exits.
TEST_F(HomeToOverviewNudgeControllerTest, HiddenOnTabletModeExit) {
  base::HistogramTester histogram_tester;
  TabletModeControllerTestApi().EnterTabletMode();
  CreateUserSessions(1);
  ScopedWindowList extra_windows = CreateAndMinimizeWindows(2);

  ASSERT_TRUE(GetNudgeController());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
  GetNudgeController()->FireShowTimerForTesting();

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_EQ(
      gfx::Transform(),
      GetHotseatWidget()->GetLayerForNudgeAnimation()->GetTargetTransform());
}

// Tests that the nudge show is canceled when tablet mode exits.
TEST_F(HomeToOverviewNudgeControllerTest, ShowCanceledOnTabletModeExit) {
  TabletModeControllerTestApi().EnterTabletMode();
  CreateUserSessions(1);
  ScopedWindowList extra_windows = CreateAndMinimizeWindows(2);

  ASSERT_TRUE(GetNudgeController());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(GetNudgeController()->HasShowTimerForTesting());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_EQ(
      gfx::Transform(),
      GetHotseatWidget()->GetLayerForNudgeAnimation()->GetTargetTransform());
}

// Tests that the nudge show animation is canceled when tablet mode exits.
TEST_F(HomeToOverviewNudgeControllerTest,
       ShowAnimationCanceledOnTabletModeExit) {
  TabletModeControllerTestApi().EnterTabletMode();
  CreateUserSessions(1);
  ScopedWindowList extra_windows = CreateAndMinimizeWindows(2);

  ASSERT_TRUE(GetNudgeController());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  GetNudgeController()->FireShowTimerForTesting();
  ASSERT_TRUE(GetNudgeWidget()->GetLayer()->GetAnimator()->is_animating());

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(GetNudgeController()->HasShowTimerForTesting());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_EQ(
      gfx::Transform(),
      GetHotseatWidget()->GetLayerForNudgeAnimation()->GetTargetTransform());
}

// Tests that the nudge is hidden when the screen is locked.
TEST_F(HomeToOverviewNudgeControllerTest, HiddenOnScreenLock) {
  TabletModeControllerTestApi().EnterTabletMode();
  CreateUserSessions(1);
  ScopedWindowList extra_windows = CreateAndMinimizeWindows(2);

  ASSERT_TRUE(GetNudgeController());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
  GetNudgeController()->FireShowTimerForTesting();

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_EQ(
      gfx::Transform(),
      GetHotseatWidget()->GetLayerForNudgeAnimation()->GetTargetTransform());

  // Nudge should not be shown if a window is shown and hidden behind a lock
  // screen.
  test_clock_.Advance(base::Hours(25));
  ScopedWindowList locked_session_window = CreateAndMinimizeWindows(1);
  EXPECT_FALSE(GetNudgeController()->HasShowTimerForTesting());
}

// Tests that the nudge show is canceled if the in-app shelf is shown before the
// show timer runs.
TEST_F(HomeToOverviewNudgeControllerTest, InAppShelfShownBeforeShowTimer) {
  TabletModeControllerTestApi().EnterTabletMode();
  CreateUserSessions(1);
  ScopedWindowList extra_windows = CreateAndMinimizeWindows(2);

  ASSERT_TRUE(GetNudgeController());

  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_TRUE(GetNudgeController()->HasShowTimerForTesting());

  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_FALSE(GetNudgeController()->HasShowTimerForTesting());

  // When the home screen is shown the next time, the nudge should be shown
  // again, without timeout to hide it.
  WindowState::Get(window.get())->Minimize();
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
  GetNudgeController()->FireShowTimerForTesting();
  EXPECT_TRUE(GetNudgeController()->nudge_for_testing());

  EXPECT_FALSE(GetNudgeController()->HasHideTimerForTesting());
}

// Tests that in-app shelf will hide the nudge if it happens during the
// animation to show the nudge.
TEST_F(HomeToOverviewNudgeControllerTest, NudgeHiddenDuringShowAnimation) {
  TabletModeControllerTestApi().EnterTabletMode();
  CreateUserSessions(1);
  ScopedWindowList extra_windows = CreateAndMinimizeWindows(2);

  ASSERT_TRUE(GetNudgeController());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  GetNudgeController()->FireShowTimerForTesting();
  ASSERT_TRUE(GetNudgeWidget()->GetLayer()->GetAnimator()->is_animating());

  // Cache the widget, as GetNudgeWidget() will start returning nullptr when the
  // nudge starts hiding.
  ContextualNudge* nudge = GetNudgeController()->nudge_for_testing();
  views::Widget* nudge_widget = nudge->GetWidget();
  WidgetCloseObserver widget_close_observer(nudge_widget);

  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  EXPECT_FALSE(GetNudgeWidget());
  EXPECT_FALSE(nudge_widget->IsVisible());
  EXPECT_EQ(
      gfx::Transform(),
      GetHotseatWidget()->GetLayerForNudgeAnimation()->GetTargetTransform());

  EXPECT_TRUE(widget_close_observer.WidgetClosed());

  EXPECT_TRUE(GetHotseatWidget()
                  ->GetLayerForNudgeAnimation()
                  ->GetAnimator()
                  ->is_animating());
  EXPECT_EQ(
      gfx::Transform(),
      GetHotseatWidget()->GetLayerForNudgeAnimation()->GetTargetTransform());

  // When the nudge is shown again, it should be hidden after a timeout.
  test_clock_.Advance(base::Hours(25));
  WindowState::Get(window.get())->Minimize();
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
  GetNudgeController()->FireShowTimerForTesting();
  EXPECT_TRUE(GetNudgeController()->nudge_for_testing());

  EXPECT_TRUE(GetNudgeController()->HasHideTimerForTesting());
}

// Tests that there is no crash if the nudge widget gets closed unexpectedly.
TEST_F(HomeToOverviewNudgeControllerTest, NoCrashIfNudgeWidgetGetsClosed) {
  TabletModeControllerTestApi().EnterTabletMode();
  CreateUserSessions(1);
  ScopedWindowList windows = CreateAndMinimizeWindows(2);

  ASSERT_TRUE(GetNudgeController());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());

  GetNudgeController()->FireShowTimerForTesting();
  GetNudgeWidget()->CloseNow();
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());

  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
}

// Tests that tapping on the nudge hides the nudge.
TEST_F(HomeToOverviewNudgeControllerTest, TapOnTheNudgeClosesTheNudge) {
  base::HistogramTester histogram_tester;
  TabletModeControllerTestApi().EnterTabletMode();
  CreateUserSessions(1);
  ScopedWindowList windows = CreateAndMinimizeWindows(2);

  ASSERT_TRUE(GetNudgeController());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());

  GetNudgeController()->FireShowTimerForTesting();

  ASSERT_TRUE(GetNudgeController()->nudge_for_testing());
  views::Widget* nudge_widget = GetNudgeWidget();
  WidgetCloseObserver widget_close_observer(nudge_widget);

  GetEventGenerator()->GestureTapAt(
      nudge_widget->GetWindowBoundsInScreen().CenterPoint());

  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_TRUE(widget_close_observer.WidgetClosed());

  EXPECT_EQ(
      gfx::Transform(),
      GetHotseatWidget()->GetLayerForNudgeAnimation()->GetTargetTransform());
}

TEST_F(HomeToOverviewNudgeControllerTest, TapOnTheNudgeDuringShowAnimation) {
  TabletModeControllerTestApi().EnterTabletMode();
  CreateUserSessions(1);
  ScopedWindowList extra_windows = CreateAndMinimizeWindows(2);

  ASSERT_TRUE(GetNudgeController());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  GetNudgeController()->FireShowTimerForTesting();
  ASSERT_TRUE(GetNudgeWidget()->GetLayer()->GetAnimator()->is_animating());

  // Cache the widget, as GetNudgeWidget() will start returning nullptr when the
  // nudge starts hiding.
  ContextualNudge* nudge = GetNudgeController()->nudge_for_testing();
  views::Widget* nudge_widget = nudge->GetWidget();
  WidgetCloseObserver widget_close_observer(nudge_widget);

  GetEventGenerator()->GestureTapAt(
      nudge_widget->GetWindowBoundsInScreen().CenterPoint());

  ASSERT_TRUE(nudge_widget->GetLayer()->GetAnimator()->is_animating());
  EXPECT_TRUE(nudge_widget->IsVisible());
  EXPECT_EQ(gfx::Transform(), nudge_widget->GetLayer()->GetTargetTransform());
  EXPECT_EQ(
      gfx::Transform(),
      GetHotseatWidget()->GetLayerForNudgeAnimation()->GetTargetTransform());
  EXPECT_FALSE(widget_close_observer.WidgetClosed());

  ASSERT_TRUE(nudge->label()->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(0.0f, nudge->label()->layer()->GetTargetOpacity());
  nudge->label()->layer()->GetAnimator()->StopAnimating();

  EXPECT_FALSE(GetNudgeWidget());
  EXPECT_FALSE(nudge_widget->IsVisible());
  EXPECT_EQ(
      gfx::Transform(),
      GetHotseatWidget()->GetLayerForNudgeAnimation()->GetTargetTransform());

  EXPECT_TRUE(widget_close_observer.WidgetClosed());

  EXPECT_TRUE(GetHotseatWidget()
                  ->GetLayerForNudgeAnimation()
                  ->GetAnimator()
                  ->is_animating());
  EXPECT_EQ(
      gfx::Transform(),
      GetHotseatWidget()->GetLayerForNudgeAnimation()->GetTargetTransform());
}

// Tests that the nudge stops showing up if the user performs the gesture few
// times.
TEST_F(HomeToOverviewNudgeControllerTest, NoNudgeAfterSuccessfulGestures) {
  TabletModeControllerTestApi().EnterTabletMode();
  CreateUserSessions(1);
  ScopedWindowList windows = CreateAndMinimizeWindows(2);

  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());

  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
  GetNudgeController()->FireShowTimerForTesting();

  // Perform home to overview gesture kSuccessLimitHomeToOverview times.
  for (int i = 0; i < contextual_tooltip::kSuccessLimitHomeToOverview; ++i) {
    SCOPED_TRACE(testing::Message() << "Attempt " << i);

    // Perform home to overview gesture.
    wm::ActivateWindow(windows[0].get());
    WindowState::Get(windows[0].get())->Minimize();

    // Simluate swipe up and hold gesture on the home screen (which should
    // transition to overview).
    const gfx::Point start = GetPrimaryShelf()
                                 ->hotseat_widget()
                                 ->GetWindowBoundsInScreen()
                                 .CenterPoint();
    GetEventGenerator()->GestureScrollSequenceWithCallback(
        start, start + gfx::Vector2d(0, -100), base::Milliseconds(50),
        /*num_steps = */ 12,
        base::BindRepeating(
            [](ui::EventType type, const gfx::Vector2dF& offset) {
              if (type != ui::EventType::kGestureScrollUpdate) {
                return;
              }

              // If the swipe home to overview controller started the timer to
              // transition to overview (which happens after swipe moves far
              // enough), run it to trigger transition to overview.
              SwipeHomeToOverviewController* swipe_controller =
                  GetPrimaryShelf()
                      ->shelf_layout_manager()
                      ->swipe_home_to_overview_controller_for_testing();
              ASSERT_TRUE(swipe_controller);

              base::OneShotTimer* transition_timer =
                  swipe_controller->overview_transition_timer_for_testing();
              if (transition_timer->IsRunning())
                transition_timer->FireNow();
            }));

    // No point in continuing the test if transition to overview failed.
    ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  }

  // The nudge should not be shown next time the user transitions to home.
  test_clock_.Advance(base::Hours(25));
  ScopedWindowList extra_window = CreateAndMinimizeWindows(1);

  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
  EXPECT_FALSE(GetNudgeController()->HasShowTimerForTesting());
  EXPECT_EQ(
      gfx::Transform(),
      GetHotseatWidget()->GetLayerForNudgeAnimation()->GetTargetTransform());
}

// Tests that swipe up and hold gesture that starts on top of contextual nudge
// widget works - i.e. that home still transitions to overview.
TEST_F(HomeToOverviewNudgeControllerTest, HomeToOverviewGestureFromNudge) {
  TabletModeControllerTestApi().EnterTabletMode();
  CreateUserSessions(1);
  ScopedWindowList windows = CreateAndMinimizeWindows(2);

  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());

  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
  GetNudgeController()->FireShowTimerForTesting();

  // Simluate swipe up and hold gesture on home screen from the nudge widget.
  const gfx::Point start =
      GetNudgeWidget()->GetWindowBoundsInScreen().CenterPoint();
  GetEventGenerator()->GestureScrollSequenceWithCallback(
      start, start + gfx::Vector2d(0, -100), base::Milliseconds(50),
      /*num_steps = */ 12,
      base::BindRepeating([](ui::EventType type, const gfx::Vector2dF& offset) {
        if (type != ui::EventType::kGestureScrollUpdate) {
          return;
        }

        // If the swipe home to overview controller started the timer to
        // transition to overview (which happens after swipe moves far
        // enough), run it to trigger transition to overview.
        SwipeHomeToOverviewController* swipe_controller =
            GetPrimaryShelf()
                ->shelf_layout_manager()
                ->swipe_home_to_overview_controller_for_testing();
        ASSERT_TRUE(swipe_controller);

        base::OneShotTimer* transition_timer =
            swipe_controller->overview_transition_timer_for_testing();
        if (transition_timer->IsRunning())
          transition_timer->FireNow();
      }));

  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that nudge and hotseat get repositioned appropriatelly if the display
// bounds change.
TEST_F(HomeToOverviewNudgeControllerTest,
       NudgeBoundsUpdatedOnDisplayBoundsChange) {
  UpdateDisplay("768x1200");
  TabletModeControllerTestApi().EnterTabletMode();
  CreateUserSessions(1);
  ScopedWindowList windows = CreateAndMinimizeWindows(2);

  ASSERT_TRUE(GetNudgeController());
  ASSERT_TRUE(GetNudgeController()->HasShowTimerForTesting());
  GetNudgeController()->FireShowTimerForTesting();

  {
    SCOPED_TRACE("Initial bounds");
    SanityCheckNudgeBounds();
  }

  UpdateDisplay("1200x768");

  {
    SCOPED_TRACE("Updated bounds");
    SanityCheckNudgeBounds();
  }
}

// Home to Overview Nudge should be hidden when shelf controls are enabled.
TEST_P(HomeToOverviewNudgeControllerTestWithA11yPrefs,
       HideNudgesForShelfControls) {
  SCOPED_TRACE(testing::Message() << "Pref=" << GetParam());

  // Enters tablet mode and sets up two minimized windows. This will create the
  // show nudge timer.
  TabletModeControllerTestApi().EnterTabletMode();
  CreateUserSessions(1);
  ScopedWindowList windows = CreateAndMinimizeWindows(2);
  ASSERT_TRUE(GetNudgeController());
  EXPECT_TRUE(GetNudgeController()->HasShowTimerForTesting());

  // Fires the show nudge timer to make the nudge visible.
  GetNudgeController()->FireShowTimerForTesting();
  EXPECT_TRUE(GetNudgeController()->nudge_for_testing());

  // Enabling accessibility shelf controls should hide the nudge.
  Shell::Get()
      ->session_controller()
      ->GetLastActiveUserPrefService()
      ->SetBoolean(GetParam(), true);
  EXPECT_FALSE(GetNudgeController()->nudge_for_testing());
}

// Home to Overview Nudge should not be shown when shelf controls are enabled.
TEST_P(HomeToOverviewNudgeControllerTestWithA11yPrefs,
       DisableNudgesForShelfControls) {
  SCOPED_TRACE(testing::Message() << "Pref=" << GetParam());
  // Enabling accessibility shelf controls should disable the nudge.
  Shell::Get()
      ->session_controller()
      ->GetLastActiveUserPrefService()
      ->SetBoolean(GetParam(), true);

  // Enters tablet mode and sets up two minimized windows. This should not
  // trigger the nudge show timer because shelf controls are on.
  TabletModeControllerTestApi().EnterTabletMode();
  CreateUserSessions(1);
  ScopedWindowList windows = CreateAndMinimizeWindows(2);

  EXPECT_FALSE(GetNudgeController());
}
}  // namespace ash
