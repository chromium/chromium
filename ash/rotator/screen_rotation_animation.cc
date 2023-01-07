// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rotator/screen_rotation_animation.h"

#include <memory>

#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/interpolated_transform.h"

namespace ash {

ScreenRotationAnimation::ScreenRotationAnimation(ui::Layer* layer,
                                                 int start_degrees,
                                                 int end_degrees,
                                                 float initial_opacity,
                                                 float target_opacity,
                                                 gfx::Point pivot,
                                                 base::TimeDelta duration,
                                                 gfx::Tween::Type tween_type)
    : ui::LayerAnimationElement(
          LayerAnimationElement::TRANSFORM | LayerAnimationElement::OPACITY,
          duration),
      tween_type_(tween_type),
      initial_opacity_(initial_opacity),
      target_opacity_(target_opacity) {
  std::unique_ptr<ui::InterpolatedTransform> rotation =
      std::make_unique<ui::InterpolatedTransformAboutPivot>(
          pivot, std::make_unique<ui::InterpolatedRotation>(start_degrees,
                                                            end_degrees));

  // Use the target transform/bounds in case the layer is already animating.
  gfx::Transform current_transform = layer->GetTargetTransform();
  interpolated_transform_ =
      std::make_unique<ui::InterpolatedConstantTransform>(current_transform);
  interpolated_transform_->SetChild(std::move(rotation));
}

ScreenRotationAnimation::~ScreenRotationAnimation() = default;

void ScreenRotationAnimation::OnStart(ui::LayerAnimationDelegate* delegate) {}

bool ScreenRotationAnimation::OnProgress(double current,
                                         ui::LayerAnimationDelegate* delegate) {
  const double tweened = gfx::Tween::CalculateValue(tween_type_, current);
  delegate->SetTransformFromAnimation(
      interpolated_transform_->Interpolate(tweened),
      ui::PropertyChangeReason::FROM_ANIMATION);
  delegate->SetOpacityFromAnimation(
      gfx::Tween::FloatValueBetween(tweened, initial_opacity_, target_opacity_),
      ui::PropertyChangeReason::FROM_ANIMATION);
  return true;
}

void ScreenRotationAnimation::OnGetTarget(TargetValue* target) const {
  target->transform = interpolated_transform_->Interpolate(1.0);
}

void ScreenRotationAnimation::OnAbort(ui::LayerAnimationDelegate* delegate) {
  // ui::Layer's d'tor passes its ui::LayerAnimator a null delegate before
  // deleting it. This is then passed here: http://crbug.com/661313
  if (!delegate)
    return;

  TargetValue target_value;
  OnGetTarget(&target_value);
  delegate->SetTransformFromAnimation(target_value.transform,
                                      ui::PropertyChangeReason::FROM_ANIMATION);
}

}  // namespace ash
