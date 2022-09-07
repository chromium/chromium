// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/apps_grid_row_change_animator.h"

#include <utility>

#include "ash/app_list/views/apps_grid_view.h"
#include "base/auto_reset.h"
#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/bounds_animator.h"

namespace ash {

AppsGridRowChangeAnimator::AppsGridRowChangeAnimator(
    AppsGridView* apps_grid_view)
    : apps_grid_view_(apps_grid_view) {}

AppsGridRowChangeAnimator::~AppsGridRowChangeAnimator() = default;

void AppsGridRowChangeAnimator::AnimateBetweenRows(AppListItemView* view,
                                                   const gfx::Rect& current,
                                                   const gfx::Rect& target) {
  base::AutoReset<bool> auto_reset(&setting_up_animation_, true);

  // The direction used to calculate the offset for animating the item view into
  // and the layer copy out of the grid.
  const int dir = current.y() < target.y() ? 1 : -1;
  int offset =
      apps_grid_view_->GetTotalTileSize(apps_grid_view_->GetSelectedPage())
          .width();

  // The transform for moving the layer copy out and off screen.
  gfx::Transform layer_copy_transform;

  // The bounds where `view` will begin animating to `target`.
  gfx::RectF target_in;

  // The layer copy of the view which is animating between rows.
  std::unique_ptr<ui::Layer> row_change_layer;

  if (row_change_layers_.count(view)) {
    row_change_layer = std::move(row_change_layers_[view]);
    row_change_layers_.erase(view);

    // Reverse existing row change animation, by swapping the positions of the
    // mid-animation layer copy and `view`. Then animate each to the correct
    // target.

    // Calculate 'target_in' so that 'view' can start its animation in the place
    // of the layer copy.
    gfx::RectF layer_copy_bounds = gfx::RectF(row_change_layer->bounds());
    row_change_layer->transform().TransformRect(&layer_copy_bounds);
    target_in = gfx::RectF(apps_grid_view_->items_container_->GetMirroredRect(
        gfx::ToRoundedRect(layer_copy_bounds)));

    // Calculate the current bounds of `view` including its layer transform.
    gfx::RectF current_bounds_in_animation =
        gfx::RectF(view->GetMirroredBounds());
    view->layer()->transform().TransformRect(&current_bounds_in_animation);

    // Set the bounds of the layer copy to the current bounds of'view'.
    row_change_layer->SetBounds(
        gfx::ToRoundedRect(current_bounds_in_animation));
    row_change_layer->SetTransform(gfx::Transform());

    // Calculate where offscreen the layer copy will animate to.
    gfx::Rect current_out =
        apps_grid_view_->bounds_animator_->GetTargetBounds(view);
    current_out.Offset(dir * offset, 0);

    // Calculate the transform to move the layer copy off screen. Do not mirror
    // `current_bounds_in_animation` because it has already been mirrored.
    layer_copy_transform = gfx::TransformBetweenRects(
        gfx::RectF(current_bounds_in_animation),
        gfx::RectF(
            apps_grid_view_->items_container_->GetMirroredRect(current_out)));

    // Swap the opacity of the layer copy and the item view.
    const float layer_copy_opacity = row_change_layer->opacity();
    row_change_layer->SetOpacity(view->layer()->opacity());
    view->layer()->SetOpacity(layer_copy_opacity);
  } else {
    // Create the row change animation for this view. `view` will animate from
    // offscreen into the 'target' location, while a layer copy of 'view' will
    // animate from the original `view` bounds to offscreen.
    view->EnsureLayer();
    row_change_layer = view->RecreateLayer();

    // Calculate where offscreen the layer copy will animate to.
    gfx::Rect current_out = current;
    current_out.Offset(dir * offset, 0);

    // Calculate the transform to move the layer copy off screen.
    layer_copy_transform = gfx::TransformBetweenRects(
        gfx::RectF(apps_grid_view_->items_container_->GetMirroredRect(current)),
        gfx::RectF(
            apps_grid_view_->items_container_->GetMirroredRect(current_out)));

    view->layer()->SetOpacity(0.0f);

    // Calculate offscreen position to begin animating `view` from.
    target_in = gfx::RectF(target);
    target_in.Offset(-dir * offset, 0);
  }

  // Fade out and animate out the copied layer. Fade in the real item view.
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::Milliseconds(300))
      .SetTransform(row_change_layer.get(), layer_copy_transform,
                    gfx::Tween::ACCEL_40_DECEL_100_3)
      .SetOpacity(row_change_layer.get(), 0.0f,
                  gfx::Tween::ACCEL_40_DECEL_100_3)
      .SetOpacity(view->layer(), 1.0f, gfx::Tween::ACCEL_40_DECEL_100_3);

  // Stop animating to reset the bounds and transform of `view` before starting
  // the next animation.
  apps_grid_view_->bounds_animator_->StopAnimatingView(view);
  // Bounds animate the real item view to the target position.
  view->SetBoundsRect(gfx::ToRoundedRect(target_in));
  apps_grid_view_->bounds_animator_->AnimateViewTo(view, target);

  row_change_layers_[view] = std::move(row_change_layer);
}

void AppsGridRowChangeAnimator::OnBoundsAnimatorDone() {
  if (setting_up_animation_)
    return;

  // Erase row change layers for any item views which are not currently
  // animating.
  for (auto it = row_change_layers_.begin(); it != row_change_layers_.end();) {
    if (!apps_grid_view_->bounds_animator_->IsAnimating(it->first))
      it = row_change_layers_.erase(it);
    else
      it++;
  }
}

void AppsGridRowChangeAnimator::CancelAnimation(views::View* view) {
  row_change_layers_.erase(view);
}

bool AppsGridRowChangeAnimator::IsAnimating() const {
  return !row_change_layers_.empty() || setting_up_animation_;
}

}  // namespace ash
