// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/progress_indicator/progress_icon_animation.h"

#include "base/memory/ptr_util.h"
#include "ui/gfx/animation/tween.h"

namespace ash {

ProgressIconAnimation::ProgressIconAnimation()
    : ProgressIndicatorAnimation(
          /*duration=*/base::Milliseconds(400),
          /*is_cyclic=*/false) {}

ProgressIconAnimation::~ProgressIconAnimation() = default;

// static
std::unique_ptr<ProgressIconAnimation> ProgressIconAnimation::Create() {
  // NOTE: `base::WrapUnique()` is necessary due to constructor visibility.
  auto animation = base::WrapUnique(new ProgressIconAnimation());
  animation->Init();
  return animation;
}

void ProgressIconAnimation::UpdateAnimatableProperties(double fraction) {
  // Tween.
  fraction = gfx::Tween::CalculateValue(gfx::Tween::Type::ACCEL_20_DECEL_100,
                                        fraction);

  // Animatable properties.
  inner_icon_translate_y_scale_factor_ =
      gfx::Tween::FloatValueBetween(fraction, /*start=*/-0.5f, /*target=*/0.f);
  inner_ring_stroke_width_scale_factor_ =
      gfx::Tween::FloatValueBetween(fraction, /*start=*/0.f, /*target=*/1.f);
  opacity_ = gfx::Tween::FloatValueBetween(std::min(fraction * 2.0, 1.0),
                                           /*start=*/0.f, /*target=*/1.f);
}

}  // namespace ash
