// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_TYPE_BUTTON_GROUP_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_TYPE_BUTTON_GROUP_H_

#include "ash/style/option_button_base.h"
#include "ash/style/option_button_group.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_type_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace arc::input_overlay {

class Action;
class DisplayOverlayController;

// ActionTypeButtonGroup is a component of the button options menu of
// game controls, and a group of action type buttons with a horizontal
// layout. When one button is selected, the other buttons will be
// unselected. The selected button will have both its icon and label
// change color.
class ActionTypeButtonGroup : public ash::OptionButtonGroup,
                              public ash::OptionButtonBase::Delegate {
  METADATA_HEADER(ActionTypeButtonGroup, ash::OptionButtonGroup)

 public:
  static std::unique_ptr<ActionTypeButtonGroup> CreateButtonGroup(
      DisplayOverlayController* controller,
      Action* action);

  ActionTypeButtonGroup(DisplayOverlayController* controller, Action* action);
  ActionTypeButtonGroup(const ActionTypeButtonGroup&) = delete;
  ActionTypeButtonGroup& operator=(const ActionTypeButtonGroup&) = delete;
  ~ActionTypeButtonGroup() override;

  // Process the arrow key pressed event. Returns `true` if `event` is
  // processed.
  bool HandleArrowKeyPressed(ActionTypeButton* button,
                             const ui::KeyEvent& event);

  void set_action(Action* action) { action_ = action; }

 private:
  friend class ButtonOptionsMenuTest;

  void Init();

  // Add an action type button.
  ActionTypeButton* AddActionTypeButton(
      ActionTypeButton::PressedCallback callback,
      const std::u16string& label,
      const gfx::VectorIcon& icon);

  // Functions related to buttons:
  void OnActionTapButtonPressed();
  void OnActionMoveButtonPressed();

  // OptionButtonGroup:
  ActionTypeButton* AddButton(ActionTypeButton::PressedCallback callback,
                              const std::u16string& label) override;

  // OptionButtonBase::Delegate:
  void OnButtonSelected(ash::OptionButtonBase* button) override;
  void OnButtonClicked(ash::OptionButtonBase* button) override;

  raw_ptr<DisplayOverlayController> controller_ = nullptr;
  raw_ptr<Action, DanglingUntriaged> action_;

  ActionType selected_action_type_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_TYPE_BUTTON_GROUP_H_
