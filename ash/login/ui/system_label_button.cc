// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/system_label_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {
constexpr int kUserInfoBubbleWidth = 192;
constexpr int kUserInfoBubbleExternalPadding = 8;
constexpr int kSystemButtonHeight = 32;
constexpr int kSystemButtonIconSize = 20;
constexpr int kSystemButtonMarginTopBottomDp = 6;
constexpr int kSystemButtonMarginLeftRightDp = 16;
constexpr int kSystemButtonBorderRadius = 16;
constexpr int kSystemButtonImageLabelSpacing = 8;
constexpr int kSystemButtonMaxLabelWidthDp =
    kUserInfoBubbleWidth - 2 * kUserInfoBubbleExternalPadding -
    kSystemButtonIconSize - kSystemButtonImageLabelSpacing -
    2 * kSystemButtonBorderRadius;

// Default base layer used for the bubble background, above which the system
// label button lives.
constexpr const AshColorProvider::BaseLayerType kBubbleLayerType =
    AshColorProvider::BaseLayerType::kTransparent80;

SkPath GetSystemButtonHighlightPath(const views::View* view) {
  gfx::Rect rect(view->GetLocalBounds());
  return SkPath().addRoundRect(gfx::RectToSkRect(rect),
                               kSystemButtonBorderRadius,
                               kSystemButtonBorderRadius);
}

}  // namespace

SystemLabelButton::SystemLabelButton(PressedCallback callback,
                                     const std::u16string& text,
                                     bool multiline)
    : LabelButton(std::move(callback), text) {
  SetImageLabelSpacing(kSystemButtonImageLabelSpacing);
  if (multiline) {
    label()->SetMultiLine(true);
    label()->SetMaximumWidth(kSystemButtonMaxLabelWidthDp);
  }
  SetMinSize(gfx::Size(0, kSystemButtonHeight));
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetTextSubpixelRenderingEnabled(false);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);

  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetInstallFocusRingOnFocus(true);
  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kSystemButtonBorderRadius);
}

void SystemLabelButton::PaintButtonContents(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(background_color_);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawPath(GetSystemButtonHighlightPath(this), flags);
}

gfx::Insets SystemLabelButton::GetInsets() const {
  return gfx::Insets::TLBR(
      kSystemButtonMarginTopBottomDp, kSystemButtonMarginLeftRightDp,
      kSystemButtonMarginTopBottomDp, kSystemButtonMarginLeftRightDp);
}

void SystemLabelButton::OnThemeChanged() {
  views::LabelButton::OnThemeChanged();
  SetBackgroundAndFont(alert_mode_);
}

void SystemLabelButton::SetBackgroundAndFont(bool alert_mode) {
  // Do not check if alert mode has already been set since the variable might
  // have been initialized by default while the colors have not been set yet.
  alert_mode_ = alert_mode;

  background_color_ = AshColorProvider::Get()->GetControlsLayerColor(
      alert_mode
          ? AshColorProvider::ControlsLayerType::kControlBackgroundColorAlert
          : AshColorProvider::ControlsLayerType::
                kControlBackgroundColorInactive);

  label()->SetFontList(gfx::FontList().DeriveWithWeight(
      alert_mode ? gfx::Font::Weight::BOLD : gfx::Font::Weight::MEDIUM));

  SetEnabledTextColors(AshColorProvider::Get()->GetContentLayerColor(
      alert_mode ? AshColorProvider::ContentLayerType::kButtonLabelColorPrimary
                 : AshColorProvider::ContentLayerType::kButtonLabelColor));

  // In default mode, this won't be the exact resulting color of the button as
  // neither |background_color_| nor the color bubble below are fully opaque.
  // Nevertheless, the result is visually satisfying and better than without
  // applying any background color.
  SkColor effective_background_color = color_utils::GetResultingPaintColor(
      background_color_,
      AshColorProvider::Get()->GetBaseLayerColor(kBubbleLayerType));
  StyleUtil::ConfigureInkDropAttributes(this,
                                        StyleUtil::kBaseColor |
                                            StyleUtil::kInkDropOpacity |
                                            StyleUtil::kHighlightOpacity,
                                        effective_background_color);
}

}  // namespace ash
