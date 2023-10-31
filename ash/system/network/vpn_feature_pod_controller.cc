// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/vpn_feature_pod_controller.h"

#include <string>

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/network/vpn_list.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::network_config::mojom::ConnectionStateType;
using chromeos::network_config::mojom::NetworkStateProperties;

namespace ash {

namespace {

bool IsVPNVisibleInSystemTray() {
  TrayNetworkStateModel* model =
      Shell::Get()->system_tray_model()->network_state_model();

  // Show the VPN entry in the ash tray bubble if at least one third-party VPN
  // provider is installed.
  if (model->vpn_list()->HaveExtensionOrArcVpnProviders()) {
    return true;
  }

  // Note: At this point, only built-in VPNs are considered.
  return !model->IsBuiltinVpnProhibited() && model->has_vpn();
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

std::unique_ptr<FeatureTile> VPNFeaturePodController::CreateTile(bool compact) {
  DCHECK(!tile_);
  auto tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&FeaturePodControllerBase::OnIconPressed,
                          weak_ptr_factory_.GetWeakPtr()));
  tile_ = tile.get();
  tile_->SetVectorIcon(kUnifiedMenuVpnIcon);
  tile_->SetLabel(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_VPN_SHORT));
  tile_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_VPN_TOOLTIP));
  tile_->CreateDecorativeDrillInArrow();
  // Init the tile with invisible state. `Update()` will update visibility.
  tile_->SetVisible(false);
  Update();
  return tile;
}

QsFeatureCatalogName VPNFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kVPN;
}

void VPNFeaturePodController::OnIconPressed() {
  TrackDiveInUMA();
  tray_controller_->ShowVPNDetailedView();
}

void VPNFeaturePodController::ActiveNetworkStateChanged() {
  Update();
}

void VPNFeaturePodController::Update() {
  const bool is_vpn_visible = IsVPNVisibleInSystemTray();

  // Log UMA metrics if the tile changes from invisible to visible.
  if (!tile_->GetVisible() && is_vpn_visible) {
    TrackVisibilityUMA();
  }
  tile_->SetVisible(is_vpn_visible);

  // No need to update label/toggle state if the button/tile isn't visible.
  if (!is_vpn_visible) {
    return;
  }

  const NetworkStateProperties* vpn =
      Shell::Get()->system_tray_model()->network_state_model()->active_vpn();
  const bool is_active =
      vpn && vpn->connection_state != ConnectionStateType::kNotConnected;
  const std::u16string sub_label = l10n_util::GetStringUTF16(
      is_active ? IDS_ASH_STATUS_TRAY_VPN_CONNECTED_SHORT
                : IDS_ASH_STATUS_TRAY_VPN_DISCONNECTED_SHORT);
  tile_->SetSubLabel(sub_label);
  tile_->SetToggled(is_active);
}

}  // namespace ash
