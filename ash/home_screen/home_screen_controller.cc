// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/home_screen_controller.h"

#include <memory>
#include <vector>

#include "ash/home_screen/home_launcher_gesture_handler.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "ash/home_screen/window_scale_animation.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "base/barrier_closure.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"

namespace ash {
namespace {

// Minimizes all windows in |windows| that aren't in the home screen container,
// and are not in |windows_to_ignore|. Done in reverse order to preserve the mru
// ordering.
// Returns true if any windows are minimized.
bool MinimizeAllWindows(const aura::Window::Windows& windows,
                        const aura::Window::Windows& windows_to_ignore) {
  aura::Window* container = Shell::Get()->GetPrimaryRootWindow()->GetChildById(
      kShellWindowId_HomeScreenContainer);
  aura::Window::Windows windows_to_minimize;
  for (auto it = windows.rbegin(); it != windows.rend(); it++) {
    if (!container->Contains(*it) && !base::Contains(windows_to_ignore, *it) &&
        !WindowState::Get(*it)->IsMinimized()) {
      windows_to_minimize.push_back(*it);
    }
  }

  window_util::HideAndMaybeMinimizeWithoutAnimation(windows_to_minimize,
                                                    /*minimize=*/true);
  return !windows_to_minimize.empty();
}

}  // namespace

HomeScreenController::HomeScreenController()
    : home_launcher_gesture_handler_(
          std::make_unique<HomeLauncherGestureHandler>()) {
  Shell::Get()->overview_controller()->AddObserver(this);
  Shell::Get()->wallpaper_controller()->AddObserver(this);
}

HomeScreenController::~HomeScreenController() {
  Shell::Get()->wallpaper_controller()->RemoveObserver(this);
  Shell::Get()->overview_controller()->RemoveObserver(this);
}

void HomeScreenController::Show() {
  DCHECK(Shell::Get()->tablet_mode_controller()->InTabletMode());

  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    return;

  delegate_->ShowHomeScreenView();
  UpdateVisibility();

  aura::Window* window = delegate_->GetHomeScreenWindow();
  if (window)
    Shelf::ForWindow(window)->MaybeUpdateShelfBackground();
}

bool HomeScreenController::GoHome(int64_t display_id) {
  DCHECK(Shell::Get()->tablet_mode_controller()->InTabletMode());

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  const bool split_view_active = split_view_controller->InSplitViewMode();

  if (!features::IsDragFromShelfToHomeOrOverviewEnabled()) {
    if (home_launcher_gesture_handler_->ShowHomeLauncher(
            Shell::Get()->display_manager()->GetDisplayForId(display_id))) {
      return true;
    }

    if (overview_controller->InOverviewSession()) {
      // End overview mode.
      overview_controller->EndOverview(
          OverviewSession::EnterExitOverviewType::kSlideOutExit);
      return true;
    }

    if (split_view_active) {
      // End split view mode.
      split_view_controller->EndSplitView(
          SplitViewController::EndReason::kHomeLauncherPressed);
      return true;
    }

    // The home screen opens for the current active desk, there's no need to
    // minimize windows in the inactive desks.
    if (MinimizeAllWindows(
            Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(
                kActiveDesk),
            {} /*windows_to_ignore*/)) {
      return true;
    }

    return false;
  }

  // The home screen opens for the current active desk, there's no need to
  // minimize windows in the inactive desks.
  aura::Window::Windows windows =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);

  // The foreground window or windows (for split mode) - the windows that will
  // not be minimized without animations (instead they wil bee animated into the
  // home screen).
  std::vector<aura::Window*> active_windows;
  if (split_view_active) {
    active_windows = {split_view_controller->left_window(),
                      split_view_controller->right_window()};
    base::EraseIf(active_windows, [](aura::Window* window) { return !window; });
  } else if (!windows.empty() && !WindowState::Get(windows[0])->IsMinimized()) {
    active_windows.push_back(windows[0]);
  }

  if (split_view_active) {
    // End split view mode.
    split_view_controller->EndSplitView(
        SplitViewController::EndReason::kHomeLauncherPressed);

    // If overview session is active (e.g. on one side of the split view), end
    // it immediately, to prevent overview UI being visible while transitioning
    // to home screen.
    if (overview_controller->InOverviewSession()) {
      overview_controller->EndOverview(
          OverviewSession::EnterExitOverviewType::kImmediateExit);
    }
  }

  // If overview is active (if overview was active in split view, it exited by
  // this point), just fade it out to home screen.
  if (overview_controller->InOverviewSession()) {
    overview_controller->EndOverview(
        OverviewSession::EnterExitOverviewType::kFadeOutExit);
    return true;
  }

  // First minimize all inactive windows.
  const bool window_minimized =
      MinimizeAllWindows(windows, active_windows /*windows_to_ignore*/);

