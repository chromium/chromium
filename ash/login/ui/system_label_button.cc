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

// The color of the system button's text and icon in alert mode.
constexpr SkColor kSystemButtonContentColorAlert =
    SkColorSetA(gfx::kGoogleGrey900, SK_AlphaOPAQUE);
// The background color of the system button in alert mode.
constexpr SkColor kSystemButtonBackgroundColorAlert =
    SkColorSetA(gfx::kGoogleRed300, SK_AlphaOPAQUE);

// The color of the system button's text in default mode.
constexpr SkColor kSystemButtonContentColorDefault =
    SkColorSetA(gfx::kGoogleGrey200, SK_AlphaOPAQUE);

// The color of the base color used for ink drop in default mode.
constexpr SkColor kInkDropBaseColorDefault = SK_ColorWHITE;

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

SystemLabelButton::SystemLabelButton(views::ButtonListener* listener,
                                     const base::string16& text,
                                     DisplayType display_type,
                                     bool multiline)
    : LabelButton(listener, text), display_type_(display_type) {
  SetImageLabelSpacing(kSystemButtonImageLabelSpacing);
  if (multiline) {
    label()->SetMultiLine(true);
    label()->SetMaximumWidth(kSystemButtonMaxLabelWidthDp);
  }
  label()->SetFontList(
      gfx::FontList().DeriveWithWeight(gfx::Font::Weight::BOLD));
  SetMinSize(gfx::Size(0, kSystemButtonHeight));
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  if (display_type == DisplayType::ALERT_WITH_ICON) {
    SetImage(
        views::Button::STATE_NORMAL,
        CreateVectorIcon(kLockScreenAlertIcon, kSystemButtonContentColorAlert));
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
  if (alert_mode)
    background_color_ = kSystemButtonBackgroundColorAlert;
  else {
    background_color_ = AshColorProvider::Get()->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  }

  SkColor font_color = alert_mode ? kSystemButtonContentColorAlert
                                  : kSystemButtonContentColorDefault;
  SetEnabledTextColors(font_color);

  if (alert_mode) {
    const AshColorProvider::RippleAttributes ripple_attributes =
        AshColorProvider::Get()->GetRippleAttributes(background_color_);
    SetInkDropBaseColor(ripple_attributes.base_color);
    SetInkDropVisibleOpacity(ripple_attributes.inkdrop_opacity);
    SetInkDropHighlightOpacity(ripple_attributes.highlight_opacity);
  } else {
    // using RippleAttributes here doesn't give visually satisfying results
    // in default display mode
    SetInkDropBaseColor(kInkDropBaseColorDefault);
  }
}

}  // namespace ash
