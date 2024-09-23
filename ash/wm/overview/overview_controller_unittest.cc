// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/wm/overview/overview_controller.h"

#include <memory>

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/frame_throttler/mock_frame_throttling_observer.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/overview_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

gfx::PointF CalculateDragPoint(const WindowResizer& resizer,
                               int delta_x,
                               int delta_y) {
  gfx::PointF location = resizer.GetInitialLocation();
  location.set_x(location.x() + delta_x);
  location.set_y(location.y() + delta_y);
  return location;
}

class TestOverviewObserver : public OverviewObserver {
 public:
  enum AnimationState {
    UNKNOWN,
    COMPLETED,
    CANCELED,
  };

  explicit TestOverviewObserver(bool should_monitor_animation_state)
      : should_monitor_animation_state_(should_monitor_animation_state) {
    Shell::Get()->overview_controller()->AddObserver(this);
  }

  TestOverviewObserver(const TestOverviewObserver&) = delete;
  TestOverviewObserver& operator=(const TestOverviewObserver&) = delete;

  ~TestOverviewObserver() override {
    Shell::Get()->overview_controller()->RemoveObserver(this);
  }

  // OverviewObserver:
  void OnOverviewModeWillStart() override { ++observer_counts_.will_start; }
  void OnOverviewModeStarting() override {
    ++observer_counts_.starting;
    UpdateLastAnimationStates(
        Shell::Get()->overview_controller()->overview_session());
  }
  void OnOverviewModeStartingAnimationComplete(bool canceled) override {
    ++observer_counts_.starting_animation_complete;
    if (!should_monitor_animation_state_)
      return;

    EXPECT_EQ(UNKNOWN, starting_animation_state_);
    starting_animation_state_ = canceled ? CANCELED : COMPLETED;
    if (run_loop_)
      run_loop_->Quit();
  }
  void OnOverviewModeEnding(OverviewSession* overview_session) override {
    ++observer_counts_.ending;
    UpdateLastAnimationStates(overview_session);
  }
  void OnOverviewModeEnded() override { ++observer_counts_.ended; }
  void OnOverviewModeEndingAnimationComplete(bool canceled) override {
    ++observer_counts_.ending_animation_complete;
    if (!should_monitor_animation_state_)
      return;

    EXPECT_EQ(UNKNOWN, ending_animation_state_);
    ending_animation_state_ = canceled ? CANCELED : COMPLETED;
    if (run_loop_)
      run_loop_->Quit();
  }

  void Reset() {
    starting_animation_state_ = UNKNOWN;
    ending_animation_state_ = UNKNOWN;
  }

  void WaitForStartingAnimationComplete() {
    while (starting_animation_state_ != COMPLETED) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->RunUntilIdle();
    }
  }

  void WaitForEndingAnimationComplete() {
    while (ending_animation_state_ != COMPLETED) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->RunUntilIdle();
    }
  }

  // Checks if all the observed methods have fired the same amount of times.
  bool ObserverCountsEqual() {
    const int expected_count = observer_counts_.will_start;
    DCHECK_GT(expected_count, 0);
    if (observer_counts_.starting != expected_count)
      return false;
    if (observer_counts_.starting_animation_complete != expected_count)
      return false;
    if (observer_counts_.ending != expected_count)
      return false;
    if (observer_counts_.ended != expected_count)
      return false;
    if (observer_counts_.ending_animation_complete != expected_count)
      return false;
    return true;
  }

  bool is_ended() const { return ending_animation_state_ != UNKNOWN; }
  bool is_started() const { return starting_animation_state_ != UNKNOWN; }
  AnimationState starting_animation_state() const {
    return starting_animation_state_;
  }
  AnimationState ending_animation_state() const {
    return ending_animation_state_;
  }
  bool last_animation_was_fade() const { return last_animation_was_fade_; }

 private:
  void UpdateLastAnimationStates(OverviewSession* selector) {
    DCHECK(selector);
    const OverviewEnterExitType enter_exit_type =
        selector->enter_exit_overview_type();

    last_animation_was_fade_ =
        enter_exit_type == OverviewEnterExitType::kFadeInEnter ||
        enter_exit_type == OverviewEnterExitType::kFadeOutExit;
  }

  // Struct which keeps track of the counts a OverviewObserver method has fired.
  // These are used to verify that certain methods have a one to one ratio.
  struct ObserverCounts {
    int will_start;
    int starting;
    int starting_animation_complete;
    int ending;
    int ended;
    int ending_animation_complete;
  } observer_counts_ = {0};

  AnimationState starting_animation_state_ = UNKNOWN;
  AnimationState ending_animation_state_ = UNKNOWN;
  bool last_animation_was_fade_ = false;
  // If false, skips the checks in OnOverviewMode Starting/Ending
  // AnimationComplete.
  bool should_monitor_animation_state_;

  std::unique_ptr<base::RunLoop> run_loop_;
};

