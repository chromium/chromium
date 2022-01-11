// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_progress_icon_animation.h"

namespace ash {

// Animation.
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(200);

HoldingSpaceProgressIconAnimation::HoldingSpaceProgressIconAnimation()
    : HoldingSpaceProgressIndicatorAnimation(kAnimationDuration,
                                             /*is_cyclic=*/true) {}

HoldingSpaceProgressIconAnimation::~HoldingSpaceProgressIconAnimation() =
    default;

void HoldingSpaceProgressIconAnimation::UpdateAnimatableProperties(
    double fraction) {
  // TODO(dmblack): Implement.
}

}  // namespace ash
