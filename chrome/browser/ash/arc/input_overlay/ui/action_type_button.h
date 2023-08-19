// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_TYPE_BUTTON_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_TYPE_BUTTON_H_

#include <string>

#include "ash/style/option_button_base.h"

namespace arc::input_overlay {

// A rectangular label button with an icon above the label, both centered.
// Functions within a group of action type buttons. Please refer to
// `ActionTypeButtonGroup` for more details.
class ActionTypeButton : public ash::OptionButtonBase {
 public:
  ActionTypeButton(PressedCallback callback,
                   const std::u16string& label,
                   const gfx::VectorIcon& icon);
  ActionTypeButton(const ActionTypeButton&) = delete;
  ActionTypeButton& operator=(const ActionTypeButton&) = delete;
  ~ActionTypeButton() override;

  // Used by the button group to change text color.
  void RefreshTextColor();

 private:
  // ash::OptionButtonBase:
  const gfx::VectorIcon& GetVectorIcon() const override;
  bool IsIconOnTheLeftSide() override;

  // views::LabelButton:
  void Layout() override;
  gfx::ImageSkia GetImage(ButtonState for_state) const override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

  const gfx::VectorIcon& icon_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_TYPE_BUTTON_H_
