// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/action_button_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/system_shadow.h"
#include "ash/style/typography.h"
#include "base/check.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Action button insets when it is shown in full (text and icon).
constexpr auto kFullActionButtonInsets = gfx::Insets::TLBR(8, 12, 8, 16);

// Action button insets when it is collapsed (icon only).
constexpr auto kCollapsedActionButtonInsets = gfx::Insets(8);

// The horizontal spacing between the icon and label in an action button.
constexpr int kActionButtonIconLabelSpacing = 8;

// The corner radius for an action button.
constexpr int kActionButtonRadius = 18;

// The size of the icon in an action button.
constexpr int kActionButtonIconSize = 20;

// The diameter of the loading throbber in an action button.
constexpr int kActionButtonThrobberDiameter = 20;

}  // namespace

ActionButtonView::ActionButtonView(views::Button::PressedCallback callback,
                                   std::u16string text,
                                   const gfx::VectorIcon* icon,
                                   ActionButtonRank rank)
    : views::Button(std::move(callback)),
      rank_(rank),
      // Since this view has fully circular rounded corners, we can't use a
      // nine patch layer for the shadow. We have to use the
      // `ShadowOnTextureLayer`. For more info, see https://crbug.com/1308800.
      shadow_(SystemShadow::CreateShadowOnTextureLayer(
          SystemShadow::Type::kElevation12)),
      icon_(icon) {
  box_layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kFullActionButtonInsets,
      kActionButtonIconLabelSpacing));
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  shadow_->SetRoundedCornerRadius(kActionButtonRadius);
  capture_mode_util::SetHighlightBorder(
      this, kActionButtonRadius,
      views::HighlightBorder::Type::kHighlightBorderNoShadow);

  // An image will be set in `UpdateColorsAndIcon`.
  image_view_ = AddChildView(std::make_unique<views::ImageView>());
  label_ = AddChildView(std::make_unique<views::Label>(text));
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2, *label_);

  SetAccessibleName(text);
  UpdateColorsAndIcon();

  throbber_ = AddChildView(
      std::make_unique<views::Throbber>(kActionButtonThrobberDiameter));
  throbber_->SetVisible(false);
}

ActionButtonView::~ActionButtonView() = default;

void ActionButtonView::AddedToWidget() {
  views::Button::AddedToWidget();

  // Attach the shadow at the bottom of the widget layer.
  ui::Layer* shadow_layer = shadow_->GetLayer();
  ui::Layer* widget_layer = GetWidget()->GetLayer();
  widget_layer->Add(shadow_layer);
  widget_layer->StackAtBottom(shadow_layer);

  // Make the shadow observe the color provider source change to update the
  // colors.
  shadow_->ObserveColorProviderSource(GetWidget());
}

void ActionButtonView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // The shadow layer is a sibling of this view's layer, and should have the
  // same bounds.
  shadow_->SetContentBounds(layer()->bounds());
}

void ActionButtonView::CollapseToIconButton() {
  if (!label_->GetVisible()) {
    return;
  }
  label_->SetVisible(false);
  box_layout_->set_inside_border_insets(kCollapsedActionButtonInsets);
}

void ActionButtonView::OnThemeChanged() {
  views::Button::OnThemeChanged();
  UpdateColorsAndIcon();
}

void ActionButtonView::OnEnabledChanged() {
  views::Button::OnEnabledChanged();
  UpdateColorsAndIcon();
}

void ActionButtonView::StateChanged(ButtonState old_state) {
  if (!show_throbber_when_pressed_) {
    return;
  }

  CHECK(throbber_);
  const ButtonState button_state = GetState();
  if (button_state == ButtonState::STATE_PRESSED) {
    throbber_->SetVisible(true);
    throbber_->Start();
  } else if (button_state == STATE_NORMAL) {
    // The button state is `STATE_PRESSED` then `STATE_DISABLED` while the
    // action is in progress, but may become `STATE_NORMAL` again if the action
    // is not successfully executed. If that happens, hide the loading throbber.
    throbber_->Stop();
    throbber_->SetVisible(false);
  }
}

void ActionButtonView::UpdateColorsAndIcon() {
  // See `PillButton::UpdateBackgroundColor`.
  const ui::ColorId background_color =
      GetEnabled() ? cros_tokens::kCrosSysSystemBaseElevated
                   : cros_tokens::kCrosSysDisabledContainer;
  SetBackground(views::CreateThemedRoundedRectBackground(background_color,
                                                         kActionButtonRadius));

  // See `PillButton::UpdateTextColor` and `PillButton::UpdateIconColor`.
  // Both return the same colors.
  const ui::ColorId label_and_icon_color = GetEnabled()
                                               ? cros_tokens::kCrosSysOnSurface
                                               : cros_tokens::kCrosSysDisabled;
  label_->SetEnabledColorId(label_and_icon_color);
  label_->SetBackgroundColor(background_color);

  image_view_->SetImage(ui::ImageModel::FromVectorIcon(
      *icon_, label_and_icon_color, kActionButtonIconSize));
}

BEGIN_METADATA(ActionButtonView)
END_METADATA

}  // namespace ash
