// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_type_button_group.h"

#include "base/notreached.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "ui/views/layout/box_layout.h"

namespace arc::input_overlay {

// static
std::unique_ptr<ActionTypeButtonGroup> ActionTypeButtonGroup::CreateButtonGroup(
    DisplayOverlayController* controller,
    Action* action) {
  auto button_group =
      std::make_unique<ActionTypeButtonGroup>(controller, action);
  button_group->Init();
  return button_group;
}

ActionTypeButtonGroup::ActionTypeButtonGroup(
    DisplayOverlayController* controller,
    Action* action)
    : ash::OptionButtonGroup(/*group_width=*/0),
      controller_(controller),
      action_(action) {}

ActionTypeButtonGroup::~ActionTypeButtonGroup() = default;

void ActionTypeButtonGroup::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kHorizontal,
                       /*inside_border_insets=*/gfx::Insets::VH(8, 8),
                       /*between_child_spacing=*/8))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  auto* tap_button = AddActionTypeButton(
      base::BindRepeating(&ActionTypeButtonGroup::OnActionTapButtonPressed,
                          base::Unretained(this)),
      // TODO(b/274690042): Replace placeholder text with localized strings.
      u"Single button", kGameControlsSingleButtonIcon);
  auto* move_button = AddActionTypeButton(
      base::BindRepeating(&ActionTypeButtonGroup::OnActionMoveButtonPressed,
                          base::Unretained(this)),
      // TODO(b/274690042): Replace placeholder text with localized strings.
      u"Joystick", kGameControlsDpadKeyboardIcon);

  selected_action_type_ = action_->GetType();
  switch (selected_action_type_) {
    case ActionType::TAP:
      tap_button->SetSelected(true);
      break;
    case ActionType::MOVE:
      move_button->SetSelected(true);
      break;
    default:
      NOTREACHED();
  }
}

ActionTypeButton* ActionTypeButtonGroup::AddActionTypeButton(
    ActionTypeButton::PressedCallback callback,
    const std::u16string& label,
    const gfx::VectorIcon& icon) {
  auto* button =
      AddChildView(std::make_unique<ActionTypeButton>(callback, label, icon));
  button->set_delegate(this);
  buttons_.push_back(button);
  return button;
}

ActionTypeButton* ActionTypeButtonGroup::AddButton(
    ActionTypeButton::PressedCallback callback,
    const std::u16string& label) {
  return AddActionTypeButton(callback, label, kGlobeIcon);
}

void ActionTypeButtonGroup::OnButtonSelected(ash::OptionButtonBase* button) {
  if (!button->selected()) {
    return;
  }

  for (auto* b : buttons_) {
    if (b != button) {
      b->SetSelected(false);
    }
    auto* action_type_button = static_cast<ActionTypeButton*>(b);
    action_type_button->RefreshColors();
  }
}

void ActionTypeButtonGroup::OnButtonClicked(ash::OptionButtonBase* button) {
  button->SetSelected(true);
}

void ActionTypeButtonGroup::OnActionTapButtonPressed() {
  if (selected_action_type_ == ActionType::TAP) {
    return;
  }
  selected_action_type_ = ActionType::TAP;
  controller_->ChangeActionType(action_, ActionType::TAP);
}

void ActionTypeButtonGroup::OnActionMoveButtonPressed() {
  if (selected_action_type_ == ActionType::MOVE) {
    return;
  }
  selected_action_type_ = ActionType::MOVE;
  controller_->ChangeActionType(action_, ActionType::MOVE);
}

}  // namespace arc::input_overlay
