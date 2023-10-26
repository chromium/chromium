// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_events_observer.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/ash/net/network_health/network_health_manager.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_telemetry_sampler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/wifi_signal_strength_rssi_fetcher.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {
namespace {

constexpr int kSignalThresholdDbm = -70;

bool IsConnectedWifiNetwork(const ash::NetworkState* network_state) {
  const auto network_type =
      ::ash::NetworkTypePattern::Primitive(network_state->type());
  return network_state->IsConnectedState() &&
         network_type.Equals(ash::NetworkTypePattern::WiFi());
}

}  // namespace

BASE_FEATURE(kEnableWifiSignalEventsReporting,
             "EnableWifiSignalEventsReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEnableNetworkConnectionStateEventsReporting,
             "EnableNetworkConnectionStateEventsReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEnableVpnConnectionStateEventsReporting,
             "EnableVpnConnectionStateEventsReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

NetworkEventsObserver::NetworkEventsObserver()
    : MojoServiceEventsObserverBase<
          chromeos::network_health::mojom::NetworkEventsObserver>(this) {}

NetworkEventsObserver::~NetworkEventsObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ash::NetworkHandler::IsInitialized() ||
      !ash::NetworkHandler::Get()->network_state_handler()) {
    return;
  }
  ash::NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
}

void NetworkEventsObserver::NetworkConnectionStateChanged(
    const ash::NetworkState* network) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!network) {
    return;
  }

  NetworkConnectionState state =
      NetworkTelemetrySampler::GetNetworkConnectionState(network);

  MetricData metric_data;
  const auto network_type =
      ::ash::NetworkTypePattern::Primitive(network->type());

  if (network_type.MatchesPattern(ash::NetworkTypePattern::Physical()) &&
      base::FeatureList::IsEnabled(
          kEnableNetworkConnectionStateEventsReporting)) {
    metric_data.mutable_event_data()->set_type(
        MetricEventType::NETWORK_STATE_CHANGE);
  } else if (network_type.Equals(ash::NetworkTypePattern::VPN()) &&
             base::FeatureList::IsEnabled(
                 kEnableVpnConnectionStateEventsReporting)) {
    metric_data.mutable_event_data()->set_type(
        MetricEventType::VPN_CONNECTION_STATE_CHANGE);
  } else {
    // Only report and update `active_connection_state_map_` for VPN and
    // physical networks.
    return;
  }

  const bool active_network =
      base::Contains(active_connection_state_map_, network->guid());

  // If network was not found in the map, this means either it was already
  // reported as NOT_CONNECTED or it is newly added in a disconnected state and
  // we don't want to report the event in both cases. If network was found in
  // the map, make sure that it had a different state than the one previously
  // reported.
  if ((!active_network && state == NetworkConnectionState::NOT_CONNECTED) ||
      (active_network &&
       active_connection_state_map_.at(network->guid()) == state)) {
    // No connection state change to report.
    return;
  }

  if (state != NetworkConnectionState::NOT_CONNECTED) {
    // Update active network connection state.
    active_connection_state_map_[network->guid()] = state;
  } else if (active_network) {
    // Network was active but now disconnected, remove from map as we only
    // maintain currently active connections.
    active_connection_state_map_.erase(network->guid());
  }
  auto* const connection_change_data =
      metric_data.mutable_telemetry_data()
          ->mutable_networks_telemetry()
          ->mutable_network_connection_change_event_data();
  connection_change_data->set_guid(network->guid());
  connection_change_data->set_connection_state(state);
  OnEventObserved(std::move(metric_data));
}

void NetworkEventsObserver::OnShuttingDown() {
  ash::NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
}

void NetworkEventsObserver::OnConnectionStateChanged(
    const std::string& guid,
    chromeos::network_health::mojom::NetworkState state) {
  // |NetworkConnectionStateChanged| is used instead.
}

void NetworkEventsObserver::OnSignalStrengthChanged(
    const std::string& guid,
    chromeos::network_health::mojom::UInt32ValuePtr signal_strength) {
  CHECK(signal_strength) << "Signal strength should have a value.";

  const auto* network_state = ::ash::NetworkHandler::Get()
                                  ->network_state_handler()
                                  ->GetNetworkStateFromGuid(guid);
  if (!network_state) {
    DVLOG(1) << "Could not find network state with guid " << guid;
    return;
  }

  if (IsConnectedWifiNetwork(network_state)) {
    CheckForSignalStrengthEvent(network_state);
  }
}

