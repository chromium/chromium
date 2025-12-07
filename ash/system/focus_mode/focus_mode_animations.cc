// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_animations.h"

#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr gfx::Transform kEndTransform;

constexpr base::TimeDelta kContainerViewShiftAnimationDuration =
    base::Milliseconds(200);
constexpr base::TimeDelta kContainerViewResizeDuration =
    base::Milliseconds(200);

// Animates the transform of the layer of the given `view` from the supplied
// `begin_transform` to the identity transform.
void AnimateView(views::View* view,
                 const gfx::Transform& begin_transform,
                 const base::TimeDelta duration) {
  ui::Layer* layer = view->layer();
  layer->SetTransform(begin_transform);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(duration)
      .SetTransform(layer, kEndTransform, gfx::Tween::ACCEL_20_DECEL_100);
}

}  // namespace

void PerformViewsVerticalShitfAnimation(
    const std::vector<views::View*>& animatable_views,
    const int shift_height) {
  const auto begin_transform = gfx::Transform::MakeTranslation(0, shift_height);
  for (auto* view : animatable_views) {
    AnimateView(view, begin_transform, kContainerViewShiftAnimationDuration);
  }
}

void PerformTaskContainerViewResizeAnimation(ui::Layer* resized_container_layer,
                                             const int old_bounds_height) {
  resized_container_layer->CompleteAllAnimations();
  // TODO(b/313923915): we may want to use `SetTransform` instead of `SetBounds`
  // in future. If so, we will set a background view which is siblings with the
  // task row header the header of task and the `FocusModeTaskView`, and we will
  // only transform the background view with animation.
  const gfx::Rect target_bounds = resized_container_layer->bounds();

  // We need to calculate the actual old bounds to guarantee that we get the
  // correct old bounds if we scroll down the `FocusModeDetailedView`.
  auto old_bounds = resized_container_layer->bounds();
  old_bounds.set_height(old_bounds_height);
  resized_container_layer->SetBounds(old_bounds);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kContainerViewResizeDuration)
      .SetBounds(resized_container_layer, target_bounds,
                 gfx::Tween::ACCEL_20_DECEL_100);
}

}  // namespace ash