void WaitForShowAnimation(aura::Window* window) {
  while (window->layer()->opacity() != 1.f)
    base::RunLoop().RunUntilIdle();
}

}  // namespace

class OverviewControllerTest : public AshTestBase {
 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      chromeos::features::kOverviewSessionInitOptimizations};
};

// Tests that press the overview key in keyboard when a window is being dragged
// in clamshell mode should not toggle overview.
TEST_F(OverviewControllerTest,
       PressOverviewKeyDuringWindowDragInClamshellMode) {
  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());
  std::unique_ptr<aura::Window> dragged_window = CreateTestWindow();
  std::unique_ptr<WindowResizer> resizer =
      CreateWindowResizer(dragged_window.get(), gfx::PointF(), HTCAPTION,
                          ::wm::WINDOW_MOVE_SOURCE_MOUSE);
  resizer->Drag(CalculateDragPoint(*resizer, 10, 0), 0);
  EXPECT_TRUE(WindowState::Get(dragged_window.get())->is_dragged());
  GetEventGenerator()->PressKey(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  resizer->CompleteDrag();
}

TEST_F(OverviewControllerTest, OcclusionTestWithSnapshot) {
  using OcclusionState = aura::Window::OcclusionState;

  Shell::Get()
      ->overview_controller()
      ->set_occlusion_pause_duration_for_end_for_test(base::Milliseconds(500));
  Shell::Get()->overview_controller()->set_windows_have_snapshot_for_test(true);
  TestOverviewObserver observer(/*should_monitor_animation_state = */ true);
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  constexpr gfx::Rect kBounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window1(CreateAppWindow(kBounds));
  std::unique_ptr<aura::Window> window2(CreateAppWindow(kBounds));
  // Wait for show/hide animation because occlusion tracker because
  // the test depends on opacity.
  WaitForShowAnimation(window1.get());
  WaitForShowAnimation(window2.get());

  window1->TrackOcclusionState();
  window2->TrackOcclusionState();
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());

  // Enter with windows.
  EnterOverview();
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());

  observer.WaitForStartingAnimationComplete();
  // Occlusion tracking is paused.
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());
  WaitForOcclusionStateChange(window1.get(), OcclusionState::VISIBLE);

  // Exit with windows.
  ExitOverview();
  EXPECT_EQ(OcclusionState::VISIBLE, window1->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());
  observer.WaitForEndingAnimationComplete();
  EXPECT_EQ(OcclusionState::VISIBLE, window1->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());
  WaitForOcclusionStateChange(window1.get(), OcclusionState::OCCLUDED);

  observer.Reset();

  // Enter again.
  EnterOverview();
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());
  auto* active = window_util::GetActiveWindow();
  EXPECT_EQ(window2.get(), active);

  observer.WaitForStartingAnimationComplete();

  // Window 1 is still occluded because tracker is paused.
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());

  WaitForOcclusionStateChange(window1.get(), OcclusionState::VISIBLE);

  wm::ActivateWindow(window1.get());
  observer.WaitForEndingAnimationComplete();

  // Windows are visible because tracker is paused.
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window1->GetOcclusionState());
  WaitForOcclusionStateChange(window2.get(), OcclusionState::OCCLUDED);
  EXPECT_EQ(OcclusionState::VISIBLE, window1->GetOcclusionState());
}

