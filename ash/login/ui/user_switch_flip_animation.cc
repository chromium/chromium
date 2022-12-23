// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/user_switch_flip_animation.h"

#include <memory>

#include "ui/compositor/layer_animation_delegate.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/interpolated_transform.h"

namespace ash {

namespace {

std::unique_ptr<ui::InterpolatedTransform> BuildRotation(int width,
                                                         int start_degrees,
                                                         int end_degrees,
                                                         bool horizontal_flip) {
  gfx::Transform to_center;
  to_center.Translate(width / 2.f, 0);
  auto move_to_center =
      std::make_unique<ui::InterpolatedConstantTransform>(to_center);

  auto rotate = std::make_unique<ui::InterpolatedAxisAngleRotation>(
      gfx::Vector3dF(0, 1, 0), start_degrees, end_degrees);

  gfx::Transform from_center;
  if (horizontal_flip) {
    from_center.RotateAboutYAxis(180);
  }
  from_center.Translate(-width / 2.f, 0);
  auto move_from_center =
      std::make_unique<ui::InterpolatedConstantTransform>(from_center);

  rotate->SetChild(std::move(move_to_center));
  move_from_center->SetChild(std::move(rotate));

  return move_from_center;
}

}  // namespace

UserSwitchFlipAnimation::UserSwitchFlipAnimation(int width,
                                                 int start_degrees,
                                                 int midpoint_degrees,
                                                 int end_degrees,
                                                 base::TimeDelta duration,
                                                 gfx::Tween::Type tween_type,
                                                 base::OnceClosure on_midpoint)
    : ui::LayerAnimationElement(LayerAnimationElement::TRANSFORM, duration),
      tween_type_(tween_type),
      on_midpoint_(std::move(on_midpoint)) {
  first_half_transform_ = BuildRotation(width, start_degrees, midpoint_degrees,
                                        false /*horizontal_flip*/);
  second_half_transform_ = BuildRotation(width, midpoint_degrees, end_degrees,
                                         true /*horizontal_flip*/);
}

UserSwitchFlipAnimation::~UserSwitchFlipAnimation() = default;

void UserSwitchFlipAnimation::OnStart(ui::LayerAnimationDelegate* delegate) {}

bool UserSwitchFlipAnimation::OnProgress(double current,
                                         ui::LayerAnimationDelegate* delegate) {
  // Each animation is run with a full tween cycle, per UX.

  // First half.
  if (current < 0.5) {
    current *= 2;
    const double tweened = gfx::Tween::CalculateValue(tween_type_, current);
    delegate->SetTransformFromAnimation(
        first_half_transform_->Interpolate(tweened),
        ui::PropertyChangeReason::FROM_ANIMATION);
  }

  // Second half.
  else {
    if (on_midpoint_) {
      std::move(on_midpoint_).Run();
    }

    current = (current - 0.5) * 2;
    const double tweened = gfx::Tween::CalculateValue(tween_type_, current);
    delegate->SetTransformFromAnimation(
        second_half_transform_->Interpolate(tweened),
        ui::PropertyChangeReason::FROM_ANIMATION);
  }

  return true;
}

void UserSwitchFlipAnimation::OnGetTarget(TargetValue* target) const {}

void UserSwitchFlipAnimation::OnAbort(ui::LayerAnimationDelegate* delegate) {}

}  // namespace ash
