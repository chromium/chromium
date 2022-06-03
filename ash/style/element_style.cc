// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/element_style.h"

#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

namespace element_style {

namespace {

// Constants of the pill buttons.
constexpr gfx::Size kIconlessPillButtonSize(64, 32);
constexpr gfx::Size kIconPillButtonSize(92, 32);
constexpr int kIconPillButtonImageLabelSpacingDp = 8;

// Constants of the close buttons.
constexpr int kSmallCloseButtonSize = 16;
constexpr int kMediumCloseButtonSize = 24;
constexpr int kLargeCloseButtonSize = 32;

void DecorateIconButtonImpl(views::ImageButton* button,
                            const gfx::VectorIcon& icon,
                            bool toggled,
                            int button_size,
                            bool has_border) {
  DCHECK(!icon.is_empty());
  if (has_border) {
    button_size += 2 * kIconButtonBorderSize;
    button->SetBorder(
        views::CreateEmptyBorder(gfx::Insets(kIconButtonBorderSize)));
  }
  button->SetPreferredSize(gfx::Size(button_size, button_size));

  auto* color_provider = AshColorProvider::Get();
  const SkColor normal_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  const SkColor toggled_icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColorPrimary);
  const SkColor icon_color = toggled ? toggled_icon_color : normal_color;

  // Skip repainting if the incoming icon is the same as the current icon. If
  // the icon has been painted before, |gfx::CreateVectorIcon()| will simply
  // grab the ImageSkia from a cache, so it will be cheap. Note that this
  // assumes that toggled/disabled images changes at the same time as the normal
  // image, which it currently does.
  const gfx::ImageSkia new_normal_image =
      gfx::CreateVectorIcon(icon, kIconButtonIconSize, icon_color);
  const gfx::ImageSkia& old_normal_image =
      button->GetImage(views::Button::STATE_NORMAL);
  if (!new_normal_image.isNull() && !old_normal_image.isNull() &&
      new_normal_image.BackedBySameObjectAs(old_normal_image)) {
    return;
  }

  button->SetImage(views::Button::STATE_NORMAL, new_normal_image);
  button->SetImage(
      views::Button::STATE_DISABLED,
      gfx::CreateVectorIcon(icon, kIconButtonIconSize,
                            AshColorProvider::GetDisabledColor(normal_color)));
}

void DecorateIconlessPillButtonImpl(
    views::LabelButton* button,
    AshColorProvider::ContentLayerType text_color_id,
    AshColorProvider::ControlsLayerType background_color_id) {
  auto* color_provider = AshColorProvider::Get();
  const SkColor enabled_text_color =
      color_provider->GetContentLayerColor(text_color_id);
  button->SetEnabledTextColors(enabled_text_color);
  button->SetTextColor(views::Button::STATE_DISABLED,
                       AshColorProvider::GetDisabledColor(enabled_text_color));
  button->SetPreferredSize(kIconlessPillButtonSize);
  button->SetBackground(views::CreateRoundedRectBackground(
      color_provider->GetControlsLayerColor(background_color_id),
      kIconlessPillButtonSize.height() / 2));
}

void DecorateCloseButtonImpl(views::ImageButton* button,
                             int button_size,
                             const gfx::VectorIcon& icon) {
  auto* color_provider = AshColorProvider::Get();
  DCHECK(!icon.is_empty());
  const SkColor enabled_icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  button->SetImage(views::Button::STATE_NORMAL,
                   gfx::CreateVectorIcon(icon, enabled_icon_color));

  // Add a rounded rect background. The rounding will be half the button size so
  // it is a circle.
  const SkColor icon_background_color = color_provider->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80);
  button->SetBackground(views::CreateRoundedRectBackground(
      icon_background_color, button_size / 2));

  // TODO(minch): Add background blur as per spec. Background blur is quite
  // heavy, and we may have many close buttons showing at a time. They'll be
  // added separately so its easier to monitor performance.
}

}  // namespace

