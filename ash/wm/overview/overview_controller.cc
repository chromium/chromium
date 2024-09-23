// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_controller.h"

#include <utility>
#include <vector>

#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/gestures/wm_gesture_handler.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/delayed_animation_observer_impl.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_restore/informed_restore_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

OverviewController* g_instance = nullptr;

// It can take up to two frames until the frame created in the UI thread that
// triggered animation observer is drawn. Wait 50ms in attempt to let its draw
// and swap finish.
constexpr base::TimeDelta kOcclusionPauseDurationForStart =
    base::Milliseconds(50);

// Wait longer when exiting overview mode in case when a user may re-enter
// overview mode immediately, contents are ready.
constexpr base::TimeDelta kOcclusionPauseDurationForEnd =
    base::Milliseconds(500);

// Returns the enter/exit type that should be used if `kNormal` enter/exit type
// was originally requested. Used in two cases:
// 1) If the overview is expected to transition to/from the home screen, the
// normal enter/exit mode is expected to be overridden by either slide, or fade
// to home modes.
// 2) If overview is an informed restore session, the normal enter/exit mode is
// to overridden by `kInformedRestore`. `enter` - Whether `original_type` is
// used for entering overview. `windows` - The list of windows that are
// displayed in the overview UI.
OverviewEnterExitType MaybeOverrideEnterExitType(
    OverviewEnterExitType original_type,
    bool enter,
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows) {
  if (original_type != OverviewEnterExitType::kNormal) {
    return original_type;
  }

  if (features::IsForestFeatureEnabled() &&
      !!Shell::Get()->informed_restore_controller()->contents_data()) {
    return OverviewEnterExitType::kInformedRestore;
  }

  // Use normal type if home launcher is not available.
  if (!display::Screen::GetScreen()->InTabletMode()) {
    return original_type;
  }

  // Transition to home screen only if all windows are minimized.
  for (const aura::Window* window : windows) {
    if (!WindowState::Get(window)->IsMinimized()) {
      return original_type;
    }
  }

  // The original type is overridden even if the list of windows is empty so
  // home screen knows to animate in during overview exit animation (home screen
  // controller uses different show/hide animations depending on the overview
  // exit/enter types).
  return enter ? OverviewEnterExitType::kFadeInEnter
               : OverviewEnterExitType::kFadeOutExit;
}

}  // namespace

OverviewController::ScopedOcclusionPauser::ScopedOcclusionPauser(
    ScopedOcclusionPauser&&) = default;
OverviewController::ScopedOcclusionPauser&
OverviewController::ScopedOcclusionPauser::operator=(ScopedOcclusionPauser&&) =
    default;

OverviewController::ScopedOcclusionPauser::~ScopedOcclusionPauser() {
  if (controller_) {
    controller_->MaybeUnpauseOcclusionTracker(unpause_delay_);
  }
}

OverviewController::ScopedOcclusionPauser::ScopedOcclusionPauser(
    base::WeakPtr<OverviewController> controller,
    base::TimeDelta unpause_delay)
    : controller_(controller), unpause_delay_(unpause_delay) {
  controller_->MaybePauseOcclusionTracker();
}

