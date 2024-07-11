// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/pulsing_block_view.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "ash/style/ash_color_provider.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/check_op.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/animation_sequence_block.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace {

const base::TimeDelta kPulsingDuration = base::Milliseconds(500);

void SchedulePulsingAnimation(ui::Layer* layer) {
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
                                   base::TimeDelta animation_delay,
                                   float corner_radius)
    : block_size_(size) {
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
          .AddChild(
              views::Builder<views::View>()
                  .CopyAddressTo(&background_color_view_)
                  .SetEnabled(false)
                  .SetBackground(views::CreateSolidBackground(
                      DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()
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
  stacked_views->layer()->SetRoundedCornerRadius(
      {corner_radius, corner_radius, corner_radius, corner_radius});

  start_delay_timer_.Start(FROM_HERE, animation_delay, this,
                           &PulsingBlockView::OnStartDelayTimer);
}

PulsingBlockView::~PulsingBlockView() {}

void PulsingBlockView::OnStartDelayTimer() {
  // Restart the timer to schedule the animation if animations are disabled.
  // NOTE: `ScreenRotationAnimator` can set animations to ZERO_DURATION.
  if (ui::ScopedAnimationDurationScaleMode::is_zero()) {
    start_delay_timer_.Start(FROM_HERE, base::Seconds(1), this,
                             &PulsingBlockView::OnStartDelayTimer);
    return;
  }

  background_color_view_->SetPaintToLayer();
  background_color_view_->layer()->SetFillsBoundsOpaquely(false);

  SchedulePulsingAnimation(background_color_view_->layer());
}

void PulsingBlockView::OnThemeChanged() {
  views::View::OnThemeChanged();

  if (background_color_view_) {
    background_color_view_->SetBackground(views::CreateSolidBackground(
        DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()
            ? SkColorSetA(SK_ColorWHITE, 0x4D)
            : SkColorSetA(SK_ColorBLACK, 0x33)));
  }
}

bool PulsingBlockView::IsAnimating() {
  return background_color_view_->layer() &&
         background_color_view_->layer()->GetAnimator()->is_animating();
}

bool PulsingBlockView::FireAnimationTimerForTest() {
  if (!start_delay_timer_.IsRunning())
    return false;

  start_delay_timer_.FireNow();
  return true;
}

BEGIN_METADATA(PulsingBlockView)
END_METADATA

}  // namespace ash
