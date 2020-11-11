// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_controller.h"

#include <memory>

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/frame_throttler/mock_frame_throttling_observer.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/overview_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_wallpaper_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/client/window_types.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
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

  DISALLOW_COPY_AND_ASSIGN(TestOverviewObserver);
};

void WaitForOcclusionStateChange(aura::Window* window) {
  auto current_state = window->occlusion_state();
  while (window->occlusion_state() == current_state)
    base::RunLoop().RunUntilIdle();
}

void WaitForShowAnimation(aura::Window* window) {
  while (window->layer()->opacity() != 1.f)
    base::RunLoop().RunUntilIdle();
}

}  // namespace

using OverviewControllerTest = AshTestBase;

// Tests that press the overview key in keyboard when a window is being dragged
// in clamshell mode should not toggle overview.
TEST_F(OverviewControllerTest,
       PressOverviewKeyDuringWindowDragInClamshellMode) {
  ASSERT_FALSE(TabletModeControllerTestApi().IsTabletModeStarted());
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

TEST_F(OverviewControllerTest, AnimationCallbacksForCrossFadeWallpaper) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  TestOverviewObserver observer(/*should_monitor_animation_state = */ true);
  // Enter without windows.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(TestOverviewObserver::COMPLETED,
            observer.starting_animation_state());
  auto* wallpaper_widget_controller =
      Shell::GetPrimaryRootWindowController()->wallpaper_widget_controller();
  EXPECT_GT(wallpaper_widget_controller->GetWallpaperBlur(), 0);
  EXPECT_TRUE(wallpaper_widget_controller->IsAnimating());
  wallpaper_widget_controller->StopAnimating();

  // Exiting overview has no animations until the overview animation is
  // complete.
  overview_controller->EndOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(TestOverviewObserver::UNKNOWN, observer.ending_animation_state());
  EXPECT_EQ(wallpaper_constants::kOverviewBlur,
            wallpaper_widget_controller->GetWallpaperBlur());
  EXPECT_FALSE(wallpaper_widget_controller->IsAnimating());

  observer.WaitForEndingAnimationComplete();
  EXPECT_EQ(TestOverviewObserver::COMPLETED, observer.ending_animation_state());
  EXPECT_EQ(wallpaper_constants::kClear,
            wallpaper_widget_controller->GetWallpaperBlur());
  EXPECT_TRUE(wallpaper_widget_controller->IsAnimating());
  wallpaper_widget_controller->StopAnimating();

  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(bounds));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(bounds));

  observer.Reset();
  ASSERT_EQ(TestOverviewObserver::UNKNOWN, observer.starting_animation_state());
  ASSERT_EQ(TestOverviewObserver::UNKNOWN, observer.ending_animation_state());

  // Enter with windows.
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(TestOverviewObserver::UNKNOWN, observer.starting_animation_state());
  EXPECT_EQ(TestOverviewObserver::UNKNOWN, observer.ending_animation_state());
  EXPECT_EQ(wallpaper_constants::kClear,
            wallpaper_widget_controller->GetWallpaperBlur());
  EXPECT_FALSE(wallpaper_widget_controller->IsAnimating());

  // Exit with windows before starting animation ends.
  overview_controller->EndOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(TestOverviewObserver::CANCELED,
            observer.starting_animation_state());
  EXPECT_EQ(TestOverviewObserver::UNKNOWN, observer.ending_animation_state());
  // Blur animation never started.
  EXPECT_EQ(wallpaper_constants::kClear,
            wallpaper_widget_controller->GetWallpaperBlur());
  EXPECT_FALSE(wallpaper_widget_controller->IsAnimating());

  observer.Reset();

  // Enter again before exit animation ends.
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(TestOverviewObserver::UNKNOWN, observer.starting_animation_state());
  EXPECT_EQ(TestOverviewObserver::CANCELED, observer.ending_animation_state());
  // Blur animation will start when animation is completed.
  EXPECT_EQ(wallpaper_constants::kClear,
            wallpaper_widget_controller->GetWallpaperBlur());
  EXPECT_FALSE(wallpaper_widget_controller->IsAnimating());

  observer.Reset();

  // Activating window while entering animation should cancel the overview.
  wm::ActivateWindow(window1.get());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(TestOverviewObserver::CANCELED,
            observer.starting_animation_state());
  // Blur animation never started.
  EXPECT_EQ(wallpaper_constants::kClear,
            wallpaper_widget_controller->GetWallpaperBlur());
  EXPECT_FALSE(wallpaper_widget_controller->IsAnimating());
}