OverviewController::OverviewController()
    : occlusion_pause_duration_for_start_(kOcclusionPauseDurationForStart),
      occlusion_pause_duration_for_end_(kOcclusionPauseDurationForEnd),
      delayed_animation_task_delay_(kTransition),
      // TODO(crbug.com/40208263): Lacros windows now have a snapshot, but their
      // behavior may be a bit worse than ash windows. Keep this snapshot code
      // until we confirm it is fine to show lacros snapshotted windows all the
      // time.
      windows_have_snapshot_(true),
      overview_window_occlusion_calculator_(this) {
  Shell::Get()->activation_client()->AddObserver(this);
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

OverviewController::~OverviewController() {
  Shell::Get()->activation_client()->RemoveObserver(this);

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

  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

OverviewController::ScopedOcclusionPauser
OverviewController::PauseOcclusionTracker(base::TimeDelta unpause_delay) {
  return ScopedOcclusionPauser(weak_ptr_factory_.GetWeakPtr(), unpause_delay);
}

// static
OverviewController* OverviewController::Get() {
  CHECK(g_instance);
  return g_instance;
}

bool OverviewController::StartOverview(OverviewStartAction start_action,
                                       OverviewEnterExitType type) {
  // No need to start overview if overview is currently active.
  if (InOverviewSession())
    return true;

  if (!CanEnterOverview())
    return false;

  session_metrics_recorder_.emplace(start_action, this);
  ToggleOverview(type);
  return true;
}

bool OverviewController::EndOverview(OverviewEndAction end_action,
                                     OverviewEnterExitType type) {
  // No need to end overview if overview is already ended.
  if (!InOverviewSession())
    return true;

  if (!CanEndOverview(type))
    return false;

  overview_session_->set_overview_end_action(end_action);
  ToggleOverview(type);

  // If there is an undo toast active and the toast was created when ChromeVox
  // was enabled, then we need to close the toast when overview closes.
  DesksController::Get()->MaybeDismissPersistentDeskRemovalToast();

  return true;
}

bool OverviewController::CanEnterOverview() const {
  if (!DesksController::Get()->CanEnterOverview()) {
    return false;
  }

  // Don't allow entering overview if there is a system modal dialog or chromeOS
  // is running in kiosk app session.
  Shell* shell = Shell::Get();
  if (Shell::IsSystemModalWindowOpen() ||
      shell->screen_pinning_controller()->IsPinned()) {
    return false;
  }

  // Don't allow a window overview if the user session is not active or
  // transitioning to active.
  const session_manager::SessionState session_state =
      shell->session_controller()->GetSessionState();
  return session_state == session_manager::SessionState::ACTIVE ||
         session_state == session_manager::SessionState::LOGGED_IN_NOT_ACTIVE;
}

bool OverviewController::InOverviewSession() const {
  return overview_session_ && !overview_session_->is_shutting_down();
}

bool OverviewController::HandleContinuousScroll(float y_offset,
                                                OverviewEnterExitType type) {
  // We enter with type `kNormal` if a fast scroll happened and we want to enter
  // overview mode immediately, using ToggleOverview().
  CHECK((type ==
         OverviewEnterExitType::kContinuousAnimationEnterOnScrollUpdate) ||
        (type == OverviewEnterExitType::kNormal));

  // Determine if this is the last scroll update in this continuous scroll.
  is_continuous_scroll_in_progress_ =
      y_offset != WmGestureHandler::kVerticalThresholdDp &&
      type != OverviewEnterExitType::kNormal;

  if (!overview_session_) {
    session_metrics_recorder_.emplace(
        OverviewStartAction::k3FingerVerticalScroll, this);
    ToggleOverview(type);
    return true;
  }

  overview_session_->set_enter_exit_overview_type(type);
  return overview_session_->HandleContinuousScrollIntoOverview(y_offset);
}

void OverviewController::IncrementSelection(bool forward) {
  DCHECK(InOverviewSession());
  overview_session_->IncrementSelection(forward);
}

bool OverviewController::AcceptSelection() {
  DCHECK(InOverviewSession());
  return overview_session_->AcceptSelection();
}

bool OverviewController::IsInStartAnimation() {
  return !start_animations_.empty();
}

bool OverviewController::IsCompletingShutdownAnimations() const {
  return !delayed_animations_.empty();
}

void OverviewController::AddObserver(OverviewObserver* observer) {
  observers_.AddObserver(observer);
}

void OverviewController::RemoveObserver(OverviewObserver* observer) {
  observers_.RemoveObserver(observer);
}

void OverviewController::DelayedUpdateRoundedCornersAndShadow() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OverviewController::UpdateRoundedCornersAndShadow,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OverviewController::AddExitAnimationObserver(
    std::unique_ptr<DelayedAnimationObserver> animation_observer) {
  // No delayed animations should be created when overview mode is set to exit
  // immediately.
  DCHECK(IsCompletingShutdownAnimations() ||
         overview_session_->enter_exit_overview_type() !=
             OverviewEnterExitType::kImmediateExit);

  animation_observer->SetOwner(this);
  delayed_animations_.push_back(std::move(animation_observer));
}

void OverviewController::RemoveAndDestroyExitAnimationObserver(
    DelayedAnimationObserver* animation_observer) {
  const bool previous_empty = delayed_animations_.empty();
  std::erase_if(delayed_animations_,
                base::MatchesUniquePtr(animation_observer));

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
  std::erase_if(start_animations_, base::MatchesUniquePtr(animation_observer));

  if (!previous_empty && start_animations_.empty())
    OnStartingAnimationComplete(/*canceled=*/false);
}

void OverviewController::OnWindowActivating(ActivationReason reason,
                                            aura::Window* gained_active,
                                            aura::Window* lost_active) {
  if (InOverviewSession())
    overview_session_->OnWindowActivating(reason, gained_active, lost_active);
}

base::AutoReset<bool> OverviewController::SetDisableAppIdCheckForTests() {
  return {&disable_app_id_check_for_saved_desks_, true};
}

void OverviewController::ToggleOverview(OverviewEnterExitType type) {
  // Pause raster scale updates while the overview is being toggled. This is to
  // handle the case where a mirror view is deleted then recreated when
  // cancelling an overview exit animation, for example.
  aura::WindowOcclusionTracker::ScopedPause scoped_pause_occlusion;
  auto scoped_pause_raster =
      std::make_optional<ScopedPauseRasterScaleUpdates>();

  // Hide the virtual keyboard as it obstructs the overview mode.
  // Don't need to hide if it's the a11y keyboard, as overview mode
  // can accept text input and it resizes correctly with the a11y keyboard.
  keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();

  auto windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);

  // Hidden windows are a subset of the window excluded from overview in
  // window_util::ShouldExcludeForOverview. Excluded window won't be on the grid
  // but their visibility will remain untouched. Hidden windows will be also
  // excluded and their visibility will be set to false for the duration of
  // overview mode.
  auto should_hide_for_overview = [](aura::Window* w) -> bool {
    // Explicity hidden windows always get hidden.
    if (w->GetProperty(kHideInOverviewKey))
      return true;
    // Since overview allows moving windows, don't show window that can't be
    // moved. If they are a transient ancestor of a postionable window then they
    // can be shown and moved with their transient root.
    return w == wm::GetTransientRoot(w) &&
           !WindowState::Get(w)->IsUserPositionable();
  };
  std::vector<raw_ptr<aura::Window, VectorExperimental>> hide_windows(
      windows.size());
  auto end = base::ranges::copy_if(windows, hide_windows.begin(),
                                   should_hide_for_overview);
  hide_windows.resize(end - hide_windows.begin());
  std::erase_if(windows, window_util::ShouldExcludeForOverview);
  // Overview windows will handle showing their transient related windows, so if
  // a window in |windows| has a transient root also in |windows|, we can remove
  // it as the transient root will handle showing the window.
  // Additionally, |windows| may contain transient children and not their
  // transient root. This can lead to situations where the transient child is
  // destroyed causing its respective overview item to be destroyed, leaving its
  // transient root with no overview item. Creating the overview item with the
  // transient root instead of the transient child fixes this. See
  // crbug.com/972015.
  window_util::EnsureTransientRoots(&windows);

  if (InOverviewSession()) {
    DCHECK(CanEndOverview(type));
    CHECK(session_metrics_recorder_);
    session_metrics_recorder_->OnOverviewSessionEnding();
    // Suspend occlusion tracker until the exit animation is complete.
    exit_pauser_ = PauseOcclusionTracker(occlusion_pause_duration_for_end_);

    // We may want to slide out the overview grid in some cases, even if not
    // explicitly stated. We may also want to enter a informed restore session
    // in some cases, even if not explicitly stated.
    OverviewEnterExitType new_type =
        MaybeOverrideEnterExitType(type, /*enter=*/false, windows);
    overview_session_->set_enter_exit_overview_type(new_type);

    overview_session_->set_is_shutting_down(true);

    if (!start_animations_.empty())
      OnStartingAnimationComplete(/*canceled=*/true);
    start_animations_.clear();

    for (auto& observer : observers_) {
      observer.OnOverviewModeEnding(overview_session_.get());
    }

    if (type == OverviewEnterExitType::kFadeOutExit) {
      // FadeOutExit is used for transition to the home launcher. Minimize the
      // windows without animations to prevent them from getting maximized
      // during overview exit. Minimized widgets will get created in their
      // place, and those widgets will fade out of overview.
      std::vector<raw_ptr<aura::Window, VectorExperimental>>
          windows_to_minimize(windows.size());
      auto it = base::ranges::copy_if(
          windows, windows_to_minimize.begin(), [](aura::Window* window) {
            return !WindowState::Get(window)->IsMinimized();
          });
      windows_to_minimize.resize(
          std::distance(windows_to_minimize.begin(), it));
      window_util::MinimizeAndHideWithoutAnimation(windows_to_minimize);
    }

    // Do not show rounded corners or shadow during overview shutdown.
    overview_session_->UpdateRoundedCornersAndShadow();

    overview_session_->Shutdown();

    const bool should_end_immediately =
        overview_session_->enter_exit_overview_type() ==
        OverviewEnterExitType::kImmediateExit;
    if (should_end_immediately) {
      for (const auto& animation : delayed_animations_)
        animation->Shutdown();
      delayed_animations_.clear();
      OnEndingAnimationComplete(/*canceled=*/false);
    }

    // Don't delete |overview_session_| yet since the stack is still using it.
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, overview_session_.release());
    last_overview_session_time_ = base::Time::Now();
    for (auto& observer : observers_)
      observer.OnOverviewModeEnded();
    if (!should_end_immediately && delayed_animations_.empty())
      OnEndingAnimationComplete(/*canceled=*/false);
  } else {
    DCHECK(CanEnterOverview());
    CHECK(session_metrics_recorder_);
    session_metrics_recorder_->OnOverviewSessionInitializing();
    if (auto* active_window = window_util::GetActiveWindow(); active_window) {
      auto* active_widget =
          views::Widget::GetWidgetForNativeView(active_window);
      if (active_widget)
        paint_as_active_lock_ = active_widget->LockPaintAsActive();
    }

    // Clear any animations that may be running from last overview end.
    for (const auto& animation : delayed_animations_)
      animation->Shutdown();
    if (!delayed_animations_.empty())
      OnEndingAnimationComplete(/*canceled=*/true);
    delayed_animations_.clear();

    for (auto& observer : observers_)
      observer.OnOverviewModeWillStart();

    should_focus_overview_ = true;
    const SplitViewController::State split_view_state =
        SplitViewController::Get(Shell::GetPrimaryRootWindow())->state();
    // Prevent overview from stealing focus if |split_view_state| is
    // |SplitViewController::State::kPrimarySnapped| or
    // |SplitViewController::State::kSecondarySnapped|. Here are all the cases
    // where |split_view_state| will now have one of those two values:
    // 1. The active window is maximized in tablet mode. The user presses Alt+[.
    // 2. The active window is maximized in tablet mode. The user presses Alt+].
    // 3. The active window is snapped on the right in tablet split view.
    //    Another window is snapped on the left in tablet split view. The user
    //    presses Alt+[.
    // 4. The active window is snapped on the left in tablet split view. Another
    //    window is snapped on the right in tablet split view. The user presses
    //    Alt+].
    // 5. Overview starts because of a snapped window carrying over from
    //    clamshell mode to tablet mode.
    // 6. Overview starts on transition between user sessions.
    //
    // Note: We have to check the split view state before
    // |SplitViewController::OnOverviewModeStarting|, because in case of
    // |SplitViewController::State::kBothSnapped|, that function will insert one
    // of the two snapped windows to overview.
    if (split_view_state == SplitViewController::State::kPrimarySnapped ||
        split_view_state == SplitViewController::State::kSecondarySnapped) {
      should_focus_overview_ = false;
    } else {
      // Avoid stealing activation from a dragged active window.
      if (auto* active_window = window_util::GetActiveWindow();
          active_window && WindowState::Get(active_window)->is_dragged()) {
        DCHECK(window_util::ShouldExcludeForOverview(active_window));
        should_focus_overview_ = false;
      }
    }

    // If we don't need to force windows to be visible to get showable content
    // (i.e. they have a snapshot), suspend occlusion tracker until the enter
    // animation is complete.
    if (windows_have_snapshot_) {
      enter_pauser_ =
          PauseOcclusionTracker(occlusion_pause_duration_for_start_);
    }

    overview_session_ = std::make_unique<OverviewSession>(this);
    // We may want to slide in the overview grid in some cases, even if not
    // explicitly stated.
    OverviewEnterExitType new_type =
        MaybeOverrideEnterExitType(type, /*enter=*/true, windows);
    overview_session_->set_enter_exit_overview_type(new_type);
    for (auto& observer : observers_)
      observer.OnOverviewModeStarting();

    overview_session_->Init(
        windows, hide_windows,
        overview_window_occlusion_calculator_.GetCalculator());

    overview_session_->UpdateFrameThrottling();

    // For app dragging, there are no start animations so add a delay to delay
    // animations observing when the start animation ends, such as the shelf,
    // shadow and rounded corners.
    if (new_type == OverviewEnterExitType::kImmediateEnter &&
        !delayed_animation_task_delay_.is_zero()) {
      auto force_delay_observer =
          std::make_unique<ForceDelayObserver>(delayed_animation_task_delay_);
      AddEnterAnimationObserver(std::move(force_delay_observer));
    }

    // We do not pause the occlusion tracker (like we do for exit) because
    // windows that become visible as the animation starts should be marked as
    // visible the instant they are visible.
    if (start_animations_.empty())
      OnStartingAnimationComplete(/*canceled=*/false);

    session_metrics_recorder_->OnOverviewSessionInitialized(
        overview_session_.get());

    if (!last_overview_session_time_.is_null()) {
      UMA_HISTOGRAM_LONG_TIMES("Ash.Overview.TimeBetweenUse",
                               base::Time::Now() - last_overview_session_time_);
    }
  }

  // Let immediate raster scale updates take effect. If we are pausing the
  // occlusion tracker, defer any additional raster scale updates until after
  // occlusion pausing is done to ensure raster scale updates come after
  // occlusion updates on exit.
  scoped_pause_raster.reset();
  if (occlusion_tracker_pauser_) {
    raster_scale_pauser_.emplace();
  }
}

