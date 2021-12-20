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
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace reporting {
namespace {

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

  CrosHealthdMetricSampler sampler(
      chromeos::cros_healthd::mojom::ProbeCategoryEnum::kNetworkInterface,
      ::reporting::CrosHealthdMetricSampler::MetricType::kTelemetry);
  sampler.SetMetricData(std::move(metric_data));
  sampler.Collect(std::move(callback));
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
