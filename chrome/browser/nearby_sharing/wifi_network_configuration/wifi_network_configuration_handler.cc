// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/wifi_network_configuration/wifi_network_configuration_handler.h"

#include "chrome/browser/nearby_sharing/nearby_share_metrics.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/cross_device/logging/logging.h"

namespace {

chromeos::network_config::mojom::SecurityType MojoSecurityTypeFromAttachment(
    const WifiCredentialsAttachment::SecurityType& security_type) {
  switch (security_type) {
    case WifiCredentialsAttachment::SecurityType::kWpaPsk:
      return chromeos::network_config::mojom::SecurityType::kWpaPsk;
    case WifiCredentialsAttachment::SecurityType::kWep:
      return chromeos::network_config::mojom::SecurityType::kWepPsk;
    default:
      return chromeos::network_config::mojom::SecurityType::kNone;
  }
}

}  // namespace

WifiNetworkConfigurationHandler::WifiNetworkConfigurationHandler() {
  ash::network_config::BindToInProcessInstance(
      cros_network_config_remote_.BindNewPipeAndPassReceiver());
}

WifiNetworkConfigurationHandler::~WifiNetworkConfigurationHandler() = default;

void WifiNetworkConfigurationHandler::ConfigureWifiNetwork(
    const WifiCredentialsAttachment& wifi_credentials_attachment,
    chromeos::network_config::mojom::CrosNetworkConfig::ConfigureNetworkCallback
        callback) {
  auto wifi = chromeos::network_config::mojom::WiFiConfigProperties::New();

  wifi->passphrase = wifi_credentials_attachment.wifi_password();
  wifi->security = MojoSecurityTypeFromAttachment(
      wifi_credentials_attachment.security_type());
  wifi->ssid = wifi_credentials_attachment.ssid();
  wifi->hidden_ssid =
      chromeos::network_config::mojom::HiddenSsidMode::kDisabled;

  auto config = chromeos::network_config::mojom::ConfigProperties::New();
  config->type_config =
      chromeos::network_config::mojom::NetworkTypeConfigProperties::NewWifi(
          std::move(wifi));
  config->auto_connect =
      chromeos::network_config::mojom::AutoConnectConfig::New(true);

  cros_network_config_remote_->ConfigureNetwork(
      std::move(config), /*shared=*/false,
      base::BindOnce(
          &WifiNetworkConfigurationHandler::OnConfigureWifiNetworkResult,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WifiNetworkConfigurationHandler::OnConfigureWifiNetworkResult(
    chromeos::network_config::mojom::CrosNetworkConfig::ConfigureNetworkCallback
        callback,
    const std::optional<std::string>& network_guid,
    const std::string& error_message) {
  if (network_guid) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Successfully configured to network";
    RecordNearbyShareWifiConfigurationResultMetric(/*success=*/true);
  } else {
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Failed to configure network because "
        << error_message;
    RecordNearbyShareWifiConfigurationResultMetric(/*success=*/false);
  }
  std::move(callback).Run(network_guid, error_message);
}