void DecorateSmallIconButton(views::ImageButton* button,
                             const gfx::VectorIcon& icon,
                             bool toggled,
                             bool has_border) {
  DecorateIconButtonImpl(button, icon, toggled, kSmallIconButtonSize,
                         has_border);
}

void DecorateMediumIconButton(views::ImageButton* button,
                              const gfx::VectorIcon& icon,
                              bool toggled,
                              bool has_border) {
  DecorateIconButtonImpl(button, icon, toggled, kMediumIconButtonSize,
                         has_border);
}

void DecorateLargeIconButton(views::ImageButton* button,
                             const gfx::VectorIcon& icon,
                             bool toggled,
                             bool has_border) {
  DecorateIconButtonImpl(button, icon, toggled, kLargeIconButtonSize,
                         has_border);
}

void DecorateFloatingIconButton(views::ImageButton* button,
                                const gfx::VectorIcon& icon) {
  const SkColor enabled_icon_color =
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kButtonIconColor);
  button->SetImage(views::Button::STATE_NORMAL,
                   gfx::CreateVectorIcon(icon, enabled_icon_color));
  button->SetImage(
      views::Button::STATE_DISABLED,
      gfx::CreateVectorIcon(
          icon, AshColorProvider::GetDisabledColor(enabled_icon_color)));
}

void DecorateIconlessFloatingPillButton(views::LabelButton* button) {
  button->SetEnabledTextColors(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonLabelColorBlue));
}

void DecorateIconlessPillButton(views::LabelButton* button) {
  DecorateIconlessPillButtonImpl(
      button, AshColorProvider::ContentLayerType::kButtonLabelColor,
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
}

void DecorateIconPillButton(views::LabelButton* button,
                            const gfx::VectorIcon* icon) {
  auto* color_provider = AshColorProvider::Get();
  const SkColor enabled_icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  button->SetImage(views::Button::STATE_NORMAL,
                   gfx::CreateVectorIcon(*icon, enabled_icon_color));
  button->SetImage(
      views::Button::STATE_DISABLED,
      gfx::CreateVectorIcon(
          *icon, AshColorProvider::GetDisabledColor(enabled_icon_color)));
  button->SetImageLabelSpacing(kIconPillButtonImageLabelSpacingDp);

  const SkColor enabled_text_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonLabelColor);
  button->SetEnabledTextColors(enabled_text_color);
  button->SetTextColor(views::Button::STATE_DISABLED,
                       AshColorProvider::GetDisabledColor(enabled_text_color));
  button->SetPreferredSize(kIconPillButtonSize);
  button->SetBackground(views::CreateRoundedRectBackground(
      color_provider->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive),
      kIconPillButtonSize.height() / 2));
}

void DecorateIconlessAlertPillButton(views::LabelButton* button) {
  DecorateIconlessPillButtonImpl(
      button, AshColorProvider::ContentLayerType::kButtonLabelColorPrimary,
      AshColorProvider::ControlsLayerType::kControlBackgroundColorAlert);
}

void DecorateIconlessAccentPillButton(views::LabelButton* button) {
  DecorateIconlessPillButtonImpl(
      button, AshColorProvider::ContentLayerType::kButtonLabelColorBlue,
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
}

void DecorateIconlessProminentPillButton(views::LabelButton* button) {
  DecorateIconlessPillButtonImpl(
      button, AshColorProvider::ContentLayerType::kButtonLabelColor,
      AshColorProvider::ControlsLayerType::kControlBackgroundColorActive);
}

void DecorateSmallCloseButton(views::ImageButton* button,
                              const gfx::VectorIcon& icon) {
  DecorateCloseButtonImpl(button, kSmallCloseButtonSize, icon);
}

void DecorateMediumCloseButton(views::ImageButton* button,
                               const gfx::VectorIcon& icon) {
  DecorateCloseButtonImpl(button, kMediumCloseButtonSize, icon);
}

void DecorateLargeCloseButton(views::ImageButton* button,
                              const gfx::VectorIcon& icon) {
  DecorateCloseButtonImpl(button, kLargeCloseButtonSize, icon);
}

}  // namespace element_style

}  // namespace ash