TEST_F(OverviewControllerTest, OcclusionTestWithoutSnapshot) {
  using OcclusionState = aura::Window::OcclusionState;

  Shell::Get()
      ->overview_controller()
      ->set_occlusion_pause_duration_for_end_for_test(base::Milliseconds(500));
  Shell::Get()->overview_controller()->set_windows_have_snapshot_for_test(
      false);
  TestOverviewObserver observer(/*should_monitor_animation_state = */ true);
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  constexpr gfx::Rect kBounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window1(CreateAppWindow(kBounds));
  std::unique_ptr<aura::Window> window2(CreateAppWindow(kBounds));
  // Wait for show/hide animation because occlusion tracker because
  // the test depends on opacity.
  WaitForShowAnimation(window1.get());
  WaitForShowAnimation(window2.get());

  window1->TrackOcclusionState();
  window2->TrackOcclusionState();
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());

  // Enter with windows.
  EnterOverview();
  // Tracker is not paused for enter, and items are forced visible.
  EXPECT_EQ(OcclusionState::VISIBLE, window1->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());

  observer.WaitForStartingAnimationComplete();
  EXPECT_EQ(OcclusionState::VISIBLE, window1->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());

  // Exit with windows.
  ExitOverview();
  EXPECT_EQ(OcclusionState::VISIBLE, window1->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());
  observer.WaitForEndingAnimationComplete();
  EXPECT_EQ(OcclusionState::VISIBLE, window1->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());
  WaitForOcclusionStateChange(window1.get(), OcclusionState::OCCLUDED);

  observer.Reset();

  // Enter again.
  EnterOverview();
  EXPECT_EQ(OcclusionState::VISIBLE, window1->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());
  auto* active = window_util::GetActiveWindow();
  EXPECT_EQ(window2.get(), active);

  observer.WaitForStartingAnimationComplete();

  EXPECT_EQ(OcclusionState::VISIBLE, window1->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());

  wm::ActivateWindow(window1.get());
  observer.WaitForEndingAnimationComplete();

  // Windows are visible because tracker is paused (tracker is paused for exit).
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window1->GetOcclusionState());
  WaitForOcclusionStateChange(window2.get(), OcclusionState::OCCLUDED);
  EXPECT_EQ(OcclusionState::VISIBLE, window1->GetOcclusionState());
}

// Tests that PIP windows are not shown in overview.
TEST_F(OverviewControllerTest, PipMustNotInOverviewGridTest) {
  gfx::Rect bounds{100, 100};
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(bounds));
  WaitForShowAnimation(window.get());
  auto* controller = Shell::Get()->overview_controller();
  EnterOverview();
  // Ensure |window| is in overview with window state non-PIP.
  EXPECT_TRUE(controller->overview_session()->IsWindowInOverview(window.get()));
  WMEvent pip_event(WM_EVENT_PIP);
  WindowState::Get(window.get())->OnWMEvent(&pip_event);
  // Ensure |window| is not in overview with window state PIP.
  EXPECT_FALSE(
      controller->overview_session()->IsWindowInOverview(window.get()));
}

// Tests that beginning window selection hides the app list.
TEST_F(OverviewControllerTest, SelectingHidesAppList) {
  std::unique_ptr<aura::Window> window(CreateTestWindow());

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplay().id());
  GetAppListTestHelper()->CheckVisibility(true);

  EnterOverview();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that windows that are excluded from overview, are actually not shown in
// overview.
TEST_F(OverviewControllerTest, ExcludedWindowsHidden) {
  // Create three windows, one normal, one which is not user positionable (and
  // so should be hidden) and one specifically set to be hidden in overview.
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(), aura::client::WINDOW_TYPE_POPUP);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();
  window3->SetProperty(kHideInOverviewKey, true);

  // After creation, all windows are visible.
  ASSERT_TRUE(window1->IsVisible());
  ASSERT_TRUE(window2->IsVisible());
  ASSERT_TRUE(window3->IsVisible());

  // Enter overview. Only one of the three windows is in overview, and visible.
  EnterOverview();
  auto* session = Shell::Get()->overview_controller()->overview_session();
  ASSERT_TRUE(session);
  EXPECT_TRUE(session->IsWindowInOverview(window1.get()));
  EXPECT_FALSE(session->IsWindowInOverview(window2.get()));
  EXPECT_FALSE(session->IsWindowInOverview(window3.get()));
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());

  // On exiting overview, the windows should all be visible. Use a run loop
  // since |session| is destroyed in a post task, and the restoring windows'
  // previous visibility happens in the destructor.
  ExitOverview();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
}

