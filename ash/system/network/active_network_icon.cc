// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/active_network_icon.h"

#include "ash/public/cpp/network_config_service.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_utils.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/tray_constants.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"

using chromeos::network_config::mojom::ActivationStateType;
using chromeos::network_config::mojom::ConnectionStateType;
using chromeos::network_config::mojom::DeviceStateProperties;
using chromeos::network_config::mojom::DeviceStateType;
using chromeos::network_config::mojom::FilterType;
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkType;

namespace ash {

namespace {

const int kPurgeDelayMs = 500;

}  // namespace

ActiveNetworkIcon::ActiveNetworkIcon(TrayNetworkStateModel* model)
    : model_(model) {
  model_->AddObserver(this);
}

ActiveNetworkIcon::~ActiveNetworkIcon() {
  model_->RemoveObserver(this);
}

void ActiveNetworkIcon::GetConnectionStatusStrings(Type type,
                                                   std::u16string* a11y_name,
                                                   std::u16string* a11y_desc,
                                                   std::u16string* tooltip) {
  const NetworkStateProperties* network = nullptr;
  switch (type) {
    case Type::kSingle:
      network = model_->default_network();
      break;
    case Type::kPrimary:
      // TODO(902409): Provide strings for technology or connecting.
      network = model_->default_network();
      break;
    case Type::kCellular:
      network = model_->active_cellular();
      break;
  }

  std::u16string network_name;
  if (network) {
    network_name = network->type == NetworkType::kEthernet
                       ? l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ETHERNET)
                       : base::UTF8ToUTF16(network->name);
  }
  // Check for Activating first since activating networks may be connected.
  if (network && network->type == NetworkType::kCellular &&
      network->type_state->get_cellular()->activation_state ==
          ActivationStateType::kActivating) {
    std::u16string activating_string = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_ACTIVATING, network_name);
    if (a11y_name)
      *a11y_name = activating_string;
    if (a11y_desc)
      *a11y_desc = std::u16string();
    if (tooltip)
      *tooltip = activating_string;
  } else if (network && chromeos::network_config::StateIsConnected(
                            network->connection_state)) {
    std::u16string connected_string;
    if (auto portal_subtext = GetPortalStateSubtext(network->portal_state)) {
      connected_string = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_PORTAL, network_name, *portal_subtext);
    } else {
      connected_string = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED, network_name);
    }
    std::u16string signal_strength_string;
    if (chromeos::network_config::NetworkTypeMatchesType(
            network->type, NetworkType::kWireless)) {
      // Retrieve the string describing the signal strength, if it is applicable
      // to |network|.
      int signal_strength =
          chromeos::network_config::GetWirelessSignalStrength(network);
      switch (network_icon::GetSignalStrength(signal_strength)) {
        case network_icon::SignalStrength::NONE:
          break;
        case network_icon::SignalStrength::WEAK:
          signal_strength_string = l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_WEAK);
          break;
        case network_icon::SignalStrength::MEDIUM:
          signal_strength_string = l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_MEDIUM);
          break;
        case network_icon::SignalStrength::STRONG:
          signal_strength_string = l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_STRONG);
          break;
      }
    }
    if (a11y_name)
      *a11y_name = connected_string;
    if (a11y_desc)
      *a11y_desc = signal_strength_string;
    if (tooltip) {
      *tooltip = signal_strength_string.empty()
                     ? connected_string
                     : l10n_util::GetStringFUTF16(
                           IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED_TOOLTIP,
                           connected_string, signal_strength_string);
    }
  } else if (network &&
             network->connection_state == ConnectionStateType::kConnecting) {
    std::u16string connecting_string = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_CONNECTING, network_name);
    if (a11y_name)
      *a11y_name = connecting_string;
    if (a11y_desc)
      *a11y_desc = std::u16string();
    if (tooltip)
      *tooltip = connecting_string;
  } else {
    if (a11y_name) {
      *a11y_name = l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_NOT_CONNECTED_A11Y);
    }
    if (a11y_desc)
      *a11y_desc = std::u16string();
    if (tooltip) {
      *tooltip = l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_TOOLTIP);
    }
  }
}

