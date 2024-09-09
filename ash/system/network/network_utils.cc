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
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

using chromeos::network_config::mojom::DeviceStateProperties;
using chromeos::network_config::mojom::InhibitReason;
using chromeos::network_config::mojom::NetworkType;

}  // namespace

int GetStringIdForNetworkDetailedViewTitleRow(
    NetworkDetailedViewListType list_type) {
  if (base::FeatureList::IsEnabled(ash::features::kInstantHotspotRebrand)) {
    return (list_type == NetworkDetailedViewListType::LIST_TYPE_NETWORK
                ? IDS_ASH_STATUS_TRAY_INTERNET
                : IDS_ASH_STATUS_TRAY_VPN);
  } else {
    return (list_type == NetworkDetailedViewListType::LIST_TYPE_NETWORK
                ? IDS_ASH_STATUS_TRAY_NETWORK
                : IDS_ASH_STATUS_TRAY_VPN);
  }
}

InhibitReason GetCellularInhibitReason() {
  const DeviceStateProperties* cellular_device =
      Shell::Get()->system_tray_model()->network_state_model()->GetDevice(
          NetworkType::kCellular);
  CHECK(cellular_device);
  return cellular_device->inhibit_reason;
}

int GetCellularInhibitReasonMessageId(InhibitReason inhibit_reason) {
  switch (inhibit_reason) {
    case InhibitReason::kInstallingProfile:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_INSTALLING_PROFILE;
    case InhibitReason::kRenamingProfile:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_RENAMING_PROFILE;
    case InhibitReason::kRemovingProfile:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_REMOVING_PROFILE;
    case InhibitReason::kConnectingToProfile:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_CONNECTING_TO_PROFILE;
    case InhibitReason::kRefreshingProfileList:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_REFRESHING_PROFILE_LIST;
    case InhibitReason::kNotInhibited:
      return IDS_ASH_STATUS_TRAY_ADD_CELLULAR_LABEL;
    case InhibitReason::kResettingEuiccMemory:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_RESETTING_ESIM;
    case InhibitReason::kDisablingProfile:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_DISABLING_PROFILE;
    case InhibitReason::kRequestingAvailableProfiles:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_REQUESTING_AVAILABLE_PROFILES;
  }
}

std::optional<std::u16string> GetPortalStateSubtext(
    const chromeos::network_config::mojom::PortalState& portal_state) {
  using chromeos::network_config::mojom::PortalState;
  switch (portal_state) {
    case PortalState::kUnknown:
      [[fallthrough]];
    case PortalState::kOnline:
      return std::nullopt;
    case PortalState::kNoInternet:
      // Use 'no internet' for portal suspected and no internet states.
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED_NO_INTERNET);
    case PortalState::kPortalSuspected:
      [[fallthrough]];
    case PortalState::kPortal:
      // Use 'signin to network' for portal and proxy auth required states.
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_STATUS_SIGNIN);
    case PortalState::kDeprecatedProxyAuthRequired:
      NOTREACHED();
  }
}

bool IsNetworkDisabled(
    const chromeos::network_config::mojom::NetworkStatePropertiesPtr&
        network_properties) {
  if (network_properties->prohibited_by_policy) {
    return true;
  }

  if (!chromeos::network_config::NetworkTypeMatchesType(
          network_properties->type, NetworkType::kCellular)) {
    return false;
  }

  const chromeos::network_config::mojom::CellularStateProperties* cellular =
      network_properties->type_state->get_cellular().get();

  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted() &&
      cellular->sim_locked) {
    return true;
  }

  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted() &&
      cellular->activation_state ==
          chromeos::network_config::mojom::ActivationStateType::kNotActivated &&
      network_properties->type_state->get_cellular()->eid.empty()) {
    return true;
  }

  if (cellular->activation_state ==
      chromeos::network_config::mojom::ActivationStateType::kActivating) {
    return true;
  }

  if (IsNetworkInhibited(network_properties)) {
    return true;
  }

  if (IsCellularDeviceFlashing(network_properties)) {
    return true;
  }

  return false;
}

bool IsNetworkInhibited(
    const chromeos::network_config::mojom::NetworkStatePropertiesPtr&
        network_properties) {
  if (!chromeos::network_config::NetworkTypeMatchesType(
          network_properties->type, NetworkType::kCellular)) {
    return false;
  }

  const DeviceStateProperties* cellular_device =
      Shell::Get()->system_tray_model()->network_state_model()->GetDevice(
          NetworkType::kCellular);

  return cellular_device &&
         chromeos::network_config::IsInhibited(cellular_device);
}

bool IsCellularDeviceFlashing(
    const chromeos::network_config::mojom::NetworkStatePropertiesPtr&
        network_properties) {
  if (!chromeos::network_config::NetworkTypeMatchesType(
          network_properties->type, NetworkType::kCellular)) {
    return false;
  }

  const DeviceStateProperties* cellular_device =
      Shell::Get()->system_tray_model()->network_state_model()->GetDevice(
          NetworkType::kCellular);

  return cellular_device && cellular_device->is_flashing;
}

}  // namespace ash
