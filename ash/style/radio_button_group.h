// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef ASH_STYLE_RADIO_BUTTON_GROUP_H_
#define ASH_STYLE_RADIO_BUTTON_GROUP_H_

#include "ash/ash_export.h"
#include "ash/style/option_button_base.h"
#include "ash/style/option_button_group.h"
#include "ash/style/radio_button.h"

namespace ash {

// RadioButtonGroup is a menu component with a group of radio buttons with the
// vertical layout. When one button is selected, the other buttons will be
// unselected. Clients need to provide the width for the RadioButtonGroup, the
// height of the group is depend on the number of buttons in the group and the
// space between the buttons. Clients can customize the padding of the group,
// the padding, the type of the radio button, and the vertical space between the
// buttons. If they're not provided, the default values will be applied.
class ASH_EXPORT RadioButtonGroup : public OptionButtonGroup,
                                    public OptionButtonBase::Delegate {
  METADATA_HEADER(RadioButtonGroup, OptionButtonGroup)

 public:
  explicit RadioButtonGroup(int group_width);

  RadioButtonGroup(int group_width,
                   const gfx::Insets& inside_border_insets,
                   int between_child_spacing,
                   RadioButton::IconDirection icon_direction,
                   RadioButton::IconType icon_type,
                   const gfx::Insets& radio_button_padding,
                   int image_label_spacing);
  RadioButtonGroup(const RadioButtonGroup&) = delete;
  RadioButtonGroup& operator=(const RadioButtonGroup&) = delete;
  ~RadioButtonGroup() override;

  // OptionButtonGroup:
  RadioButton* AddButton(RadioButton::PressedCallback callback,
                         const std::u16string& label) override;

  // OptionButtonBase::Delegate:
  void OnButtonSelected(OptionButtonBase* button) override;
  void OnButtonClicked(OptionButtonBase* button) override;

 private:
  // The icon direction of the buttons.
  const RadioButton::IconDirection icon_direction_;

  // The icon type of the buttons.
  const RadioButton::IconType icon_type_;
};

}  // namespace ash

#endif  // ASH_STYLE_RADIO_BUTTON_GROUP_H_