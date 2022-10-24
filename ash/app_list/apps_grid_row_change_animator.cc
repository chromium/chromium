// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/apps_grid_row_change_animator.h"

#include <utility>

#include "ash/app_list/views/apps_grid_view.h"
#include "base/auto_reset.h"
#include "base/i18n/rtl.h"
#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform_util.h"

namespace ash {

AppsGridRowChangeAnimator::AppsGridRowChangeAnimator(
    AppsGridView* apps_grid_view)
    : apps_grid_view_(apps_grid_view) {}

AppsGridRowChangeAnimator::~AppsGridRowChangeAnimator() = default;

void AppsGridRowChangeAnimator::AnimateBetweenRows(
    AppListItemView* view,
    const gfx::Rect& current,
    const gfx::Rect& target,
    views::AnimationSequenceBlock* animation_sequence) {
  base::AutoReset<bool> auto_reset(&setting_up_animation_, true);

  // The direction used to calculate the offset for animating the item view into
  // and the layer copy out of the grid. Reversed for RTL.
  int dir = current.y() < target.y() ? 1 : -1;
  if (base::i18n::IsRTL())
    dir *= -1;

  int offset =
      apps_grid_view_->GetTotalTileSize(apps_grid_view_->GetSelectedPage())
          .width();

  // Calculate where offscreen the layer copy will animate to.
  gfx::Rect current_out = current;
  current_out.Offset(dir * offset, 0);

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
    target_in = row_change_layer->transform().MapRect(
        gfx::RectF(row_change_layer->bounds()));

    // Calculate the current bounds of `view` including its layer transform.
    gfx::RectF current_bounds_in_animation =
        view->layer()->transform().MapRect(gfx::RectF(current));

    // Set the bounds of the layer copy to the current bounds of'view'.
    row_change_layer->SetBounds(
        gfx::ToRoundedRect(current_bounds_in_animation));
    row_change_layer->SetTransform(gfx::Transform());

    layer_copy_transform = gfx::TransformBetweenRects(
        current_bounds_in_animation, gfx::RectF(current_out));

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

    layer_copy_transform = gfx::TransformBetweenRects(gfx::RectF(current),
                                                      gfx::RectF(current_out));

    view->layer()->SetOpacity(0.0f);

    // Calculate offscreen position to begin animating `view` from.
    target_in = gfx::RectF(target);
    const int target_in_direction = current.y() < target.y() ? -1 : 1;
    target_in.Offset(target_in_direction * offset, 0);
    target_in =
        gfx::RectF(apps_grid_view_->GetMirroredRect(ToRoundedRect(target_in)));
  }

  // Set the transform for the item view before animating it into the target
  // grid position.
  view->layer()->SetTransform(gfx::TransformBetweenRects(
      gfx::RectF(apps_grid_view_->GetMirroredRect(target)), target_in));
  view->SetBoundsRect(target);

  // Fade out and animate out the copied layer. Fade in the real item view.
  animation_sequence
      ->SetTransform(row_change_layer.get(), layer_copy_transform,
                     gfx::Tween::ACCEL_40_DECEL_100_3)
      .SetOpacity(row_change_layer.get(), 0.0f,
                  gfx::Tween::ACCEL_40_DECEL_100_3)
      .SetOpacity(view->layer(), 1.0f, gfx::Tween::ACCEL_40_DECEL_100_3)
      .SetTransform(view->layer(), gfx::Transform(),
                    gfx::Tween::ACCEL_40_DECEL_100_3);

  row_change_layers_[view] = std::move(row_change_layer);
}

void AppsGridRowChangeAnimator::CancelAnimation(views::View* view) {
  row_change_layers_.erase(view);
}

bool AppsGridRowChangeAnimator::IsAnimating() const {
  return !row_change_layers_.empty() || setting_up_animation_;
}

}  // namespace ash
