// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_telemetry_sampler.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using ::chromeos::cros_healthd::mojom::NetworkInterfaceInfoPtr;

namespace reporting {
namespace {

NetworkInterfaceInfoPtr GetWifiNetworkInterfaceInfo(
    const std::string& device_path,
    const ::chromeos::cros_healthd::mojom::TelemetryInfoPtr& telemetry_info) {
  if (device_path.empty() || telemetry_info.is_null() ||
      telemetry_info->network_interface_result.is_null() ||
      !telemetry_info->network_interface_result->is_network_interface_info()) {
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
      telemetry_info->network_interface_result->get_network_interface_info();
  const auto& interface_info_it = std::find_if(
      interface_info_list.begin(), interface_info_list.end(),
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

void OnHttpsLatencySamplerCompleted(OptionalMetricCallback callback,
                                    MetricData network_data,
                                    absl::optional<MetricData> latency_data) {
  if (latency_data.has_value()) {
    network_data.CheckTypeAndMergeFrom(latency_data.value());
  }
  std::move(callback).Run(std::move(network_data));
}
}  // namespace

NetworkTelemetrySampler::NetworkTelemetrySampler(Sampler* https_latency_sampler)
    : https_latency_sampler_(https_latency_sampler) {}

NetworkTelemetrySampler::~NetworkTelemetrySampler() = default;

void NetworkTelemetrySampler::MaybeCollect(OptionalMetricCallback callback) {
  auto handle_probe_result_cb =
      base::BindOnce(&NetworkTelemetrySampler::HandleNetworkTelemetryResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  ::ash::cros_healthd::ServiceConnection::GetInstance()->ProbeTelemetryInfo(
      std::vector<chromeos::cros_healthd::mojom::ProbeCategoryEnum>{
          chromeos::cros_healthd::mojom::ProbeCategoryEnum::kNetworkInterface},
      base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                         std::move(handle_probe_result_cb)));
}

void NetworkTelemetrySampler::HandleNetworkTelemetryResult(
    OptionalMetricCallback callback,
    ::chromeos::cros_healthd::mojom::TelemetryInfoPtr result) {
  bool full_telemetry_reporting_enabled = base::FeatureList::IsEnabled(
      MetricReportingManager::kEnableNetworkTelemetryReporting);

  if (result.is_null() || result->network_interface_result.is_null()) {
    DVLOG(1) << "cros_healthd: Error getting network result, result is null.";
  } else if (result->network_interface_result->is_error()) {
    DVLOG(1) << "cros_healthd: Error getting network result: "
             << result->network_interface_result->get_error()->msg;
  }

  MetricData metric_data;
  ::ash::NetworkStateHandler::NetworkStateList network_state_list;
  ::ash::NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
      ::ash::NetworkTypePattern::Default(),
      /*configured_only=*/true,
      /*visible_only=*/false,
      /*limit=*/0,  // no limit to number of results
      &network_state_list);
  if (network_state_list.empty()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  bool should_report = false;
  bool should_collect_latency = false;
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

    bool item_reported = full_telemetry_reporting_enabled;
    NetworkTelemetry* network_telemetry = nullptr;
    if (full_telemetry_reporting_enabled) {
      if (network->IsOnline()) {
        should_collect_latency = true;
      }

      network_telemetry = metric_data.mutable_telemetry_data()
                              ->mutable_networks_telemetry()
                              ->add_network_telemetry();

      network_telemetry->set_guid(network->guid());

      network_telemetry->set_connection_state(
          GetNetworkConnectionState(network));

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

    if (type.Equals(::ash::NetworkTypePattern::WiFi())) {
      const auto& network_interface_info =
          GetWifiNetworkInterfaceInfo(network->device_path(), result);
      if (!network_interface_info.is_null() &&
          !network_interface_info->get_wireless_interface_info().is_null()) {
        item_reported = true;
        if (!network_telemetry) {
          network_telemetry = metric_data.mutable_telemetry_data()
                                  ->mutable_networks_telemetry()
                                  ->add_network_telemetry();
        }
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
      if (item_reported) {
        network_telemetry->set_signal_strength(network->signal_strength());
      }
    }

    if (item_reported) {
      network_telemetry->set_type(GetNetworkType(type));
      should_report = true;
    }
  }

  if (should_collect_latency) {
    https_latency_sampler_->MaybeCollect(
        base::BindOnce(OnHttpsLatencySamplerCompleted, std::move(callback),
                       std::move(metric_data)));
    return;
  }
  if (should_report) {
    std::move(callback).Run(std::move(metric_data));
    return;
  }

  std::move(callback).Run(absl::nullopt);
}

}  // namespace reporting