gfx::ImageSkia ActiveNetworkIcon::GetImage(
    const ui::ColorProvider* color_provider,
    Type type,
    network_icon::IconType icon_type,
    bool* animating) {
  switch (type) {
    case Type::kSingle:
      return GetSingleImage(color_provider, icon_type, animating);
    case Type::kPrimary:
      return GetDualImagePrimary(color_provider, icon_type, animating);
    case Type::kCellular:
      return GetDualImageCellular(color_provider, icon_type, animating);
  }
  NOTREACHED();
}

gfx::ImageSkia ActiveNetworkIcon::GetSingleImage(
    const ui::ColorProvider* color_provider,
    network_icon::IconType icon_type,
    bool* animating) {
  // If no network, check for cellular initializing.
  const NetworkStateProperties* default_network = model_->default_network();
  if (!default_network && cellular_uninitialized_msg_ != 0) {
    if (animating)
      *animating = true;
    return network_icon::GetConnectingImageForNetworkType(
        color_provider, NetworkType::kCellular, icon_type);
  }
  return GetDefaultImageImpl(color_provider, default_network, icon_type,
                             animating);
}

gfx::ImageSkia ActiveNetworkIcon::GetDualImagePrimary(
    const ui::ColorProvider* color_provider,
    network_icon::IconType icon_type,
    bool* animating) {
  const NetworkStateProperties* default_network = model_->default_network();
  if (default_network && default_network->type == NetworkType::kCellular) {
    if (chromeos::network_config::StateIsConnected(
            default_network->connection_state)) {
      // TODO(902409): Show proper technology badges.
      if (animating)
        *animating = false;
      return gfx::CreateVectorIcon(
          kNetworkBadgeTechnologyLteIcon,
          network_icon::GetDefaultColorForIconType(color_provider, icon_type));
    }
    // If Cellular is connecting, use the active non cellular network.
    return GetDefaultImageImpl(color_provider, model_->active_non_cellular(),
                               icon_type, animating);
  }
  return GetDefaultImageImpl(color_provider, default_network, icon_type,
                             animating);
}

gfx::ImageSkia ActiveNetworkIcon::GetDualImageCellular(
    const ui::ColorProvider* color_provider,
    network_icon::IconType icon_type,
    bool* animating) {
  if (model_->GetDeviceState(NetworkType::kCellular) ==
      DeviceStateType::kUnavailable) {
    if (animating)
      *animating = false;
    return gfx::ImageSkia();
  }

  if (cellular_uninitialized_msg_ != 0) {
    if (animating)
      *animating = true;
    return network_icon::GetConnectingImageForNetworkType(
        color_provider, NetworkType::kCellular, icon_type);
  }

  const NetworkStateProperties* active_cellular = model_->active_cellular();
  if (!active_cellular) {
    if (animating)
      *animating = false;
    // For the `kCellular` icon in the `UnifiedSystemTray`: if the tray is
    // active, the icon type should be used to get the correct color.
    if (icon_type != network_icon::IconType::ICON_TYPE_TRAY_ACTIVE) {
      icon_type = network_icon::IconType::ICON_TYPE_LIST;
    }
    return network_icon::GetDisconnectedImageForNetworkType(
        color_provider, NetworkType::kCellular, icon_type);
  }

  return network_icon::GetImageForNonVirtualNetwork(
      color_provider, active_cellular, icon_type, false /* show_vpn_badge */,
      animating);
}

gfx::ImageSkia ActiveNetworkIcon::GetDefaultImageImpl(
    const ui::ColorProvider* color_provider,
    const NetworkStateProperties* network,
    network_icon::IconType icon_type,
    bool* animating) {
  if (!network) {
    VLOG(1) << __func__ << ": No network";
    return GetDefaultImageForNoNetwork(color_provider, icon_type, animating);
  }

  const NetworkStateProperties* active_vpn = model_->active_vpn();
  // Connected network with a connecting VPN.
  if (chromeos::network_config::StateIsConnected(network->connection_state) &&
      active_vpn &&
      active_vpn->connection_state == ConnectionStateType::kConnecting) {
    if (animating)
      *animating = true;
    VLOG(1) << __func__ << ": Connected with connecting VPN";
    return network_icon::GetConnectedNetworkWithConnectingVpnImage(
        color_provider, network, icon_type);
  }

  // Default behavior: connected or connecting network, possibly with VPN badge.
  bool show_vpn_badge = !!active_vpn;
  VLOG(1) << __func__ << ": Network: " << network->name;
  return network_icon::GetImageForNonVirtualNetwork(
      color_provider, network, icon_type, show_vpn_badge, animating);
}

