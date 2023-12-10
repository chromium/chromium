// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_view_util.h"

#include <optional>

#include "base/functional/callback.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/view.h"

namespace ash {

bool PrepareForLayerAnimation(views::View* view) {
  // Abort any in-progress layer animation. Views might have temporary layers
  // during animations that are cleaned up at the end. The code below needs to
  // know the final desired layer state.
  if (view->layer()) {
    DCHECK(view->layer()->GetAnimator());
    view->layer()->GetAnimator()->AbortAllAnimations();
  }

  // Add a layer for the view if it doesn't have one at baseline.
  // NOTE: animation abortion may trigger the callback that deletes the layer.
  // Therefore `created_layer` should be calculated after aborting animations.
  const bool create_layer = !view->layer();
  if (create_layer) {
    view->SetPaintToLayer();
    view->layer()->SetFillsBoundsOpaquely(false);
  }

  return create_layer;
}

void StartSlideInAnimation(views::View* view,
                           int vertical_offset,
                           const base::TimeDelta& time_delta,
                           gfx::Tween::Type tween_type,
                           base::RepeatingClosure cleanup) {
  DCHECK(view->layer());

  // Set the initial offset via a layer transform.
  gfx::Transform translate_down;
  translate_down.Translate(0, vertical_offset);
  view->layer()->SetTransform(translate_down);

  // Animate the transform back to the identity transform.
  views::AnimationBuilder()
      .OnEnded(cleanup)
      .OnAborted(cleanup)
      .Once()
      .SetTransform(view, gfx::Transform(), tween_type)
      .SetDuration(time_delta);
}

void SlideViewIntoPositionWithSequenceBlock(
    views::View* view,
    int vertical_offset,
    const std::optional<base::TimeDelta>& time_delta,
    gfx::Tween::Type tween_type,
    views::AnimationSequenceBlock* sequence_block) {
  DCHECK(view->layer());

  // Set the initial offset via a layer transform.
  gfx::Transform translate_down;
  translate_down.Translate(0, vertical_offset);
  view->layer()->SetTransform(translate_down);

  // Animate the transform back to the identity transform.
  sequence_block->SetTransform(view, gfx::Transform(), tween_type);

  if (time_delta)
    sequence_block->SetDuration(*time_delta);
}

}  // namespace ash
