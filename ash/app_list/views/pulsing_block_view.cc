// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/pulsing_block_view.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_provider.h"
#include "base/check_op.h"
#include "base/rand_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/animation_sequence_block.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace {

const base::TimeDelta kPulsingDuration = base::Milliseconds(500);

const SkColor kBlockColor = SkColorSetRGB(225, 225, 225);
const int kBlockSize = 64;

const int kAnimationDurationInMs = 600;
const float kAnimationOpacity[] = {0.4f, 0.8f, 0.4f};
const float kAnimationScale[] = {0.8f, 1.0f, 0.8f};

void SchedulePulsingAnimation(ui::Layer* layer) {
  DCHECK(layer);
  DCHECK_EQ(std::size(kAnimationOpacity), std::size(kAnimationScale));

  const gfx::Rect local_bounds(layer->bounds().size());
  views::AnimationBuilder builder;
  builder.Repeatedly();
  for (size_t i = 0; i < std::size(kAnimationOpacity); ++i) {
    builder.GetCurrentSequence()
        .SetDuration(base::Milliseconds(kAnimationDurationInMs))
        .SetOpacity(layer, kAnimationOpacity[i])
        .SetTransform(layer, gfx::GetScaleTransform(local_bounds.CenterPoint(),
                                                    kAnimationScale[i]))
        .Then();
  }
  builder.GetCurrentSequence().SetDuration(
      base::Milliseconds(kAnimationDurationInMs));
}

void ScheduleNewPulsingAnimation(ui::Layer* layer) {
  DCHECK(layer);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Repeatedly()
      .SetDuration(kPulsingDuration)
      .SetOpacity(layer, 0.5f, gfx::Tween::FAST_OUT_LINEAR_IN)
      .At(kPulsingDuration)
      .SetDuration(kPulsingDuration)
      .SetOpacity(layer, 1.0f, gfx::Tween::LINEAR);
}

}  // namespace

namespace ash {

PulsingBlockView::PulsingBlockView(const gfx::Size& size,
                                   base::TimeDelta animation_delay)
    : block_size_(size) {
  if (ash::features::IsLauncherPulsingBlocksRefreshEnabled()) {
    views::BoxLayout* layout_manager =
        SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    layout_manager->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout_manager->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);

    // Stack two views for the same block. The bottom view contains the
    // background blur, which will be visible all the time. The top view
    // contains the color of the block, which will animate its opacity.
    views::View* stacked_views = AddChildView(
        views::Builder<views::View>()
            .SetVisible(true)
            .SetLayoutManager(std::make_unique<views::FillLayout>())
            .AddChild(views::Builder<views::View>()
                          .CopyAddressTo(&background_color_view_)
                          .SetEnabled(false)
                          .SetBackground(views::CreateSolidBackground(
                              AshColorProvider::Get()->IsDarkModeEnabled()
                                  ? SkColorSetA(SK_ColorWHITE, 0x4D)
                                  : SkColorSetA(SK_ColorBLACK, 0x33)))
                          .SetPreferredSize(block_size_))
            .SetPreferredSize(block_size_)
            .SetPaintToLayer()
            .Build());

    stacked_views->layer()->SetMasksToBounds(true);
    stacked_views->layer()->SetBackgroundBlur(
        ColorProvider::kBackgroundBlurSigma);
    stacked_views->layer()->SetBackdropFilterQuality(
        ColorProvider::kBackgroundBlurQuality);
    const float radii = block_size_.height() / 2.0f;
    stacked_views->layer()->SetRoundedCornerRadius(
        {radii, radii, radii, radii});

    start_delay_timer_.Start(FROM_HERE, animation_delay, this,
                             &PulsingBlockView::OnStartDelayTimer);
  } else {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    const int max_delay = kAnimationDurationInMs * std::size(kAnimationOpacity);
    const int delay = base::RandInt(0, max_delay);
    start_delay_timer_.Start(FROM_HERE, base::Milliseconds(delay), this,
                             &PulsingBlockView::OnStartDelayTimer);
  }
}

PulsingBlockView::~PulsingBlockView() {}

const char* PulsingBlockView::GetClassName() const {
  return "PulsingBlockView";
}

void PulsingBlockView::OnStartDelayTimer() {
  if (!ash::features::IsLauncherPulsingBlocksRefreshEnabled()) {
    SchedulePulsingAnimation(layer());
    return;
  }
  background_color_view_->SetPaintToLayer();
  background_color_view_->layer()->SetFillsBoundsOpaquely(false);

  ScheduleNewPulsingAnimation(background_color_view_->layer());
}

void PulsingBlockView::OnThemeChanged() {
  views::View::OnThemeChanged();

  if (!ash::features::IsLauncherPulsingBlocksRefreshEnabled())
    return;

  if (background_color_view_) {
    background_color_view_->SetBackground(views::CreateSolidBackground(
        AshColorProvider::Get()->IsDarkModeEnabled()
            ? SkColorSetA(SK_ColorWHITE, 0x4D)
            : SkColorSetA(SK_ColorBLACK, 0x33)));
  }
}

void PulsingBlockView::OnPaint(gfx::Canvas* canvas) {
  if (ash::features::IsLauncherPulsingBlocksRefreshEnabled()) {
    views::View::OnPaint(canvas);
    return;
  }
  gfx::Rect rect(GetContentsBounds());
  rect.ClampToCenteredSize(gfx::Size(kBlockSize, kBlockSize));
  canvas->FillRect(rect, kBlockColor);
}

}  // namespace ash
