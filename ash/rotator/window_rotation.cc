// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rotator/window_rotation.h"

#include <memory>

#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/interpolated_transform.h"

namespace ash {

namespace {

const int k90DegreeTransitionDurationMs = 350;
const int k180DegreeTransitionDurationMs = 550;
const int k360DegreeTransitionDurationMs = 750;

base::TimeDelta GetTransitionDuration(int degrees) {
  if (degrees == 360)
    return base::Milliseconds(k360DegreeTransitionDurationMs);
  if (degrees == 180)
    return base::Milliseconds(k180DegreeTransitionDurationMs);
  if (degrees == 0)
    return base::Milliseconds(0);
  return base::Milliseconds(k90DegreeTransitionDurationMs);
}

}  // namespace

WindowRotation::WindowRotation(int degrees, ui::Layer* layer)
    : ui::LayerAnimationElement(LayerAnimationElement::TRANSFORM,
                                GetTransitionDuration(degrees)),
      degrees_(degrees) {
  InitTransform(layer);
}

WindowRotation::~WindowRotation() = default;

void WindowRotation::InitTransform(ui::Layer* layer) {
  // No rotation required, use the identity transform.
  if (degrees_ == 0) {
    interpolated_transform_ =
        std::make_unique<ui::InterpolatedConstantTransform>(gfx::Transform());
    return;
  }

  // Use the target transform/bounds in case the layer is already animating.
  const gfx::Transform& current_transform = layer->GetTargetTransform();
  const gfx::Rect& bounds = layer->GetTargetBounds();

  gfx::Point old_pivot;
  gfx::Point new_pivot;

  int width = bounds.width();
  int height = bounds.height();

  switch (degrees_) {
    case 90:
      new_origin_ = new_pivot = gfx::Point(width, 0);
      break;
    case -90:
      new_origin_ = new_pivot = gfx::Point(0, height);
      break;
    case 180:
    case 360:
      new_pivot = old_pivot = gfx::Point(width / 2, height / 2);
      new_origin_.SetPoint(width, height);
      break;
  }

  // Convert points to world space.
  old_pivot = current_transform.MapPoint(old_pivot);
  new_pivot = current_transform.MapPoint(new_pivot);
  new_origin_ = current_transform.MapPoint(new_origin_);

  std::unique_ptr<ui::InterpolatedTransform> rotation =
      std::make_unique<ui::InterpolatedTransformAboutPivot>(
          old_pivot, std::make_unique<ui::InterpolatedRotation>(0, degrees_));

  std::unique_ptr<ui::InterpolatedTransform> translation =
      std::make_unique<ui::InterpolatedTranslation>(
          gfx::PointF(), gfx::PointF(new_pivot.x() - old_pivot.x(),
                                     new_pivot.y() - old_pivot.y()));

  float scale_factor = 0.9f;
  std::unique_ptr<ui::InterpolatedTransform> scale_down =
      std::make_unique<ui::InterpolatedScale>(1.0f, scale_factor, 0.0f, 0.5f);

  std::unique_ptr<ui::InterpolatedTransform> scale_up =
      std::make_unique<ui::InterpolatedScale>(1.0f, 1.0f / scale_factor, 0.5f,
                                              1.0f);

  interpolated_transform_ =
      std::make_unique<ui::InterpolatedConstantTransform>(current_transform);

  scale_up->SetChild(std::move(scale_down));
  translation->SetChild(std::move(scale_up));
  rotation->SetChild(std::move(translation));
  interpolated_transform_->SetChild(std::move(rotation));
}

void WindowRotation::OnStart(ui::LayerAnimationDelegate* delegate) {}

bool WindowRotation::OnProgress(double t,
                                ui::LayerAnimationDelegate* delegate) {
  delegate->SetTransformFromAnimation(interpolated_transform_->Interpolate(t),
                                      ui::PropertyChangeReason::FROM_ANIMATION);
  return true;
}

void WindowRotation::OnGetTarget(TargetValue* target) const {
  target->transform = interpolated_transform_->Interpolate(1.0);
}

void WindowRotation::OnAbort(ui::LayerAnimationDelegate* delegate) {}

}  // namespace ash