TEST_F(OverviewControllerTest, OcclusionTest) {
  using OcclusionState = aura::Window::OcclusionState;

  Shell::Get()
      ->overview_controller()
      ->set_occlusion_pause_duration_for_end_for_test(
          base::TimeDelta::FromMilliseconds(100));
  TestOverviewObserver observer(/*should_monitor_animation_state = */ true);
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(bounds));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(bounds));
  // Wait for show/hide animation because occlusion tracker because
  // the test depends on opacity.
  WaitForShowAnimation(window1.get());
  WaitForShowAnimation(window2.get());

  window1->TrackOcclusionState();
  window2->TrackOcclusionState();
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->occlusion_state());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->occlusion_state());

  // Enter with windows.
  Shell::Get()->overview_controller()->StartOverview();
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->occlusion_state());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->occlusion_state());

  observer.WaitForStartingAnimationComplete();
  // Occlusion tracking is paused.
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->occlusion_state());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->occlusion_state());
  WaitForOcclusionStateChange(window1.get());
  EXPECT_EQ(OcclusionState::VISIBLE, window1->occlusion_state());

  // Exit with windows.
  Shell::Get()->overview_controller()->EndOverview();
  EXPECT_EQ(OcclusionState::VISIBLE, window1->occlusion_state());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->occlusion_state());
  observer.WaitForEndingAnimationComplete();
  EXPECT_EQ(OcclusionState::VISIBLE, window1->occlusion_state());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->occlusion_state());
  WaitForOcclusionStateChange(window1.get());
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->occlusion_state());

  observer.Reset();

  // Enter again.
  Shell::Get()->overview_controller()->StartOverview();
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->occlusion_state());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->occlusion_state());
  auto* active = window_util::GetActiveWindow();
  EXPECT_EQ(window2.get(), active);

  observer.WaitForStartingAnimationComplete();

  // Window 1 is still occluded because tracker is paused.
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->occlusion_state());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->occlusion_state());

  WaitForOcclusionStateChange(window1.get());
  EXPECT_EQ(OcclusionState::VISIBLE, window1->occlusion_state());

  wm::ActivateWindow(window1.get());
  observer.WaitForEndingAnimationComplete();

  // Windows are visible because tracker is paused.
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->occlusion_state());
  EXPECT_EQ(OcclusionState::VISIBLE, window1->occlusion_state());
  WaitForOcclusionStateChange(window2.get());
  EXPECT_EQ(OcclusionState::VISIBLE, window1->occlusion_state());
  EXPECT_EQ(OcclusionState::OCCLUDED, window2->occlusion_state());
}

// Tests that PIP windows are not shown in overview.
TEST_F(OverviewControllerTest, PipMustNotInOverviewGridTest) {
  gfx::Rect bounds{100, 100};
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(bounds));
  WaitForShowAnimation(window.get());
  auto* controller = Shell::Get()->overview_controller();
  controller->StartOverview();
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

  Shell::Get()->overview_controller()->StartOverview();
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
  auto* controller = Shell::Get()->overview_controller();
  controller->StartOverview();
  auto* session = controller->overview_session();
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
  controller->EndOverview();
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
  auto* overview_controller = Shell::Get()->overview_controller();

  for (bool is_tablet_mode : {false, true}) {
    SCOPED_TRACE(is_tablet_mode ? "Tablet Mode" : "Clamshell Mode");
    set_tablet_mode_enabled(is_tablet_mode);

    overview_controller->StartOverview();
    wait_for_animation(/*enter=*/true);
    overview_controller->EndOverview();
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
    overview_controller->StartOverview();
    wait_for_animation(/*enter=*/true);
    overview_controller->EndOverview();
    wait_for_animation(/*enter=*/false);
    EXPECT_TRUE(observer.ObserverCountsEqual());

    // Tests the case where we exit overview before the start animation has
    // completed.
    overview_controller->StartOverview();
    overview_controller->EndOverview();
    wait_for_animation(/*enter=*/false);
    EXPECT_TRUE(observer.ObserverCountsEqual());

    // Tests the case where we enter overview before the exit animation has
    // completed.
    overview_controller->StartOverview();
    wait_for_animation(/*enter=*/true);
    overview_controller->EndOverview();
    overview_controller->StartOverview();
    overview_controller->EndOverview();
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

  Shell::Get()->overview_controller()->StartOverview();
  EXPECT_FALSE(observer.last_animation_was_fade());

  // Exit to home launcher using fade out animation. This should minimize all
  // windows.
  Shell::Get()->overview_controller()->EndOverview(
      OverviewEnterExitType::kFadeOutExit);

  EXPECT_TRUE(observer.last_animation_was_fade());

  ASSERT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());

  // All windows are minimized, so we should use the fade in animation to enter
  // overview.
  Shell::Get()->overview_controller()->StartOverview();
  EXPECT_TRUE(observer.last_animation_was_fade());
}

