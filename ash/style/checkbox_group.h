// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_CHECKBOX_GROUP_H_
#define ASH_STYLE_CHECKBOX_GROUP_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/style/checkbox.h"
#include "ash/style/option_button_base.h"
#include "ash/style/option_button_group.h"

namespace ash {

// CheckboxGroup is a menu component with a group of checkbox buttons with the
// vertical layout. Clients need to provide the width for the CheckboxGroup,
// the height of the group depends on the number of buttons in the group and
// the space between the buttons. Clients can customize the padding of the
// group, the padding of the checkbox button, and the vertical space between the
// buttons. If they're not provided, the default values will be applied. User
// can select / unselect as many buttons in the group as they want. Click on the
// unselected button will select it, vise versa.
class ASH_EXPORT CheckboxGroup : public OptionButtonGroup,
                                 public OptionButtonBase::Delegate {
  METADATA_HEADER(CheckboxGroup, OptionButtonGroup)

 public:
  explicit CheckboxGroup(int group_width);
  CheckboxGroup(int group_width,
                const gfx::Insets& inside_border_insets,
                int between_child_spacing,
                const gfx::Insets& checkbox_padding,
                int image_label_spacing);
  CheckboxGroup(const CheckboxGroup&) = delete;
  CheckboxGroup& operator=(const CheckboxGroup&) = delete;
  ~CheckboxGroup() override;

  // OptionButtonGroup:
  Checkbox* AddButton(Checkbox::PressedCallback callback,
                      const std::u16string& label) override;

  // OptionButtonBase::Delegate:
  void OnButtonSelected(OptionButtonBase* button) override {}
  void OnButtonClicked(OptionButtonBase* button) override;
};

}  // namespace ash

#endif  // ASH_STYLE_CHECKBOX_GROUP_H_