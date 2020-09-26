// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_animation_base.h"

#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"

namespace ash {

DeskAnimationBase::DeskAnimationBase(DesksController* controller,
                                     int ending_desk_index,
                                     bool is_continuous_gesture_animation)
    : controller_(controller),
      ending_desk_index_(ending_desk_index),
      is_continuous_gesture_animation_(is_continuous_gesture_animation),
      throughput_tracker_(
          desks_util::GetSelectedCompositorForPerformanceMetrics()
              ->RequestNewThroughputTracker()) {
  DCHECK(controller_);
  DCHECK_LE(ending_desk_index_, int{controller_->desks().size()});
  DCHECK_GE(ending_desk_index_, 0);
}

DeskAnimationBase::~DeskAnimationBase() = default;

void DeskAnimationBase::Launch() {
  for (auto& observer : controller_->observers_)
    observer.OnDeskSwitchAnimationLaunching();

  throughput_tracker_.Start(GetReportCallback());

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

void DeskAnimationBase::OnStartingDeskScreenshotTaken(int ending_desk_index) {
  DCHECK(!desk_switch_animators_.empty());

  // Once all starting desk screenshots on all roots are taken and placed on
  // the screens, do the actual desk activation logic.
  for (const auto& animator : desk_switch_animators_) {
    if (!animator->starting_desk_screenshot_taken())
      return;
  }

  // Extend the compositors' timeouts in order to prevents any repaints until
  // the desks are switched and overview mode exits.
  const auto roots = Shell::GetAllRootWindows();
  for (auto* root : roots)
    root->GetHost()->compositor()->SetAllowLocksToExtendTimeout(true);

  OnStartingDeskScreenshotTakenInternal(ending_desk_index);

  for (auto* root : roots)
    root->GetHost()->compositor()->SetAllowLocksToExtendTimeout(false);

  // Continue the second phase of the animation by taking the ending desk
  // screenshot and actually animating the layers.
  for (auto& animator : desk_switch_animators_)
    animator->TakeEndingDeskScreenshot();
}

void DeskAnimationBase::OnEndingDeskScreenshotTaken() {
  DCHECK(!desk_switch_animators_.empty());

  // Once all ending desk screenshots on all roots are taken, start the
  // animation on all roots at the same time, so that they look synchrnoized.
  for (const auto& animator : desk_switch_animators_) {
    if (!animator->ending_desk_screenshot_taken())
      return;
  }

  // Continuous gesture animations do not want to start an animation on
  // creation/replacement (because they want to update). They will request an
  // animation explicitly if they need (gesture end).
  if (is_continuous_gesture_animation_)
    return;

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

  throughput_tracker_.Stop();

  for (auto& observer : controller_->observers_)
    observer.OnDeskSwitchAnimationFinished();

  controller_->OnAnimationFinished(this);
  // `this` is now deleted.
}

}  // namespace ash