void NetworkEventsObserver::OnNetworkListChanged(
    std::vector<::chromeos::network_health::mojom::NetworkPtr> networks) {}

void NetworkEventsObserver::AddObserver() {
  ash::network_health::NetworkHealthManager::GetInstance()->AddObserver(
      BindNewPipeAndPassRemote());
}

void NetworkEventsObserver::SetNetworkConnectionObservation(bool is_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  active_connection_state_map_.clear();
  if (!ash::NetworkHandler::IsInitialized() ||
      !ash::NetworkHandler::Get()->network_state_handler()) {
    LOG(ERROR)
        << "Cannot set network connection events observation for reporting. "
           "Network state handler is not initialized.";
    return;
  }
  if (!is_enabled) {
    ash::NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
    return;
  }

  ash::NetworkStateHandler::NetworkStateList network_state_list;
  ash::NetworkHandler::Get()
      ->network_state_handler()
      ->GetActiveNetworkListByType(
          ash::NetworkTypePattern::Physical() | ash::NetworkTypePattern::VPN(),
          &network_state_list);
  for (const auto* network : network_state_list) {
    active_connection_state_map_[network->guid()] =
        NetworkTelemetrySampler::GetNetworkConnectionState(network);
  }
  ash::NetworkHandler::Get()->network_state_handler()->AddObserver(this);
}

void NetworkEventsObserver::SetReportingEnabled(bool is_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MojoServiceEventsObserverBase<
      ::chromeos::network_health::mojom::NetworkEventsObserver>::
      SetReportingEnabled(is_enabled);
  SetNetworkConnectionObservation(is_enabled);
  if (!is_enabled) {
    return;
  }

  // Get signal strength.
  low_signal_reported_ = false;
  const ash::NetworkState* network_state =
      ::ash::NetworkHandler::Get()
          ->network_state_handler()
          ->ActiveNetworkByType(ash::NetworkTypePattern::WiFi());
  if (!network_state || !network_state->IsConnectedState()) {
    return;
  }
  CHECK(IsConnectedWifiNetwork(network_state));
  CheckForSignalStrengthEvent(network_state);
}

void NetworkEventsObserver::CheckForSignalStrengthEvent(
    const ash::NetworkState* network_state) {
  if (!base::FeatureList::IsEnabled(kEnableWifiSignalEventsReporting)) {
    return;
  }

  auto wifi_signal_rssi_cb = base::BindOnce(
      &NetworkEventsObserver::OnSignalStrengthChangedRssiValueReceived,
      weak_ptr_factory_.GetWeakPtr(), network_state->guid(),
      network_state->path());
  FetchWifiSignalStrengthRssi(
      base::queue<std::string>({network_state->path()}),
      base::BindPostTaskToCurrentDefault(std::move(wifi_signal_rssi_cb)));
}

void NetworkEventsObserver::OnSignalStrengthChangedRssiValueReceived(
    const std::string& guid,
    const std::string& service_path,
    base::flat_map<std::string, int> service_path_rssi_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::Contains(service_path_rssi_map, service_path)) {
    DVLOG(1) << "Wifi signal RSSI not found in the service to signal "
                "map for service: "
             << service_path << " with guid: " << guid;
    return;
  }

  const int signal_strength_dbm = service_path_rssi_map.at(service_path);
  const bool low_signal = (signal_strength_dbm < kSignalThresholdDbm);
  if (low_signal == low_signal_reported_) {
    return;  // No change in low signal state.
  }
  // State changed, report metrics.
  low_signal_reported_ = low_signal;

  MetricData metric_data;
  metric_data.mutable_event_data()->set_type(
      signal_strength_dbm < kSignalThresholdDbm
          ? MetricEventType::WIFI_SIGNAL_STRENGTH_LOW
          : MetricEventType::WIFI_SIGNAL_STRENGTH_RECOVERED);
  auto* const networks_telemetry =
      metric_data.mutable_telemetry_data()->mutable_networks_telemetry();
  networks_telemetry->mutable_signal_strength_event_data()->set_guid(guid);
  networks_telemetry->mutable_signal_strength_event_data()
      ->set_signal_strength_dbm(signal_strength_dbm);
  OnEventObserved(std::move(metric_data));
}

}  // namespace reporting
