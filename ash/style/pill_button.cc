// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/pill_button.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {

constexpr int kPillButtonHeight = 32;
constexpr int kPillButtonHorizontalSpacing = 16;
constexpr int kPillButtonMinimumWidth = 56;
constexpr int kIconSize = 20;
constexpr int kIconPillButtonImageLabelSpacingDp = 8;

// Returns true it is a floating type of PillButton, which is a type of
// PillButton without a background.
bool IsFloatingPillButton(PillButton::Type type) {
  return type == PillButton::Type::kIconlessFloating ||
         type == PillButton::Type::kIconlessAccentFloating;
}

SkColor GetPillButtonBackgroundColor(PillButton::Type type) {
  AshColorProvider::ControlsLayerType color_id =
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive;
  switch (type) {
    case PillButton::Type::kIcon:
    case PillButton::Type::kIconless:
    case PillButton::Type::kIconlessAccent:
      break;
    case PillButton::Type::kIconlessAlert:
      color_id =
          AshColorProvider::ControlsLayerType::kControlBackgroundColorAlert;
      break;
    case PillButton::Type::kIconlessProminent:
      color_id =
          AshColorProvider::ControlsLayerType::kControlBackgroundColorActive;
      break;
    case PillButton::Type::kIconlessFloating:
    case PillButton::Type::kIconlessAccentFloating:
      return SK_ColorTRANSPARENT;
  }
  return AshColorProvider::Get()->GetControlsLayerColor(color_id);
}

SkColor GetPillButtonTextColor(PillButton::Type type) {
  AshColorProvider::ContentLayerType color_id =
      AshColorProvider::ContentLayerType::kButtonLabelColor;
  switch (type) {
    case PillButton::Type::kIcon:
    case PillButton::Type::kIconless:
    case PillButton::Type::kIconlessProminent:
    case PillButton::Type::kIconlessFloating:
      break;
    case PillButton::Type::kIconlessAlert:
      color_id = AshColorProvider::ContentLayerType::kButtonLabelColorPrimary;
      break;
    case PillButton::Type::kIconlessAccent:
    case PillButton::Type::kIconlessAccentFloating:
      color_id = AshColorProvider::ContentLayerType::kButtonLabelColorBlue;
      break;
  }
  return AshColorProvider::Get()->GetContentLayerColor(color_id);
}

int GetPillButtonWidth(bool has_icon) {
  int button_width = 2 * kPillButtonHorizontalSpacing;
  if (has_icon)
    button_width += (kIconSize + kIconPillButtonImageLabelSpacingDp);
  return button_width;
}

}  // namespace

PillButton::PillButton(PressedCallback callback,
                       const std::u16string& text,
                       PillButton::Type type,
                       const gfx::VectorIcon* icon,
                       bool use_light_colors,
                       bool rounded_highlight_path)
    : views::LabelButton(std::move(callback), text),
      type_(type),
      icon_(icon),
      use_light_colors_(use_light_colors) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  const int vertical_spacing =
      std::max(kPillButtonHeight - GetPreferredSize().height() / 2, 0);
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_spacing, kPillButtonHorizontalSpacing)));
  label()->SetSubpixelRenderingEnabled(false);
  // TODO: Unify the font size, weight under ash/style as well.
  label()->SetFontList(views::Label::GetDefaultFontList().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  StyleUtil::SetUpInkDropForButton(
      this, gfx::Insets(),
      /*highlight_on_hover=*/false,
      /*highlight_on_focus=*/false,
      /*background_color=*/
      use_light_colors ? SK_ColorWHITE : gfx::kPlaceholderColor);
  if (rounded_highlight_path) {
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  kPillButtonHeight / 2.f);
  }
  if (!IsFloatingPillButton(type_)) {
    SetBackground(views::CreateRoundedRectBackground(
        GetPillButtonBackgroundColor(type), kPillButtonHeight / 2.f));
  }
  SetTooltipText(text);
}

PillButton::~PillButton() = default;

gfx::Size PillButton::CalculatePreferredSize() const {
  gfx::Size size(label()->GetPreferredSize().width() +
                     GetPillButtonWidth(type_ == PillButton::Type::kIcon),
                 kPillButtonHeight);
  size.SetToMax(gfx::Size(kPillButtonMinimumWidth, kPillButtonHeight));
  return size;
}

int PillButton::GetHeightForWidth(int width) const {
  return kPillButtonHeight;
}

void PillButton::OnThemeChanged() {
  views::LabelButton::OnThemeChanged();

  auto* color_provider = AshColorProvider::Get();

  SkColor enabled_icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  SkColor enabled_text_color = GetPillButtonTextColor(type_);
  views::FocusRing::Get(this)->SetColor(color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
  if (!IsFloatingPillButton(type_))
    background()->SetNativeControlColor(GetPillButtonBackgroundColor(type_));

  // Override the colors to light mode if `use_light_colors_` is true when D/L
  // is not enabled.
  if (use_light_colors_ && !features::IsDarkLightModeEnabled()) {
    ScopedLightModeAsDefault scoped_light_mode_as_default;
    enabled_icon_color = color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kButtonIconColor);
    enabled_text_color = GetPillButtonTextColor(type_);
    views::FocusRing::Get(this)->SetColor(color_provider->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kFocusRingColor));
    if (!IsFloatingPillButton(type_))
      background()->SetNativeControlColor(GetPillButtonBackgroundColor(type_));
  }

  if (type_ == PillButton::Type::kIcon) {
    DCHECK(icon_);
    SetImage(views::Button::STATE_NORMAL,
             gfx::CreateVectorIcon(*icon_, kIconSize, enabled_icon_color));
    SetImage(views::Button::STATE_DISABLED,
             gfx::CreateVectorIcon(
                 *icon_, kIconSize,
                 AshColorProvider::GetDisabledColor(enabled_icon_color)));
    SetImageLabelSpacing(kIconPillButtonImageLabelSpacingDp);
  }

  SetEnabledTextColors(enabled_text_color);
  SetTextColor(views::Button::STATE_DISABLED,
               AshColorProvider::GetDisabledColor(enabled_text_color));
}

BEGIN_METADATA(PillButton, views::LabelButton)
END_METADATA

}  // namespace ash