bool OverviewController::CanEndOverview(OverviewEnterExitType type) const {
  // Do not allow ending overview if we're in single split mode unless swiping
  // up from the shelf in tablet mode, or ending overview immediately without
  // animations.
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  if (split_view_controller->InTabletSplitViewMode() &&
      split_view_controller->state() !=
          SplitViewController::State::kBothSnapped &&
      InOverviewSession() && overview_session_->IsEmpty() &&
      type != OverviewEnterExitType::kImmediateExit) {
    return false;
  }

  return DesksController::Get()->CanEndOverview();
}

void OverviewController::OnStartingAnimationComplete(bool canceled) {
  CHECK(overview_session_);

  // Observers should not do anything which may cause overview to quit
  // explicitly (i.e. ToggleOverview()) or implicity (i.e. activation change).
  overview_session_->OnStartingAnimationComplete(canceled,
                                                 should_focus_overview_);
  for (auto& observer : observers_) {
    observer.OnOverviewModeStartingAnimationComplete(canceled);
  }

  enter_pauser_.reset();
}

void OverviewController::OnEndingAnimationComplete(bool canceled) {
  for (auto& observer : observers_)
    observer.OnOverviewModeEndingAnimationComplete(canceled);

  exit_pauser_.reset();

  // Resume the activation frame state.
  if (!canceled) {
    paint_as_active_lock_.reset();
  }

  // Ends the manual frame throttling at the end of overview exit.
  Shell::Get()->frame_throttling_controller()->EndThrottling();
}