// Some ash codes are reliant on some OverviewObserver calls matching (i.e. the
// amount of starts should match the amount of ends). This test verifies that
// behavior. Tests for both tablet and clamshell mode.
TEST_F(OverviewControllerTest, ObserverCallsMatch) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  TestOverviewObserver observer(/*should_monitor_animation_state=*/false);

  // Helper which waits for an overview animation to finish.
  auto wait_for_animation = [](bool enter) {
    ShellTestApi().WaitForOverviewAnimationState(
        enter ? OverviewAnimationState::kEnterAnimationComplete
              : OverviewAnimationState::kExitAnimationComplete);
  };

  auto set_tablet_mode_enabled = [](bool enabled) {
    TabletMode::Waiter waiter(enabled);
    if (enabled)
      TabletModeControllerTestApi().EnterTabletMode();
    else
      TabletModeControllerTestApi().LeaveTabletMode();
    waiter.Wait();
  };

  // Tests the case where we enter without windows and do regular enter/exit
  // (wait for enter animation to finish before exiting).
  for (bool is_tablet_mode : {false, true}) {
    SCOPED_TRACE(is_tablet_mode ? "Tablet Mode" : "Clamshell Mode");
    set_tablet_mode_enabled(is_tablet_mode);

    EnterOverview();
    wait_for_animation(/*enter=*/true);
    ExitOverview();
    wait_for_animation(/*enter=*/false);
    EXPECT_TRUE(observer.ObserverCountsEqual());
  }

  // Create one window for the next set of tests.
  std::unique_ptr<aura::Window> window(CreateTestWindow());

  for (bool is_tablet_mode : {false, true}) {
    SCOPED_TRACE(is_tablet_mode ? "Tablet Mode" : "Clamshell Mode");
    set_tablet_mode_enabled(is_tablet_mode);

    // Tests the case where we enter with windows and do regular enter/exit
    // (wait for enter animation to finish before exiting).
    EnterOverview();
    wait_for_animation(/*enter=*/true);
    ExitOverview();
    wait_for_animation(/*enter=*/false);
    EXPECT_TRUE(observer.ObserverCountsEqual());

    // Tests the case where we exit overview before the start animation has
    // completed.
    EnterOverview();
    ExitOverview();
    wait_for_animation(/*enter=*/false);
    EXPECT_TRUE(observer.ObserverCountsEqual());

    // Tests the case where we enter overview before the exit animation has
    // completed.
    EnterOverview();
    wait_for_animation(/*enter=*/true);
    ExitOverview();
    EnterOverview();
    ExitOverview();
    wait_for_animation(/*enter=*/false);
    EXPECT_TRUE(observer.ObserverCountsEqual());
  }
}

// Tests which animation for overview is used in tablet if all windows
// are minimized, and that if overview is exited from the home launcher all
// windows are minimized.
TEST_F(OverviewControllerTest, OverviewEnterExitAnimationTablet) {
  TestOverviewObserver observer(/*should_monitor_animation_state = */ false);

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  // Ensure calls to SetEnabledForTest complete.
  base::RunLoop().RunUntilIdle();

  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(bounds));

  EnterOverview();
  EXPECT_FALSE(observer.last_animation_was_fade());

  // Exit to home launcher using fade out animation. This should minimize all
  // windows.
  ExitOverview(OverviewEnterExitType::kFadeOutExit);

  EXPECT_TRUE(observer.last_animation_was_fade());

  ASSERT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());

  // All windows are minimized, so we should use the fade in animation to enter
  // overview.
  EnterOverview();
  EXPECT_TRUE(observer.last_animation_was_fade());
}

// Tests that fade animations are not used to enter or exit overview in
// clamshell.
TEST_F(OverviewControllerTest, OverviewEnterExitAnimationClamshell) {
  TestOverviewObserver observer(/*should_monitor_animation_state = */ false);

  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(bounds));

  EnterOverview();
  EXPECT_FALSE(observer.last_animation_was_fade());

  ExitOverview();
  EXPECT_FALSE(observer.last_animation_was_fade());

  // Even with all window minimized, overview should not use fade animation to
  // enter.
  ASSERT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  WindowState::Get(window.get())->Minimize();
  EnterOverview();
  EXPECT_FALSE(observer.last_animation_was_fade());
}

