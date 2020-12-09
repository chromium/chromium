// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/home_screen_controller.h"

#include <memory>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "ash/home_screen/window_scale_animation.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/metrics_util.h"
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
#include "base/bind.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/display/manager/display_manager.h"
#include "ui/wm/core/window_animations.h"

namespace ash {
namespace {

constexpr char kHomescreenAnimationHistogram[] =
    "Ash.Homescreen.AnimationSmoothness";

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

  window_util::MinimizeAndHideWithoutAnimation(windows_to_minimize);
  return !windows_to_minimize.empty();
}

// Layer animation observer that waits for layer animator to schedule, and
// complete animations. When all animations complete, it fires |callback| and
// deletes itself.
class WindowAnimationsCallback : public ui::LayerAnimationObserver {
 public:
  WindowAnimationsCallback(base::OnceClosure callback,
                           ui::LayerAnimator* animator)
      : callback_(std::move(callback)), animator_(animator) {
    animator_->AddObserver(this);
  }
  ~WindowAnimationsCallback() override { animator_->RemoveObserver(this); }

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    FireCallbackIfDone();
  }
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {
    FireCallbackIfDone();
  }
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}
  void OnDetachedFromSequence(ui::LayerAnimationSequence* sequence) override {
    FireCallbackIfDone();
  }

 private:
  // Fires the callback if all scheduled animations completed (either ended or
  // got aborted).
  void FireCallbackIfDone() {
    if (!callback_ || animator_->is_animating())
      return;
    std::move(callback_).Run();
    delete this;
  }

  base::OnceClosure callback_;
  ui::LayerAnimator* animator_;
};

}  // namespace

HomeScreenController::HomeScreenController() {
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

  auto* app_list_controller = Shell::Get()->app_list_controller();
  if (app_list_controller->IsShowingEmbeddedAssistantUI()) {
    app_list_controller->presenter()->ShowEmbeddedAssistantUI(false);
  }
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  const bool split_view_active = split_view_controller->InSplitViewMode();

  // The home screen opens for the current active desk, there's no need to
  // minimize windows in the inactive desks.
  aura::Window::Windows windows =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);

  // The foreground window or windows (for split mode) - the windows that will
  // not be minimized without animations (instead they wil bee animated into the
  // home screen).
  std::vector<aura::Window*> foreground_windows;
  if (split_view_active) {
    foreground_windows = {split_view_controller->left_window(),
                          split_view_controller->right_window()};
    base::EraseIf(foreground_windows,
                  [](aura::Window* window) { return !window; });
  } else if (!windows.empty() && !WindowState::Get(windows[0])->IsMinimized()) {
    foreground_windows.push_back(windows[0]);
  }

  if (split_view_active) {
    // If overview session is active (e.g. on one side of the split view), end
    // it immediately, to prevent overview UI being visible while transitioning
    // to home screen.
    if (overview_controller->InOverviewSession())
      overview_controller->EndOverview(OverviewEnterExitType::kImmediateExit);

    // End split view mode.
    split_view_controller->EndSplitView(
        SplitViewController::EndReason::kHomeLauncherPressed);
  }

  // If overview is active (if overview was active in split view, it exited by
  // this point), just fade it out to home screen.
  if (overview_controller->InOverviewSession()) {
    overview_controller->EndOverview(OverviewEnterExitType::kFadeOutExit);
    return true;
  }

  // First minimize all inactive windows.
  const bool window_minimized =
      MinimizeAllWindows(windows, foreground_windows /*windows_to_ignore*/);

  if (foreground_windows.empty())
    return window_minimized;

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
    for (auto* window : foreground_windows) {
      animation_disablers.push_back(
          std::make_unique<ScopedAnimationDisabler>(window));
    }

    delegate_->OnHomeLauncherPositionChanged(100 /* percent_shown */,
                                             display_id);
  }

  StartTrackingAnimationSmoothness(display_id);

  base::RepeatingClosure window_transforms_callback = base::BarrierClosure(
      foreground_windows.size(),
      base::BindOnce(&HomeScreenController::NotifyHomeLauncherTransitionEnded,
                     weak_ptr_factory_.GetWeakPtr(), true /*shown*/,
                     display_id));

  // Minimize currently active windows, but this time, using animation.
  // Home screen will show when all the windows are done minimizing.
  for (auto* foreground_window : foreground_windows) {
    if (::wm::WindowAnimationsDisabled(foreground_window)) {
      WindowState::Get(foreground_window)->Minimize();
      window_transforms_callback.Run();
    } else {
      // Create animator observer that will fire |window_transforms_callback|
      // once the window layer stops animating - it deletes itself when
      // animations complete.
      new WindowAnimationsCallback(window_transforms_callback,
                                   foreground_window->layer()->GetAnimator());
      WindowState::Get(foreground_window)->Minimize();
    }
  }

  return true;
}

