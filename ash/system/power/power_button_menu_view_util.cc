// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_menu_view_util.h"

#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {

void SetLayerAnimation(ui::Layer* layer,
                       ui::ImplicitAnimationObserver* observer,
                       bool show,
                       const gfx::Transform& transform) {
  DCHECK(layer);

  auto* animator = layer->GetAnimator();
  animator->AbortAllAnimations();

  ui::ScopedLayerAnimationSettings animation_settings(animator);
  animation_settings.SetTweenType(show ? gfx::Tween::EASE_IN
                                       : gfx::Tween::FAST_OUT_LINEAR_IN);
  animation_settings.SetTransitionDuration(kPowerButtonMenuAnimationDuration);
  animation_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  if (observer) {
    animation_settings.AddObserver(observer);
  }

  layer->SetOpacity(show ? 1.0f : 0.f);
  layer->SetTransform(transform);
}

}  // namespace ash