// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access_menu_button.h"

#include "ash/style/ash_color_provider.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/mojom/ax_node_data.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"

namespace ash {

namespace {
constexpr int kButtonBottomPaddingDefaultDip = 8;
constexpr int kButtonBottomPaddingSmallDip = 1;
constexpr int kButtonTopPaddingDip = 16;
constexpr int kIconSizeDip = 20;
constexpr int kLabelMinSidePaddingDip = 8;
constexpr int kLabelMaxWidthDip =
    SwitchAccessMenuButton::kWidthDip - 2 * kLabelMinSidePaddingDip;
constexpr int kLabelTopPaddingDefaultDip = 16;
constexpr int kLabelTopPaddingSmallDip = 8;
constexpr int kTextLineHeightDip = 20;
}  // namespace

SwitchAccessMenuButton::SwitchAccessMenuButton(std::string action_name,
                                               const gfx::VectorIcon& icon,
                                               int label_text_id)
    : views::Button(this),
      action_name_(action_name),
      image_view_(new views::ImageView()),
      label_(new views::Label(l10n_util::GetStringUTF16(label_text_id),
                              views::style::CONTEXT_BUTTON)) {
  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(kButtonTopPaddingDip, kLabelMinSidePaddingDip,
                  kButtonBottomPaddingDefaultDip, kLabelMinSidePaddingDip),
      kLabelTopPaddingDefaultDip);

  SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
  SkColor label_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);

  image_view_->SetImage(gfx::CreateVectorIcon(icon, kIconSizeDip, icon_color));
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetEnabledColor(label_color);
  label_->SetMultiLine(true);
  label_->SetMaximumWidth(kLabelMaxWidthDip);

  AddChildView(image_view_);
  AddChildView(label_);

  // The layout padding changes with the size of the text label.
  gfx::Size label_size = label_->CalculatePreferredSize();
  int left_padding_dip = (kWidthDip - label_size.width()) / 2;
  int right_padding_dip = kWidthDip - left_padding_dip - label_size.width();
  int bottom_padding_dip = kButtonBottomPaddingDefaultDip;
  if (label_size.height() > kTextLineHeightDip) {
    bottom_padding_dip = kButtonBottomPaddingSmallDip;
    layout->set_between_child_spacing(kLabelTopPaddingSmallDip);
  }
  layout->set_inside_border_insets(
      gfx::Insets(kButtonTopPaddingDip, left_padding_dip, bottom_padding_dip,
                  right_padding_dip));
  SetLayoutManager(std::move(layout));
}

void SwitchAccessMenuButton::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  NotifyAccessibilityEvent(ax::mojom::Event::kClicked,
                           /*send_native_event=*/false);
}

void SwitchAccessMenuButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::Button::GetAccessibleNodeData(node_data);
  node_data->AddStringAttribute(ax::mojom::StringAttribute::kValue,
                                action_name_);
}

}  // namespace ash
