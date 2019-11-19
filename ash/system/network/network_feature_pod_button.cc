// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_feature_pod_button.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/active_network_icon.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::network_config::mojom::ActivationStateType;
using chromeos::network_config::mojom::CellularStateProperties;
using chromeos::network_config::mojom::ConnectionStateType;
using chromeos::network_config::mojom::DeviceStateType;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkType;

namespace ash {

namespace {

base::string16 GetSubLabelForConnectedNetwork(
    const NetworkStateProperties* network) {
  DCHECK(network &&
         chromeos::network_config::StateIsConnected(network->connection_state));

  if (!chromeos::network_config::NetworkStateMatchesType(
          network, NetworkType::kWireless)) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED);
  }

  if (network->type == NetworkType::kCellular) {
    CellularStateProperties* cellular =
        network->type_state->get_cellular().get();
    if (cellular->network_technology == onc::cellular::kTechnologyCdma1Xrtt) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_ONE_X);
    }
    if (cellular->network_technology == onc::cellular::kTechnologyGsm) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_GSM);
    }
    if (cellular->network_technology == onc::cellular::kTechnologyGprs) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_GPRS);
    }
    if (cellular->network_technology == onc::cellular::kTechnologyEdge) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_EDGE);
    }
    if (cellular->network_technology == onc::cellular::kTechnologyEvdo ||
        cellular->network_technology == onc::cellular::kTechnologyUmts) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_THREE_G);
    }
    if (cellular->network_technology == onc::cellular::kTechnologyHspa) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_HSPA);
    }
    if (cellular->network_technology == onc::cellular::kTechnologyHspaPlus) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_HSPA_PLUS);
    }
    if (cellular->network_technology == onc::cellular::kTechnologyLte) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_LTE);
    }
    if (cellular->network_technology == onc::cellular::kTechnologyLteAdvanced) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_LTE_PLUS);
    }

    // All connectivity types exposed by Shill should be covered above. However,
    // as a fail-safe, return the default "Connected" string here to protect
    // against Shill providing an unexpected value.
    NOTREACHED();
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED);
  }

  int signal_strength =
      chromeos::network_config::GetWirelessSignalStrength(network);
  switch (network_icon::GetSignalStrength(signal_strength)) {
    case network_icon::SignalStrength::NONE:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED);
    case network_icon::SignalStrength::WEAK:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_WEAK_SUBLABEL);
    case network_icon::SignalStrength::MEDIUM:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_MEDIUM_SUBLABEL);
    case network_icon::SignalStrength::STRONG:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_STRONG_SUBLABEL);
  }
  NOTREACHED();
  return l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED);
}

}  // namespace

NetworkFeaturePodButton::NetworkFeaturePodButton(
    FeaturePodControllerBase* controller)
    : FeaturePodButton(controller) {
  Shell::Get()->system_tray_model()->network_state_model()->AddObserver(this);
  ShowDetailedViewArrow();
  Update();
}

NetworkFeaturePodButton::~NetworkFeaturePodButton() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  Shell::Get()->system_tray_model()->network_state_model()->RemoveObserver(
      this);
}

void NetworkFeaturePodButton::NetworkIconChanged() {
  Update();
}

void NetworkFeaturePodButton::ActiveNetworkStateChanged() {
  Update();
}

const char* NetworkFeaturePodButton::GetClassName() const {
  return "NetworkFeaturePodButton";
}

void NetworkFeaturePodButton::Update() {
  bool animating = false;
  gfx::ImageSkia image =
      Shell::Get()->system_tray_model()->active_network_icon()->GetImage(
          ActiveNetworkIcon::Type::kSingle,
          network_icon::ICON_TYPE_DEFAULT_VIEW, &animating);
  if (animating)
    network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  else
    network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);

  TrayNetworkStateModel* model =
      Shell::Get()->system_tray_model()->network_state_model();
  const NetworkStateProperties* network = model->default_network();

  bool toggled = network || model->GetDeviceState(NetworkType::kWiFi) ==
                                DeviceStateType::kEnabled;
  SetToggled(toggled);
  icon_button()->SetImage(views::Button::STATE_NORMAL, image);

  base::string16 network_name;
  if (network) {
    network_name = network->type == NetworkType::kEthernet
                       ? l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ETHERNET)
                       : base::UTF8ToUTF16(network->name);
  }
  // Check for Activating first since activating networks may be connected.
  if (network && network->type == NetworkType::kCellular &&
      network->type_state->get_cellular()->activation_state ==
          ActivationStateType::kActivating) {
    SetLabel(network_name);
    SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_ACTIVATING_SUBLABEL));
  } else if (network && chromeos::network_config::StateIsConnected(
                            network->connection_state)) {
    SetLabel(network_name);
    SetSubLabel(GetSubLabelForConnectedNetwork(network));
  } else if (network &&
             network->connection_state == ConnectionStateType::kConnecting) {
    SetLabel(network_name);
    SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_CONNECTING_SUBLABEL));
  } else {
    SetLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_LABEL));
    SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_SUBLABEL));
  }
  base::string16 tooltip;
  Shell::Get()
      ->system_tray_model()
      ->active_network_icon()
      ->GetConnectionStatusStrings(ActiveNetworkIcon::Type::kSingle,
                                   /*a11y_name=*/nullptr,
                                   /*a11y_desc=*/nullptr, &tooltip);
  UpdateTooltip(tooltip);
}

void NetworkFeaturePodButton::UpdateTooltip(
    const base::string16& connection_state_message) {
  // When the button is enabled, use tooltips to alert the user of the actions
  // that will be taken when interacting with the button/toggle. However, if the
  // button is disabled, those actions cannot be taken, so simply display the
  // state of the connection as a tooltip
  if (!GetEnabled()) {
    SetIconTooltip(connection_state_message);
    SetLabelTooltip(connection_state_message);
    return;
  }

  SetIconTooltip(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NETWORK_TOGGLE_TOOLTIP, connection_state_message));
  SetLabelTooltip(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS_TOOLTIP, connection_state_message));
}

}  // namespace ash