// Tests that fade animations are not used to enter or exit overview in
// clamshell.
TEST_F(OverviewControllerTest, OverviewEnterExitAnimationClamshell) {
  TestOverviewObserver observer(/*should_monitor_animation_state = */ false);

  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(bounds));

  Shell::Get()->overview_controller()->StartOverview();
  EXPECT_FALSE(observer.last_animation_was_fade());

  Shell::Get()->overview_controller()->EndOverview();
  EXPECT_FALSE(observer.last_animation_was_fade());

  // Even with all window minimized, overview should not use fade animation to
  // enter.
  ASSERT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  WindowState::Get(window.get())->Minimize();
  Shell::Get()->overview_controller()->StartOverview();
  EXPECT_FALSE(observer.last_animation_was_fade());
}

TEST_F(OverviewControllerTest, WallpaperAnimationTiming) {
  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(bounds));
  WindowState::Get(window.get())->Minimize();

  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  Shell::Get()->overview_controller()->StartOverview(
      OverviewEnterExitType::kFadeInEnter);
  auto* wallpaper_widget_controller =
      Shell::GetPrimaryRootWindowController()->wallpaper_widget_controller();
  EXPECT_GT(wallpaper_widget_controller->GetWallpaperBlur(), 0);
  EXPECT_TRUE(wallpaper_widget_controller->IsAnimating());
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
  Shell::Get()->overview_controller()->StartOverview();

  // Exit to home launcher using fade out animation. This should minimize all
  // windows.
  TestOverviewObserver observer(/*should_monitor_animation_state = */ true);
  Shell::Get()->overview_controller()->EndOverview(
      OverviewEnterExitType::kFadeOutExit);

  EXPECT_TRUE(observer.last_animation_was_fade());

  // Verify that the overview exits cleanly.
  observer.WaitForEndingAnimationComplete();

  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
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
        keyboard::KeyboardBoundsFromRootBounds(
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
  ASSERT_TRUE(keyboard::WaitUntilShown());

  Shell::Get()->overview_controller()->StartOverview();

  // Timeout failure here if the keyboard does not hide.
  keyboard::WaitUntilHidden();
}

TEST_F(OverviewVirtualKeyboardTest,
       ToggleOverviewModeDoesNotHideLockedVirtualKeyboard) {
  keyboard_ui_controller()->ShowKeyboard(true /* locked */);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  Shell::Get()->overview_controller()->StartOverview();
  EXPECT_FALSE(keyboard::IsKeyboardHiding());
}

// Tests that frame throttling starts and ends accordingly when overview starts
// and ends.
TEST_F(OverviewControllerTest, FrameThrottling) {
  MockFrameThrottlingObserver observer;
  FrameThrottlingController* frame_throttling_controller =
      Shell::Get()->frame_throttling_controller();
  frame_throttling_controller->AddObserver(&observer);
  const int browser_window_count = 3;
  const int arc_window_count = 2;
  const int total_window_count = browser_window_count + arc_window_count;
  std::unique_ptr<aura::Window> created_windows[total_window_count];
  std::vector<aura::Window*> windows(total_window_count, nullptr);
  for (int i = 0; i < total_window_count; ++i) {
    created_windows[i] = CreateAppWindow(gfx::Rect(), i < browser_window_count
                                                          ? AppType::BROWSER
                                                          : AppType::ARC_APP);
    windows[i] = created_windows[i].get();
  }

  auto* controller = Shell::Get()->overview_controller();
  EXPECT_CALL(observer, OnThrottlingStarted(
                            testing::UnorderedElementsAreArray(windows),
                            frame_throttling_controller->throttled_fps()));
  controller->StartOverview();

  EXPECT_CALL(observer, OnThrottlingEnded());
  controller->EndOverview();
  frame_throttling_controller->RemoveObserver(&observer);
}

}  // namespace ash
