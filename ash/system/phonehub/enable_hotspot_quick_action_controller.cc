// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/enable_hotspot_quick_action_controller.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/phonehub/quick_action_item.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

using Status = chromeos::phonehub::TetherController::Status;

EnableHotspotQuickActionController::EnableHotspotQuickActionController(
    chromeos::phonehub::TetherController* tether_controller)
    : tether_controller_(tether_controller) {
  DCHECK(tether_controller_);
  tether_controller_->AddObserver(this);
}

EnableHotspotQuickActionController::~EnableHotspotQuickActionController() {
  tether_controller_->RemoveObserver(this);
}

QuickActionItem* EnableHotspotQuickActionController::CreateItem() {
  DCHECK(!item_);
  item_ = new QuickActionItem(this, IDS_ASH_PHONE_HUB_ENABLE_HOTSPOT_TITLE,
                              kSystemMenuPhoneIcon);
  // When the UI has just opened, scan to see if there is a connection
  // available.
  if (tether_controller_->GetStatus() == Status::kConnectionUnavailable)
    tether_controller_->ScanForAvailableConnection();
  OnTetherStatusChanged();
  return item_;
}

void EnableHotspotQuickActionController::OnButtonPressed(bool is_now_enabled) {
  is_now_enabled ? tether_controller_->Disconnect()
                 : tether_controller_->AttemptConnection();
}

void EnableHotspotQuickActionController::OnTetherStatusChanged() {
  switch (tether_controller_->GetStatus()) {
    case Status::kIneligibleForFeature:
      item_->SetVisible(false);
      return;
    case Status::kConnectionUnavailable:
      SetState(ActionState::kNotAvailable);
      break;
    case Status::kConnectionAvailable:
      SetState(ActionState::kNotConnected);
      break;
    case Status::kConnecting:
      SetState(ActionState::kConnecting);
      break;
    case Status::kConnected:
      SetState(ActionState::kConnected);
      break;
  }
  item_->SetVisible(true);
}

void EnableHotspotQuickActionController::SetState(ActionState state) {
  item_->SetEnabled(true);

  bool icon_enabled;
  int state_text_id;
  int sub_label_text;
  switch (state) {
    case ActionState::kNotAvailable:
      icon_enabled = false;
      state_text_id =
          IDS_ASH_PHONE_HUB_QUICK_ACTIONS_NOT_AVAILABLE_STATE_TOOLTIP;
      sub_label_text = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_NOT_AVAILABLE_STATE;
      break;
    case ActionState::kNotConnected:
      icon_enabled = false;
      state_text_id =
          IDS_ASH_PHONE_HUB_QUICK_ACTIONS_NOT_CONNECTED_STATE_TOOLTIP;
      sub_label_text = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_NOT_CONNECTED_STATE;
      break;
    case ActionState::kConnecting:
      icon_enabled = true;
      state_text_id = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_CONNECTING_STATE_TOOLTIP;
      sub_label_text = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_CONNECTING_STATE;
      break;
    case ActionState::kConnected:
      icon_enabled = true;
      state_text_id = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_CONNECTED_STATE_TOOLTIP;
      sub_label_text = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_CONNECTED_STATE;
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