void HomeScreenController::NotifyHomeLauncherTransitionEnded(
    bool shown,
    int64_t display_id) {
  RecordAnimationSmoothness();
  if (delegate_)
    delegate_->OnHomeLauncherAnimationComplete(shown, display_id);
}

void HomeScreenController::SetDelegate(HomeScreenDelegate* delegate) {
  delegate_ = delegate;
}

void HomeScreenController::OnWindowDragStarted() {
  in_window_dragging_ = true;
  UpdateVisibility();

  // Dismiss Assistant if it's running when a window drag starts.
  if (Shell::Get()->app_list_controller()->IsShowingEmbeddedAssistantUI()) {
    Shell::Get()->app_list_controller()->presenter()->ShowEmbeddedAssistantUI(
        false);
  }
}

void HomeScreenController::OnWindowDragEnded(bool animate) {
  in_window_dragging_ = false;
  UpdateVisibility();
  if (ShouldShowHomeScreen()) {
    home_screen_presenter_.ScheduleOverviewModeAnimation(
        HomeScreenPresenter::TransitionType::kScaleHomeIn, animate);
  }
}

bool HomeScreenController::IsHomeScreenVisible() const {
  return delegate_->IsHomeScreenVisible();
}

void HomeScreenController::StartTrackingAnimationSmoothness(
    int64_t display_id) {
  auto* root_window = Shell::GetRootWindowForDisplayId(display_id);
  auto* compositor = root_window->layer()->GetCompositor();
  smoothness_tracker_ = compositor->RequestNewThroughputTracker();
  smoothness_tracker_->Start(
      metrics_util::ForSmoothness(base::BindRepeating([](int smoothness) {
        UMA_HISTOGRAM_PERCENTAGE(kHomescreenAnimationHistogram, smoothness);
      })));
}

void HomeScreenController::RecordAnimationSmoothness() {
  if (!smoothness_tracker_)
    return;
  smoothness_tracker_->Stop();
  smoothness_tracker_.reset();
}

void HomeScreenController::OnAppListViewShown() {
  split_view_observer_.Add(
      SplitViewController::Get(delegate_->GetHomeScreenWindow()));
  UpdateVisibility();
}

void HomeScreenController::OnAppListViewClosing() {
  split_view_observer_.RemoveAll();
}

void HomeScreenController::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  UpdateVisibility();
}

void HomeScreenController::OnOverviewModeStarting() {
  const OverviewEnterExitType overview_enter_type =
      Shell::Get()
          ->overview_controller()
          ->overview_session()
          ->enter_exit_overview_type();

  const bool animate =
      IsHomeScreenVisible() &&
      overview_enter_type == OverviewEnterExitType::kFadeInEnter;

  home_screen_presenter_.ScheduleOverviewModeAnimation(
      HomeScreenPresenter::TransitionType::kScaleHomeOut, animate);
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
      OverviewEnterExitType::kFadeOutExit) {
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

  // For kFadeOutExit OverviewEnterExitType, the home animation is scheduled in
  // OnOverviewModeEnding(), so there is nothing else to do at this point.
  if (canceled || *overview_exit_type_ == OverviewEnterExitType::kFadeOutExit) {
    overview_exit_type_ = base::nullopt;
    return;
  }

  const bool animate =
      *overview_exit_type_ == OverviewEnterExitType::kFadeOutExit;
  overview_exit_type_ = base::nullopt;

  home_screen_presenter_.ScheduleOverviewModeAnimation(
      HomeScreenPresenter::TransitionType::kScaleHomeIn, animate);

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

  if (ShouldShowHomeScreen())
    window->Show();
  else
    window->Hide();
}

bool HomeScreenController::ShouldShowHomeScreen() const {
  if (in_window_dragging_ || in_wallpaper_preview_)
    return false;

  auto* window = delegate_->GetHomeScreenWindow();
  if (!window)
    return false;

  auto* shell = Shell::Get();
  if (!shell->tablet_mode_controller()->InTabletMode())
    return false;
  if (shell->overview_controller()->InOverviewSession())
    return false;

  return !SplitViewController::Get(window)->InSplitViewMode();
}

}  // namespace ash
