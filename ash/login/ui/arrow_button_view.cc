// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/arrow_button_view.h"

#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {
namespace {

constexpr int kArrowIconBackroundRadius = 25;

constexpr const int kBorderForFocusRingDp = 3;

// How long does a single step of the loading animation take - i.e., the time it
// takes for the arc to grow from a point to a full circle.
constexpr base::TimeDelta kLoadingAnimationStepDuration = base::Seconds(2);
// Size that transform animation will scale view up by.
constexpr SkScalar kTransformScaleSize = 1.2;
// How long a single scale up step of the transform animation takes.
constexpr base::TimeDelta kTransformScaleUpDuration = base::Milliseconds(500);
// How long a single scale down step of the transform animation takes.
constexpr base::TimeDelta kTransformScaleDownDuration =
    base::Milliseconds(1350);
// Time delay in between scaling up and down in the middle of transform
// animation cycle.
constexpr base::TimeDelta kTransformScaleDelayDuration =
    base::Milliseconds(150);
// Time delay in between each full cycle of the repeating transform animation.
constexpr base::TimeDelta kTransformDelayDuration = base::Milliseconds(1000);

void PaintLoadingArc(gfx::Canvas* canvas,
                     const gfx::Rect& bounds,
                     double loading_fraction) {
  gfx::Rect oval = bounds;
  // Inset to make sure the whole arc is inside the visible rect.
  oval.Inset(gfx::Insets::VH(/*vertical=*/1, /*horizontal=*/1));

  SkPath path;
  path.arcTo(RectToSkRect(oval), /*startAngle=*/-90,
             /*sweepAngle=*/360 * loading_fraction, /*forceMoveTo=*/true);

  cc::PaintFlags flags;
  // Use the same color as the arrow icon.
  flags.setColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);
  canvas->DrawPath(path, flags);
}

}  // namespace

ArrowButtonView::ArrowButtonView(PressedCallback callback, int size)
    : LoginButton(std::move(callback)) {
  SetBorder(views::CreateEmptyBorder(kBorderForFocusRingDp));
  SetPreferredSize(gfx::Size(size + 2 * kBorderForFocusRingDp,
                             size + 2 * kBorderForFocusRingDp));
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetBackgroundColorId(kColorAshControlBackgroundColorInactive);
  // Layer rendering is needed for animation.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::PreemptionStrategy::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  views::FocusRing::Get(this)->SetPathGenerator(
      std::make_unique<views::FixedSizeCircleHighlightPathGenerator>(
          kArrowIconBackroundRadius));
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(kLockScreenArrowIcon,
                                               kColorAshButtonIconColor));
  SetImageModel(views::Button::STATE_DISABLED,
                ui::ImageModel::FromVectorIcon(
                    kLockScreenArrowIcon, kColorAshButtonIconDisabledColor));
}

ArrowButtonView::~ArrowButtonView() = default;

void ArrowButtonView::PaintButtonContents(gfx::Canvas* canvas) {
  // Draw arrow icon.
  views::ImageButton::PaintButtonContents(canvas);

  // Draw the arc of the loading animation.
  if (loading_animation_) {
    const gfx::Rect rect(GetContentsBounds());
    PaintLoadingArc(canvas, rect, loading_animation_->GetCurrentValue());
  }
}

void ArrowButtonView::OnThemeChanged() {
  LoginButton::OnThemeChanged();
  SchedulePaint();
}

void ArrowButtonView::RunTransformAnimation() {
  StopAnimating();

  auto transform_sequence = std::make_unique<ui::LayerAnimationSequence>();

  // Translate by |center_offset| so that the view scales outward from center
  // point.
  gfx::Size preferred_size = CalculatePreferredSize({});
  auto center_offset = gfx::Vector2d(preferred_size.width() / 2.0,
                                     preferred_size.height() / 2.0);
  gfx::Transform transform;
  transform.Translate(center_offset);
  // Make view larger.
  transform.Scale(/*x=*/kTransformScaleSize, /*y=*/kTransformScaleSize);
  transform.Translate(-center_offset);
  auto element = ui::LayerAnimationElement::CreateTransformElement(
      transform, kTransformScaleUpDuration);
  element->set_tween_type(gfx::Tween::Type::ACCEL_40_DECEL_20);
  transform_sequence->AddElement(std::move(element));

  element = ui::LayerAnimationElement::CreatePauseElement(
      0, kTransformScaleDelayDuration);
  transform_sequence->AddElement(std::move(element));

  // Make view original size again.
  element = ui::LayerAnimationElement::CreateTransformElement(
      gfx::Transform(), kTransformScaleDownDuration);
  element->set_tween_type(gfx::Tween::Type::FAST_OUT_SLOW_IN_3);
  transform_sequence->AddElement(std::move(element));

  element =
      ui::LayerAnimationElement::CreatePauseElement(0, kTransformDelayDuration);
  transform_sequence->AddElement(std::move(element));

  transform_sequence->set_is_repeating(true);

  // Animator takes ownership of transform_sequence.
  layer()->GetAnimator()->StartAnimation(transform_sequence.release());
}

void ArrowButtonView::StopAnimating() {
  layer()->GetAnimator()->StopAnimating();
}

void ArrowButtonView::EnableLoadingAnimation(bool enabled) {
  if (!enabled) {
    if (!loading_animation_) {
      return;
    }
    loading_animation_.reset();
    SchedulePaint();
    return;
  }

  if (loading_animation_) {
    return;
  }

  // Use MultiAnimation in order to have a continuously running analog of
  // LinearAnimation.
  loading_animation_ =
      std::make_unique<gfx::MultiAnimation>(gfx::MultiAnimation::Parts{
          gfx::MultiAnimation::Part(kLoadingAnimationStepDuration,
                                    gfx::Tween::LINEAR),
      });
  loading_animation_->set_delegate(&loading_animation_delegate_);
  loading_animation_->Start();
}

ArrowButtonView::LoadingAnimationDelegate::LoadingAnimationDelegate(
    ArrowButtonView* owner)
    : owner_(owner) {}

ArrowButtonView::LoadingAnimationDelegate::~LoadingAnimationDelegate() =
    default;

void ArrowButtonView::LoadingAnimationDelegate::AnimationProgressed(
    const gfx::Animation* /*animation*/) {
  owner_->SchedulePaint();
}

void ArrowButtonView::SetBackgroundColorId(ui::ColorId color_id) {
  SetBackground(views::CreateThemedRoundedRectBackground(
      color_id, GetPreferredSize().width() / 2, 2 * kBorderForFocusRingDp));
}

BEGIN_METADATA(ArrowButtonView)
END_METADATA

}  // namespace ash
