// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_animations.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/view.h"

// The time duration for widgets to fade in.
constexpr int kFadeInDelay = 0;
constexpr int kFadeInDuration = 100;

namespace ash {

// Fade in animation using AnimationBuilder.
void FadeInView(ui::Layer* layer,
                int delay_in_ms,
                int duration_in_ms,
                gfx::Tween::Type tween_type = gfx::Tween::LINEAR) {
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetOpacity(layer, 0.0f)
      .Then()
      .SetDuration(base::Milliseconds(duration_in_ms))
      .SetOpacity(layer, 1.0f, tween_type);
}

void PerformFadeInDesksTemplatesGridView(ui::Layer* layer) {
  // TODO(sophiewen): Perform fade out of other overview items.
  FadeInView(layer, kFadeInDelay, kFadeInDuration);
}

}  // namespace ash
