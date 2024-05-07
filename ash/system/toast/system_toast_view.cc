// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/system_toast_view.h"

#include <string>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/scoped_a11y_override_window_setter.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/shell.h"
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
    gfx::Insets::TLBR(2, 16, 2, 2);
constexpr gfx::Insets kMultilineToastWithButtonInteriorMargin =
    gfx::Insets::TLBR(8, 24, 8, 12);

// Contents constants
constexpr int kToastLabelMaxWidth = 512;
constexpr int kToastLeadingIconSize = 20;
constexpr int kToastLeadingIconPaddingWidth = 14;
constexpr int kDismissButtonFocusRingHaloInset = 1;

}  // namespace

SystemToastView::SystemToastView(const std::u16string& text,
                                 const std::u16string& dismiss_text,
                                 base::RepeatingClosure dismiss_callback,
                                 const gfx::VectorIcon* leading_icon)
    : scoped_a11y_overrider_(
          std::make_unique<ScopedA11yOverrideWindowSetter>()) {
  // Paint to layer so the background can be transparent.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));
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
          .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
          .SetAutoColorReadabilityEnabled(false)
          .SetSubpixelRenderingEnabled(false)
          .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
              TypographyToken::kCrosBody2))
          .SetMultiLine(true)
          .SetMaximumWidth(kToastLabelMaxWidth)
          .SetMaxLines(2)
          .Build());

  const bool has_button = !dismiss_text.empty();
  if (has_button) {
    AddChildView(
        views::Builder<PillButton>()
            .CopyAddressTo(&dismiss_button_)
            .SetID(VIEW_ID_TOAST_BUTTON)
            .SetCallback(std::move(dismiss_callback))
            .SetText(dismiss_text)
            .SetTooltipText(dismiss_text)
            .SetPillButtonType(PillButton::Type::kAccentFloatingWithoutIcon)
            .SetFocusBehavior(views::View::FocusBehavior::ALWAYS)
            .Build());

    // The button's focus ring predicate is overridden since it's not directly
    // focus accessible by tab traversal. The `is_dismiss_button_highlighted_`
    // member variable is set through `ToggleButtonA11yFocus`.
    auto* button_focus_ring = views::FocusRing::Get(dismiss_button_);
    button_focus_ring->SetHaloInset(kDismissButtonFocusRingHaloInset);
    button_focus_ring->SetOutsetFocusRingDisabled(true);
    button_focus_ring->SetHasFocusPredicate(base::BindRepeating(
        [](const SystemToastView* toast_view, const views::View* view) {
          return toast_view->is_dismiss_button_highlighted_;
        },
        base::Unretained(this)));
    button_focus_ring->SetVisible(false);
  }

  // Need to size label to get the required number of lines.
  label_->SizeToPreferredSize();
  SetInteriorMargin(label_->GetRequiredLines() > 1
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

void SystemToastView::SetText(const std::u16string& text) {
  label_->SetText(text);
}

void SystemToastView::ToggleButtonA11yFocus() {
  if (!dismiss_button_) {
    return;
  }

  // `is_dismiss_button_highlighted_` indicates the desired focus state.
  is_dismiss_button_highlighted_ = !is_dismiss_button_highlighted_;
  if (is_dismiss_button_highlighted_) {
    scoped_a11y_overrider_->MaybeUpdateA11yOverrideWindow(
        dismiss_button_->GetWidget()->GetNativeWindow());
    dismiss_button_->NotifyAccessibilityEvent(ax::mojom::Event::kSelection,
                                              true);
  }

  auto* button_focus_ring = views::FocusRing::Get(dismiss_button_);
  button_focus_ring->SetVisible(is_dismiss_button_highlighted_);
  button_focus_ring->SchedulePaint();
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

BEGIN_METADATA(SystemToastView)
END_METADATA

}  // namespace ash
