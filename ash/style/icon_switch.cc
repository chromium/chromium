// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/icon_switch.h"

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_id.h"
#include "base/bind.h"
#include "base/check.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr gfx::Insets kDefaultInsideBorderInsets(2);
constexpr int kDefaultChildSpacing = 2;

}  // namespace

IconSwitch::IconSwitch()
    : IconSwitch(/*has_background=*/true,
                 kDefaultInsideBorderInsets,
                 kDefaultChildSpacing) {}

IconSwitch::IconSwitch(bool has_background,
                       const gfx::Insets& inside_border_insets,
                       int between_child_spacing)
    : has_background_(has_background) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, inside_border_insets,
      between_child_spacing));

  enabled_changed_subscription_ = AddEnabledChangedCallback(base::BindRepeating(
      &IconSwitch::OnEnableChanged, base::Unretained(this)));
}

IconSwitch::~IconSwitch() = default;

IconButton* IconSwitch::AddButton(IconButton::PressedCallback callback,
                                  const gfx::VectorIcon* icon,
                                  const std::u16string& tooltip_text) {
  auto* button = AddChildView(std::make_unique<IconButton>(
      callback, IconButton::Type::kMediumFloating, icon, tooltip_text,
      /*is_togglable=*/true, /*has_border=*/true));
  button->set_delegate(this);
  buttons_.push_back(button);
  return button;
}

void IconSwitch::ToggleButtonOnAtIndex(size_t index) {
  DCHECK_LT(index, buttons_.size());
  buttons_[index]->SetToggled(true);
}

void IconSwitch::AddedToWidget() {
  if (!has_background_)
    return;

  // Creates a solid background when the view is added to widget.
  SetBackground(views::CreateRoundedRectBackground(
      GetBackgroundColor(), GetPreferredSize().height() / 2));
}

void IconSwitch::OnThemeChanged() {
  views::View::OnThemeChanged();

  // Updates background color on theme changed.
  if (auto* background = GetBackground())
    background->SetNativeControlColor(GetBackgroundColor());
}

void IconSwitch::OnButtonToggled(IconButton* button) {
  if (!button->toggled())
    return;

  for (auto* b : buttons_)
    if (b != button)
      b->SetToggled(false);
}

void IconSwitch::OnButtonClicked(IconButton* button) {
  button->SetToggled(true);
}

void IconSwitch::OnEnableChanged() {
  const bool enabled = GetEnabled();

  for (auto* button : buttons_)
    button->SetEnabled(enabled);

  if (auto* background = GetBackground())
    background->SetNativeControlColor(GetBackgroundColor());
}

SkColor IconSwitch::GetBackgroundColor() const {
  DCHECK(GetWidget());

  SkColor color = GetColorProvider()->GetColor(
      features::IsJellyEnabled()
          ? cros_tokens::kCrosSysSystemOnBase
          : static_cast<ui::ColorId>(kColorAshControlBackgroundColorInactive));
  if (!GetEnabled()) {
    color = SkColorSetA(color, cros_styles::GetOpacity(
                                   cros_styles::OpacityName::kDisabledOpacity));
  }
  return color;
}

BEGIN_METADATA(IconSwitch, views::View)
END_METADATA

}  // namespace ash
