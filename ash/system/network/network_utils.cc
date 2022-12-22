// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_utils.h"

#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/tray_network_state_model.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

std::string GetNetworkTypeName(
    chromeos::network_config::mojom::NetworkType network_type) {
  switch (network_type) {
    case chromeos::network_config::mojom::NetworkType::kCellular:
      [[fallthrough]];
    case chromeos::network_config::mojom::NetworkType::kTether:
      [[fallthrough]];
    case chromeos::network_config::mojom::NetworkType::kMobile:
      return "Mobile";
    case chromeos::network_config::mojom::NetworkType::kWiFi:
      return "WiFi";
    default:
      // A network type of other is unexpected, and no success
      // metric for it exists.
      NOTREACHED();
      return "";
  }
}

}  // namespace

void RecordNetworkRowClickedAction(NetworkRowClickedAction action) {
  base::UmaHistogramEnumeration("ChromeOS.SystemTray.Network.RowClickedAction",
                                action);
}

void RecordDetailedViewSection(DetailedViewSection section) {
  base::UmaHistogramEnumeration("ChromeOS.SystemTray.Network.SectionShown",
                                section);
}

void RecordNetworkTypeToggled(
    chromeos::network_config::mojom::NetworkType network_type,
    bool new_state) {
  const std::string network_name = GetNetworkTypeName(network_type);

  DCHECK(!network_name.empty());

  base::UmaHistogramBoolean(
      base::StrCat({"ChromeOS.SystemTray.Network.", network_name, ".Toggled"}),
      new_state);
}

absl::optional<std::u16string> GetPortalStateSubtext(
    const chromeos::network_config::mojom::PortalState& portal_state) {
  if (!ash::features::IsCaptivePortalUI2022Enabled()) {
    return absl::nullopt;
  }
  using chromeos::network_config::mojom::PortalState;
  switch (portal_state) {
    case PortalState::kUnknown:
      [[fallthrough]];
    case PortalState::kOnline:
      return absl::nullopt;
    case PortalState::kPortalSuspected:
      [[fallthrough]];
    case PortalState::kNoInternet:
      // Use 'no internet' for portal suspected and no internet states.
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED_NO_INTERNET);
    case PortalState::kPortal:
      [[fallthrough]];
    case PortalState::kProxyAuthRequired:
      // Use 'signin to network' for portal and proxy auth required states.
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_STATUS_SIGNIN);
  }
}

bool IsNetworkDisabled(
    const chromeos::network_config::mojom::NetworkStatePropertiesPtr&
        network_properties) {
  if (network_properties->prohibited_by_policy) {
    return true;
  }

  if (!chromeos::network_config::NetworkTypeMatchesType(
          network_properties->type,
          chromeos::network_config::mojom::NetworkType::kCellular)) {
    return false;
  }

  const chromeos::network_config::mojom::CellularStateProperties* cellular =
      network_properties->type_state->get_cellular().get();

  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted() &&
      cellular->sim_locked) {
    return true;
  }

  if (cellular->activation_state ==
      chromeos::network_config::mojom::ActivationStateType::kActivating) {
    return true;
  }

  if (IsNetworkInhibited(network_properties)) {
    return true;
  }

  return false;
}

bool IsNetworkInhibited(
    const chromeos::network_config::mojom::NetworkStatePropertiesPtr&
        network_properties) {
  if (!chromeos::network_config::NetworkTypeMatchesType(
          network_properties->type,
          chromeos::network_config::mojom::NetworkType::kCellular)) {
    return false;
  }

  const chromeos::network_config::mojom::DeviceStateProperties*
      cellular_device =
          Shell::Get()->system_tray_model()->network_state_model()->GetDevice(
              chromeos::network_config::mojom::NetworkType::kCellular);

  return cellular_device &&
         chromeos::network_config::IsInhibited(cellular_device);
}

}  // namespace ash
