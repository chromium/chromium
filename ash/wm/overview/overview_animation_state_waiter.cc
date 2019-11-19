// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_animation_state_waiter.h"

#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"

namespace ash {

OverviewAnimationStateWaiter::OverviewAnimationStateWaiter(
    OverviewAnimationState expected_state,
    DoneCallback callback)
    : expected_state_(expected_state), callback_(std::move(callback)) {
  Shell::Get()->overview_controller()->AddObserver(this);
}

OverviewAnimationStateWaiter::~OverviewAnimationStateWaiter() {
  Shell::Get()->overview_controller()->RemoveObserver(this);
}

void OverviewAnimationStateWaiter::Cancel() {
  std::move(callback_).Run(false);
  delete this;
}

void OverviewAnimationStateWaiter::OnOverviewModeStartingAnimationComplete(
    bool canceled) {
  if (expected_state_ != OverviewAnimationState::kEnterAnimationComplete)
    return;
  std::move(callback_).Run(!canceled);
  delete this;
}

void OverviewAnimationStateWaiter::OnOverviewModeEndingAnimationComplete(
    bool canceled) {
  if (expected_state_ != OverviewAnimationState::kExitAnimationComplete)
    return;
  std::move(callback_).Run(!canceled);
  delete this;
}

}  // namespace ash
