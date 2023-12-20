// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/silence_phone_quick_action_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/quick_action_item.h"
#include "base/functional/bind.h"
#include "base/timer/timer.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

using phone_hub_metrics::LogQuickActionClick;
using phone_hub_metrics::QuickAction;

// Time to wait until we check the state of the phone to prevent showing wrong
// state
constexpr base::TimeDelta kWaitForRequestTimeout = base::Seconds(10);

}  // namespace

SilencePhoneQuickActionController::SilencePhoneQuickActionController(
    phonehub::DoNotDisturbController* dnd_controller)
    : dnd_controller_(dnd_controller) {
  DCHECK(dnd_controller_);
  dnd_controller_->AddObserver(this);
}

SilencePhoneQuickActionController::~SilencePhoneQuickActionController() {
  dnd_controller_->RemoveObserver(this);
}

bool SilencePhoneQuickActionController::IsItemEnabled() {
  return item_->IsToggled();
}

QuickActionItem* SilencePhoneQuickActionController::CreateItem() {
  DCHECK(!item_);
  item_ = new QuickActionItem(
      this,
      features::IsPhoneHubShortQuickActionPodsTitlesEnabled()
          ? IDS_ASH_PHONE_HUB_SILENCE_PHONE_SHORTENED_TITLE
          : IDS_ASH_PHONE_HUB_SILENCE_PHONE_TITLE,
      kPhoneHubSilencePhoneIcon);
  item_->icon_button()->SetButtonBehavior(
      FeaturePodIconButton::DisabledButtonBehavior::
          kCanDisplayDisabledToggleValue);
  OnDndStateChanged();
  return item_;
}

void SilencePhoneQuickActionController::OnButtonPressed(bool is_now_enabled) {
  // Button should not be pressed if it is disabled.
  DCHECK(state_ != ActionState::kDisabled);

  LogQuickActionClick(is_now_enabled ? QuickAction::kToggleQuietModeOff
                                     : QuickAction::kToggleQuietModeOn);

  requested_state_ = is_now_enabled ? ActionState::kOff : ActionState::kOn;
  SetItemState(requested_state_.value());

  check_requested_state_timer_ = std::make_unique<base::OneShotTimer>();
  check_requested_state_timer_->Start(
      FROM_HERE, kWaitForRequestTimeout,
      base::BindOnce(&SilencePhoneQuickActionController::CheckRequestedState,
                     base::Unretained(this)));

  dnd_controller_->RequestNewDoNotDisturbState(!is_now_enabled);
}

void SilencePhoneQuickActionController::OnDndStateChanged() {
  UpdateQuickActionItemUi();
}

void SilencePhoneQuickActionController::SetItemState(ActionState state) {
  bool icon_enabled;
  bool button_enabled;
  int state_text_id;
  int sub_label_text;
  switch (state) {
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
    case ActionState::kDisabled:
      icon_enabled = dnd_controller_->IsDndEnabled();
      button_enabled = false;
      state_text_id = IDS_ASH_PHONE_HUB_SILENCE_BUTTON_NOT_AVAILABLE_TOOLTIP;
      sub_label_text = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_NOT_AVAILABLE_STATE;
  }

  item_->SetEnabled(button_enabled);
  item_->SetToggled(icon_enabled);
  item_->SetSubLabel(l10n_util::GetStringUTF16(sub_label_text));
  if (state == ActionState::kDisabled) {
    item_->SetTooltip(l10n_util::GetStringUTF16(state_text_id));
  } else {
    std::u16string tooltip_state =
        l10n_util::GetStringFUTF16(state_text_id, item_->GetItemLabel());
    item_->SetTooltip(l10n_util::GetStringFUTF16(
        IDS_ASH_PHONE_HUB_QUICK_ACTIONS_TOGGLE_TOOLTIP, item_->GetItemLabel(),
        tooltip_state));
  }
}

void SilencePhoneQuickActionController::CheckRequestedState() {
  // If the current state is different from the requested state, it means that
  // we fail to change the state, so switch back to the original one.
  if (state_ != requested_state_)
    SetItemState(state_);

  check_requested_state_timer_.reset();
  requested_state_.reset();
}

SilencePhoneQuickActionController::ActionState
SilencePhoneQuickActionController::GetItemState() {
  return state_;
}

void SilencePhoneQuickActionController::UpdateQuickActionItemUi() {
  if (!dnd_controller_->CanRequestNewDndState()) {
    state_ = ActionState::kDisabled;
  } else if (dnd_controller_->IsDndEnabled()) {
    state_ = ActionState::kOn;
  } else {
    state_ = ActionState::kOff;
  }

  SetItemState(state_);
  // If |requested_state_| correctly resembles the current state, reset it and
  // the timer. Reset also if the state is |kDisabled| since we are not
  // requesting a state change.
  if (state_ == requested_state_ || state_ == ActionState::kDisabled) {
    check_requested_state_timer_.reset();
    requested_state_.reset();
  }
}

}  // namespace ash
