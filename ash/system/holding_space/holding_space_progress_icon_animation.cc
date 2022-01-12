// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_progress_icon_animation.h"

#include "ui/gfx/animation/tween.h"

namespace ash {

HoldingSpaceProgressIconAnimation::HoldingSpaceProgressIconAnimation()
    : HoldingSpaceProgressIndicatorAnimation(
          /*duration=*/base::Milliseconds(400),
          /*is_cyclic=*/false) {}

HoldingSpaceProgressIconAnimation::~HoldingSpaceProgressIconAnimation() =
    default;

void HoldingSpaceProgressIconAnimation::UpdateAnimatableProperties(
    double fraction) {
  // Tween.
  fraction = gfx::Tween::CalculateValue(gfx::Tween::Type::ACCEL_20_DECEL_100,
                                        fraction);

  // Animatable properties.
  inner_icon_translate_y_scale_factor_ =
      gfx::Tween::FloatValueBetween(fraction, /*start=*/-0.5f, /*target=*/0.f);
  inner_ring_stroke_width_scale_factor_ =
      gfx::Tween::FloatValueBetween(fraction, /*start=*/0.f, /*target=*/1.f);
}

}  // namespace ash
