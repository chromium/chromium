// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/controls/scroll_view_gradient_helper.h"

#include <memory>

#include "ash/controls/gradient_layer_delegate.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace {

// Height of the gradient in DIPs.
constexpr int kGradientHeight = 16;

}  // namespace

ScrollViewGradientHelper::ScrollViewGradientHelper(
    views::ScrollView* scroll_view)
    : scroll_view_(scroll_view) {
  DCHECK(scroll_view_);
  DCHECK(scroll_view_->layer());
  on_contents_scrolled_subscription_ =
      scroll_view_->AddContentsScrolledCallback(
          base::BindRepeating(&ScrollViewGradientHelper::UpdateGradientZone,
                              base::Unretained(this)));
  on_contents_scroll_ended_subscription_ =
      scroll_view_->AddContentsScrollEndedCallback(
          base::BindRepeating(&ScrollViewGradientHelper::UpdateGradientZone,
                              base::Unretained(this)));
  scroll_view_->SetPreferredViewportMargins(
      gfx::Insets::VH(kGradientHeight, 0));
}

ScrollViewGradientHelper::~ScrollViewGradientHelper() {
  RemoveMaskLayer();
  scroll_view_->SetPreferredViewportMargins(gfx::Insets());
}

void ScrollViewGradientHelper::UpdateGradientZone() {
  DCHECK(scroll_view_->contents());

  const gfx::Rect visible_rect = scroll_view_->GetVisibleRect();
  // Show the top gradient if the scroll view is not scrolled to the top.
  const bool show_top_gradient = visible_rect.y() > 0;
  // Show the bottom gradient if the scroll view is not scrolled to the bottom.
  const bool show_bottom_gradient =
      visible_rect.bottom() < scroll_view_->contents()->bounds().bottom();

  const gfx::Rect scroll_view_bounds = scroll_view_->bounds();
  gfx::Rect top_gradient_bounds;
  if (show_top_gradient) {
    top_gradient_bounds =
        gfx::Rect(0, 0, scroll_view_bounds.width(), kGradientHeight);
  }
  gfx::Rect bottom_gradient_bounds;
  if (show_bottom_gradient) {
    bottom_gradient_bounds =
        gfx::Rect(0, scroll_view_bounds.height() - kGradientHeight,
                  scroll_view_bounds.width(), kGradientHeight);
  }

  // If no gradient is needed, remove the mask layer.
  if (top_gradient_bounds.IsEmpty() && bottom_gradient_bounds.IsEmpty()) {
    RemoveMaskLayer();
    return;
  }

  // If a gradient is needed, lazily create the GradientLayerDelegate.
  if (!gradient_layer_) {
    DVLOG(1) << "Adding gradient mask layer";
    // Animate showing the gradient to avoid a visual "pop" at the end of the
    // clamshell launcher open animation.
    gradient_layer_ =
        std::make_unique<GradientLayerDelegate>(/*animate_in=*/true);
    scroll_view_->layer()->SetMaskLayer(gradient_layer_->layer());
  }

  // If bounds didn't change, there's nothing to update.
  if (top_gradient_bounds == gradient_layer_->start_fade_zone_bounds() &&
      bottom_gradient_bounds == gradient_layer_->end_fade_zone_bounds()) {
    return;
  }

  // Update the fade in / fade out zones.
  gradient_layer_->set_start_fade_zone(
      {top_gradient_bounds, /*fade_in=*/true, /*is_horizontal=*/false});
  gradient_layer_->set_end_fade_zone(
      {bottom_gradient_bounds, /*fade_in=*/false, /*is_horizontal=*/false});
  gradient_layer_->layer()->SetBounds(scroll_view_->layer()->bounds());
  scroll_view_->SchedulePaint();
}

void ScrollViewGradientHelper::RemoveMaskLayer() {
  if (!gradient_layer_)
    return;
  DVLOG(1) << "Removing gradient mask layer";
  scroll_view_->layer()->SetMaskLayer(nullptr);
  gradient_layer_.reset();
}

}  // namespace ash
