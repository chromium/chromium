// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_MAGNIFIER_ANIMATION_WAITER_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_MAGNIFIER_ANIMATION_WAITER_H_

#include "content/public/test/test_utils.h"

namespace ash {

class FullscreenMagnifierController;

// FullscreenMagnifierController moves the magnifier window with animation
// when the magnifier is set to be enabled. This waiter class lets a consumer
// wait until the animation completes, i.e. after a mouse move.
class MagnifierAnimationWaiter {
 public:
  explicit MagnifierAnimationWaiter(FullscreenMagnifierController* controller);

  MagnifierAnimationWaiter(const MagnifierAnimationWaiter&) = delete;
  MagnifierAnimationWaiter& operator=(const MagnifierAnimationWaiter&) = delete;

  ~MagnifierAnimationWaiter();

  // Wait until the Fullscreen magnifier finishes animating.
  void Wait();

 private:
  void OnTimer();

  FullscreenMagnifierController* controller_;  // not owned
  scoped_refptr<content::MessageLoopRunner> runner_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_MAGNIFIER_ANIMATION_WAITER_H_
