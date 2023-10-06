// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/system_toast_view.h"

#include <string>

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/pill_button.h"
#include "ash/style/system_shadow.h"
#include "ash/style/typography.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

namespace {

// Margin constants
constexpr gfx::Insets kToastInteriorMargin = gfx::Insets::VH(8, 16);
constexpr gfx::Insets kMultilineToastInteriorMargin = gfx::Insets::VH(8, 24);
constexpr gfx::Insets kToastWithButtonInteriorMargin =
    gfx::Insets::TLBR(2, 16, 2, 0);
constexpr gfx::Insets kMultilineToastWithButtonInteriorMargin =
    gfx::Insets::TLBR(8, 24, 8, 12);

// Contents constants
constexpr int kToastLabelMaxWidth = 512;
constexpr int kToastLeadingIconSize = 20;
constexpr int kToastLeadingIconPaddingWidth = 14;

}  // namespace

SystemToastView::SystemToastView(const ToastData& toast_data) {
  // Paint to layer so the background can be transparent.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));
  SetOrientation(views::LayoutOrientation::kHorizontal);
  SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  if (!toast_data.leading_icon->is_empty()) {
    AddChildView(
        views::Builder<views::ImageView>()
            .SetID(VIEW_ID_TOAST_IMAGE_VIEW)
            .SetPreferredSize(
                gfx::Size(kToastLeadingIconSize, kToastLeadingIconSize))
            .SetImage(ui::ImageModel::FromVectorIcon(
                *toast_data.leading_icon, cros_tokens::kCrosSysOnSurface))
            .Build());

    auto* icon_padding = AddChildView(std::make_unique<views::View>());
    icon_padding->SetPreferredSize(
        gfx::Size(kToastLeadingIconPaddingWidth, kToastLeadingIconSize));
  }

  auto* label = AddChildView(
      views::Builder<views::Label>()
          .SetID(VIEW_ID_TOAST_LABEL)
          .SetText(toast_data.text)
          .SetTooltipText(toast_data.text)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
          .SetAutoColorReadabilityEnabled(false)
          .SetSubpixelRenderingEnabled(false)
          .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
              TypographyToken::kCrosBody2))
          .SetMultiLine(true)
          .SetMaxLines(2)
          .Build());

  const bool has_button = !toast_data.dismiss_text.empty();
  if (has_button) {
    AddChildView(
        views::Builder<PillButton>()
            .SetID(VIEW_ID_TOAST_BUTTON)
            .SetCallback(std::move(toast_data.dismiss_callback))
            .SetText(toast_data.dismiss_text)
            .SetTooltipText(toast_data.dismiss_text)
            .SetPillButtonType(PillButton::Type::kAccentFloatingWithoutIcon)
            .SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY)
            .Build());
  }

  label->SetMaximumWidth(kToastLabelMaxWidth);
  label->SetPreferredSize(label->GetPreferredSize());
  SetInteriorMargin(label->GetRequiredLines() > 1
                        ? has_button ? kMultilineToastWithButtonInteriorMargin
                                     : kMultilineToastInteriorMargin
                    : has_button ? kToastWithButtonInteriorMargin
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
}

SystemToastView::~SystemToastView() = default;

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

BEGIN_METADATA(SystemToastView, views::View)
END_METADATA

}  // namespace ash