  // Animate currently active windows into the home screen - they will be
  // minimized by WindowTransformToHomeScreenAnimation when the transition
  // finishes.
  if (!active_windows.empty()) {
    {
      // Disable window animations before updating home launcher target
      // position. Calling OnHomeLauncherPositionChanged() can cause
      // display work area update, and resulting cross-fade window bounds change
      // animation can interfere with WindowTransformToHomeScreenAnimation
      // visuals.
      //
      // TODO(https://crbug.com/1019531): This can be removed once transitions
      // between in-app state and home do not cause work area updates.
      std::vector<std::unique_ptr<ScopedAnimationDisabler>> animation_disablers;
      for (auto* window : active_windows) {
        animation_disablers.push_back(
            std::make_unique<ScopedAnimationDisabler>(window));
      }

      delegate_->OnHomeLauncherPositionChanged(100 /* percent_shown */,
                                               display_id);
    }

    base::RepeatingClosure window_transforms_callback = base::BarrierClosure(
        active_windows.size(),
        base::BindOnce(&HomeScreenController::NotifyHomeLauncherTransitionEnded,
                       weak_ptr_factory_.GetWeakPtr(), true /*shown*/,
                       display_id));

    for (auto* active_window : active_windows) {
      BackdropWindowMode original_backdrop_mode =
          active_window->GetProperty(kBackdropWindowMode);
      active_window->SetProperty(kBackdropWindowMode,
                                 BackdropWindowMode::kDisabled);

      // Do the scale-down transform for the entire transient tree.
      for (auto* window : GetTransientTreeIterator(active_window)) {
        // Self-destructed when window transform animation is done.
        new WindowScaleAnimation(
            window,
            WindowScaleAnimation::WindowScaleType::kScaleDownToHomeScreen,
            window == active_window
                ? base::make_optional(original_backdrop_mode)
                : base::nullopt,
            window == active_window ? window_transforms_callback
                                    : base::NullCallback());
      }
    }
  }

  return window_minimized || !active_windows.empty();
}

void HomeScreenController::NotifyHomeLauncherTransitionEnded(
    bool shown,
    int64_t display_id) {
  if (delegate_)
    delegate_->OnHomeLauncherAnimationComplete(shown, display_id);
}

void HomeScreenController::SetDelegate(HomeScreenDelegate* delegate) {
  delegate_ = delegate;
}

void HomeScreenController::OnWindowDragStarted() {
  in_window_dragging_ = true;
  UpdateVisibility();
}

void HomeScreenController::OnWindowDragEnded() {
  in_window_dragging_ = false;
  UpdateVisibility();
}

bool HomeScreenController::IsHomeScreenVisible() const {
  return delegate_->IsHomeScreenVisible();
}

void HomeScreenController::OnOverviewModeStarting() {
  const OverviewSession::EnterExitOverviewType overview_enter_type =
      Shell::Get()
          ->overview_controller()
          ->overview_session()
          ->enter_exit_overview_type();

  const bool animate =
      overview_enter_type ==
          OverviewSession::EnterExitOverviewType::kSlideInEnter ||
      overview_enter_type ==
          OverviewSession::EnterExitOverviewType::kFadeInEnter;
  const HomeScreenPresenter::TransitionType transition =
      overview_enter_type ==
              OverviewSession::EnterExitOverviewType::kFadeInEnter
          ? HomeScreenPresenter::TransitionType::kScaleHomeOut
          : HomeScreenPresenter::TransitionType::kSlideHomeOut;

  home_screen_presenter_.ScheduleOverviewModeAnimation(transition, animate);
}

void HomeScreenController::OnOverviewModeEnding(
    OverviewSession* overview_session) {
  // The launcher will be shown after overview mode finishes animating, in
  // OnOverviewModeEndingAnimationComplete(). Overview however is nullptr by
  // the time the animations are finished, so cache the exit type here.
  overview_exit_type_ =
      base::make_optional(overview_session->enter_exit_overview_type());

  // If the overview is fading out, start the home screen animation in parallel.
  // Otherwise the transition will be initiated in
  // OnOverviewModeEndingAnimationComplete().
  if (overview_session->enter_exit_overview_type() ==
      OverviewSession::EnterExitOverviewType::kFadeOutExit) {
    home_screen_presenter_.ScheduleOverviewModeAnimation(
        HomeScreenPresenter::TransitionType::kScaleHomeIn, true /*animate*/);

    // Make sure the window visibility is updated, in case it was previously
    // hidden due to overview being shown.
    UpdateVisibility();
  }
}

void HomeScreenController::OnOverviewModeEndingAnimationComplete(
    bool canceled) {
  DCHECK(overview_exit_type_.has_value());

  // For kFadeOutExit EnterExitOverviewType, the home animation is scheduled in
  // OnOverviewModeEnding(), so there is nothing else to do at this point.
  if (canceled || *overview_exit_type_ ==
                      OverviewSession::EnterExitOverviewType::kFadeOutExit) {
    overview_exit_type_ = base::nullopt;
    return;
  }

  const bool animate =
      *overview_exit_type_ ==
          OverviewSession::EnterExitOverviewType::kSlideOutExit ||
      *overview_exit_type_ ==
          OverviewSession::EnterExitOverviewType::kFadeOutExit;
  const HomeScreenPresenter::TransitionType transition =
      *overview_exit_type_ ==
              OverviewSession::EnterExitOverviewType::kFadeOutExit
          ? HomeScreenPresenter::TransitionType::kScaleHomeIn
          : HomeScreenPresenter::TransitionType::kSlideHomeIn;
  overview_exit_type_ = base::nullopt;

  home_screen_presenter_.ScheduleOverviewModeAnimation(transition, animate);

  // Make sure the window visibility is updated, in case it was previously
  // hidden due to overview being shown.
  UpdateVisibility();
}

void HomeScreenController::OnWallpaperPreviewStarted() {
  in_wallpaper_preview_ = true;
  UpdateVisibility();
}

void HomeScreenController::OnWallpaperPreviewEnded() {
  in_wallpaper_preview_ = false;
  UpdateVisibility();
}

void HomeScreenController::UpdateVisibility() {
  if (!Shell::Get()->tablet_mode_controller()->InTabletMode())
    return;

  aura::Window* window = delegate_->GetHomeScreenWindow();
  if (!window)
    return;

  const bool in_overview =
      Shell::Get()->overview_controller()->InOverviewSession();
  if (in_overview || in_wallpaper_preview_ || in_window_dragging_)
    window->Hide();
  else
    window->Show();
}

}  // namespace ash
