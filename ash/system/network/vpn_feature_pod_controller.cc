// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/vpn_feature_pod_controller.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/tray_vpn.h"
#include "ash/system/network/vpn_list.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

using chromeos::NetworkHandler;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ash {

VPNFeaturePodController::VPNFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : network_state_observer_(std::make_unique<TrayNetworkStateObserver>(this)),
      tray_controller_(tray_controller) {}

VPNFeaturePodController::~VPNFeaturePodController() = default;

FeaturePodButton* VPNFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new FeaturePodButton(this);
  button_->SetVectorIcon(kUnifiedMenuVpnIcon);
  button_->SetLabel(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_VPN_SHORT));
  button_->SetIconAndLabelTooltips(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_VPN_TOOLTIP));
  button_->ShowDetailedViewArrow();
  button_->DisableLabelButtonFocus();
  Update();
  return button_;
}

void VPNFeaturePodController::OnIconPressed() {
  tray_controller_->ShowVPNDetailedView();
}

SystemTrayItemUmaType VPNFeaturePodController::GetUmaType() const {
  return SystemTrayItemUmaType::UMA_VPN;
}

void VPNFeaturePodController::NetworkStateChanged(bool notify_a11y) {
  Update();
}

void VPNFeaturePodController::Update() {
  // NetworkHandler can be uninitialized in unit tests.
  if (!chromeos::NetworkHandler::IsInitialized())
    return;

  button_->SetVisible(tray::IsVPNVisibleInSystemTray());
  if (!button_->visible())
    return;

  button_->SetSubLabel(l10n_util::GetStringUTF16(
      tray::IsVPNConnected() ? IDS_ASH_STATUS_TRAY_VPN_CONNECTED_SHORT
                             : IDS_ASH_STATUS_TRAY_VPN_DISCONNECTED_SHORT));
  button_->SetToggled(tray::IsVPNEnabled() && tray::IsVPNConnected());
}

}  // namespace ash
