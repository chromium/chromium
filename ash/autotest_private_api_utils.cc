// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/autotest_private_api_utils.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/scoped_skip_user_session_blocked_check.h"

namespace ash {
namespace {

class HomeLauncherStateWaiter {
 public:
  HomeLauncherStateWaiter(bool target_shown, base::OnceClosure closure)
      : target_shown_(target_shown), closure_(std::move(closure)) {
    Shell::Get()
        ->app_list_controller()
        ->SetHomeLauncherAnimationCallbackForTesting(base::BindRepeating(
            &HomeLauncherStateWaiter::OnHomeLauncherAnimationCompleted,
            base::Unretained(this)));
  }
  ~HomeLauncherStateWaiter() {
    Shell::Get()
        ->app_list_controller()
        ->SetHomeLauncherAnimationCallbackForTesting(base::NullCallback());
  }

 private:
  // Passed to AppListControllerImpl as a callback to run when home launcher
  // transition animation is complete.
  void OnHomeLauncherAnimationCompleted(bool shown) {
    if (shown == target_shown_) {
      std::move(closure_).Run();
      delete this;
    }
  }

  bool target_shown_;
  base::OnceClosure closure_;

  DISALLOW_COPY_AND_ASSIGN(HomeLauncherStateWaiter);
};

// A waiter that waits until the animation ended with the target state, and
// execute the callback.  This self destruction upon completion.
class LauncherStateWaiter {
 public:
  LauncherStateWaiter(ash::AppListViewState state, base::OnceClosure closure)
      : target_state_(state), closure_(std::move(closure)) {
    Shell::Get()
        ->app_list_controller()
        ->SetStateTransitionAnimationCallbackForTesting(base::BindRepeating(
            &LauncherStateWaiter::OnStateChanged, base::Unretained(this)));
  }
  ~LauncherStateWaiter() {
    Shell::Get()
        ->app_list_controller()
        ->SetStateTransitionAnimationCallbackForTesting(base::NullCallback());
  }

  void OnStateChanged(ash::AppListViewState state) {
    if (target_state_ == state) {
      std::move(closure_).Run();
      delete this;
    }
  }

 private:
  ash::AppListViewState target_state_;
  base::OnceClosure closure_;

  DISALLOW_COPY_AND_ASSIGN(LauncherStateWaiter);
};

}  // namespace

std::vector<aura::Window*> GetAppWindowList() {
  ScopedSkipUserSessionBlockedCheck skip_session_blocked;
  return Shell::Get()->mru_window_tracker()->BuildWindowForCycleWithPipList(
      ash::kAllDesks);
}

bool WaitForLauncherState(AppListViewState target_state,
                          base::Closure closure) {
  // In the tablet mode, some of the app-list state switching is handled
  // differently. For open and close, HomeLauncherGestureHandler handles the
  // gestures and animation. HomeLauncherStateWaiter can wait for such
  // animation. For switching between the search and apps-grid,
  // LauncherStateWaiter can wait for the animation.
  bool should_wait_for_home_launcher = false;
  if (Shell::Get()->tablet_mode_controller()->InTabletMode() &&
      target_state != AppListViewState::kFullscreenSearch) {
    // App-list can't enter into kPeeking or kHalf state. Thus |target_state|
    // should be either kClosed or kFullscreenAllApps.
    DCHECK(target_state == AppListViewState::kClosed ||
           target_state == AppListViewState::kFullscreenAllApps);
    const AppListViewState current_state =
        Shell::Get()->app_list_controller()->GetAppListViewState();
    should_wait_for_home_launcher =
        (target_state == AppListViewState::kClosed) ||
        (current_state != AppListViewState::kFullscreenSearch);
  }
  if (should_wait_for_home_launcher) {
    // We don't check if the home launcher is animating to the target visibility
    // because a) home launcher behavior is deterministic, b) correctly
    // deteching if the home launcher is animating to visibile/invisible require
    // some refactoring.
    bool target_visible = target_state != ash::AppListViewState::kClosed;
    new HomeLauncherStateWaiter(target_visible, closure);
  } else {
    // Don't wait if the launcher is already in the target state and not
    // animating.
    auto* app_list_view =
        Shell::Get()->app_list_controller()->presenter()->GetView();
    bool animating =
        app_list_view &&
        app_list_view->GetWidget()->GetLayer()->GetAnimator()->is_animating();
    bool at_target_state =
        (!app_list_view && target_state == ash::AppListViewState::kClosed) ||
        (app_list_view && app_list_view->app_list_state() == target_state);
    if (at_target_state && !animating) {
      std::move(closure).Run();
      return true;
    }
    new LauncherStateWaiter(target_state, closure);
  }
  return false;
}

}  // namespace ash
