// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/button_style.h"

#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {

constexpr gfx::Size kPillButtonSize(32, 32);
constexpr int kIconSize = 20;
constexpr int kIconPillButtonImageLabelSpacingDp = 8;

constexpr gfx::Insets kInkDropInsets(4);

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
      break;
    case PillButton::Type::kIconlessAlert:
      color_id = AshColorProvider::ContentLayerType::kButtonLabelColorPrimary;
      break;
    case PillButton::Type::kIconlessAccent:
      color_id = AshColorProvider::ContentLayerType::kButtonLabelColorBlue;
      break;
  }
  return AshColorProvider::Get()->GetContentLayerColor(color_id);
}

gfx::Insets GetInkDropInsets(TrayPopupInkDropStyle ink_drop_style) {
  if (ink_drop_style == TrayPopupInkDropStyle::HOST_CENTERED ||
      ink_drop_style == TrayPopupInkDropStyle::INSET_BOUNDS) {
    return kInkDropInsets;
  }
  return gfx::Insets();
}

gfx::Size GetPillButtonSize(bool has_icon) {
  gfx::Size button_size(kPillButtonSize);
  if (has_icon) {
    button_size.set_width(button_size.width() + kIconSize +
                          kIconPillButtonImageLabelSpacingDp);
  }
  return button_size;
}

std::unique_ptr<views::InkDrop> CreateInkDrop(views::Button* host,
                                              bool highlight_on_hover,
                                              bool highlight_on_focus) {
  return views::InkDrop::CreateInkDropForFloodFillRipple(
      views::InkDrop::Get(host), highlight_on_hover, highlight_on_focus);
}

std::unique_ptr<views::InkDropRipple> CreateInkDropRipple(
    TrayPopupInkDropStyle ink_drop_style,
    const views::Button* host) {
  const AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes();
  return std::make_unique<views::FloodFillInkDropRipple>(
      host->size(), GetInkDropInsets(ink_drop_style),
      views::InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
      ripple_attributes.base_color, ripple_attributes.inkdrop_opacity);
}

std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight(
    const views::View* host) {
  const AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes();
  auto highlight = std::make_unique<views::InkDropHighlight>(
      gfx::SizeF(host->size()), ripple_attributes.base_color);
  highlight->set_visible_opacity(ripple_attributes.highlight_opacity);
  return highlight;
}

}  // namespace

// static
void PillButton::ConfigureInkDrop(views::Button* button,
                                  TrayPopupInkDropStyle ink_drop_style,
                                  bool highlight_on_hover,
                                  bool highlight_on_focus) {
  button->SetInstallFocusRingOnFocus(true);
  views::InkDropHost* const ink_drop = views::InkDrop::Get(button);
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  button->SetHasInkDropActionOnClick(true);
  ink_drop->SetCreateInkDropCallback(base::BindRepeating(
      &CreateInkDrop, button, highlight_on_hover, highlight_on_focus));
  ink_drop->SetCreateRippleCallback(
      base::BindRepeating(&CreateInkDropRipple, ink_drop_style, button));
  ink_drop->SetCreateHighlightCallback(
      base::BindRepeating(&CreateInkDropHighlight, button));
}

PillButton::PillButton(PressedCallback callback,
                       const std::u16string& text,
                       PillButton::Type type,
                       const gfx::VectorIcon* icon)
    : views::LabelButton(std::move(callback), text),
      type_(type),
      button_size_(GetPillButtonSize(type_ == PillButton::Type::kIcon)),
      icon_(icon) {
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  label()->SetElideBehavior(gfx::NO_ELIDE);
  label()->SetSubpixelRenderingEnabled(false);
  // TODO: Unify the font size, weight under ash/style as well.
  label()->SetFontList(views::Label::GetDefaultFontList().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  ConfigureInkDrop(this, TrayPopupInkDropStyle::FILL_BOUNDS,
                   /*highlight_on_hover=*/false, /*highlight_on_focus=*/false);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                button_size_.height() / 2.f);
  SetBackground(views::CreateRoundedRectBackground(
      GetPillButtonBackgroundColor(type), button_size_.height() / 2.f));
}

PillButton::~PillButton() = default;

gfx::Size PillButton::CalculatePreferredSize() const {
  return gfx::Size(label()->GetPreferredSize().width() + button_size_.width(),
                   button_size_.height());
}

int PillButton::GetHeightForWidth(int width) const {
  return button_size_.height();
}

void PillButton::OnThemeChanged() {
  views::LabelButton::OnThemeChanged();

  auto* color_provider = AshColorProvider::Get();
  if (type_ == PillButton::Type::kIcon) {
    DCHECK(icon_);
    const SkColor enabled_icon_color = color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kButtonIconColor);
    SetImage(views::Button::STATE_NORMAL,
             gfx::CreateVectorIcon(*icon_, kIconSize, enabled_icon_color));
    SetImage(views::Button::STATE_DISABLED,
             gfx::CreateVectorIcon(
                 *icon_, kIconSize,
                 AshColorProvider::GetDisabledColor(enabled_icon_color)));
    SetImageLabelSpacing(kIconPillButtonImageLabelSpacingDp);
  }

  const SkColor enabled_text_color = GetPillButtonTextColor(type_);
  SetEnabledTextColors(enabled_text_color);
  SetTextColor(views::Button::STATE_DISABLED,
               AshColorProvider::GetDisabledColor(enabled_text_color));
  views::FocusRing::Get(this)->SetColor(color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
  background()->SetNativeControlColor(GetPillButtonBackgroundColor(type_));
}

BEGIN_METADATA(PillButton, views::LabelButton)
END_METADATA

}  // namespace ash
