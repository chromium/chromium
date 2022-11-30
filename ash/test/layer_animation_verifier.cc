// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/layer_animation_verifier.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// A threshold set empirically.
constexpr int kMinimumComparisonCount = 8;

// Returns whether `layer` is in animation progress.
bool IsLayerUnderAnimation(ui::Layer* layer) {
  return layer->GetAnimator()->is_animating();
}

}  // namespace

LayerAnimationVerifier::LayerAnimationVerifier(ui::Layer* layer_with_animation,
                                               views::View* observed_view)
    : layer_with_animation_(layer_with_animation),
      observed_view_(observed_view) {
  DCHECK_EQ(GetCompositor(), observed_view->GetWidget()->GetCompositor());

  // Ensure that `LayerAnimationVerifier` observes a layer animation from start
  // to end without missing any part. Due to the same reason, the layer
  // animation is expected to have completed when `LayerAnimationVerifier` is
  // destructed.
  EXPECT_FALSE(IsLayerUnderAnimation(layer_with_animation_));

  GetCompositor()->AddObserver(this);
}

LayerAnimationVerifier::~LayerAnimationVerifier() {
  GetCompositor()->RemoveObserver(this);
  EXPECT_FALSE(IsLayerUnderAnimation(layer_with_animation_));

  // We need to collect enough data to verify the animation.
  EXPECT_GT(comparison_count_, kMinimumComparisonCount);
}

void LayerAnimationVerifier::OnCompositingDidCommit(
    ui::Compositor* compositor) {
  const gfx::Rect current_screen_bounds = observed_view_->GetBoundsInScreen();

  // Compare with `last_screen_bounds_` if any.
  if (last_screen_bounds_) {
    CompareWithLastBoundsRect(current_screen_bounds);
    return;
  }

  last_screen_bounds_ = current_screen_bounds;
  EXPECT_FALSE(x_direction_);
  EXPECT_FALSE(y_direction_);
}

void LayerAnimationVerifier::CompareWithLastBoundsRect(
    const gfx::Rect& current_screen_bounds) {
  ++comparison_count_;

  if (!x_direction_) {
    if (current_screen_bounds.x() != last_screen_bounds_->x()) {
      x_direction_ = CalculateMoveDirection(current_screen_bounds.x(),
                                            last_screen_bounds_->x());
    }
  } else {
    EXPECT_EQ(*x_direction_, CalculateMoveDirection(current_screen_bounds.x(),
                                                    last_screen_bounds_->x()));
  }

  if (!y_direction_) {
    if (current_screen_bounds.y() != last_screen_bounds_->y()) {
      y_direction_ = CalculateMoveDirection(current_screen_bounds.y(),
                                            last_screen_bounds_->y());
    }
  } else {
    EXPECT_EQ(*y_direction_, CalculateMoveDirection(current_screen_bounds.y(),
                                                    last_screen_bounds_->y()));
  }
}

LayerAnimationVerifier::MoveDirection
LayerAnimationVerifier::CalculateMoveDirection(int current_data,
                                               int last_data) const {
  if (current_data >= last_data)
    return MoveDirection::kForward;

  return MoveDirection::kBackward;
}

ui::Compositor* LayerAnimationVerifier::GetCompositor() {
  return layer_with_animation_->GetCompositor();
}

}  // namespace ash
