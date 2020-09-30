// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/silence_phone_quick_action_controller.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/phonehub/quick_action_item.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

SilencePhoneQuickActionController::SilencePhoneQuickActionController(
    chromeos::phonehub::DoNotDisturbController* dnd_controller)
    : dnd_controller_(dnd_controller) {
  DCHECK(dnd_controller_);
  dnd_controller_->AddObserver(this);
}

SilencePhoneQuickActionController::~SilencePhoneQuickActionController() {
  dnd_controller_->RemoveObserver(this);
}

QuickActionItem* SilencePhoneQuickActionController::CreateItem() {
  DCHECK(!item_);
  item_ = new QuickActionItem(this, IDS_ASH_PHONE_HUB_SILENCE_PHONE_TITLE,
                              kSystemMenuPhoneIcon);
  OnDndStateChanged();
  return item_;
}

void SilencePhoneQuickActionController::OnButtonPressed(bool is_now_enabled) {
  SetState(ActionState::kConnecting);
  dnd_controller_->RequestNewDoNotDisturbState(!is_now_enabled);
  // TODO(leandre): Add a timer to switch back to off state after connecting
  // failed.
}

void SilencePhoneQuickActionController::OnDndStateChanged() {
  dnd_controller_->IsDndEnabled() ? SetState(ActionState::kOn)
                                  : SetState(ActionState::kOff);
}

void SilencePhoneQuickActionController::SetState(ActionState state) {
  bool icon_enabled;
  int state_text_id;
  int sub_label_text;
  switch (state) {
    case ActionState::kOff:
      icon_enabled = false;
      state_text_id = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_DISABLED_STATE_TOOLTIP;
      sub_label_text = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_OFF_STATE;
      break;
    case ActionState::kConnecting:
      icon_enabled = true;
      state_text_id = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_CONNECTING_STATE_TOOLTIP;
      sub_label_text = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_CONNECTING_STATE;
      break;
    case ActionState::kOn:
      icon_enabled = true;
      state_text_id = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_ENABLED_STATE_TOOLTIP;
      sub_label_text = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_ON_STATE;
      break;
  }

  item_->SetToggled(icon_enabled);
  item_->SetSubLabel(l10n_util::GetStringUTF16(sub_label_text));
  base::string16 tooltip_state =
      l10n_util::GetStringFUTF16(state_text_id, item_->GetItemLabel());
  item_->SetIconTooltip(
      l10n_util::GetStringFUTF16(IDS_ASH_PHONE_HUB_QUICK_ACTIONS_TOGGLE_TOOLTIP,
                                 item_->GetItemLabel(), tooltip_state));
}

}  // namespace ash