// Tests that overview session exits cleanly if exit is requested before
// previous enter animations finish.
TEST_F(OverviewControllerTest, OverviewExitWhileStillEntering) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  // Ensure calls to SetEnabledForTest complete.
  base::RunLoop().RunUntilIdle();

  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(bounds));
  wm::ActivateWindow(window.get());

  // Start overview session - set non zero animation duration so overview is
  // started asynchronously.
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  EnterOverview();

  // Exit to home launcher using fade out animation. This should minimize all
  // windows.
  TestOverviewObserver observer(/*should_monitor_animation_state = */ true);
  ExitOverview(OverviewEnterExitType::kFadeOutExit);

  EXPECT_TRUE(observer.last_animation_was_fade());

  // Verify that the overview exits cleanly.
  observer.WaitForEndingAnimationComplete();

  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
}

// Tests that overview animations continue even if a window gets destroyed
// during the animation.
TEST_F(OverviewControllerTest, CloseWindowDuringAnimation) {
  // Create two windows. They should both be visible so that they both get
  // animated.
  std::unique_ptr<aura::Window> window1 = CreateAppWindow(gfx::Rect(250, 100));
  std::unique_ptr<aura::Window> window2 =
      CreateAppWindow(gfx::Rect(250, 250, 250, 100));

  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  EnterOverview();

  // Destroy a window during the enter animation.
  window1.reset();
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kEnterAnimationComplete);
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  ExitOverview();

  // Destroy a window during the exit animation.
  window2.reset();
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kExitAnimationComplete);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Quick test on all `OverviewStartAction`s to verify that they are recorded
// correctly in uma metric.
TEST_F(OverviewControllerTest, OverviewStartActionHistogramTest) {
  base::HistogramTester histogram_tester;
  constexpr char kOverviewStartActionHistogram[] = "Ash.Overview.StartAction";
  OverviewController* overview_controller = OverviewController::Get();

  for (OverviewStartAction start_action : {
           OverviewStartAction::kSplitView,
           OverviewStartAction::kAccelerator,
           OverviewStartAction::kDragWindowFromShelf,
           OverviewStartAction::kExitHomeLauncher,
           OverviewStartAction::kOverviewButton,
           OverviewStartAction::kOverviewButtonLongPress,
           OverviewStartAction::kBentoBar_DEPRECATED,
           OverviewStartAction::k3FingerVerticalScroll,
           OverviewStartAction::kDevTools,
           OverviewStartAction::kTests,
           OverviewStartAction::kOverviewDeskSwitch,
           OverviewStartAction::kDeskButton,
           OverviewStartAction::kFasterSplitScreenSetup,
       }) {
    // Verify the initial count for the histogram.
    histogram_tester.ExpectBucketCount(kOverviewStartActionHistogram,
                                       start_action,
                                       /*expected_count=*/0);
    overview_controller->StartOverview(start_action);
    histogram_tester.ExpectBucketCount(kOverviewStartActionHistogram,
                                       start_action,
                                       /*expected_count=*/1);
    overview_controller->EndOverview(OverviewEndAction::kTests);
  }
}

// Quick test on all `OverviewEndAction`s to verify that they are recorded
// correctly in uma metric.
TEST_F(OverviewControllerTest, OverviewEndActionHistogramTest) {
  base::HistogramTester histogram_tester;
  constexpr char kOverviewEndActionHistogram[] = "Ash.Overview.EndAction";
  OverviewController* overview_controller = OverviewController::Get();

  for (OverviewEndAction end_action : {
           OverviewEndAction::kSplitView,
           OverviewEndAction::kDragWindowFromShelf,
           OverviewEndAction::kEnterHomeLauncher,
           OverviewEndAction::kClickingOutsideWindowsInOverview,
           OverviewEndAction::kWindowActivating,
           OverviewEndAction::kLastWindowRemoved,
           OverviewEndAction::kDisplayAdded,
           OverviewEndAction::kKeyEscapeOrBack,
           OverviewEndAction::kDeskActivation,
           OverviewEndAction::kOverviewButton,
           OverviewEndAction::kOverviewButtonLongPress,
           OverviewEndAction::k3FingerVerticalScroll,
           OverviewEndAction::kEnabledDockedMagnifier,
           OverviewEndAction::kUserSwitch,
           OverviewEndAction::kStartedWindowCycle,
           OverviewEndAction::kShuttingDown,
           OverviewEndAction::kAppListActivatedInClamshell,
           OverviewEndAction::kShelfAlignmentChanged,
           OverviewEndAction::kDevTools,
           OverviewEndAction::kTests,
           OverviewEndAction::kShowGlanceables_DEPRECATED,
       }) {
    // Verify the initial count for the histogram.
    histogram_tester.ExpectBucketCount(kOverviewEndActionHistogram, end_action,
                                       /*expected_count=*/0);
    overview_controller->StartOverview(OverviewStartAction::kTests);
    overview_controller->EndOverview(end_action);
    histogram_tester.ExpectBucketCount(kOverviewEndActionHistogram, end_action,
                                       /*expected_count=*/1);
  }
}

