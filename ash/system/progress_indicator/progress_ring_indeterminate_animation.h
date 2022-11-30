// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_RING_INDETERMINATE_ANIMATION_H_
#define ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_RING_INDETERMINATE_ANIMATION_H_

#include "ash/system/progress_indicator/progress_ring_animation.h"

namespace ash {

// An animation for a `ProgressIndicator` to paint an indeterminate progress
// ring in lieu of the determinate progress ring that would otherwise be
// painted.
class ProgressRingIndeterminateAnimation : public ProgressRingAnimation {
 public:
  ProgressRingIndeterminateAnimation();
  ProgressRingIndeterminateAnimation(
      const ProgressRingIndeterminateAnimation&) = delete;
  ProgressRingIndeterminateAnimation& operator=(
      const ProgressRingIndeterminateAnimation&) = delete;
  ~ProgressRingIndeterminateAnimation() override;

 private:
  // ProgressRingAnimation:
  void UpdateAnimatableProperties(double fraction,
                                  float* start_value,
                                  float* end_value,
                                  float* outer_ring_opacity) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_RING_INDETERMINATE_ANIMATION_H_
