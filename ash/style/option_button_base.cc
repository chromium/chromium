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
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {

constexpr int kButtonHeight = 36;

}  // namespace

OptionButtonBase::OptionButtonBase(int button_width,
                                   PressedCallback callback,
                                   const std::u16string& label,
                                   const gfx::Insets& insets,
                                   int image_label_spacing)
    : views::LabelButton(std::move(callback), label),
      min_width_(button_width),
      image_label_spacing_(image_label_spacing) {
  SetBorder(views::CreateEmptyBorder(insets));
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false);
  SetImageLabelSpacing(image_label_spacing_);
  views::InstallRectHighlightPathGenerator(this);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetColorId(ui::kColorAshFocusRing);

  SetAndUpdateAccessibleDefaultActionVerb();
  GetViewAccessibility().SetCheckedState(selected_
                                             ? ax::mojom::CheckedState::kTrue
                                             : ax::mojom::CheckedState::kFalse);
}

OptionButtonBase::~OptionButtonBase() = default;

void OptionButtonBase::SetSelected(bool selected) {
  if (selected_ == selected) {
    return;
  }

  selected_ = selected;
  GetViewAccessibility().SetCheckedState(selected_
                                             ? ax::mojom::CheckedState::kTrue
                                             : ax::mojom::CheckedState::kFalse);
  UpdateImage();

  if (delegate_) {
    delegate_->OnButtonSelected(this);
  }
  SetAndUpdateAccessibleDefaultActionVerb();
  OnSelectedChanged();
}

void OptionButtonBase::SetLabelStyle(TypographyToken token) {
  TypographyProvider::Get()->StyleLabel(token, *label());
}

gfx::Size OptionButtonBase::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  int preferred_width =
      kIconSize + image_label_spacing_ +
      label()
          ->GetPreferredSize(views::SizeBounds(label()->width(), {}))
          .width() +
      GetInsets().width();
  return gfx::Size(std::max(preferred_width, min_width_), kButtonHeight);
}

gfx::Size OptionButtonBase::GetMinimumSize() const {
  return gfx::Size(min_width_, kButtonHeight);
}

void OptionButtonBase::SetLabelColorId(ui::ColorId color_id) {
  label()->SetEnabledColorId(color_id);
}

void OptionButtonBase::Layout(PassKey) {
  gfx::Rect local_bounds = GetLocalBounds();
  gfx::Rect local_content_bounds(local_bounds);
  local_content_bounds.Inset(GetInsets());

  ink_drop_container()->SetBoundsRect(local_bounds);

  views::Label* label = this->label();
  gfx::Size label_size(
      local_content_bounds.width() - image_label_spacing_ - kIconSize,
      label->GetPreferredSize(views::SizeBounds(label->width(), {})).height());

  gfx::Point image_origin = local_content_bounds.origin();
  image_origin.Offset(0, (local_content_bounds.height() - kIconSize) / 2);
  gfx::Point label_origin = local_content_bounds.origin();
  label_origin.Offset(
      0, (local_content_bounds.height() - label_size.height()) / 2);

  if (IsIconOnTheLeftSide()) {
    label_origin.Offset(kIconSize + image_label_spacing_, 0);
  } else {
    image_origin.Offset(local_content_bounds.width() - kIconSize, 0);
  }

  image_container_view()->SetBoundsRect(
      gfx::Rect(image_origin, gfx::Size(kIconSize, kIconSize)));
  label->SetBoundsRect(gfx::Rect(label_origin, label_size));
  LayoutSuperclass<Button>(this);
}

void OptionButtonBase::OnThemeChanged() {
  views::Button::OnThemeChanged();
  UpdateImage();
  UpdateTextColor();
}

void OptionButtonBase::NotifyClick(const ui::Event& event) {
  if (delegate_) {
    delegate_->OnButtonClicked(this);
  }
  views::LabelButton::NotifyClick(event);
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
  SetEnabledTextColorIds(cros_tokens::kCrosSysOnSurface);
  SetTextColorId(ButtonState::STATE_DISABLED, KColorAshTextDisabledColor);
}

void OptionButtonBase::SetAndUpdateAccessibleDefaultActionVerb() {
  SetDefaultActionVerb(selected_ ? ax::mojom::DefaultActionVerb::kUncheck
                                 : ax::mojom::DefaultActionVerb::kCheck);
  UpdateAccessibleDefaultActionVerb();
}

void OptionButtonBase::SetLabelFontList(const gfx::FontList& font_list) {
  label()->SetFontList(font_list);
}

BEGIN_METADATA(OptionButtonBase)
END_METADATA

}  // namespace ash
