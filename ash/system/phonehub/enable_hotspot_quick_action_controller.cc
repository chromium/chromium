// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/enable_hotspot_quick_action_controller.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/quick_action_item.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

using Status = phonehub::TetherController::Status;
using phone_hub_metrics::LogQuickActionClick;
using phone_hub_metrics::QuickAction;

EnableHotspotQuickActionController::EnableHotspotQuickActionController(
    phonehub::TetherController* tether_controller)
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
                              kPhoneHubEnableHotspotIcon);
  OnTetherStatusChanged();
  return item_;
}

void EnableHotspotQuickActionController::OnButtonPressed(bool is_now_enabled) {
  LogQuickActionClick(is_now_enabled ? QuickAction::kToggleHotspotOff
                                     : QuickAction::kToggleHotspotOn);

  is_now_enabled ? tether_controller_->Disconnect()
                 : tether_controller_->AttemptConnection();
}

void EnableHotspotQuickActionController::OnTetherStatusChanged() {
  switch (tether_controller_->GetStatus()) {
    case Status::kIneligibleForFeature:
      item_->SetVisible(false);
      return;
    case Status::kConnectionUnavailable:
      [[fallthrough]];
    case Status::kConnectionAvailable:
      SetState(ActionState::kOff);
      break;
    case Status::kConnecting:
      SetState(ActionState::kConnecting);
      break;
    case Status::kConnected:
      SetState(ActionState::kConnected);
      break;
    case Status::kNoReception:
      SetState(ActionState::kNoReception);
      break;
  }
  item_->SetVisible(true);
}

void EnableHotspotQuickActionController::SetState(ActionState state) {
  item_->SetEnabled(true);

  bool icon_enabled;
  bool button_enabled;
  int state_text_id;
  int sub_label_text;
  SkColor sub_label_color;
  switch (state) {
    case ActionState::kOff:
      icon_enabled = false;
      button_enabled = true;
      state_text_id = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_DISABLED_STATE_TOOLTIP;
      sub_label_text = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_OFF_STATE;
      sub_label_color = AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorSecondary);
      break;
    case ActionState::kConnecting:
      icon_enabled = true;
      button_enabled = true;
      state_text_id = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_CONNECTING_STATE_TOOLTIP;
      sub_label_text = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_CONNECTING_STATE;
      sub_label_color = AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorSecondary);
      break;
    case ActionState::kConnected:
      icon_enabled = true;
      button_enabled = true;
      state_text_id = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_CONNECTED_STATE_TOOLTIP;
      sub_label_text = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_CONNECTED_STATE;
      sub_label_color = AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPositive);
      break;
    case ActionState::kNoReception:
      icon_enabled = false;
      button_enabled = false;
      state_text_id =
          IDS_ASH_PHONE_HUB_ENABLE_HOTSPOT_NO_RECEPTION_STATE_TOOLTIP;
      sub_label_text = IDS_ASH_PHONE_HUB_QUICK_ACTIONS_NOT_AVAILABLE_STATE;
      sub_label_color = AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorSecondary);
      break;
  }

  item_->SetToggled(icon_enabled);
  item_->SetEnabled(button_enabled);
  item_->SetSubLabel(l10n_util::GetStringUTF16(sub_label_text));
  item_->SetSubLabelColor(sub_label_color);

  if (state == ActionState::kNoReception) {
    item_->SetTooltip(l10n_util::GetStringUTF16(state_text_id));
  } else {
    std::u16string tooltip_state =
        l10n_util::GetStringFUTF16(state_text_id, item_->GetItemLabel());
    item_->SetTooltip(l10n_util::GetStringFUTF16(
        IDS_ASH_PHONE_HUB_QUICK_ACTIONS_TOGGLE_TOOLTIP, item_->GetItemLabel(),
        tooltip_state));
  }
}

}  // namespace ash