void OverviewController::MaybePauseOcclusionTracker() {
  pause_count_++;
  if (pause_count_ > 1) {
    return;
  }

  reset_pauser_task_.Cancel();
  occlusion_tracker_pauser_ =
      std::make_unique<aura::WindowOcclusionTracker::ScopedPause>();
}

void OverviewController::MaybeUnpauseOcclusionTracker(base::TimeDelta delay) {
  pause_count_--;
  if (pause_count_ > 0) {
    return;
  }

  reset_pauser_task_.Reset(base::BindOnce(&OverviewController::ResetPauser,
                                          weak_ptr_factory_.GetWeakPtr()));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, reset_pauser_task_.callback(), delay);
}

void OverviewController::ResetPauser() {
  CHECK_EQ(pause_count_, 0);
  if (!overview_session_) {
    occlusion_tracker_pauser_.reset();
    raster_scale_pauser_.reset();
    return;
  }

  const bool ignore_activations = overview_session_->ignore_activations();
  overview_session_->set_ignore_activations(true);
  occlusion_tracker_pauser_.reset();
  raster_scale_pauser_.reset();

  // Unpausing the occlusion tracker may trigger window activations. Even though
  // window activations are paused, overview might still exit. See
  // http://b/334908991 for more details.
  if (overview_session_) {
    overview_session_->set_ignore_activations(ignore_activations);
  }
}

void OverviewController::UpdateRoundedCornersAndShadow() {
  if (overview_session_)
    overview_session_->UpdateRoundedCornersAndShadow();
}

}  // namespace ash
