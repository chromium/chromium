// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_feature_pod_controller_legacy.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_feature_pod_button_legacy.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkType;

namespace ash {

namespace {

// Returns true if the network is actually toggled.
bool SetNetworkEnabled(bool enabled) {
  TrayNetworkStateModel* model =
      Shell::Get()->system_tray_model()->network_state_model();
  const NetworkStateProperties* network = model->default_network();

  // For cellular and tether, users are only allowed to disable them from
  // feature pod toggle.
  if (!enabled && network &&
      (network->type == NetworkType::kCellular ||
       network->type == NetworkType::kTether)) {
    model->SetNetworkTypeEnabledState(network->type, false);
    return true;
  }

  if (network && network->type != NetworkType::kWiFi)
    return false;

  model->SetNetworkTypeEnabledState(NetworkType::kWiFi, enabled);
  return true;
}

}  // namespace

NetworkFeaturePodControllerLegacy::NetworkFeaturePodControllerLegacy(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  DCHECK(!ash::features::IsQuickSettingsNetworkRevampEnabled());
}

NetworkFeaturePodControllerLegacy::~NetworkFeaturePodControllerLegacy() =
    default;

FeaturePodButton* NetworkFeaturePodControllerLegacy::CreateButton() {
  DCHECK(!button_);
  button_ = new NetworkFeaturePodButtonLegacy(this);
  UpdateButton();
  TrackVisibilityUMA();
  return button_;
}

QsFeatureCatalogName NetworkFeaturePodControllerLegacy::GetCatalogName() {
  return QsFeatureCatalogName::kNetwork;
}

void NetworkFeaturePodControllerLegacy::OnIconPressed() {
  bool was_enabled = button_->IsToggled();
  bool can_toggle = SetNetworkEnabled(!was_enabled);
  if (can_toggle)
    TrackToggleUMA(/*target_toggle_state=*/!was_enabled);

  // If network was disabled, show network list as well as enabling network.
  // Also, if the network could not be toggled e.g. Ethernet, show network list.
  if (!was_enabled || !can_toggle) {
    TrackDiveInUMA();
    tray_controller_->ShowNetworkDetailedView(!can_toggle /* force */);
  }
}

void NetworkFeaturePodControllerLegacy::OnLabelPressed() {
  TrackDiveInUMA();
  SetNetworkEnabled(true);
  tray_controller_->ShowNetworkDetailedView(true /* force */);
}

void NetworkFeaturePodControllerLegacy::UpdateButton() {
  // Network setting is always immutable in lock screen.
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  button_->SetEnabled(!session_controller->IsScreenLocked());
  button_->Update();
}

}  // namespace ash
