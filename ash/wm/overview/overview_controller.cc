// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_controller.h"

#include <algorithm>
#include <utility>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/delayed_animation_observer_impl.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_wallpaper_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// It can take up to two frames until the frame created in the UI thread that
// triggered animation observer is drawn. Wait 50ms in attempt to let its draw
// and swap finish.
constexpr base::TimeDelta kOcclusionPauseDurationForStart =
    base::TimeDelta::FromMilliseconds(50);

// Wait longer when exiting overview mode in case when a user may re-enter
// overview mode immediately, contents are ready.
constexpr base::TimeDelta kOcclusionPauseDurationForEnd =
    base::TimeDelta::FromMilliseconds(500);

// Returns the enter/exit type that should be used if kNormal enter/exit type
// was originally requested - if the overview is expected to transition to/from
// the home screen, the normal enter/exit mode is expected to be overridden by
// either slide, or fade to home modes.
// |enter| - Whether |original_type| is used for entering overview.
// |windows| - The list of windows that are displayed in the overview UI.
OverviewSession::EnterExitOverviewType MaybeOverrideEnterExitTypeForHomeScreen(
    OverviewSession::EnterExitOverviewType original_type,
    bool enter,
    const std::vector<aura::Window*>& windows) {
  if (original_type != OverviewSession::EnterExitOverviewType::kNormal)
    return original_type;

  // Use normal type if home launcher is not available.
  if (!Shell::Get()->tablet_mode_controller()->InTabletMode())
    return original_type;

  // Transition to home screen only if all windows are minimized.
  for (const aura::Window* window : windows) {
    if (!WindowState::Get(window)->IsMinimized()) {
      return original_type;
    }
  }

  // If kDragFromShelfToHomeOrOverview is enabled, overview is expected to fade
  // in or out to home screen (when all windows are minimized).
  if (ash::features::IsDragFromShelfToHomeOrOverviewEnabled()) {
    return enter ? OverviewSession::EnterExitOverviewType::kFadeInEnter
                 : OverviewSession::EnterExitOverviewType::kFadeOutExit;
  }

  // When kDragFromShelfToHomeOrOverview is enabled, the original type is
  // overridden even if the list of windows is empty so home screen knows to
  // animate in during overview exit animation (home screen controller uses
  // different show/hide animations depending on the overview exit/enter types).
  if (windows.empty())
    return original_type;

  return enter ? OverviewSession::EnterExitOverviewType::kSlideInEnter
               : OverviewSession::EnterExitOverviewType::kSlideOutExit;
}

}  // namespace

OverviewController::OverviewController()
    : occlusion_pause_duration_for_end_(kOcclusionPauseDurationForEnd),
      overview_wallpaper_controller_(
          std::make_unique<OverviewWallpaperController>()),
      delayed_animation_task_delay_(kTransition) {
  Shell::Get()->activation_client()->AddObserver(this);
}

OverviewController::~OverviewController() {
  Shell::Get()->activation_client()->RemoveObserver(this);
  overview_wallpaper_controller_.reset();

  // Destroy widgets that may be still animating if shell shuts down soon after
  // exiting overview mode.
  for (auto& animation_observer : delayed_animations_)
    animation_observer->Shutdown();
  for (auto& animation_observer : start_animations_)
    animation_observer->Shutdown();

  if (overview_session_) {
    overview_session_->Shutdown();
    overview_session_.reset();
  }
}

bool OverviewController::StartOverview(
    OverviewSession::EnterExitOverviewType type) {
  // No need to start overview if overview is currently active.
  if (InOverviewSession())
    return true;

  if (!CanEnterOverview())
    return false;

  ToggleOverview(type);
  return true;
}

bool OverviewController::EndOverview(
    OverviewSession::EnterExitOverviewType type) {
  // No need to end overview if overview is already ended.
  if (!InOverviewSession())
    return true;

  if (!CanEndOverview(type))
    return false;

  ToggleOverview(type);
  return true;
}

bool OverviewController::InOverviewSession() const {
  return overview_session_ && !overview_session_->is_shutting_down();
}

void OverviewController::IncrementSelection(bool forward) {
  DCHECK(InOverviewSession());
  overview_session_->IncrementSelection(forward);
}

bool OverviewController::AcceptSelection() {
  DCHECK(InOverviewSession());
  return overview_session_->AcceptSelection();
}

