// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/pin_keyboard_animation.h"

#include <memory>

#include "ui/compositor/layer_animation_delegate.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/interpolated_transform.h"

namespace ash {

PinKeyboardAnimation::PinKeyboardAnimation(bool grow,
                                           int height,
                                           base::TimeDelta duration,
                                           gfx::Tween::Type tween_type)
    : ui::LayerAnimationElement(
          LayerAnimationElement::TRANSFORM | LayerAnimationElement::OPACITY,
          duration),
      tween_type_(tween_type) {
  if (!grow) {
    std::swap(start_opacity_, end_opacity_);
  }

  transform_ = std::make_unique<ui::InterpolatedScale>(
      gfx::Point3F(1, start_opacity_, 1), gfx::Point3F(1, end_opacity_, 1));
}

PinKeyboardAnimation::~PinKeyboardAnimation() = default;

void PinKeyboardAnimation::OnStart(ui::LayerAnimationDelegate* delegate) {}

bool PinKeyboardAnimation::OnProgress(double current,
                                      ui::LayerAnimationDelegate* delegate) {
  const double tweened = gfx::Tween::CalculateValue(tween_type_, current);
  delegate->SetOpacityFromAnimation(
      gfx::Tween::FloatValueBetween(tweened, start_opacity_, end_opacity_),
      ui::PropertyChangeReason::FROM_ANIMATION);
  delegate->SetTransformFromAnimation(transform_->Interpolate(tweened),
                                      ui::PropertyChangeReason::FROM_ANIMATION);
  return true;
}

void PinKeyboardAnimation::OnGetTarget(TargetValue* target) const {}

void PinKeyboardAnimation::OnAbort(ui::LayerAnimationDelegate* delegate) {}

}  // namespace ash