gfx::ImageSkia ActiveNetworkIcon::GetDefaultImageForNoNetwork(
    const ui::ColorProvider* color_provider,
    network_icon::IconType icon_type,
    bool* animating) {
  if (animating)
    *animating = false;
  if (model_->GetDeviceState(NetworkType::kWiFi) == DeviceStateType::kEnabled) {
    // WiFi is enabled but no connections available.
    return network_icon::GetImageForWiFiNoConnections(color_provider,
                                                      icon_type);
  }
  // WiFi is disabled, show a full icon with a strikethrough.
  return network_icon::GetImageForWiFiEnabledState(
      color_provider, false /* not enabled*/, icon_type);
}

void ActiveNetworkIcon::SetCellularUninitializedMsg() {
  const DeviceStateProperties* cellular =
      model_->GetDevice(NetworkType::kCellular);
  if (cellular && cellular->device_state == DeviceStateType::kUninitialized) {
    cellular_uninitialized_msg_ = IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR;
    uninitialized_state_time_ = base::Time::Now();
    return;
  }

  // If cellular is scanning, we want to show a 'connecting' image. However,
  // there may be some cases where cellular's scanning property is true while
  // the device is disabling. In this instance, we don't want to show
  // 'connecting'. Only set cellular_uninitialized_msg_ to scanning if the
  // device is scanning and enabled or enabling.
  if (cellular &&
      (cellular->device_state == DeviceStateType::kEnabled ||
       cellular->device_state == DeviceStateType::kEnabling) &&
      cellular->scanning) {
    cellular_uninitialized_msg_ = IDS_ASH_STATUS_TRAY_MOBILE_SCANNING;
    uninitialized_state_time_ = base::Time::Now();
    return;
  }

  // If cellular is not scanning and cellular device is enabled reset cellular
  // initializing state.
  if (cellular && !cellular->scanning &&
      (cellular->device_state == DeviceStateType::kEnabled ||
       cellular->device_state == DeviceStateType::kEnabling)) {
    cellular_uninitialized_msg_ = 0;
  }

  // There can be a delay between leaving the Initializing state and when
  // a Cellular device shows up, so keep showing the initializing
  // animation for a bit to avoid flashing the disconnect icon.
  const int kInitializingDelaySeconds = 1;
  base::TimeDelta dtime = base::Time::Now() - uninitialized_state_time_;
  if (dtime.InSeconds() >= kInitializingDelaySeconds)
    cellular_uninitialized_msg_ = 0;
}

// TrayNetworkStateObserver

void ActiveNetworkIcon::ActiveNetworkStateChanged() {
  SetCellularUninitializedMsg();
}

void ActiveNetworkIcon::DeviceStateListChanged() {
  SetCellularUninitializedMsg();
}

void ActiveNetworkIcon::NetworkListChanged() {
  if (purge_timer_.IsRunning())
    return;
  purge_timer_.Start(FROM_HERE, base::Milliseconds(kPurgeDelayMs),
                     base::BindOnce(&ActiveNetworkIcon::PurgeNetworkIconCache,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void ActiveNetworkIcon::PurgeNetworkIconCache() {
  model_->cros_network_config()->GetNetworkStateList(
      NetworkFilter::New(FilterType::kVisible, NetworkType::kAll,
                         /*limit=*/0),
      base::BindOnce(
          [](std::vector<
              chromeos::network_config::mojom::NetworkStatePropertiesPtr>
                 networks) {
            std::set<std::string> network_guids;
            for (const auto& iter : networks) {
              network_guids.insert(iter->guid);
            }
            network_icon::PurgeNetworkIconCache(network_guids);
          }));
}

}  // namespace ash
