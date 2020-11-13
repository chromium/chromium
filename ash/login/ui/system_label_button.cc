// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/system_label_button.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
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

SkPath GetSystemButtonHighlightPath(const views::View* view) {
  gfx::Rect rect(view->GetLocalBounds());
  return SkPath().addRoundRect(gfx::RectToSkRect(rect),
                               kSystemButtonBorderRadius,
                               kSystemButtonBorderRadius);
}

}  // namespace

SystemLabelButton::SystemLabelButton(PressedCallback callback,
                                     const base::string16& text,
                                     DisplayType display_type,
                                     bool multiline)
    : LabelButton(std::move(callback), text), display_type_(display_type) {
  SetImageLabelSpacing(kSystemButtonImageLabelSpacing);
  if (multiline) {
    label()->SetMultiLine(true);
    label()->SetMaximumWidth(kSystemButtonMaxLabelWidthDp);
  }
  SetMinSize(gfx::Size(0, kSystemButtonHeight));
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  if (display_type == DisplayType::ALERT_WITH_ICON) {
    SetImage(
        views::Button::STATE_NORMAL,
        CreateVectorIcon(
            kLockScreenAlertIcon,
            AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kButtonIconColorPrimary)));
  }
  SetTextSubpixelRenderingEnabled(false);
  SetInkDropMode(InkDropMode::ON);
  bool is_alert = display_type == DisplayType::ALERT_WITH_ICON ||
                  display_type == DisplayType::ALERT_NO_ICON;
  SetAlertMode(is_alert);

  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetInstallFocusRingOnFocus(true);
  focus_ring()->SetColor(ShelfConfig::Get()->shelf_focus_border_color());
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
  return gfx::Insets(
      kSystemButtonMarginTopBottomDp, kSystemButtonMarginLeftRightDp,
      kSystemButtonMarginTopBottomDp, kSystemButtonMarginLeftRightDp);
}

void SystemLabelButton::SetDisplayType(DisplayType display_type) {
  // We only support transitions from a non-icon display type to another.
  DCHECK(display_type_ != DisplayType::ALERT_WITH_ICON);
  DCHECK(display_type != DisplayType::ALERT_WITH_ICON);
  display_type_ = display_type;
  bool alert_mode = display_type == DisplayType::ALERT_NO_ICON;
  SetAlertMode(alert_mode);
}

void SystemLabelButton::SetAlertMode(bool alert_mode) {
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

  const AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes(background_color_);
  SetInkDropBaseColor(ripple_attributes.base_color);
  SetInkDropVisibleOpacity(ripple_attributes.inkdrop_opacity);
  SetInkDropHighlightOpacity(ripple_attributes.highlight_opacity);
}

}  // namespace ash
