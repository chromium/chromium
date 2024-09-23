// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_telemetry_sampler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/wifi_signal_strength_rssi_fetcher.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

namespace {

using ::ash::cros_healthd::mojom::NetworkInterfaceInfoPtr;

::ash::NetworkStateHandler::NetworkStateList GetNetworkStateList() {
  ::ash::NetworkStateHandler::NetworkStateList network_state_list;
  ::ash::NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
      ::ash::NetworkTypePattern::Default(),
      /*configured_only=*/true,
      /*visible_only=*/false,
      /*limit=*/0,  // no limit to number of results
      &network_state_list);
  return network_state_list;
}

NetworkInterfaceInfoPtr GetWifiNetworkInterfaceInfo(
    const std::string& device_path,
    const ash::cros_healthd::mojom::TelemetryInfoPtr& cros_healthd_telemetry) {
  if (device_path.empty() || cros_healthd_telemetry.is_null() ||
      cros_healthd_telemetry->network_interface_result.is_null() ||
      !cros_healthd_telemetry->network_interface_result
           ->is_network_interface_info()) {
    return nullptr;
  }

  auto* const device_state =
      ::ash::NetworkHandler::Get()->network_state_handler()->GetDeviceState(
          device_path);
  if (!device_state) {
    return nullptr;
  }

  const std::string& interface_name = device_state->interface();
  const auto& interface_info_list =
      cros_healthd_telemetry->network_interface_result
          ->get_network_interface_info();
  const auto& interface_info_it = base::ranges::find_if(
      interface_info_list,
      [&interface_name](const NetworkInterfaceInfoPtr& interface_info) {
        return !interface_info.is_null() &&
               interface_info->is_wireless_interface_info() &&
               !interface_info->get_wireless_interface_info().is_null() &&
               interface_info->get_wireless_interface_info()->interface_name ==
                   interface_name;
      });
  if (interface_info_it == interface_info_list.end()) {
    return nullptr;
  }

  return interface_info_it->Clone();
}

NetworkType GetNetworkType(const ash::NetworkTypePattern& type) {
  if (type.Equals(ash::NetworkTypePattern::Cellular())) {
    return NetworkType::CELLULAR;
  }
  if (type.MatchesPattern(ash::NetworkTypePattern::EthernetOrEthernetEAP())) {
    return NetworkType::ETHERNET;
  }
  if (type.Equals(ash::NetworkTypePattern::Tether())) {
    return NetworkType::TETHER;
  }
  if (type.Equals(ash::NetworkTypePattern::VPN())) {
    return NetworkType::VPN;
  }
  if (type.Equals(ash::NetworkTypePattern::WiFi())) {
    return NetworkType::WIFI;
  }
  NOTREACHED_IN_MIGRATION()
      << "Unsupported network type: " << type.ToDebugString();
  return NetworkType::NETWORK_TYPE_UNSPECIFIED;  // Unsupported
}

}  // namespace

// static
NetworkConnectionState NetworkTelemetrySampler::GetNetworkConnectionState(
    const ash::NetworkState* network) {
  if (network->IsConnectedState()) {
    auto portal_state = network->GetPortalState();
    switch (portal_state) {
      case ash::NetworkState::PortalState::kUnknown:
        return NetworkConnectionState::CONNECTED;
      case ash::NetworkState::PortalState::kOnline:
        return NetworkConnectionState::ONLINE;
      case ash::NetworkState::PortalState::kPortalSuspected:
      case ash::NetworkState::PortalState::kPortal:
      case ash::NetworkState::PortalState::kNoInternet:
        return NetworkConnectionState::PORTAL;
    }
  }
  if (network->IsConnectingState()) {
    return NetworkConnectionState::CONNECTING;
  }
  return NetworkConnectionState::NOT_CONNECTED;
}

NetworkTelemetrySampler::NetworkTelemetrySampler() = default;

NetworkTelemetrySampler::~NetworkTelemetrySampler() = default;

