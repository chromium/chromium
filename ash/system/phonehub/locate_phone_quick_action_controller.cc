// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/locate_phone_quick_action_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/quick_action_item.h"
#include "base/functional/bind.h"
#include "base/timer/timer.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

using phone_hub_metrics::LogQuickActionClick;
using phone_hub_metrics::QuickAction;

namespace {

// Time to wait until we check the state of the phone to prevent showing wrong
// state
constexpr base::TimeDelta kWaitForRequestTimeout = base::Seconds(5);

}  // namespace

using Status = phonehub::FindMyDeviceController::Status;

LocatePhoneQuickActionController::LocatePhoneQuickActionController(
    phonehub::FindMyDeviceController* find_my_device_controller)
    : find_my_device_controller_(find_my_device_controller) {
  DCHECK(find_my_device_controller_);
  find_my_device_controller_->AddObserver(this);
}

LocatePhoneQuickActionController::~LocatePhoneQuickActionController() {
  find_my_device_controller_->RemoveObserver(this);
}

QuickActionItem* LocatePhoneQuickActionController::CreateItem() {
  DCHECK(!item_);
  item_ = new QuickActionItem(
      this,
      features::IsPhoneHubShortQuickActionPodsTitlesEnabled()
          ? IDS_ASH_PHONE_HUB_LOCATE_PHONE_SHORTENED_TITLE
          : IDS_ASH_PHONE_HUB_LOCATE_PHONE_TITLE,
      kPhoneHubLocatePhoneIcon);
  OnPhoneRingingStateChanged();
  return item_;
}

void LocatePhoneQuickActionController::OnButtonPressed(bool is_now_enabled) {
  LogQuickActionClick(is_now_enabled ? QuickAction::kToggleLocatePhoneOff
                                     : QuickAction::kToggleLocatePhoneOn);

  requested_state_ = is_now_enabled ? ActionState::kOff : ActionState::kOn;
  SetItemState(requested_state_.value());

  check_requested_state_timer_ = std::make_unique<base::OneShotTimer>();
  check_requested_state_timer_->Start(
      FROM_HERE, kWaitForRequestTimeout,
      base::BindOnce(&LocatePhoneQuickActionController::CheckRequestedState,
                     base::Unretained(this)));

  find_my_device_controller_->RequestNewPhoneRingingState(!is_now_enabled);
}

void LocatePhoneQuickActionController::OnPhoneRingingStateChanged() {
  UpdateState();
}

void LocatePhoneQuickActionController::UpdateState() {
  UpdateQuickActionItemUi();
}

void LocatePhoneQuickActionController::SetItemState(ActionState state) {
  bool icon_enabled;
  bool button_enabled;
  int state_text_id;
  int sub_label_text;
  switch (state) {
    case ActionState::kNotAvailable:
      icon_enabled = false;
      button_enabled = false;
      state_text_id = IDS_ASH_PHONE_HUB_LOCATE_BUTTON_NOT_AVAILABLE_TOOLTIP;
      sub_label_text = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_NOT_AVAILABLE_STATE;
      break;
    case ActionState::kOff:
      icon_enabled = false;
      button_enabled = true;
      state_text_id = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_DISABLED_STATE_TOOLTIP;
      sub_label_text = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_OFF_STATE;
      break;
    case ActionState::kOn:
      icon_enabled = true;
      button_enabled = true;
      state_text_id = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_ENABLED_STATE_TOOLTIP;
      sub_label_text = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_ON_STATE;
      break;
  }

  item_->SetEnabled(button_enabled);
  item_->SetToggled(icon_enabled);
  item_->SetSubLabel(l10n_util::GetStringUTF16(sub_label_text));
  if (state == ActionState::kNotAvailable) {
    item_->SetTooltip(l10n_util::GetStringUTF16(state_text_id));
  } else {
    std::u16string tooltip_state =
        l10n_util::GetStringFUTF16(state_text_id, item_->GetItemLabel());
    item_->SetTooltip(l10n_util::GetStringFUTF16(
        IDS_ASH_PHONE_HUB_QUICK_ACTIONS_TOGGLE_TOOLTIP, item_->GetItemLabel(),
        tooltip_state));
  }
}

void LocatePhoneQuickActionController::CheckRequestedState() {
  // If the current state is different from the requested state, it means that
  // we fail to change the state, so switch back to the original one.
  if (state_ != requested_state_)
    SetItemState(state_);

  check_requested_state_timer_.reset();
  requested_state_.reset();
}

void LocatePhoneQuickActionController::UpdateQuickActionItemUi() {
  // Disable Locate Phone if Silence Phone is on, otherwise change accordingly
  // based on status from FindMyDeviceController.
  switch (find_my_device_controller_->GetPhoneRingingStatus()) {
    case Status::kRingingOff:
      state_ = ActionState::kOff;
      break;
    case Status::kRingingOn:
      state_ = ActionState::kOn;
      break;
    case Status::kRingingNotAvailable:
      state_ = ActionState::kNotAvailable;
      break;
  }

  SetItemState(state_);

  // If |requested_state_| correctly resembles the current state, reset it and
  // the timer.
  if (state_ == requested_state_) {
    check_requested_state_timer_.reset();
    requested_state_.reset();
  }
}

}  // namespace ash