void OverviewController::OnOverviewButtonTrayLongPressed(
    const gfx::Point& event_location) {
  // Do nothing if split view is not enabled.
  if (!ShouldAllowSplitView())
    return;

  // Depending on the state of the windows and split view, a long press has many
  // different results.
  // 1. Already in split view - exit split view. The active snapped window
  // becomes maximized. If overview was seen alongside a snapped window, then
  // overview mode ends.
  // 2. Not in overview mode - enter split view iff there is an active window
  // and it is snappable.
  // 3. In overview mode - enter split view iff there are at least two windows
  // in the overview grid for the display where the overview button was long
  // pressed, and the first window in that overview grid is snappable.

  // TODO(crbug.com/970013): Properly implement the multi-display behavior (in
  // tablet position with an external pointing device).
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  // Exit split view mode if we are already in it.
  if (split_view_controller->InSplitViewMode()) {
    // In some cases the window returned by window_util::GetActiveWindow will be
    // an item in overview mode (maybe the overview mode dummy focus widget).
    // The active window may also be a transient descendant of the left or right
    // snapped window, in which we want to activate the transient window's
    // ancestor (left or right snapped window). Manually set |active_window| as
    // either the left or right window.
    aura::Window* active_window = window_util::GetActiveWindow();
    while (::wm::GetTransientParent(active_window))
      active_window = ::wm::GetTransientParent(active_window);
    if (!split_view_controller->IsWindowInSplitView(active_window))
      active_window = split_view_controller->GetDefaultSnappedWindow();
    DCHECK(active_window);
    split_view_controller->EndSplitView();
    EndOverview();
    MaximizeIfSnapped(active_window);
    wm::ActivateWindow(active_window);
    base::RecordAction(
        base::UserMetricsAction("Tablet_LongPressOverviewButtonExitSplitView"));
    return;
  }

  OverviewItem* item_to_snap = nullptr;
  if (!InOverviewSession()) {
    // The current active window may be a transient child.
    aura::Window* active_window = window_util::GetActiveWindow();
    while (active_window && ::wm::GetTransientParent(active_window))
      active_window = ::wm::GetTransientParent(active_window);

    // Do nothing if there are no active windows.
    if (!active_window)
      return;

    // Show a toast if the window cannot be snapped.
    if (!CanSnapInSplitview(active_window)) {
      ShowAppCannotSnapToast();
      return;
    }

    // If we are not in overview mode, enter overview mode and then find the
    // window item to snap.
    StartOverview();
    DCHECK(overview_session_);
    OverviewGrid* current_grid = overview_session_->GetGridWithRootWindow(
        active_window->GetRootWindow());
    if (current_grid)
      item_to_snap = current_grid->GetOverviewItemContaining(active_window);
  } else {
    // Currently in overview mode, with no snapped windows. Retrieve the first
    // overview item and attempt to snap that window.
    DCHECK(overview_session_);
    OverviewGrid* current_grid = overview_session_->GetGridWithRootWindow(
        window_util::GetRootWindowAt(event_location));
    if (current_grid) {
      const auto& windows = current_grid->window_list();
      if (windows.size() > 1)
        item_to_snap = windows[0].get();
    }
  }

  // Do nothing if no item was retrieved, or if the retrieved item is
  // unsnappable.
  if (!item_to_snap || !CanSnapInSplitview(item_to_snap->GetWindow()))
    return;

  split_view_controller->SnapWindow(item_to_snap->GetWindow(),
                                    SplitViewController::LEFT);
  base::RecordAction(
      base::UserMetricsAction("Tablet_LongPressOverviewButtonEnterSplitView"));
}

bool OverviewController::IsInStartAnimation() {
  return !start_animations_.empty();
}

bool OverviewController::IsCompletingShutdownAnimations() const {
  return !delayed_animations_.empty();
}

void OverviewController::PauseOcclusionTracker() {
  if (occlusion_tracker_pauser_)
    return;

  reset_pauser_task_.Cancel();
  occlusion_tracker_pauser_ =
      std::make_unique<aura::WindowOcclusionTracker::ScopedPause>();
}

void OverviewController::UnpauseOcclusionTracker(base::TimeDelta delay) {
  reset_pauser_task_.Reset(base::BindOnce(&OverviewController::ResetPauser,
                                          weak_ptr_factory_.GetWeakPtr()));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, reset_pauser_task_.callback(), delay);
}

void OverviewController::AddObserver(OverviewObserver* observer) {
  observers_.AddObserver(observer);
}

void OverviewController::RemoveObserver(OverviewObserver* observer) {
  observers_.RemoveObserver(observer);
}

