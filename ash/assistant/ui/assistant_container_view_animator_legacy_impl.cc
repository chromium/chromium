// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_container_view_animator_legacy_impl.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/ui/assistant_container_view.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "base/metrics/histogram_macros.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

// Animation.
constexpr int kAnimationDurationMs = 250;

// Helpers ---------------------------------------------------------------------

int GetCompositorFrameNumber(ui::Layer* layer) {
  ui::Compositor* compositor = layer->GetCompositor();
  return compositor ? compositor->activated_frame_count() : 0;
}

float GetCompositorRefreshRate(ui::Layer* layer) {
  ui::Compositor* compositor = layer->GetCompositor();
  return compositor ? compositor->refresh_rate() : 60.0f;
}

}  // namespace

// AssistantContainerViewAnimatorLegacyImpl ------------------------------------

AssistantContainerViewAnimatorLegacyImpl::
    AssistantContainerViewAnimatorLegacyImpl(
        AssistantController* assistant_controller,
        AssistantContainerView* assistant_container_view)
    : AssistantContainerViewAnimator(assistant_controller,
                                     assistant_container_view) {}

AssistantContainerViewAnimatorLegacyImpl::
    ~AssistantContainerViewAnimatorLegacyImpl() = default;

void AssistantContainerViewAnimatorLegacyImpl::Init() {
  // Initialize background layer.
  background_layer_.SetFillsBoundsOpaquely(false);
  UpdateBackground();

  // Add background layer to the non-client view layer.
  assistant_container_view_->GetNonClientViewLayer()->Add(&background_layer_);

  // The AssistantContainerView layer masks to bounds to ensure clipping of
  // child layers during animation.
  assistant_container_view_->layer()->SetMasksToBounds(true);
}

void AssistantContainerViewAnimatorLegacyImpl::OnBoundsChanged() {
  UpdateBackground();
  assistant_container_view_->SchedulePaint();
}

void AssistantContainerViewAnimatorLegacyImpl::OnPreferredSizeChanged() {
  if (!assistant_container_view_->GetWidget())
    return;

  end_radius_ = assistant_controller_->ui_controller()->model()->ui_mode() ==
                        AssistantUiMode::kMiniUi
                    ? kMiniUiCornerRadiusDip
                    : kCornerRadiusDip;

  const bool visible =
      assistant_controller_->ui_controller()->model()->visibility() ==
      AssistantVisibility::kVisible;

  // When visible, size changes are animated.
  if (visible) {
    animation_ = std::make_unique<gfx::SlideAnimation>(this);
    animation_->SetSlideDuration(kAnimationDurationMs);

    // Cache start and end animation values.
    start_size_ = gfx::SizeF(assistant_container_view_->size());
    end_size_ = gfx::SizeF(assistant_container_view_->GetPreferredSize());
    start_radius_ = assistant_container_view_->GetCornerRadius();

    // Cache start frame number for measuring animation smoothness.
    start_frame_number_ =
        GetCompositorFrameNumber(assistant_container_view_->layer());

    // Start animation.
    animation_->Show();
    return;
  }

  // Clear any existing animation.
  animation_.reset();

  // Update corner radius and resize without animation.
  assistant_container_view_->SetCornerRadius(end_radius_);
  assistant_container_view_->SizeToContents();
}

void AssistantContainerViewAnimatorLegacyImpl::AnimationProgressed(
    const gfx::Animation* animation) {
  if (!assistant_container_view_->GetWidget())
    return;

  // Retrieve current bounds.
  gfx::Rect bounds =
      assistant_container_view_->GetWidget()->GetWindowBoundsInScreen();

  // Our view is horizontally centered and bottom aligned. As such, we should
  // retain the same |center_x| and |bottom| positions after resizing.
  const int bottom = bounds.bottom();
  const int center_x = bounds.CenterPoint().x();

  // Interpolate size at our current animation value.
  const gfx::SizeF size = gfx::Tween::SizeValueBetween(
      animation->GetCurrentValue(), start_size_, end_size_);

  // Use our interpolated size.
  bounds.set_size(gfx::Size(size.width(), size.height()));

  // Maintain original |center_x| and |bottom| positions.
  bounds.set_x(center_x - (bounds.width() / 2));
  bounds.set_y(bottom - bounds.height());

  // Interpolate the correct corner radius.
  const int corner_radius = gfx::Tween::LinearIntValueBetween(
      animation->GetCurrentValue(), start_radius_, end_radius_);

  // Update corner radius and bounds.
  assistant_container_view_->SetCornerRadius(corner_radius);
  assistant_container_view_->GetWidget()->SetBounds(bounds);
}

void AssistantContainerViewAnimatorLegacyImpl::AnimationEnded(
    const gfx::Animation* animation) {
  const int ideal_frames =
      GetCompositorRefreshRate(assistant_container_view_->layer()) *
      kAnimationDurationMs / base::Time::kMillisecondsPerSecond;

  const int actual_frames =
      GetCompositorFrameNumber(assistant_container_view_->layer()) -
      start_frame_number_;

  if (actual_frames <= 0)
    return;

  // The |actual_frames| could be |ideal_frames| + 1. The reason could be that
  // the animation timer is running with interval of 0.016666 sec, which could
  // animate one more frame than expected due to rounding error.
  const int smoothness = std::min(100 * actual_frames / ideal_frames, 100);
  UMA_HISTOGRAM_PERCENTAGE("Assistant.ContainerView.Resize.AnimationSmoothness",
                           smoothness);
}

void AssistantContainerViewAnimatorLegacyImpl::UpdateBackground() {
  gfx::ShadowValues shadow_values =
      gfx::ShadowValue::MakeMdShadowValues(wm::kShadowElevationActiveWindow);

  shadow_delegate_ = std::make_unique<views::BorderShadowLayerDelegate>(
      shadow_values, assistant_container_view_->GetLocalBounds(),
      assistant_container_view_->GetBackgroundColor(),
      assistant_container_view_->GetCornerRadius());

  background_layer_.set_delegate(shadow_delegate_.get());
  background_layer_.SetBounds(
      gfx::ToEnclosingRect(shadow_delegate_->GetPaintedBounds()));
}

}  // namespace ash