void NetworkTelemetrySampler::MaybeCollect(OptionalMetricCallback callback) {
  auto handle_probe_result_cb =
      base::BindOnce(&NetworkTelemetrySampler::CollectWifiSignalStrengthRssi,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  ash::cros_healthd::ServiceConnection::GetInstance()
      ->GetProbeService()
      ->ProbeTelemetryInfo(
          std::vector<ash::cros_healthd::mojom::ProbeCategoryEnum>{
              ash::cros_healthd::mojom::ProbeCategoryEnum::kNetworkInterface},
          base::BindPostTaskToCurrentDefault(
              std::move(handle_probe_result_cb)));
}

void NetworkTelemetrySampler::CollectWifiSignalStrengthRssi(
    OptionalMetricCallback callback,
    ash::cros_healthd::mojom::TelemetryInfoPtr cros_healthd_telemetry) {
  base::queue<std::string> service_paths;
  ::ash::NetworkStateHandler::NetworkStateList network_state_list =
      GetNetworkStateList();
  for (const auto* network : network_state_list) {
    ::ash::NetworkTypePattern type =
        ::ash::NetworkTypePattern::Primitive(network->type());
    if (!type.Equals(::ash::NetworkTypePattern::WiFi()) ||
        network->signal_strength() == 0) {
      continue;
    }
    service_paths.push(network->path());
  }

  if (service_paths.empty()) {
    CollectNetworksStates(std::move(callback),
                          std::move(cros_healthd_telemetry),
                          /*service_path_rssi_map=*/{});
    return;
  }

  auto wifi_signal_rssi_cb =
      base::BindOnce(&NetworkTelemetrySampler::CollectNetworksStates,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(cros_healthd_telemetry));
  FetchWifiSignalStrengthRssi(
      std::move(service_paths),
      base::BindPostTaskToCurrentDefault(std::move(wifi_signal_rssi_cb)));
}

void NetworkTelemetrySampler::CollectNetworksStates(
    OptionalMetricCallback callback,
    ash::cros_healthd::mojom::TelemetryInfoPtr cros_healthd_telemetry,
    base::flat_map<std::string, int> service_path_rssi_map) {
  if (cros_healthd_telemetry.is_null() ||
      cros_healthd_telemetry->network_interface_result.is_null()) {
    DVLOG(1) << "cros_healthd: Error getting network result, result is null.";
  } else if (cros_healthd_telemetry->network_interface_result->is_error()) {
    DVLOG(1)
        << "cros_healthd: Error getting network result: "
        << cros_healthd_telemetry->network_interface_result->get_error()->msg;
  }

  MetricData metric_data;
  ::ash::NetworkStateHandler::NetworkStateList network_state_list =
      GetNetworkStateList();
  if (network_state_list.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  bool should_report = false;
  for (const auto* network : network_state_list) {
    ::ash::NetworkTypePattern type =
        ::ash::NetworkTypePattern::Primitive(network->type());
    // Only collect and report networks of any types that are connected, or wifi
    // networks that have signal strength regardless of their connection states.
    if (!network->IsConnectedState() &&
        (!type.Equals(::ash::NetworkTypePattern::WiFi()) ||
         network->signal_strength() == 0)) {
      continue;
    }

    should_report = true;

    NetworkTelemetry* const network_telemetry =
        metric_data.mutable_telemetry_data()
            ->mutable_networks_telemetry()
            ->add_network_telemetry();

    network_telemetry->set_guid(network->guid());

    network_telemetry->set_type(GetNetworkType(type));

    network_telemetry->set_connection_state(GetNetworkConnectionState(network));

    if (!network->device_path().empty()) {
      network_telemetry->set_device_path(network->device_path());
    }

    if (!network->GetIpAddress().empty()) {
      network_telemetry->set_ip_address(network->GetIpAddress());
    }

    if (!network->GetGateway().empty()) {
      network_telemetry->set_gateway(network->GetGateway());
    }

    if (type.Equals(::ash::NetworkTypePattern::WiFi())) {
      network_telemetry->set_signal_strength(network->signal_strength());
      if (base::Contains(service_path_rssi_map, network->path())) {
        network_telemetry->set_signal_strength_dbm(
            service_path_rssi_map.at(network->path()));
      } else {
        DVLOG(1) << "Wifi signal RSSI not found in the service to signal "
                    "map for service: "
                 << network->path();
      }

      const auto& network_interface_info = GetWifiNetworkInterfaceInfo(
          network->device_path(), cros_healthd_telemetry);
      if (!network_interface_info.is_null() &&
          !network_interface_info->get_wireless_interface_info().is_null()) {
        const auto& wireless_info =
            network_interface_info->get_wireless_interface_info();

        // Power management can be set even if the device is not connected to an
        // access point.
        network_telemetry->set_power_management_enabled(
            wireless_info->power_management_on);

        // wireless link info is only available when the device is
        // connected to the access point.
        if (!wireless_info->wireless_link_info.is_null() &&
            network->IsConnectedState()) {
          const auto& wireless_link_info = wireless_info->wireless_link_info;
          network_telemetry->set_tx_bit_rate_mbps(
              wireless_link_info->tx_bit_rate_mbps);
          network_telemetry->set_rx_bit_rate_mbps(
              wireless_link_info->rx_bit_rate_mbps);
          network_telemetry->set_tx_power_dbm(wireless_link_info->tx_power_dBm);
          network_telemetry->set_encryption_on(
              wireless_link_info->encyption_on);
          network_telemetry->set_link_quality(wireless_link_info->link_quality);
        }
      }
    }
  }

  if (should_report) {
    std::move(callback).Run(std::move(metric_data));
    return;
  }

  std::move(callback).Run(std::nullopt);
}

}  // namespace reporting
