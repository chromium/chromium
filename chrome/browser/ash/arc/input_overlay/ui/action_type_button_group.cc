// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_type_button_group.h"

#include <utility>

#include "base/notreached.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_utils.h"

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

bool ActionTypeButtonGroup::HandleArrowKeyPressed(ActionTypeButton* button,
                                                  const ui::KeyEvent& event) {
  DCHECK(event.type() == ui::EventType::kKeyPressed);

  const size_t selected_index = std::distance(
      buttons_.begin(), std::find(buttons_.begin(), buttons_.end(), button));
  const size_t buttons_size = buttons_.size();
  size_t next_index = selected_index;
  switch (event.key_code()) {
    case ui::VKEY_RIGHT:
    case ui::VKEY_DOWN:
      next_index = (selected_index + 1u) % buttons_size;
      break;
    case ui::VKEY_LEFT:
    case ui::VKEY_UP:
      next_index = (selected_index + buttons_size - 1u) % buttons_size;
      break;
    default:
      break;
  }

  if (next_index != selected_index) {
    buttons_[next_index]->NotifyClick(event);
    return true;
  }
  return false;
}

void ActionTypeButtonGroup::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kHorizontal,
                       /*inside_border_insets=*/gfx::Insets(),
                       /*between_child_spacing=*/8))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  auto* tap_button = AddActionTypeButton(
      base::BindRepeating(&ActionTypeButtonGroup::OnActionTapButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_INPUT_OVERLAY_BUTTON_TYPE_SINGLE_BUTTON_LABEL),
      kGameControlsSingleButtonIcon);
  auto* move_button = AddActionTypeButton(
      base::BindRepeating(&ActionTypeButtonGroup::OnActionMoveButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_INPUT_OVERLAY_BUTTON_TYPE_JOYSTICK_BUTTON_LABEL),
      kGameControlsDpadKeyboardIcon);

  selected_action_type_ = action_->GetType();
  switch (selected_action_type_) {
    case ActionType::TAP:
      tap_button->SetSelected(true);
      break;
    case ActionType::MOVE:
      move_button->SetSelected(true);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  GetViewAccessibility().SetRole(ax::mojom::Role::kRadioGroup);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_BUTTON_OPTIONS_BUTTON_TYPE));
}

ActionTypeButton* ActionTypeButtonGroup::AddActionTypeButton(
    ActionTypeButton::PressedCallback callback,
    const std::u16string& label,
    const gfx::VectorIcon& icon) {
  auto* button = AddChildView(
      std::make_unique<ActionTypeButton>(std::move(callback), label, icon));
  button->set_delegate(this);
  buttons_.push_back(button);
  return button;
}

ActionTypeButton* ActionTypeButtonGroup::AddButton(
    ActionTypeButton::PressedCallback callback,
    const std::u16string& label) {
  return AddActionTypeButton(std::move(callback), label, kGlobeIcon);
}

void ActionTypeButtonGroup::OnButtonSelected(ash::OptionButtonBase* button) {
  if (!button->selected()) {
    return;
  }

  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  for (ash::OptionButtonBase* b : buttons_) {
    if (b != button) {
      b->SetSelected(false);
      if (b->HasFocus()) {
        button->RequestFocus();
      }
      b->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
    }
    if (auto* action_type_button = views::AsViewClass<ActionTypeButton>(b)) {
      action_type_button->RefreshColors();
    }
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
  RecordButtonOptionsMenuFunctionTriggered(
      controller_->GetPackageName(),
      ButtonOptionsMenuFunction::kOptionSingleButton);
}

void ActionTypeButtonGroup::OnActionMoveButtonPressed() {
  if (selected_action_type_ == ActionType::MOVE) {
    return;
  }
  selected_action_type_ = ActionType::MOVE;
  controller_->ChangeActionType(action_, ActionType::MOVE);
  RecordButtonOptionsMenuFunctionTriggered(
      controller_->GetPackageName(),
      ButtonOptionsMenuFunction::kOptionJoystick);
}

BEGIN_METADATA(ActionTypeButtonGroup)
END_METADATA

}  // namespace arc::input_overlay
