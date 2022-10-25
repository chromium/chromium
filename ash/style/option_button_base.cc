// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/option_button_base.h"

#include "ash/style/ash_color_id.h"
#include "ash/style/style_util.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {

constexpr int kButtonHeight = 36;
constexpr int kImageLabelSpacingDP = 12;

}  // namespace

OptionButtonBase::OptionButtonBase(int button_width,
                                   PressedCallback callback,
                                   const std::u16string& label,
                                   const gfx::Insets& insets)
    : views::LabelButton(std::move(callback), label) {
  SetPreferredSize(gfx::Size(button_width, kButtonHeight));
  SetBorder(views::CreateEmptyBorder(insets));
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false);
  views::InstallRectHighlightPathGenerator(this);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(ui::kColorAshFocusRing);
}

OptionButtonBase::~OptionButtonBase() = default;

void OptionButtonBase::SetSelected(bool selected) {
  if (selected_ == selected)
    return;

  selected_ = selected;
  UpdateImage();

  if (delegate_)
    delegate_->OnButtonSelected(this);
}

void OptionButtonBase::Layout() {
  gfx::Rect local_bounds = GetLocalBounds();
  gfx::Rect local_content_bounds(local_bounds);
  local_content_bounds.Inset(GetInsets());

  ink_drop_container()->SetBoundsRect(local_bounds);

  views::Label* label = this->label();
  gfx::Size label_size(
      local_content_bounds.width() - kImageLabelSpacingDP - kIconSize,
      label->GetPreferredSize().height());

  gfx::Point image_origin = local_content_bounds.origin();
  image_origin.Offset(0, (local_content_bounds.height() - kIconSize) / 2);
  gfx::Point label_origin = local_content_bounds.origin();
  label_origin.Offset(
      0, (local_content_bounds.height() - label_size.height()) / 2);

  if (IsIconOnTheLeftSide()) {
    label_origin.Offset(kIconSize + kImageLabelSpacingDP, 0);
  } else {
    image_origin.Offset(local_content_bounds.width() - kIconSize, 0);
  }

  image()->SetBoundsRect(
      gfx::Rect(image_origin, gfx::Size(kIconSize, kIconSize)));
  label->SetBoundsRect(gfx::Rect(label_origin, label_size));
  Button::Layout();
}

void OptionButtonBase::OnThemeChanged() {
  views::Button::OnThemeChanged();
  UpdateImage();
  UpdateTextColor();
}

void OptionButtonBase::NotifyClick(const ui::Event& event) {
  if (delegate_)
    delegate_->OnButtonClicked(this);
  views::LabelButton::NotifyClick(event);
}

void OptionButtonBase::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  LabelButton::GetAccessibleNodeData(node_data);
  const ax::mojom::CheckedState checked_state =
      selected_ ? ax::mojom::CheckedState::kTrue
                : ax::mojom::CheckedState::kFalse;
  node_data->SetCheckedState(checked_state);
  if (GetEnabled()) {
    node_data->SetDefaultActionVerb(selected_
                                        ? ax::mojom::DefaultActionVerb::kUncheck
                                        : ax::mojom::DefaultActionVerb::kCheck);
  }
}

SkColor OptionButtonBase::GetIconImageColor() const {
  SkColor active_color =
      GetColorProvider()->GetColor(selected_ ? cros_tokens::kCrosSysPrimary
                                             : cros_tokens::kCrosSysSecondary);
  SkColor disabled_color = GetColorProvider()->GetColor(
      selected_ ? kColorAshIconPrimaryDisabledColor
                : kColorAshIconSecondaryDisabledColor);
  return GetEnabled() ? active_color : disabled_color;
}

void OptionButtonBase::UpdateTextColor() {
  const auto* color_provider = GetColorProvider();
  const SkColor text_color =
      color_provider->GetColor(cros_tokens::kCrosSysOnSurface);
  SetEnabledTextColors(text_color);
  SetTextColor(ButtonState::STATE_DISABLED,
               color_provider->GetColor(KColorAshTextDisabledColor));
}

BEGIN_METADATA(OptionButtonBase, views::LabelButton)
END_METADATA

}  // namespace ash