// A subclass of DeskSwitchAnimationWaiter that additionally attempts to start
// overview after the desk animation screenshots have been taken. Using the
// regular DeskSwitchAnimatorWaiter and attempting to start overview before
// calling Wait() would be similar to performing a desk switch when overview is
// already open. This waiter mocks the behavior of trying to enter overview
// while the desk switch is already in motion.
class DeskSwitchStartOverviewAnimationWaiter
    : public DeskSwitchAnimationWaiter {
 public:
  DeskSwitchStartOverviewAnimationWaiter() = default;
  DeskSwitchStartOverviewAnimationWaiter(
      const DeskSwitchStartOverviewAnimationWaiter&) = delete;
  DeskSwitchStartOverviewAnimationWaiter& operator=(
      const DeskSwitchStartOverviewAnimationWaiter&) = delete;
  ~DeskSwitchStartOverviewAnimationWaiter() override = default;

  // DeskSwitchAnimationWaiter:
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override {
    Shell::Get()->overview_controller()->StartOverview(
        OverviewStartAction::kTests);
  }
};

// Tests that entering overview while performing a desk animation is disallowed,
// but exiting is still done.
TEST_F(OverviewControllerTest, OverviewEnterExitWhileDeskAnimation) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk1 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk2 = desks_controller->GetDeskAtIndex(1);

  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Animate to desk 2. Try to enter overview while animating. On desk animation
  // finished, we shouldn't be in overview.
  DeskSwitchStartOverviewAnimationWaiter waiter;
  desks_controller->ActivateDesk(desk2, DesksSwitchSource::kDeskSwitchShortcut);
  waiter.Wait();
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

  EnterOverview();
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Tests that exiting overview works as it is part of the desk switch
  // animation.
  ActivateDesk(desk1);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that clipping the window to remove the top view inset (header) works as
// expected.
TEST_F(OverviewControllerTest, WindowClipping) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  window->SetBounds(gfx::Rect(300, 300));
  window->SetProperty(aura::client::kTopViewInset, 20);
  ASSERT_EQ(gfx::Rect(), window->layer()->GetTargetClipRect());

  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Tests that the clipping bounds in overview will clip away the top inset.
  // There is a extra pixel added to account for what seems to be a rounding
  // error.
  EnterOverview();
  WaitForOverviewEnterAnimation();
  EXPECT_EQ(gfx::Rect(0, 21, 300, 279), window->layer()->GetTargetClipRect());

  // Tests that we animate to the window size from the overview clip on exit.
  ExitOverview();
  EXPECT_EQ(gfx::Rect(300, 300), window->layer()->GetTargetClipRect());

  // Tests that the clipping is removed after the animation ends.
  WaitForOverviewExitAnimation();
  EXPECT_EQ(gfx::Rect(), window->layer()->GetTargetClipRect());
}

class OverviewVirtualKeyboardTest : public OverviewControllerTest {
 protected:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    OverviewControllerTest::SetUp();

    TabletModeControllerTestApi().EnterTabletMode();
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(keyboard::IsKeyboardEnabled());
    keyboard::test::WaitUntilLoaded();

    keyboard_ui_controller()->GetKeyboardWindow()->SetBounds(
        keyboard::test::KeyboardBoundsFromRootBounds(
            Shell::GetPrimaryRootWindow()->bounds(), 100));
    // Wait for keyboard window to load.
    base::RunLoop().RunUntilIdle();
  }

  keyboard::KeyboardUIController* keyboard_ui_controller() {
    return keyboard::KeyboardUIController::Get();
  }
};

