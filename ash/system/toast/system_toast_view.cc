// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/system_toast_view.h"

#include <string>
#include <string_view>
#include <utility>

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/system_shadow.h"
#include "ash/style/typography.h"
#include "base/check.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// Margin constants
constexpr gfx::Insets kToastInteriorMargin = gfx::Insets::VH(8, 16);
constexpr gfx::Insets kMultilineToastInteriorMargin = gfx::Insets::VH(8, 24);
constexpr gfx::Insets kToastWithTextButtonInteriorMargin =
    gfx::Insets::TLBR(2, 16, 2, 2);
constexpr gfx::Insets kMultilineToastWithTextButtonInteriorMargin =
    gfx::Insets::TLBR(8, 24, 8, 12);

// Contents constants
constexpr int kToastLabelMaxWidth = 512;
constexpr int kToastLeadingIconSize = 20;
constexpr int kToastLeadingIconPaddingWidth = 14;
constexpr int kTextButtonFocusRingHaloInset = 1;
// Padding to add space between the toast's body text and icon button.
constexpr gfx::Insets kToastIconButtonLeftPadding =
    gfx::Insets::TLBR(0, 14, 0, 0);

}  // namespace

SystemToastView::SystemToastView(const std::u16string& text,
                                 ButtonType button_type,
                                 const std::u16string& button_text,
                                 const gfx::VectorIcon* button_icon,
                                 base::RepeatingClosure button_callback,
                                 const gfx::VectorIcon* leading_icon) {
  // Paint to layer so the background can be transparent.
  SetPaintToLayer();
  if (chromeos::features::IsSystemBlurEnabled()) {
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  }

  const ui::ColorId background_color_id =
      chromeos::features::IsSystemBlurEnabled()
          ? static_cast<ui::ColorId>(kColorAshShieldAndBase80)
          : cros_tokens::kCrosSysSystemBaseElevatedOpaque;
  SetBackground(views::CreateSolidBackground(background_color_id));
  SetOrientation(views::LayoutOrientation::kHorizontal);
  SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  if (!leading_icon->is_empty()) {
    AddChildView(views::Builder<views::ImageView>()
                     .SetID(VIEW_ID_TOAST_IMAGE_VIEW)
                     .SetPreferredSize(gfx::Size(kToastLeadingIconSize,
                                                 kToastLeadingIconSize))
                     .SetImage(ui::ImageModel::FromVectorIcon(
                         *leading_icon, cros_tokens::kCrosSysOnSurface))
                     .Build());

    auto* icon_padding = AddChildView(std::make_unique<views::View>());
    icon_padding->SetPreferredSize(
        gfx::Size(kToastLeadingIconPaddingWidth, kToastLeadingIconSize));
  }

  AddChildView(
      views::Builder<views::Label>()
          .CopyAddressTo(&label_)
          .SetID(VIEW_ID_TOAST_LABEL)
          .SetText(text)
          .SetTooltipText(text)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetEnabledColor(cros_tokens::kCrosSysOnSurface)
          .SetAutoColorReadabilityEnabled(false)
          .SetSubpixelRenderingEnabled(false)
          .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
              TypographyToken::kCrosBody2))
          .SetMultiLine(true)
          .SetMaximumWidth(kToastLabelMaxWidth)
          .SetMaxLines(2)
          .SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification(
                           views::LayoutOrientation::kHorizontal,
                           views::MinimumFlexSizeRule::kScaleToZero,
                           views::MaximumFlexSizeRule::kScaleToMaximum))
          .Build());

  switch (button_type) {
    case ButtonType::kNone:
      CHECK(button_text.empty());
      CHECK(button_icon->is_empty());
      break;
    case ButtonType::kTextButton: {
      CHECK(!button_text.empty());
      AddChildView(
          views::Builder<PillButton>()
              .CopyAddressTo(&button_)
              .SetID(VIEW_ID_TOAST_BUTTON)
              .SetCallback(std::move(button_callback))
              .SetText(button_text)
              .SetTooltipText(button_text)
              .SetPillButtonType(PillButton::Type::kAccentFloatingWithoutIcon)
              .SetFocusBehavior(views::View::FocusBehavior::ALWAYS)
              .Build());
      auto* button_focus_ring = views::FocusRing::Get(button_);
      button_focus_ring->SetHaloInset(kTextButtonFocusRingHaloInset);
      button_focus_ring->SetOutsetFocusRingDisabled(true);
      break;
    }
    case ButtonType::kIconButton: {
      CHECK(!button_text.empty());
      CHECK(!button_icon->is_empty());
      auto* icon_button =
          AddChildView(IconButton::Builder()
                           .SetViewId(VIEW_ID_TOAST_BUTTON)
                           .SetType(IconButton::Type::kSmallFloating)
                           .SetVectorIcon(button_icon)
                           .SetCallback(std::move(button_callback))
                           .SetAccessibleName(button_text)
                           .Build());
      icon_button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
      icon_button->SetIconColor(cros_tokens::kCrosSysPrimary);
      icon_button->SetProperty(views::kMarginsKey, kToastIconButtonLeftPadding);
      button_ = icon_button;
      break;
    }
  }

  // Need to size label to get the required number of lines.
  label_->SizeToPreferredSize();
  const bool has_text_button = button_type == ButtonType::kTextButton;
  SetInteriorMargin(label_->GetRequiredLines() > 1
                        ? has_text_button
                              ? kMultilineToastWithTextButtonInteriorMargin
                              : kMultilineToastInteriorMargin
                    : has_text_button ? kToastWithTextButtonInteriorMargin
                                      : kToastInteriorMargin);

  const int rounded_corner_radius = GetPreferredSize().height() / 2;
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(rounded_corner_radius));
  SetBorder(std::make_unique<views::HighlightBorder>(
      rounded_corner_radius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));

  // Since toasts have a large corner radius, we use the shadow on texture
  // layer. Refer to `ash::SystemShadowOnTextureLayer` for more details.
  shadow_ =
      SystemShadow::CreateShadowOnTextureLayer(SystemShadow::Type::kElevation4);
  shadow_->SetRoundedCornerRadius(rounded_corner_radius);

  SetProperty(views::kElementIdentifierKey, kSystemToastViewElementId);
}

SystemToastView::~SystemToastView() = default;

void SystemToastView::SetText(std::u16string_view text) {
  label_->SetText(text);
}

std::u16string_view SystemToastView::GetText() const {
  return label_->GetText();
}

void SystemToastView::AddedToWidget() {
  shadow_->ObserveColorProviderSource(GetWidget());

  // Attach the shadow at the bottom of the widget so it shows behind the view.
  auto* shadow_layer = shadow_->GetLayer();
  auto* widget_layer = GetWidget()->GetLayer();
  widget_layer->Add(shadow_layer);
  widget_layer->StackAtBottom(shadow_layer);
}

void SystemToastView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // `shadow_` should have the same bounds as the view's layer.
  shadow_->SetContentBounds(layer()->bounds());
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SystemToastView,
                                      kSystemToastViewElementId);

BEGIN_METADATA(SystemToastView)
END_METADATA

}  // namespace ash
