// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/magnifier_animation_waiter.h"
#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "base/timer/timer.h"

namespace ash {

MagnifierAnimationWaiter::MagnifierAnimationWaiter(
    FullscreenMagnifierController* controller)
    : controller_(controller) {}

MagnifierAnimationWaiter::~MagnifierAnimationWaiter() = default;

void MagnifierAnimationWaiter::Wait() {
  base::RepeatingTimer check_timer;
  check_timer.Start(FROM_HERE, base::Milliseconds(10), this,
                    &MagnifierAnimationWaiter::OnTimer);
  runner_ = new content::MessageLoopRunner;
  runner_->Run();
}

void MagnifierAnimationWaiter::OnTimer() {
  DCHECK(runner_.get());
  if (!controller_->IsOnAnimationForTesting()) {
    runner_->Quit();
  }
}

}  // namespace ash