void OverviewController::DelayedUpdateRoundedCornersAndShadow() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&OverviewController::UpdateRoundedCornersAndShadow,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OverviewController::AddExitAnimationObserver(
    std::unique_ptr<DelayedAnimationObserver> animation_observer) {
  // No delayed animations should be created when overview mode is set to exit
  // immediately.
  DCHECK_NE(overview_session_->enter_exit_overview_type(),
            OverviewSession::EnterExitOverviewType::kImmediateExit);

  animation_observer->SetOwner(this);
  delayed_animations_.push_back(std::move(animation_observer));
}

void OverviewController::RemoveAndDestroyExitAnimationObserver(
    DelayedAnimationObserver* animation_observer) {
  const bool previous_empty = delayed_animations_.empty();
  base::EraseIf(delayed_animations_,
                base::MatchesUniquePtr(animation_observer));

  // If something has been removed and its the last observer, unblur the
  // wallpaper and let observers know. This function may be called while still
  // in overview (ie. splitview restores one window but leaves overview active)
  // so check that |overview_session_| is null before notifying.
  if (!overview_session_ && !previous_empty && delayed_animations_.empty())
    OnEndingAnimationComplete(/*canceled=*/false);
}

void OverviewController::AddEnterAnimationObserver(
    std::unique_ptr<DelayedAnimationObserver> animation_observer) {
  animation_observer->SetOwner(this);
  start_animations_.push_back(std::move(animation_observer));
}

void OverviewController::RemoveAndDestroyEnterAnimationObserver(
    DelayedAnimationObserver* animation_observer) {
  const bool previous_empty = start_animations_.empty();
  base::EraseIf(start_animations_, base::MatchesUniquePtr(animation_observer));

  if (!previous_empty && start_animations_.empty())
    OnStartingAnimationComplete(/*canceled=*/false);
}

void OverviewController::OnWindowActivating(ActivationReason reason,
                                            aura::Window* gained_active,
                                            aura::Window* lost_active) {
  if (InOverviewSession())
    overview_session_->OnWindowActivating(reason, gained_active, lost_active);
}

void OverviewController::OnAttemptToReactivateWindow(
    aura::Window* request_active,
    aura::Window* actual_active) {
  if (InOverviewSession()) {
    overview_session_->OnWindowActivating(
        ::wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
        request_active, actual_active);
  }
}

bool OverviewController::HasBlurForTest() const {
  return overview_wallpaper_controller_->has_blur();
}

bool OverviewController::HasBlurAnimationForTest() const {
  return overview_wallpaper_controller_->HasBlurAnimationForTesting();
}

std::vector<aura::Window*>
OverviewController::GetWindowsListInOverviewGridsForTest() {
  std::vector<aura::Window*> windows;
  for (const std::unique_ptr<OverviewGrid>& grid :
       overview_session_->grid_list()) {
    for (const auto& overview_item : grid->window_list())
      windows.push_back(overview_item->GetWindow());
  }
  return windows;
}

std::vector<aura::Window*>
OverviewController::GetItemWindowListInOverviewGridsForTest() {
  std::vector<aura::Window*> windows;
  for (const std::unique_ptr<OverviewGrid>& grid :
       overview_session_->grid_list()) {
    for (const auto& overview_item : grid->window_list())
      windows.push_back(overview_item->item_widget()->GetNativeWindow());
  }
  return windows;
}

