// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_telemetry_sampler.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace reporting {
namespace {

void HandleNetworkResult(
    MetricCallback callback,
    MetricData metric_data,
    chromeos::cros_healthd::mojom::TelemetryInfoPtr result) {
  const auto& network_result = result->network_interface_result;
  if (network_result.is_null()) {
    std::move(callback).Run(metric_data);
    return;
  }

  switch (network_result->which()) {
    case chromeos::cros_healthd::mojom::NetworkInterfaceResult::Tag::ERROR: {
      DVLOG(1) << "cros_healthd: Error getting network result: "
               << network_result->get_error()->msg;
      break;
    }

    case chromeos::cros_healthd::mojom::NetworkInterfaceResult::Tag::
        NETWORK_INTERFACE_INFO: {
      for (const auto& network_info :
           network_result->get_network_interface_info()) {
        // Handle wireless interface telemetry
        if (network_info->is_wireless_interface_info()) {
          auto* network_telemetry_list = metric_data.mutable_telemetry_data()
                                             ->mutable_networks_telemetry();
          ::reporting::NetworkTelemetry* network_telemetry_out = nullptr;

          const auto& wireless_info =
              network_info->get_wireless_interface_info();
          for (int i = 0; i < network_telemetry_list->network_telemetry_size();
               ++i) {
            const auto& network = network_telemetry_list->network_telemetry(i);

            // Find a matching network from network health probe.
            if (!network.has_device_path()) {
              continue;
            }
            int name_idx = network.device_path().rfind("/");
            if (name_idx == std::string::npos ||
                name_idx >= network.device_path().length() - 1) {
              continue;
            }

            // Matching network found.
            if (wireless_info->interface_name ==
                network.device_path().substr(name_idx + 1)) {
              network_telemetry_out =
                  network_telemetry_list->mutable_network_telemetry(i);
              // Power management can be set even if the device is not connected
              // to an access point.
              network_telemetry_out->set_power_management_enabled(
                  wireless_info->power_management_on);

              // wireless link info is only avialble when the device is
              // connected to the access point.
              if (wireless_info->wireless_link_info) {
                const auto& wireless_link_info =
                    wireless_info->wireless_link_info;
                network_telemetry_out->set_tx_bit_rate_mbps(
                    wireless_link_info->tx_bit_rate_mbps);
                network_telemetry_out->set_rx_bit_rate_mbps(
                    wireless_link_info->rx_bit_rate_mbps);
                network_telemetry_out->set_tx_power_dbm(
                    wireless_link_info->tx_power_dBm);
                network_telemetry_out->set_encryption_on(
                    wireless_link_info->encyption_on);
                network_telemetry_out->set_link_quality(
                    wireless_link_info->link_quality);
                network_telemetry_out->set_access_point_address(
                    wireless_link_info->access_point_address_str);
              }
            }
          }
        }
      }
    }
  }

  std::move(callback).Run(std::move(metric_data));
}

NetworkConnectionState GetNetworkConnectionState(
    const chromeos::NetworkState* network) {
  if (network->IsConnectedState() && network->IsCaptivePortal()) {
    return NetworkConnectionState::PORTAL;
  }
  if (network->IsConnectedState() && network->IsOnline()) {
    return NetworkConnectionState::ONLINE;
  }
  if (network->IsConnectedState()) {
    return NetworkConnectionState::CONNECTED;
  }
  if (network->IsConnectingState()) {
    return NetworkConnectionState::CONNECTING;
  }
  return NetworkConnectionState::NOT_CONNECTED;
}

NetworkType GetNetworkType(const ::chromeos::NetworkTypePattern& type) {
  if (type.Equals(::chromeos::NetworkTypePattern::Cellular())) {
    return NetworkType::CELLULAR;
  }
  if (type.MatchesPattern(
          ::chromeos::NetworkTypePattern::EthernetOrEthernetEAP())) {
    return NetworkType::ETHERNET;
  }
  if (type.Equals(::chromeos::NetworkTypePattern::Tether())) {
    return NetworkType::TETHER;
  }
  if (type.Equals(::chromeos::NetworkTypePattern::VPN())) {
    return NetworkType::VPN;
  }
  if (type.Equals(::chromeos::NetworkTypePattern::WiFi())) {
    return NetworkType::WIFI;
  }
  NOTREACHED() << "Unsupported network type: " << type.ToDebugString();
  return NetworkType::NETWORK_TYPE_UNSPECIFIED;  // Unsupported
}

void OnHttpsLatencySamplerCompleted(MetricCallback callback,
                                    MetricData metric_data) {
  chromeos::NetworkStateHandler::NetworkStateList network_state_list;
  chromeos::NetworkStateHandler* const network_state_handler =
      chromeos::NetworkHandler::Get()->network_state_handler();
  network_state_handler->GetNetworkListByType(
      chromeos::NetworkTypePattern::Default(),
      /*configured_only=*/true,
      /*visible_only=*/false,
      /*limit=*/0,  // no limit to number of results
      &network_state_list);

  if (!metric_data.has_telemetry_data()) {
    DVLOG(1)
        << "Metric data is expected to contain HttpsLatency telemetry data, "
        << " but telemetry data is empty.";
  }

  auto* const telemetry_data = metric_data.mutable_telemetry_data();
  for (const chromeos::NetworkState* network : network_state_list) {
    auto* const network_telemetry =
        telemetry_data->mutable_networks_telemetry()->add_network_telemetry();
    network_telemetry->set_guid(network->guid());
    network_telemetry->set_connection_state(GetNetworkConnectionState(network));
    ::chromeos::NetworkTypePattern type =
        ::chromeos::NetworkTypePattern::Primitive(network->type());
    network_telemetry->set_type(GetNetworkType(type));
    if (type.Equals(::chromeos::NetworkTypePattern::WiFi())) {
      network_telemetry->set_signal_strength(network->signal_strength());
    }

    if (!network->device_path().empty()) {
      network_telemetry->set_device_path(network->device_path());
    }

    if (!network->GetIpAddress().empty()) {
      network_telemetry->set_ip_address(network->GetIpAddress());
    }

    if (!network->GetGateway().empty()) {
      network_telemetry->set_gateway(network->GetGateway());
    }
  }

  chromeos::cros_healthd::ServiceConnection::GetInstance()->ProbeTelemetryInfo(
      std::vector<chromeos::cros_healthd::mojom::ProbeCategoryEnum>{
          chromeos::cros_healthd::mojom::ProbeCategoryEnum::kNetworkInterface},
      base::BindOnce(HandleNetworkResult, std::move(callback),
                     std::move(metric_data)));
}
}  // namespace

NetworkTelemetrySampler::NetworkTelemetrySampler(Sampler* https_latency_sampler)
    : https_latency_sampler_(https_latency_sampler) {}

NetworkTelemetrySampler::~NetworkTelemetrySampler() = default;

void NetworkTelemetrySampler::Collect(MetricCallback callback) {
  https_latency_sampler_->Collect(
      base::BindOnce(OnHttpsLatencySamplerCompleted, std::move(callback)));
}
}  // namespace reporting