TEST_F(OverviewVirtualKeyboardTest, ToggleOverviewModeHidesVirtualKeyboard) {
  keyboard_ui_controller()->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  EnterOverview();

  // Timeout failure here if the keyboard does not hide.
  keyboard::test::WaitUntilHidden();
}

TEST_F(OverviewVirtualKeyboardTest,
       ToggleOverviewModeDoesNotHideLockedVirtualKeyboard) {
  keyboard_ui_controller()->ShowKeyboard(true /* locked */);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  EnterOverview();
  EXPECT_FALSE(keyboard::test::IsKeyboardHiding());
}

// Tests that frame throttling starts and ends accordingly when overview starts
// and ends.
TEST_F(OverviewControllerTest, FrameThrottling) {
  MockFrameThrottlingObserver observer;
  FrameThrottlingController* frame_throttling_controller =
      Shell::Get()->frame_throttling_controller();
  frame_throttling_controller->AddArcObserver(&observer);
  const int browser_window_count = 3;
  const int arc_window_count = 2;

  const std::vector<viz::FrameSinkId> ids{{1u, 1u}, {2u, 2u}, {3u, 3u}};
  std::unique_ptr<aura::Window>
      created_windows[browser_window_count + arc_window_count];
  for (int i = 0; i < browser_window_count; ++i) {
    created_windows[i] =
        CreateAppWindow(gfx::Rect(), chromeos::AppType::BROWSER);
    created_windows[i]->SetEmbedFrameSinkId(ids[i]);
  }

  std::vector<aura::Window*> arc_windows(arc_window_count, nullptr);
  for (int i = 0; i < arc_window_count; ++i) {
    created_windows[i + browser_window_count] =
        CreateAppWindow(gfx::Rect(), chromeos::AppType::ARC_APP);
    arc_windows[i] = created_windows[i + browser_window_count].get();
  }

  EXPECT_CALL(observer,
              OnThrottlingStarted(
                  testing::UnorderedElementsAreArray(arc_windows),
                  frame_throttling_controller->GetCurrentThrottledFrameRate()));
  EnterOverview();
  EXPECT_THAT(frame_throttling_controller->GetFrameSinkIdsToThrottle(),
              ::testing::UnorderedElementsAreArray(ids));

  EXPECT_CALL(observer, OnThrottlingEnded());
  ExitOverview();
  EXPECT_TRUE(frame_throttling_controller->GetFrameSinkIdsToThrottle().empty());

  frame_throttling_controller->RemoveArcObserver(&observer);
}

// Tests that Ash.Overview.DeskCount metric is recorded.
TEST_F(OverviewControllerTest, RecordsDeskCountMetric) {
  base::HistogramTester histogram_tester;
  EnterOverview();
  ExitOverview();
  histogram_tester.ExpectUniqueSample("Ash.Overview.DeskCount", 1, 1);

  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kKeyboard);
  ASSERT_EQ(2u, DesksController::Get()->desks().size());
  EnterOverview();
  ExitOverview();
  histogram_tester.ExpectBucketCount("Ash.Overview.DeskCount", 1, 1);
  histogram_tester.ExpectBucketCount("Ash.Overview.DeskCount", 2, 1);
}

class OverviewEnterFromWallpaperTest : public OverviewControllerTest {
 public:
  OverviewEnterFromWallpaperTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kEnterOverviewFromWallpaper}, {});
  }
  ~OverviewEnterFromWallpaperTest() override = default;

  WallpaperView* wallpaper_view() {
    return Shell::Get()
        ->GetPrimaryRootWindowController()
        ->wallpaper_widget_controller()
        ->wallpaper_view();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the user can enter/exit overview by clicking on the wallpaper.
TEST_F(OverviewEnterFromWallpaperTest,
       OverviewEnterExitClamshellFromWallpaper) {
  std::unique_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(gfx::Rect(400, 400)));

  ASSERT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

  GetEventGenerator()->set_current_screen_location(
      wallpaper_view()->GetBoundsInScreen().right_center());
  GetEventGenerator()->ClickLeftButton();
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  GetEventGenerator()->ClickLeftButton();
  ASSERT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

}  // namespace ash
