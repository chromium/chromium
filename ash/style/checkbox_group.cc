// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/checkbox_group.h"

#include <utility>

#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

namespace {}  // namespace

CheckboxGroup::CheckboxGroup(int group_width)
    : OptionButtonGroup(group_width) {}

CheckboxGroup::CheckboxGroup(int group_width,
                             const gfx::Insets& inside_border_insets,
                             int between_child_spacing,
                             const gfx::Insets& checkbox_padding,
                             int image_label_spacing)
    : OptionButtonGroup(group_width,
                        inside_border_insets,
                        between_child_spacing,
                        checkbox_padding,
                        image_label_spacing) {}

CheckboxGroup::~CheckboxGroup() = default;

Checkbox* CheckboxGroup::AddButton(Checkbox::PressedCallback callback,
                                   const std::u16string& label) {
  auto* button = AddChildView(std::make_unique<Checkbox>(
      group_width_ - inside_border_insets_.width(), std::move(callback), label,
      button_padding_, image_label_spacing_));
  button->set_delegate(this);
  buttons_.push_back(button);
  return button;
}

void CheckboxGroup::OnButtonClicked(OptionButtonBase* button) {
  button->SetSelected(!button->selected());
}

BEGIN_METADATA(CheckboxGroup)
END_METADATA

}  // namespace ash
