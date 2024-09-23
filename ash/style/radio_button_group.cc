// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/radio_button_group.h"

#include <utility>

#include "ash/style/radio_button.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

namespace {}  // namespace

RadioButtonGroup::RadioButtonGroup(int group_width)
    : OptionButtonGroup(group_width),
      icon_direction_(RadioButton::IconDirection::kLeading),
      icon_type_(RadioButton::IconType::kCircle) {
  GetViewAccessibility().SetRole(ax::mojom::Role::kRadioGroup);
}

RadioButtonGroup::RadioButtonGroup(int group_width,
                                   const gfx::Insets& inside_border_insets,
                                   int between_child_spacing,
                                   RadioButton::IconDirection icon_direction,
                                   RadioButton::IconType icon_type,
                                   const gfx::Insets& radio_button_padding,
                                   int image_label_spacing)
    : OptionButtonGroup(group_width,
                        inside_border_insets,
                        between_child_spacing,
                        radio_button_padding,
                        image_label_spacing),
      icon_direction_(icon_direction),
      icon_type_(icon_type) {
  GetViewAccessibility().SetRole(ax::mojom::Role::kRadioGroup);
}

RadioButtonGroup::~RadioButtonGroup() = default;

RadioButton* RadioButtonGroup::AddButton(RadioButton::PressedCallback callback,
                                         const std::u16string& label) {
  auto* button = AddChildView(std::make_unique<RadioButton>(
      group_width_ - inside_border_insets_.width(), std::move(callback), label,
      icon_direction_, icon_type_, button_padding_, image_label_spacing_));
  button->set_delegate(this);
  buttons_.push_back(button);
  return button;
}

void RadioButtonGroup::OnButtonSelected(OptionButtonBase* button) {
  if (!button->selected()) {
    return;
  }

  for (ash::OptionButtonBase* b : buttons_) {
    if (b != button) {
      b->SetSelected(false);
    }
  }
  button->ScrollViewToVisible();
}

void RadioButtonGroup::OnButtonClicked(OptionButtonBase* button) {
  button->SetSelected(true);
}

BEGIN_METADATA(RadioButtonGroup)
END_METADATA

}  // namespace ash
