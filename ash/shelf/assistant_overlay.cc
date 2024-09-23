// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/assistant_overlay.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/paint_context.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {
constexpr int kFullRetractDurationMs = 300;
constexpr int kFullBurstDurationMs = 200;

constexpr float kRippleCircleInitRadiusDip = 40.f;
constexpr float kRippleCircleStartRadiusDip = 1.f;
constexpr float kRippleCircleRadiusDip = 77.f;
constexpr float kRippleCircleBurstRadiusDip = 96.f;
constexpr int kRippleExpandDurationMs = 400;
constexpr int kRippleOpacityDurationMs = 100;
constexpr int kRippleOpacityRetractDurationMs = 200;
constexpr float kRippleOpacity = 0.2f;

constexpr int kHideDurationMs = 200;

}  // namespace

AssistantOverlay::AssistantOverlay(HomeButton* host_view)
    : ripple_layer_(std::make_unique<ui::Layer>()),
      host_view_(host_view),
      circle_layer_delegate_(gfx::kPlaceholderColor,
                             kRippleCircleInitRadiusDip) {
  SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  layer()->SetName("AssistantOverlay:ROOT_LAYER");
  layer()->SetMasksToBounds(false);

  ripple_layer_->SetBounds(gfx::Rect(0, 0, kRippleCircleInitRadiusDip * 2,
                                     kRippleCircleInitRadiusDip * 2));
  ripple_layer_->set_delegate(&circle_layer_delegate_);
  ripple_layer_->SetFillsBoundsOpaquely(false);
  ripple_layer_->SetMasksToBounds(true);
  ripple_layer_->SetName("AssistantOverlay:PAINTED_LAYER");
  layer()->Add(ripple_layer_.get());
}

AssistantOverlay::~AssistantOverlay() {
  StopObservingImplicitAnimations();
}

void AssistantOverlay::StartAnimation(bool show_icon) {
  animation_state_ = AnimationState::STARTING;
  show_icon_ = show_icon;
  SetVisible(true);

  // Remove clip_rect from host_view_ and its ancestors as the animation goes
  // beyond its size. We delete the object once end/hide/burst animation ends
  // which will follow this starting animation.
  scoped_no_clip_rect_ = host_view_->CreateScopedNoClipRect();

  // Setup ripple initial state.
  ripple_layer_->SetOpacity(0);

  SkScalar scale_factor =
      kRippleCircleStartRadiusDip / kRippleCircleInitRadiusDip;
  gfx::Transform transform;

  const gfx::PointF center = host_view_->GetCenterPoint();
  transform.Translate(center.x() - kRippleCircleStartRadiusDip,
                      center.y() - kRippleCircleStartRadiusDip);
  transform.Scale(scale_factor, scale_factor);
  ripple_layer_->SetTransform(transform);

  // Setup ripple animations.
  {
    scale_factor = kRippleCircleRadiusDip / kRippleCircleInitRadiusDip;
    transform.MakeIdentity();
    transform.Translate(center.x() - kRippleCircleRadiusDip,
                        center.y() - kRippleCircleRadiusDip);
    transform.Scale(scale_factor, scale_factor);

    ui::ScopedLayerAnimationSettings settings(ripple_layer_->GetAnimator());
    settings.SetTransitionDuration(base::Milliseconds(kRippleExpandDurationMs));
    settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN_2);

    ripple_layer_->SetTransform(transform);

    settings.SetTransitionDuration(
        base::Milliseconds(kRippleOpacityDurationMs));
    ripple_layer_->SetOpacity(kRippleOpacity);
  }
}

void AssistantOverlay::BurstAnimation() {
  animation_state_ = AnimationState::BURSTING;

  const gfx::PointF center = host_view_->GetCenterPoint();
  gfx::Transform transform;

  // Setup ripple animations.
  {
    SkScalar scale_factor =
        kRippleCircleBurstRadiusDip / kRippleCircleInitRadiusDip;
    transform.Translate(center.x() - kRippleCircleBurstRadiusDip,
                        center.y() - kRippleCircleBurstRadiusDip);
    transform.Scale(scale_factor, scale_factor);

    ui::ScopedLayerAnimationSettings settings(ripple_layer_->GetAnimator());
    settings.SetTransitionDuration(base::Milliseconds(kFullBurstDurationMs));
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::PreemptionStrategy::ENQUEUE_NEW_ANIMATION);
    settings.AddObserver(this);

    ripple_layer_->SetTransform(transform);
    ripple_layer_->SetOpacity(0);
  }
}

void AssistantOverlay::EndAnimation() {
  if (IsBursting() || IsHidden()) {
    // Too late, user action already fired, we have to finish what's started.
    // Or the widget has already been hidden, no need to play the end animation.
    return;
  }

  // Play reverse animation
  // Setup ripple animations.
  SkScalar scale_factor =
      kRippleCircleStartRadiusDip / kRippleCircleInitRadiusDip;
  gfx::Transform transform;

  const gfx::PointF center = host_view_->GetCenterPoint();
  transform.Translate(center.x() - kRippleCircleStartRadiusDip,
                      center.y() - kRippleCircleStartRadiusDip);
  transform.Scale(scale_factor, scale_factor);

  {
    ui::ScopedLayerAnimationSettings settings(ripple_layer_->GetAnimator());
    settings.SetPreemptionStrategy(ui::LayerAnimator::PreemptionStrategy::
                                       IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTransitionDuration(base::Milliseconds(kFullRetractDurationMs));
    settings.SetTweenType(gfx::Tween::SLOW_OUT_LINEAR_IN);
    settings.AddObserver(this);

    ripple_layer_->SetTransform(transform);

    settings.SetTransitionDuration(
        base::Milliseconds(kRippleOpacityRetractDurationMs));
    ripple_layer_->SetOpacity(0);
  }
}

void AssistantOverlay::HideAnimation() {
  animation_state_ = AnimationState::HIDDEN;

  // Setup ripple animations.
  {
    ui::ScopedLayerAnimationSettings settings(ripple_layer_->GetAnimator());
    settings.SetTransitionDuration(base::Milliseconds(kHideDurationMs));
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::PreemptionStrategy::ENQUEUE_NEW_ANIMATION);
    settings.AddObserver(this);

    ripple_layer_->SetOpacity(0);
  }
}

void AssistantOverlay::OnThemeChanged() {
  views::View::OnThemeChanged();
  circle_layer_delegate_.set_color(
      GetColorProvider()->GetColor(kColorAshInkDropOpaqueColor));
  SchedulePaint();
}

void AssistantOverlay::OnImplicitAnimationsCompleted() {
  scoped_no_clip_rect_.reset();
}

BEGIN_METADATA(AssistantOverlay)
END_METADATA

}  // namespace ash
