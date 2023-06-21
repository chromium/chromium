// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_type_button.h"

#include "ash/style/ash_color_id.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"

namespace arc::input_overlay {

namespace {

constexpr int kButtonWidth = 110;
constexpr int kActionTypeButtonHeight = 94;
constexpr int kActionTypeIconSize = 48;
constexpr int kLabelIconSpacing = 8;

}  // namespace

ActionTypeButton::ActionTypeButton(PressedCallback callback,
                                   const std::u16string& label,
                                   const gfx::VectorIcon& icon)
    : ash::OptionButtonBase(kButtonWidth,
                            std::move(callback),
                            label,
                            gfx::Insets::VH(10, 12)),
      icon_(icon) {
  SetPreferredSize(gfx::Size(kButtonWidth, kActionTypeButtonHeight));
  SetVisible(true);
  SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
}

ActionTypeButton::~ActionTypeButton() = default;

void ActionTypeButton::Layout() {
  SizeToPreferredSize();
  gfx::Rect local_bounds = GetLocalBounds();
  gfx::Rect local_content_bounds(local_bounds);
  local_content_bounds.Inset(GetInsets());

  ink_drop_container()->SetBoundsRect(local_bounds);

  views::Label* label = this->label();
  gfx::Size label_size(label->GetPreferredSize().width(),
                       label->GetPreferredSize().height());

  gfx::Point image_origin = local_content_bounds.origin();
  image_origin.Offset((local_content_bounds.width() - kActionTypeIconSize) / 2,
                      0);
  gfx::Point label_origin = local_content_bounds.origin();
  label_origin.Offset((local_content_bounds.width() - label_size.width()) / 2,
                      kActionTypeIconSize + kLabelIconSpacing);

  image()->SetBoundsRect(gfx::Rect(
      image_origin, gfx::Size(kActionTypeIconSize, kActionTypeIconSize)));
  label->SetBoundsRect(gfx::Rect(label_origin, label_size));
  Button::Layout();
}

gfx::ImageSkia ActionTypeButton::GetImage(ButtonState for_state) const {
  return gfx::CreateVectorIcon(GetVectorIcon(), kActionTypeIconSize,
                               GetIconImageColor());
}

const gfx::VectorIcon& ActionTypeButton::GetVectorIcon() const {
  return icon_;
}

bool ActionTypeButton::IsIconOnTheLeftSide() {
  return false;
}

gfx::Size ActionTypeButton::CalculatePreferredSize() const {
  return gfx::Size(kButtonWidth, kActionTypeButtonHeight);
}

void ActionTypeButton::OnThemeChanged() {
  views::Button::OnThemeChanged();
  UpdateImage();
  RefreshTextColor();
}

void ActionTypeButton::RefreshTextColor() {
  auto active_color_id = selected() ? cros_tokens::kCrosSysPrimary
                                    : cros_tokens::kCrosSysOnSurface;
  auto disabled_color_id = selected()
                               ? ash::kColorAshIconPrimaryDisabledColor
                               : ash::kColorAshIconSecondaryDisabledColor;
  SetEnabledTextColorIds(active_color_id);
  SetTextColorId(ButtonState::STATE_DISABLED, disabled_color_id);
}

}  // namespace arc::input_overlay
