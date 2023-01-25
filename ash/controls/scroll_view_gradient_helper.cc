// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/controls/scroll_view_gradient_helper.h"

#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/animation_builder.h"

namespace ash {

const base::TimeDelta kAnimationDuration = base::Milliseconds(50);

ScrollViewGradientHelper::ScrollViewGradientHelper(
    views::ScrollView* scroll_view,
    int gradient_height)
    : scroll_view_(scroll_view), gradient_height_(gradient_height) {
  DCHECK(scroll_view_);
  DCHECK(scroll_view_->layer());
  on_contents_scrolled_subscription_ =
      scroll_view_->AddContentsScrolledCallback(
          base::BindRepeating(&ScrollViewGradientHelper::UpdateGradientMask,
                              base::Unretained(this)));
  on_contents_scroll_ended_subscription_ =
      scroll_view_->AddContentsScrollEndedCallback(
          base::BindRepeating(&ScrollViewGradientHelper::UpdateGradientMask,
                              base::Unretained(this)));
  scroll_view_->SetPreferredViewportMargins(
      gfx::Insets::VH(gradient_height_, 0));
}

ScrollViewGradientHelper::~ScrollViewGradientHelper() {
  RemoveMaskLayer();
  scroll_view_->SetPreferredViewportMargins(gfx::Insets());
}

void ScrollViewGradientHelper::UpdateGradientMask() {
  DCHECK(scroll_view_->contents());

  const gfx::Rect visible_rect = scroll_view_->GetVisibleRect();
  // Show the top gradient if the scroll view is not scrolled to the top.
  const bool show_top_gradient = visible_rect.y() > 0;
  // Show the bottom gradient if the scroll view is not scrolled to the bottom.
  const bool show_bottom_gradient =
      visible_rect.bottom() < scroll_view_->contents()->bounds().bottom();

  // If no gradient is needed, remove the gradient mask.
  if (scroll_view_->contents()->bounds().IsEmpty()) {
    RemoveMaskLayer();
    return;
  }
  if (!show_top_gradient && !show_bottom_gradient) {
    RemoveMaskLayer();
    return;
  }

  // Vertical linear gradient, from top to bottom.
  gfx::LinearGradient gradient_mask(/*angle=*/-90);
  // Clamp fade_position to the ~middle. If we don't do this, then in degenerate
  // cases (where the gradients are larger than the scroll view itself) we would
  // end up passing bogus values to the gradient mask.
  const float fade_position = std::min(
      static_cast<float>(gradient_height_) / scroll_view_->bounds().height(),
      0.49f);

  // Top fade in section.
  if (show_top_gradient) {
    gradient_mask.AddStep(/*fraction=*/0, /*alpha=*/0);
    gradient_mask.AddStep(fade_position, 255);
  }

  // Bottom fade out section.
  if (show_bottom_gradient) {
    gradient_mask.AddStep(/*fraction=*/(1 - fade_position), /*alpha=*/255);
    gradient_mask.AddStep(1, 0);
  }

  // If a gradient update is needed, add the gradient mask to the scroll view
  // layer.
  if (scroll_view_->layer()->gradient_mask() != gradient_mask) {
    DVLOG(1) << "Adding gradient mask";

    if (first_time_update_) {
      scroll_view_->layer()->SetGradientMask(gradient_mask);
    } else {
      // On first call to UpdateGradientMask, animate in the gradient.
      AnimateMaskLayer(gradient_mask);
      first_time_update_ = true;
    }
  }
}

void ScrollViewGradientHelper::AnimateMaskLayer(
    const gfx::LinearGradient& target_gradient) {
  // Instead of starting the animation with fully transparent frame,
  // use an initial value so the first frame is opaque.
  gfx::LinearGradient start_gradient(target_gradient);
  for (auto& step : start_gradient.steps()) {
    if (step.alpha < 255)
      step.alpha = 255;
  }
  scroll_view_->layer()->SetGradientMask(start_gradient);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kAnimationDuration)
      .SetGradientMask(scroll_view_, target_gradient);
}

void ScrollViewGradientHelper::RemoveMaskLayer() {
  if (!scroll_view_->layer()->HasGradientMask())
    return;

  DVLOG(1) << "Removing gradient mask";
  scroll_view_->layer()->SetGradientMask(gfx::LinearGradient::GetEmpty());
}

}  // namespace ash
