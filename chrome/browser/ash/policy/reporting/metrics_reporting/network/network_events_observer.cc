// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_events_observer.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/net/network_health/network_health_manager.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/wifi_signal_strength_rssi_fetcher.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

NetworkEventsObserver::NetworkEventsObserver()
    : MojoServiceEventsObserverBase<
          chromeos::network_health::mojom::NetworkEventsObserver>(this) {}

NetworkEventsObserver::~NetworkEventsObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NetworkEventsObserver::OnConnectionStateChanged(
    const std::string& guid,
    chromeos::network_health::mojom::NetworkState state) {
  using NetworkStateMojom = chromeos::network_health::mojom::NetworkState;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (last_reported_connection_guid_.has_value() &&
      last_reported_connection_guid_.value() == guid &&
      last_reported_connection_state_.has_value() &&
      last_reported_connection_state_.value() == state) {
    DVLOG(1) << "Connection state already reported";
    return;
  }
  last_reported_connection_guid_ = guid;
  last_reported_connection_state_ = state;

  MetricData metric_data;
  metric_data.mutable_event_data()->set_type(
      MetricEventType::NETWORK_STATE_CHANGE);
  auto* const connection_change_data =
      metric_data.mutable_telemetry_data()
          ->mutable_networks_telemetry()
          ->mutable_network_connection_change_event_data();
  connection_change_data->set_guid(guid);
  switch (state) {
    case NetworkStateMojom::kOnline:
      connection_change_data->set_connection_state(
          NetworkConnectionState::ONLINE);
      break;
    case NetworkStateMojom::kConnected:
      connection_change_data->set_connection_state(
          NetworkConnectionState::CONNECTED);
      break;
    case NetworkStateMojom::kPortal:
      connection_change_data->set_connection_state(
          NetworkConnectionState::PORTAL);
      break;
    case NetworkStateMojom::kConnecting:
      connection_change_data->set_connection_state(
          NetworkConnectionState::CONNECTING);
      break;
    case NetworkStateMojom::kNotConnected:
      connection_change_data->set_connection_state(
          NetworkConnectionState::NOT_CONNECTED);
      break;
    default:
      NOTREACHED();
  }
  OnEventObserved(std::move(metric_data));
}

void NetworkEventsObserver::OnSignalStrengthChanged(
    const std::string& guid,
    chromeos::network_health::mojom::UInt32ValuePtr signal_strength) {
  DCHECK(signal_strength) << "Signal strength should have a value.";

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

void NetworkEventsObserver::SetReportingEnabled(bool is_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MojoServiceEventsObserverBase<
      ::chromeos::network_health::mojom::NetworkEventsObserver>::
      SetReportingEnabled(is_enabled);
  if (!is_enabled) {
    return;
  }
  // Reset connection state fields.
  last_reported_connection_guid_ = absl::nullopt;
  last_reported_connection_state_ = absl::nullopt;

  // Get signal strength.
  low_signal_reported_ = false;
  const ash::NetworkState* network_state =
      ::ash::NetworkHandler::Get()
          ->network_state_handler()
          ->ActiveNetworkByType(ash::NetworkTypePattern::WiFi());
  if (!network_state || !network_state->IsConnectedState()) {
    return;
  }
  DCHECK(IsConnectedWifiNetwork(network_state));
  CheckForSignalStrengthEvent(network_state);
}

void NetworkEventsObserver::CheckForSignalStrengthEvent(
    const ash::NetworkState* network_state) {
  auto wifi_signal_rssi_cb = base::BindOnce(
      &NetworkEventsObserver::OnSignalStrengthChangedRssiValueReceived,
      weak_ptr_factory_.GetWeakPtr(), network_state->guid(),
      network_state->path());
  FetchWifiSignalStrengthRssi(
      base::queue<std::string>({network_state->path()}),
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         std::move(wifi_signal_rssi_cb)));
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
          ? MetricEventType::NETWORK_SIGNAL_STRENGTH_LOW
          : MetricEventType::NETWORK_SIGNAL_STRENGTH_RECOVERED);
  OnEventObserved(std::move(metric_data));
}

}  // namespace reporting
