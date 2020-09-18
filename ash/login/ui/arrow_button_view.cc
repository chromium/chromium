// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/arrow_button_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {
namespace {

// Arrow icon size.
constexpr int kArrowIconSizeDp = 20;
constexpr int kArrowIconBackroundRadius = 25;
// An alpha value for disabled button.
constexpr SkAlpha kButtonDisabledAlpha = 0x80;
// How long does a single step of the loading animation take - i.e., the time it
// takes for the arc to grow from a point to a full circle.
constexpr base::TimeDelta kLoadingAnimationStepDuration =
    base::TimeDelta::FromSeconds(2);

void PaintLoadingArc(gfx::Canvas* canvas,
                     const gfx::Rect& bounds,
                     double loading_fraction) {
  gfx::Rect oval = bounds;
  // Inset to make sure the whole arc is inside the visible rect.
  oval.Inset(/*horizontal=*/1, /*vertical=*/1);

  SkPath path;
  path.arcTo(RectToSkRect(oval), /*startAngle=*/-90,
             /*sweepAngle=*/360 * loading_fraction, /*forceMoveTo=*/true);

  cc::PaintFlags flags;
  flags.setColor(gfx::kGoogleGrey100);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);
  canvas->DrawPath(path, flags);
}

}  // namespace

ArrowButtonView::ArrowButtonView(views::ButtonListener* listener, int size)
    : LoginButton(listener), size_(size) {
  SetPreferredSize(gfx::Size(size, size));
  SetFocusBehavior(FocusBehavior::ALWAYS);

  // Layer rendering is needed for animation.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetImage(Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kLockScreenArrowIcon, kArrowIconSizeDp,
                                 SK_ColorWHITE));
  SetImage(
      views::Button::STATE_DISABLED,
      gfx::CreateVectorIcon(kLockScreenArrowIcon, kArrowIconSizeDp,
                            SkColorSetA(SK_ColorWHITE, kButtonDisabledAlpha)));
  focus_ring()->SetPathGenerator(
      std::make_unique<views::FixedSizeCircleHighlightPathGenerator>(
          kArrowIconBackroundRadius));
}

ArrowButtonView::~ArrowButtonView() = default;

void ArrowButtonView::PaintButtonContents(gfx::Canvas* canvas) {
  const gfx::Rect rect(GetContentsBounds());

  // Draw background.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(background_color_);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(gfx::PointF(rect.CenterPoint()), size_ / 2, flags);

  // Draw arrow icon.
  views::ImageButton::PaintButtonContents(canvas);

  // Draw the arc of the loading animation.
  if (loading_animation_)
    PaintLoadingArc(canvas, rect, loading_animation_->GetCurrentValue());
}

void ArrowButtonView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  LoginButton::GetAccessibleNodeData(node_data);
  // TODO(tbarzic): Fix this - https://crbug.com/961930.
  if (GetAccessibleName().empty())
    node_data->SetNameExplicitlyEmpty();
}

const char* ArrowButtonView::GetClassName() const {
  return "ArrowButtonView";
}

void ArrowButtonView::SetBackgroundColor(SkColor color) {
  background_color_ = color;
  SchedulePaint();
}

void ArrowButtonView::EnableLoadingAnimation(bool enabled) {
  if (!enabled) {
    if (!loading_animation_)
      return;
    loading_animation_.reset();
    SchedulePaint();
    return;
  }

  if (loading_animation_)
    return;

  // Use MultiAnimation in order to have a continuously running analog of
  // LinearAnimation.
  loading_animation_ = std::make_unique<gfx::MultiAnimation>(
      gfx::MultiAnimation::Parts{
          gfx::MultiAnimation::Part(kLoadingAnimationStepDuration,
                                    gfx::Tween::LINEAR),
      },
      gfx::MultiAnimation::kDefaultTimerInterval);
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

}  // namespace ash
