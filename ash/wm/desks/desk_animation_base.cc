// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_animation_base.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"

namespace ash {

DeskAnimationBase::DeskAnimationBase(DesksController* controller,
                                     int ending_desk_index,
                                     bool is_continuous_gesture_animation)
    : controller_(controller),
      ending_desk_index_(ending_desk_index),
      is_continuous_gesture_animation_(is_continuous_gesture_animation) {
  DCHECK(controller_);
  DCHECK_LE(ending_desk_index_, static_cast<int>(controller_->desks().size()));
  DCHECK_GE(ending_desk_index_, 0);
}

DeskAnimationBase::~DeskAnimationBase() {
  for (auto& observer : controller_->observers_)
    observer.OnDeskSwitchAnimationFinished();

  if (finished_callback_)
    std::move(finished_callback_).Run();
}

void DeskAnimationBase::Launch() {
  launch_time_ = base::TimeTicks::Now();

  for (auto& observer : controller_->observers_)
    observer.OnDeskSwitchAnimationLaunching();

  // The throughput tracker measures the animation when the user lifts their
  // fingers off the trackpad, which is done in EndSwipeAnimation.
  if (!is_continuous_gesture_animation_) {
    // Request a new sequence tracker so the tracking number can't be reused.
    throughput_tracker_ =
        desks_util::GetSelectedCompositorForPerformanceMetrics()
            ->RequestNewThroughputTracker();
    throughput_tracker_->Start(GetSmoothnessReportCallback());
  }

  // This step makes sure that the containers of the target desk are shown at
  // the beginning of the animation (but not actually visible to the user yet,
  // until the desk is actually activated at a later step of the animation).
  // This is needed because a window on the target desk can be focused before
  // the desk becomes active (See `DesksController::OnWindowActivating()`).
  // This window must be able to accept events (See
  // `aura::Window::CanAcceptEvent()`) even though its desk is still being
  // activated. https://crbug.com/1008574.
  controller_->desks()[ending_desk_index_]->PrepareForActivationAnimation();

  DCHECK(!desk_switch_animators_.empty());
  for (auto& animator : desk_switch_animators_)
    animator->TakeStartingDeskScreenshot();
}

bool DeskAnimationBase::Replace(bool moving_left, DesksSwitchSource source) {
  return false;
}

bool DeskAnimationBase::UpdateSwipeAnimation(float scroll_delta_x) {
  return false;
}

bool DeskAnimationBase::EndSwipeAnimation() {
  return false;
}

bool DeskAnimationBase::CanEnterOverview() const {
  return is_overview_toggle_allowed_;
}

bool DeskAnimationBase::CanEndOverview() const {
  return is_overview_toggle_allowed_;
}

void DeskAnimationBase::OnStartingDeskScreenshotTaken(int ending_desk_index) {
  DCHECK(!desk_switch_animators_.empty());

  // If an animator fails, for any reason, we abort the whole project and
  // activate the target desk without any animation.
  if (AnimatorFailed()) {
    // This will effectively delete `this`.
    ActivateTargetDeskWithoutAnimation();
    return;
  }

  // Once all starting desk screenshots on all roots are taken and placed on
  // the screens, do the actual desk activation logic.
  for (const auto& animator : desk_switch_animators_) {
    if (!animator->starting_desk_screenshot_taken())
      return;
  }

  // If ending desk index goes out of sync with the one provided due to screenshot delay 
  // and user action, end animation. Speculative fix for http://b/307304567.
  if (ending_desk_index != ending_desk_index_) {
    // This will effectively delete `this`.
    ActivateTargetDeskWithoutAnimation();
    return;
  }

  // Extend the compositors' timeouts in order to prevents any repaints until
  // the desks are switched and overview mode exits.
  const auto roots = Shell::GetAllRootWindows();
  for (aura::Window* root : roots) {
    root->GetHost()->compositor()->SetAllowLocksToExtendTimeout(true);
  }

  OnStartingDeskScreenshotTakenInternal(ending_desk_index);

  for (aura::Window* root : roots) {
    root->GetHost()->compositor()->SetAllowLocksToExtendTimeout(false);
  }

  // Continue the second phase of the animation by taking the ending desk
  // screenshot and actually animating the layers.
  for (auto& animator : desk_switch_animators_)
    animator->TakeEndingDeskScreenshot();
}

void DeskAnimationBase::OnEndingDeskScreenshotTaken() {
  DCHECK(!desk_switch_animators_.empty());

  // If an animator fails, for any reason, we abort the whole project and
  // activate the target desk without any animation.
  if (AnimatorFailed()) {
    // This will effectively delete `this`.
    ActivateTargetDeskWithoutAnimation();
    return;
  }

  // Once all ending desk screenshots on all roots are taken, start the
  // animation on all roots at the same time, so that they look synchrnoized.
  for (const auto& animator : desk_switch_animators_) {
    if (!animator->ending_desk_screenshot_taken())
      return;
  }

  // In the normal case for a continuous gesture animation, the ending desk
  // screenshot will be taken while the user is still swiping. In this case, we
  // want to skip the animation which will eventually delete `this`. There is a
  // bug (https://crbug.com/1191545) where users who try to quickly swipe do not
  // see an animation but expect to. If the gesture has ended, and has been
  // determined to be fast, we will start the animation to delete `this`.
  const bool skip_start_animation =
      is_continuous_gesture_animation_ && !did_continuous_gesture_end_fast_;
  if (skip_start_animation)
    return;

  if (!launch_time_.is_null()) {
    GetLatencyReportCallback().Run(base::TimeTicks::Now() - launch_time_);
    launch_time_ = base::TimeTicks();
  }

  for (auto& animator : desk_switch_animators_)
    animator->StartAnimation();
}

void DeskAnimationBase::OnDeskSwitchAnimationFinished() {
  DCHECK(!desk_switch_animators_.empty());

  // Once all desk switch animations on all roots finish, destroy all the
  // animators.
  for (const auto& animator : desk_switch_animators_) {
    if (!animator->animation_finished())
      return;
  }

  OnDeskSwitchAnimationFinishedInternal();

  desk_switch_animators_.clear();
  if (throughput_tracker_.has_value()) {
    throughput_tracker_->Stop();
  }
  if (skip_notify_controller_on_animation_finished_for_testing_)
    return;

  controller_->OnAnimationFinished(this);
  // `this` is now deleted.
}

RootWindowDeskSwitchAnimator*
DeskAnimationBase::GetDeskSwitchAnimatorAtIndexForTesting(size_t index) const {
  DCHECK_LT(index, desk_switch_animators_.size());
  return desk_switch_animators_[index].get();
}

void DeskAnimationBase::ActivateDeskDuringAnimation(
    const Desk* desk,
    bool update_window_activation) {
  // Normally we do not allow toggling overview while there is an active
  // animation. The only exception is when we are doing a desk activation and
  // are starting the animation in overview. The desk switch animations require
  // taking a screenshot of the starting and ending desks before animating
  // between the two screenshots, and these screenshots need to represent what
  // the new desk will look like for the user. If we start the animation in
  // overview, we want to allow `ActivateDeskInternal()` to end overview on the
  // old active desk (and enter overview on the new active desk if the overview
  // desk navigation feature is enabled). Once `ActivateDeskInternal()` finishes
  // updating the active desk and overview states, we immediately set
  // `is_overview_toggle_allowed_` to false to prevent any subsequent overview
  // toggling (i.e. user input).
  is_overview_toggle_allowed_ =
      features::IsOverviewDeskNavigationEnabled() &&
      Shell::Get()->overview_controller()->InOverviewSession();
  controller_->ActivateDeskInternal(desk, update_window_activation);
  is_overview_toggle_allowed_ = false;
}

void DeskAnimationBase::ActivateTargetDeskWithoutAnimation() {
  auto* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession()) {
    // Setting this is required. The overview controller will ask the desk
    // controller if exiting overview is allowed, and since we are technically
    // still in an animation, the desk controller will ask the animation (which
    // is us) if overview can be toggled.
    is_overview_toggle_allowed_ = true;
    overview_controller->EndOverview(OverviewEndAction::kDeskActivation,
                                     OverviewEnterExitType::kImmediateExit);
  }

  const auto& desks = controller_->desks();
  if (ending_desk_index_ < static_cast<int>(desks.size())) {
    controller_->ActivateDeskInternal(desks[ending_desk_index_].get(), true);
  }

  controller_->OnAnimationFinished(this);
  // `this` is now deleted.
}

bool DeskAnimationBase::AnimatorFailed() const {
  for (const auto& animator : desk_switch_animators_) {
    if (animator->animator_failed()) {
      return true;
    }
  }

  return false;
}

}  // namespace ash
