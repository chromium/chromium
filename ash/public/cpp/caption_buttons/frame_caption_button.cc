// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/caption_buttons/frame_caption_button.h"

#include "ash/public/cpp/ash_constants.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/window/hit_test_utils.h"

namespace ash {

namespace {

// Ink drop parameters.
constexpr float kInkDropVisibleOpacity = 0.06f;
constexpr int kInkDropCornerRadius = 14;

// The duration of the crossfade animation when swapping the button's images.
const int kSwapImagesAnimationDurationMs = 200;

// The duration of the fade out animation of the old icon during a crossfade
// animation as a ratio of |kSwapImagesAnimationDurationMs|.
const float kFadeOutRatio = 0.5f;

// The ratio applied to the button's alpha when the button is disabled.
const float kDisabledButtonAlphaRatio = 0.5f;

// Returns the amount by which the inkdrop ripple and mask should be insetted
// from the button size in order to achieve a circular inkdrop with a size
// equals to kInkDropHighlightSize.
gfx::Insets GetInkdropInsets(const gfx::Size& button_size) {
  constexpr gfx::Size kInkDropHighlightSize{2 * kInkDropCornerRadius,
                                            2 * kInkDropCornerRadius};
  return gfx::Insets(
      (button_size.height() - kInkDropHighlightSize.height()) / 2,
      (button_size.width() - kInkDropHighlightSize.width()) / 2);
}

}  // namespace

// static
const char FrameCaptionButton::kViewClassName[] = "FrameCaptionButton";

FrameCaptionButton::FrameCaptionButton(views::ButtonListener* listener,
                                       CaptionButtonIcon icon,
                                       int hit_test_type)
    : Button(listener),
      icon_(icon),
      background_color_(SK_ColorWHITE),
      color_mode_(ColorMode::kDefault),
      paint_as_active_(false),
      alpha_(255),
      swap_images_animation_(new gfx::SlideAnimation(this)) {
  views::SetHitTestComponent(this, hit_test_type);

  set_animate_on_state_change(true);
  swap_images_animation_->Reset(1);

  set_has_ink_drop_action_on_click(true);
  SetInkDropMode(InkDropMode::ON);
  set_ink_drop_visible_opacity(kInkDropVisibleOpacity);
  UpdateInkDropBaseColor();

  // Do not flip the gfx::Canvas passed to the OnPaint() method. The snap left
  // and snap right button icons should not be flipped. The other icons are
  // horizontally symmetrical.
}

FrameCaptionButton::~FrameCaptionButton() = default;

// static
SkColor FrameCaptionButton::GetButtonColor(ColorMode color_mode,
                                           SkColor background_color) {
  if (color_mode == ColorMode::kThemed)
    return color_utils::GetThemedAssetColor(background_color);

  DCHECK_EQ(color_mode, ColorMode::kDefault);
  return color_utils::IsDark(background_color) ? gfx::kGoogleGrey200
                                               : gfx::kGoogleGrey700;
}

// static
float FrameCaptionButton::GetInactiveButtonColorAlphaRatio() {
  return 0.38f;
}

void FrameCaptionButton::SetImage(CaptionButtonIcon icon,
                                  Animate animate,
                                  const gfx::VectorIcon& icon_definition) {
  gfx::ImageSkia new_icon_image = gfx::CreateVectorIcon(
      icon_definition, GetButtonColor(color_mode_, background_color_));

  // The early return is dependent on |animate| because callers use SetImage()
  // with ANIMATE_NO to progress the crossfade animation to the end.
  if (icon == icon_ &&
      (animate == ANIMATE_YES || !swap_images_animation_->is_animating()) &&
      new_icon_image.BackedBySameObjectAs(icon_image_)) {
    return;
  }

  if (animate == ANIMATE_YES)
    crossfade_icon_image_ = icon_image_;

  icon_ = icon;
  icon_definition_ = &icon_definition;
  icon_image_ = new_icon_image;

  if (animate == ANIMATE_YES) {
    swap_images_animation_->Reset(0);
    swap_images_animation_->SetSlideDuration(kSwapImagesAnimationDurationMs);
    swap_images_animation_->Show();
  } else {
    swap_images_animation_->Reset(1);
  }

  SchedulePaint();
}

bool FrameCaptionButton::IsAnimatingImageSwap() const {
  return swap_images_animation_->is_animating();
}

void FrameCaptionButton::SetAlpha(int alpha) {
  if (alpha_ != alpha) {
    alpha_ = alpha;
    SchedulePaint();
  }
}

const char* FrameCaptionButton::GetClassName() const {
  return kViewClassName;
}

void FrameCaptionButton::OnGestureEvent(ui::GestureEvent* event) {
  // Button does not become pressed when the user drags off and then back
  // onto the button. Make FrameCaptionButton pressed in this case because this
  // behavior is more consistent with AlternateFrameSizeButton.
  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
      event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    if (HitTestPoint(event->location())) {
      SetState(STATE_PRESSED);
      RequestFocus();
      event->StopPropagation();
    } else {
      SetState(STATE_NORMAL);
    }
  } else if (event->type() == ui::ET_GESTURE_SCROLL_END) {
    if (HitTestPoint(event->location())) {
      SetState(STATE_HOVERED);
      NotifyClick(*event);
      event->StopPropagation();
    }
  }
  Button::OnGestureEvent(event);
}