void OverviewController::ToggleOverview(
    OverviewSession::EnterExitOverviewType type) {
  // Hide the virtual keyboard as it obstructs the overview mode.
  // Don't need to hide if it's the a11y keyboard, as overview mode
  // can accept text input and it resizes correctly with the a11y keyboard.
  keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();

  auto windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);

  // Hidden windows will be removed by window_util::ShouldExcludeForOverview so
  // we must copy them out first.
  std::vector<aura::Window*> hide_windows(windows.size());
  auto end = std::copy_if(
      windows.begin(), windows.end(), hide_windows.begin(),
      [](aura::Window* w) { return w->GetProperty(kHideInOverviewKey); });
  hide_windows.resize(end - hide_windows.begin());
  base::EraseIf(windows, window_util::ShouldExcludeForOverview);
  // Overview windows will handle showing their transient related windows, so if
  // a window in |windows| has a transient root also in |windows|, we can remove
  // it as the transient root will handle showing the window.
  window_util::RemoveTransientDescendants(&windows);

  if (InOverviewSession()) {
    DCHECK(CanEndOverview(type));
    TRACE_EVENT_ASYNC_BEGIN0("ui", "OverviewController::ExitOverview", this);

    // Suspend occlusion tracker until the exit animation is complete.
    PauseOcclusionTracker();

    // We may want to slide out the overview grid in some cases, even if not
    // explicitly stated.
    OverviewSession::EnterExitOverviewType new_type =
        MaybeOverrideEnterExitTypeForHomeScreen(type, /*enter=*/false, windows);
    overview_session_->set_enter_exit_overview_type(new_type);

    overview_session_->set_is_shutting_down(true);

    if (!start_animations_.empty())
      OnStartingAnimationComplete(/*canceled=*/true);
    start_animations_.clear();

    if (type == OverviewSession::EnterExitOverviewType::kSlideOutExit ||
        type == OverviewSession::EnterExitOverviewType::kFadeOutExit ||
        type == OverviewSession::EnterExitOverviewType::kSwipeFromShelf) {
      // Minimize the windows without animations. When the home launcher button
      // is pressed, minimized widgets will get created in their place, and
      // those widgets will be slid out of overview. Otherwise,
      // HomeLauncherGestureHandler will handle sliding the windows out and when
      // this function is called, we do not need to create minimized widgets.
      std::vector<aura::Window*> windows_to_hide_minimize(windows.size());
      auto it = std::copy_if(windows.begin(), windows.end(),
                             windows_to_hide_minimize.begin(),
                             [](aura::Window* window) {
                               return !WindowState::Get(window)->IsMinimized();
                             });
      windows_to_hide_minimize.resize(
          std::distance(windows_to_hide_minimize.begin(), it));
      window_util::HideAndMaybeMinimizeWithoutAnimation(
          windows_to_hide_minimize, true);
    }

    // Do not show mask and show during overview shutdown.
    overview_session_->UpdateRoundedCornersAndShadow();

    for (auto& observer : observers_)
      observer.OnOverviewModeEnding(overview_session_.get());
    overview_session_->Shutdown();

    if (overview_session_->enter_exit_overview_type() ==
        OverviewSession::EnterExitOverviewType::kImmediateExit) {
      for (const auto& animation : delayed_animations_)
        animation->Shutdown();
      delayed_animations_.clear();
    }

    // Don't delete |overview_session_| yet since the stack is still using it.
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
        FROM_HERE, overview_session_.release());
    last_overview_session_time_ = base::Time::Now();
    for (auto& observer : observers_)
      observer.OnOverviewModeEnded();
    if (delayed_animations_.empty())
      OnEndingAnimationComplete(/*canceled=*/false);
  } else {
    DCHECK(CanEnterOverview());
    TRACE_EVENT_ASYNC_BEGIN0("ui", "OverviewController::EnterOverview", this);

    // Clear any animations that may be running from last overview end.
    for (const auto& animation : delayed_animations_)
      animation->Shutdown();
    if (!delayed_animations_.empty())
      OnEndingAnimationComplete(/*canceled=*/true);
    delayed_animations_.clear();

    for (auto& observer : observers_)
      observer.OnOverviewModeWillStart();

    // |should_focus_overview_| shall be true except when split view mode starts
    // on transition between clamshell mode and tablet mode, on transition
    // between user sessions, or on transition between virtual desks. Those are
    // the cases where code arranges split view by first snapping a window on
    // one side and then starting overview to be seen on the other side, meaning
    // that the split view state here will be
    // |SplitViewController::State::kLeftSnapped| or
    // |SplitViewController::State::kRightSnapped|. We have to check the split
    // view state before |SplitViewController::OnOverviewModeStarting|, because
    // in case of |SplitViewController::State::kBothSnapped|, that function will
    // insert one of the two snapped windows to overview.
    should_focus_overview_ = true;
    for (aura::Window* root_window : Shell::GetAllRootWindows()) {
      const SplitViewController::State split_view_state =
          SplitViewController::Get(root_window)->state();
      if (split_view_state == SplitViewController::State::kLeftSnapped ||
          split_view_state == SplitViewController::State::kRightSnapped) {
        should_focus_overview_ = false;
        break;
      }
    }

    // Suspend occlusion tracker until the enter animation is complete.
    PauseOcclusionTracker();

    overview_session_ = std::make_unique<OverviewSession>(this);
    // We may want to slide in the overview grid in some cases, even if not
    // explicitly stated.
    OverviewSession::EnterExitOverviewType new_type =
        MaybeOverrideEnterExitTypeForHomeScreen(type, /*enter=*/true, windows);
    overview_session_->set_enter_exit_overview_type(new_type);
    for (auto& observer : observers_)
      observer.OnOverviewModeStarting();
    overview_session_->Init(windows, hide_windows);

    // When fading in from home, start animating blur immediately (if animation
    // is required) - with this transition the item widgets are positioned in
    // the overview immediately, so delaying blur start until start animations
    // finish looks janky.
    overview_wallpaper_controller_->Blur(
        /*animate_only=*/new_type ==
        OverviewSession::EnterExitOverviewType::kFadeInEnter);

    // For app dragging, there are no start animations so add a delay to delay
    // animations observing when the start animation ends, such as the shelf,
    // shadow and rounded corners.
    if (new_type == OverviewSession::EnterExitOverviewType::kImmediateEnter &&
        !delayed_animation_task_delay_.is_zero()) {
      auto force_delay_observer =
          std::make_unique<ForceDelayObserver>(delayed_animation_task_delay_);
      AddEnterAnimationObserver(std::move(force_delay_observer));
    }

    if (start_animations_.empty())
      OnStartingAnimationComplete(/*canceled=*/false);

    if (!last_overview_session_time_.is_null()) {
      UMA_HISTOGRAM_LONG_TIMES("Ash.WindowSelector.TimeBetweenUse",
                               base::Time::Now() - last_overview_session_time_);
    }
  }
}

