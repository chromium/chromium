// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_animations.h"

#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/animation/animation_builder.h"

namespace ash {

namespace {

// The time duration for widgets to fade in.
constexpr int kFadeInDurationMs = 100;

// The time duration for widgets to fade out.
constexpr int kFadeOutDurationMs = 100;

// Fade in animation using AnimationBuilder.
void FadeInLayer(ui::Layer* layer, int duration_in_ms) {
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetOpacity(layer, 0.0f)
      .Then()
      .SetDuration(base::Milliseconds(duration_in_ms))
      .SetOpacity(layer, 1.0f, gfx::Tween::LINEAR);
}

// Fade out animation using AnimationBuilder.
void FadeOutLayer(ui::Layer* layer,
                  base::OnceClosure on_animation_ended_callback,
                  int duration_in_ms) {
  std::pair<base::OnceClosure, base::OnceClosure> split =
      base::SplitOnceCallback(std::move(on_animation_ended_callback));

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(std::move(split.first))
      .OnAborted(std::move(split.second))
      .Once()
      .SetDuration(base::TimeDelta())
      .SetOpacity(layer, 1.0f)
      .Then()
      .SetDuration(base::Milliseconds(duration_in_ms))
      .SetOpacity(layer, 0.0f, gfx::Tween::LINEAR);
}

}  // namespace

void PerformFadeInLayer(ui::Layer* layer, bool animate) {
  if (!animate) {
    layer->SetOpacity(1.f);
    return;
  }

  FadeInLayer(layer, kFadeInDurationMs);
}

void PerformFadeOutLayer(ui::Layer* layer,
                         bool animate,
                         base::OnceClosure on_animation_ended_callback) {
  if (!animate) {
    layer->SetOpacity(0.f);
    return;
  }

  FadeOutLayer(layer, std::move(on_animation_ended_callback),
               kFadeOutDurationMs);
}

}  // namespace ash
