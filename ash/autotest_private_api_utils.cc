// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/autotest_private_api_utils.h"

#include <optional>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/scoped_skip_user_session_blocked_check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/screen.h"

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

  HomeLauncherStateWaiter(const HomeLauncherStateWaiter&) = delete;
  HomeLauncherStateWaiter& operator=(const HomeLauncherStateWaiter&) = delete;

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

  LauncherStateWaiter(const LauncherStateWaiter&) = delete;
  LauncherStateWaiter& operator=(const LauncherStateWaiter&) = delete;

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
};

class LauncherAnimationWaiter : public ui::LayerAnimationObserver {
 public:
  LauncherAnimationWaiter(AppListView* view, base::OnceClosure closure)
      : closure_(std::move(closure)) {
    observation_.Observe(view->GetWidget()->GetLayer()->GetAnimator());
  }
  ~LauncherAnimationWaiter() override = default;
  LauncherAnimationWaiter(const LauncherAnimationWaiter&) = delete;
  LauncherAnimationWaiter& operator=(const LauncherAnimationWaiter&) = delete;

 private:
  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    std::move(closure_).Run();
    delete this;
  }
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {
    OnLayerAnimationEnded(sequence);
  }
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  base::OnceClosure closure_;
  base::ScopedObservation<ui::LayerAnimator, ui::LayerAnimationObserver>
      observation_{this};
};

bool WaitForHomeLauncherState(bool target_visible, base::OnceClosure closure) {
  if (Shell::Get()->app_list_controller()->IsVisible(
          /*display_id=*/std::nullopt) == target_visible) {
    std::move(closure).Run();
    return true;
  }

  new HomeLauncherStateWaiter(target_visible, std::move(closure));
  return false;
}

bool WaitForLauncherAnimation(base::OnceClosure closure) {
  auto* app_list_view =
      Shell::Get()->app_list_controller()->fullscreen_presenter()->GetView();
  if (!app_list_view) {
    std::move(closure).Run();
    return true;
  }
  bool animating =
      app_list_view->GetWidget()->GetLayer()->GetAnimator()->is_animating();
  if (!animating) {
    std::move(closure).Run();
    return true;
  }
  new LauncherAnimationWaiter(app_list_view, std::move(closure));
  return false;
}

}  // namespace

std::vector<raw_ptr<aura::Window, VectorExperimental>> GetAppWindowList() {
  ScopedSkipUserSessionBlockedCheck skip_session_blocked;
  return Shell::Get()->mru_window_tracker()->BuildAppWindowList(kAllDesks);
}

bool WaitForLauncherState(AppListViewState target_state,
                          base::OnceClosure closure) {
  const bool in_tablet_mode = display::Screen::GetScreen()->InTabletMode();
  if (in_tablet_mode) {
    // App-list can't enter kPeeking or kHalf state in tablet mode. Thus
    // |target_state| should be either kClosed, kFullscreenAllApps or
    // kFullscreenSearch.
    DCHECK(target_state == AppListViewState::kClosed ||
           target_state == AppListViewState::kFullscreenAllApps ||
           target_state == AppListViewState::kFullscreenSearch);
  }

  // In the tablet mode, home launcher visibility state needs special handling,
  // as app list view visibility does not match home launcher visibility. The
  // app list view is always visible, but the home launcher may be obscured by
  // app windows. The waiter interprets waits for kClosed state as waits
  // "home launcher not visible" state - note that the app list view
  // is actually expected to be in a visible state.
  AppListViewState effective_target_state =
      in_tablet_mode && target_state == AppListViewState::kClosed
          ? AppListViewState::kFullscreenAllApps
          : target_state;

  std::optional<bool> target_home_launcher_visibility;
  if (in_tablet_mode)
    target_home_launcher_visibility = target_state != AppListViewState::kClosed;

  // Don't wait if the launcher is already in the target state and not
  // animating.
  auto* app_list_view =
      Shell::Get()->app_list_controller()->fullscreen_presenter()->GetView();
  bool animating =
      app_list_view &&
      app_list_view->GetWidget()->GetLayer()->GetAnimator()->is_animating();
  bool at_target_state =
      (!app_list_view && effective_target_state == AppListViewState::kClosed) ||
      (app_list_view &&
       app_list_view->app_list_state() == effective_target_state);

  if (at_target_state && !animating) {
    // In tablet mode, ensure that the home launcher is in the expected state.
    if (target_home_launcher_visibility.has_value()) {
      return WaitForHomeLauncherState(*target_home_launcher_visibility,
                                      std::move(closure));
    }
    std::move(closure).Run();
    return true;
  }

  // In tablet mode, ensure that the home launcher is in the expected state.
  base::OnceClosure callback =
      target_home_launcher_visibility.has_value()
          ? base::BindOnce(base::IgnoreResult(&WaitForHomeLauncherState),
                           *target_home_launcher_visibility, std::move(closure))
          : std::move(closure);
  if (at_target_state)
    return WaitForLauncherAnimation(std::move(callback));
  new LauncherStateWaiter(
      target_state,
      base::BindOnce(base::IgnoreResult(&WaitForLauncherAnimation),
                     std::move(callback)));
  return false;
}

}  // namespace ash
