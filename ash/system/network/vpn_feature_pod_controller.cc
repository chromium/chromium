// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/vpn_feature_pod_controller.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/network/vpn_list.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

using chromeos::network_config::mojom::ConnectionStateType;
using chromeos::network_config::mojom::NetworkStateProperties;

namespace ash {

namespace {

bool IsVPNVisibleInSystemTray() {
  LoginStatus login_status = Shell::Get()->session_controller()->login_status();
  if (login_status == LoginStatus::NOT_LOGGED_IN)
    return false;

  TrayNetworkStateModel* model =
      Shell::Get()->system_tray_model()->network_state_model();

  // Show the VPN entry in the ash tray bubble if at least one third-party VPN
  // provider is installed.
  if (model->vpn_list()->HaveExtensionOrArcVpnProviders())
    return true;

  // Also show the VPN entry if at least one VPN network is configured.
  return model->has_vpn();
}

}  // namespace

VPNFeaturePodController::VPNFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  Shell::Get()->system_tray_model()->network_state_model()->AddObserver(this);
}

VPNFeaturePodController::~VPNFeaturePodController() {
  Shell::Get()->system_tray_model()->network_state_model()->RemoveObserver(
      this);
}

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

void VPNFeaturePodController::ActiveNetworkStateChanged() {
  Update();
}

void VPNFeaturePodController::Update() {
  button_->SetVisible(IsVPNVisibleInSystemTray());
  if (!button_->GetVisible())
    return;

  const NetworkStateProperties* vpn =
      Shell::Get()->system_tray_model()->network_state_model()->active_vpn();
  bool is_active =
      vpn && vpn->connection_state != ConnectionStateType::kNotConnected;
  button_->SetSubLabel(l10n_util::GetStringUTF16(
      is_active ? IDS_ASH_STATUS_TRAY_VPN_CONNECTED_SHORT
                : IDS_ASH_STATUS_TRAY_VPN_DISCONNECTED_SHORT));
  button_->SetToggled(is_active);
}

}  // namespace ash
