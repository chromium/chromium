// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/session_state_animator.h"

#include <utility>

#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/window_animations.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

bool IsTabletModeEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshEnableTabletMode);
}

}  // namespace

const int SessionStateAnimator::kAllLockScreenContainersMask =
    SessionStateAnimator::LOCK_SCREEN_WALLPAPER |
    SessionStateAnimator::LOCK_SCREEN_CONTAINERS |
    SessionStateAnimator::LOCK_SCREEN_RELATED_CONTAINERS;

const int SessionStateAnimator::kAllNonRootContainersMask =
    SessionStateAnimator::kAllLockScreenContainersMask |
    SessionStateAnimator::WALLPAPER | SessionStateAnimator::SHELF |
    SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS;

SessionStateAnimator::AnimationSequence::AnimationSequence(
    base::OnceClosure callback)
    : sequence_ended_(false),
      animation_completed_(false),
      invoke_callback_(false),
      callback_(std::move(callback)) {}

SessionStateAnimator::AnimationSequence::~AnimationSequence() = default;

void SessionStateAnimator::AnimationSequence::EndSequence() {
  sequence_ended_ = true;
  CleanupIfSequenceCompleted();
}

void SessionStateAnimator::AnimationSequence::OnAnimationCompleted() {
  animation_completed_ = true;
  invoke_callback_ = true;
  CleanupIfSequenceCompleted();
}

void SessionStateAnimator::AnimationSequence::OnAnimationAborted() {
  animation_completed_ = true;
  invoke_callback_ = false;
  CleanupIfSequenceCompleted();
}

void SessionStateAnimator::AnimationSequence::CleanupIfSequenceCompleted() {
  if (sequence_ended_ && animation_completed_) {
    if (invoke_callback_)
      std::move(callback_).Run();
    delete this;
  }
}

SessionStateAnimator::SessionStateAnimator() = default;

SessionStateAnimator::~SessionStateAnimator() = default;

base::TimeDelta SessionStateAnimator::GetDuration(
    SessionStateAnimator::AnimationSpeed speed) {
  switch (speed) {
    case ANIMATION_SPEED_IMMEDIATE:
      return base::TimeDelta();
    case ANIMATION_SPEED_UNDOABLE:
      return base::TimeDelta::FromMilliseconds(400);
    case ANIMATION_SPEED_MOVE_WINDOWS:
      return base::TimeDelta::FromMilliseconds(350);
    case ANIMATION_SPEED_UNDO_MOVE_WINDOWS:
      return base::TimeDelta::FromMilliseconds(350);
    case ANIMATION_SPEED_SHUTDOWN:
      return IsTabletModeEnabled() ? base::TimeDelta::FromMilliseconds(1500)
                                   : base::TimeDelta::FromMilliseconds(1000);
    case ANIMATION_SPEED_REVERT_SHUTDOWN:
      return base::TimeDelta::FromMilliseconds(500);
  }
  // Satisfy compilers that do not understand that we will return from switch
  // above anyway.
  DCHECK(false) << "Unhandled animation speed " << speed;
  return base::TimeDelta();
}

}  // namespace ash