views::PaintInfo::ScaleType FrameCaptionButton::GetPaintScaleType() const {
  return views::PaintInfo::ScaleType::kUniformScaling;
}

std::unique_ptr<views::InkDrop> FrameCaptionButton::CreateInkDrop() {
  auto ink_drop = std::make_unique<views::InkDropImpl>(this, size());
  ink_drop->SetAutoHighlightMode(views::InkDropImpl::AutoHighlightMode::NONE);
  ink_drop->SetShowHighlightOnHover(false);
  return ink_drop;
}

std::unique_ptr<views::InkDropRipple> FrameCaptionButton::CreateInkDropRipple()
    const {
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetInkdropInsets(size()), GetInkDropCenterBasedOnLastEvent(),
      GetInkDropBaseColor(), ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDropMask> FrameCaptionButton::CreateInkDropMask()
    const {
  return std::make_unique<views::RoundRectInkDropMask>(
      size(), GetInkdropInsets(size()), kInkDropCornerRadius);
}

void FrameCaptionButton::SetBackgroundColor(SkColor background_color) {
  if (background_color_ == background_color)
    return;

  background_color_ = background_color;
  // Refresh the icon since the color may have changed.
  if (icon_definition_)
    SetImage(icon_, ANIMATE_NO, *icon_definition_);
  UpdateInkDropBaseColor();
}

void FrameCaptionButton::SetColorMode(ColorMode color_mode) {
  color_mode_ = color_mode;
  UpdateInkDropBaseColor();
}

void FrameCaptionButton::PaintButtonContents(gfx::Canvas* canvas) {
  constexpr SkAlpha kHighlightVisibleOpacity = 0x14;
  SkAlpha highlight_alpha = SK_AlphaTRANSPARENT;
  if (hover_animation().is_animating()) {
    highlight_alpha = hover_animation().CurrentValueBetween(
        SK_AlphaTRANSPARENT, kHighlightVisibleOpacity);
  } else if (state() == STATE_HOVERED || state() == STATE_PRESSED) {
    // Painting a circular highlight in both "hovered" and "pressed" states
    // simulates and ink drop highlight mode of
    // AutoHighlightMode::SHOW_ON_RIPPLE.
    highlight_alpha = kHighlightVisibleOpacity;
  }

  if (highlight_alpha != SK_AlphaTRANSPARENT) {
    // We paint the highlight manually here rather than relying on the ink drop
    // highlight as it doesn't work well when the button size is changing while
    // the window is moving as a result of the animation from normal to
    // maximized state or vice versa. https://crbug.com/840901.
    cc::PaintFlags flags;
    flags.setColor(GetInkDropBaseColor());
    flags.setAlpha(highlight_alpha);
    const gfx::Point center(GetMirroredRect(GetContentsBounds()).CenterPoint());
    canvas->DrawCircle(center, kInkDropCornerRadius, flags);
  }

  int icon_alpha = swap_images_animation_->CurrentValueBetween(0, 255);
  int crossfade_icon_alpha = 0;
  if (icon_alpha < static_cast<int>(kFadeOutRatio * 255))
    crossfade_icon_alpha = static_cast<int>(255 - icon_alpha / kFadeOutRatio);

  int centered_origin_x = (width() - icon_image_.width()) / 2;
  int centered_origin_y = (height() - icon_image_.height()) / 2;

  if (crossfade_icon_alpha > 0 && !crossfade_icon_image_.isNull()) {
    canvas->SaveLayerAlpha(GetAlphaForIcon(alpha_));
    cc::PaintFlags flags;
    flags.setAlpha(icon_alpha);
    canvas->DrawImageInt(icon_image_, centered_origin_x, centered_origin_y,
                         flags);

    flags.setAlpha(crossfade_icon_alpha);
    flags.setBlendMode(SkBlendMode::kPlus);
    canvas->DrawImageInt(crossfade_icon_image_, centered_origin_x,
                         centered_origin_y, flags);
    canvas->Restore();
  } else {
    if (!swap_images_animation_->is_animating())
      icon_alpha = alpha_;
    cc::PaintFlags flags;
    flags.setAlpha(GetAlphaForIcon(icon_alpha));
    canvas->DrawImageInt(icon_image_, centered_origin_x, centered_origin_y,
                         flags);
  }
}

int FrameCaptionButton::GetAlphaForIcon(int base_alpha) const {
  if (!enabled())
    return base_alpha * kDisabledButtonAlphaRatio;

  if (paint_as_active_)
    return base_alpha;

  // Paint icons as active when they are hovered over or pressed.
  double inactive_alpha = GetInactiveButtonColorAlphaRatio();

  if (hover_animation().is_animating()) {
    inactive_alpha =
        hover_animation().CurrentValueBetween(inactive_alpha, 1.0f);
  } else if (state() == STATE_PRESSED || state() == STATE_HOVERED) {
    inactive_alpha = 1.0f;
  }
  return base_alpha * inactive_alpha;
}

void FrameCaptionButton::UpdateInkDropBaseColor() {
  set_ink_drop_base_color(
      color_utils::IsDark(GetButtonColor(color_mode_, background_color_))
          ? SK_ColorBLACK
          : SK_ColorWHITE);
}

}  // namespace ash