bool OverviewController::CanEnterOverview() {
  // Prevent toggling overview during the split view divider snap animation.
  if (SplitViewController::Get(Shell::GetPrimaryRootWindow())
          ->IsDividerAnimating()) {
    return false;
  }

  // Don't allow a window overview if the user session is not active (e.g.
  // locked or in user-adding screen) or a modal dialog is open or running in
  // kiosk app session.
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  return session_controller->GetSessionState() ==
             session_manager::SessionState::ACTIVE &&
         !Shell::IsSystemModalWindowOpen() &&
         !Shell::Get()->screen_pinning_controller()->IsPinned() &&
         !session_controller->IsRunningInAppMode();
}

bool OverviewController::CanEndOverview(
    OverviewSession::EnterExitOverviewType type) {
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  // Prevent toggling overview during the split view divider snap animation.
  if (split_view_controller->IsDividerAnimating())
    return false;

  // Do not allow ending overview if we're in single split mode unless swiping
  // up from the shelf in tablet mode, or ending overview immediately without
  // animations.
  if (split_view_controller->InTabletSplitViewMode() &&
      split_view_controller->state() !=
          SplitViewController::State::kBothSnapped &&
      InOverviewSession() && overview_session_->IsEmpty() &&
      type != OverviewSession::EnterExitOverviewType::kSwipeFromShelf &&
      type != OverviewSession::EnterExitOverviewType::kImmediateExit) {
    return false;
  }

  return true;
}

void OverviewController::OnStartingAnimationComplete(bool canceled) {
  DCHECK(overview_session_);

  // For kFadeInEnter, wallpaper blur is initiated on transition start,
  // so it doesn't have to be requested again on starting animation end.
  if (!canceled && overview_session_->enter_exit_overview_type() !=
                       OverviewSession::EnterExitOverviewType::kFadeInEnter) {
    overview_wallpaper_controller_->Blur(/*animate_only=*/true);
  }

  for (auto& observer : observers_)
    observer.OnOverviewModeStartingAnimationComplete(canceled);
  overview_session_->OnStartingAnimationComplete(canceled,
                                                 should_focus_overview_);
  UnpauseOcclusionTracker(kOcclusionPauseDurationForStart);
  TRACE_EVENT_ASYNC_END1("ui", "OverviewController::EnterOverview", this,
                         "canceled", canceled);
}

void OverviewController::OnEndingAnimationComplete(bool canceled) {
  // Unblur when animation is completed (or right away if there was no
  // delayed animation) unless it's canceled, in which case, we should keep
  // the blur.
  if (!canceled)
    overview_wallpaper_controller_->Unblur();

  for (auto& observer : observers_)
    observer.OnOverviewModeEndingAnimationComplete(canceled);
  UnpauseOcclusionTracker(occlusion_pause_duration_for_end_);
  TRACE_EVENT_ASYNC_END1("ui", "OverviewController::ExitOverview", this,
                         "canceled", canceled);
}

void OverviewController::ResetPauser() {
  occlusion_tracker_pauser_.reset();
}

void OverviewController::UpdateRoundedCornersAndShadow() {
  if (overview_session_)
    overview_session_->UpdateRoundedCornersAndShadow();
}

}  // namespace ash
