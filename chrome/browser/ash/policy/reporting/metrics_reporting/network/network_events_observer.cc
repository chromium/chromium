// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_events_observer.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/wifi_signal_strength_rssi_fetcher.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {
namespace {

bool IsConnectedWifiNetwork(const ::chromeos::NetworkState* network_state) {
  const auto network_type =
      ::ash::NetworkTypePattern::Primitive(network_state->type());
  return network_state->IsConnectedState() &&
         network_type.Equals(::ash::NetworkTypePattern::WiFi());
}

}  // namespace

NetworkEventsObserver::NetworkEventsObserver()
    : CrosHealthdEventsObserverBase<
          chromeos::network_health::mojom::NetworkEventsObserver>(this) {}

NetworkEventsObserver::~NetworkEventsObserver() = default;

void NetworkEventsObserver::OnConnectionStateChanged(
    const std::string& guid,
    chromeos::network_health::mojom::NetworkState state) {
  using NetworkStateMojom = chromeos::network_health::mojom::NetworkState;

  MetricData metric_data;
  metric_data.mutable_event_data()->set_type(
      MetricEventType::NETWORK_CONNECTION_STATE_CHANGE);
  auto* const network_telemetry = metric_data.mutable_telemetry_data()
                                      ->mutable_networks_telemetry()
                                      ->add_network_telemetry();
  network_telemetry->set_guid(guid);
  switch (state) {
    case NetworkStateMojom::kOnline:
      network_telemetry->set_connection_state(NetworkConnectionState::ONLINE);
      break;
    case NetworkStateMojom::kConnected:
      network_telemetry->set_connection_state(
          NetworkConnectionState::CONNECTED);
      break;
    case NetworkStateMojom::kPortal:
      network_telemetry->set_connection_state(NetworkConnectionState::PORTAL);
      break;
    case NetworkStateMojom::kConnecting:
      network_telemetry->set_connection_state(
          NetworkConnectionState::CONNECTING);
      break;
    case NetworkStateMojom::kNotConnected:
      network_telemetry->set_connection_state(
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
  const auto* network_state = ::ash::NetworkHandler::Get()
                                  ->network_state_handler()
                                  ->GetNetworkStateFromGuid(guid);
  if (signal_strength.is_null()) {
    NOTREACHED() << "Signal strength is null";
    return;
  }
  if (!network_state) {
    NOTREACHED() << "Could not find network state with guid " << guid;
    return;
  }
  if (!IsConnectedWifiNetwork(network_state)) {
    return;
  }

  auto wifi_signal_rssi_cb = base::BindOnce(
      &NetworkEventsObserver::OnSignalStrengthChangedRssiValueReceived,
      weak_ptr_factory_.GetWeakPtr(), /*guid=*/guid,
      /*service_path=*/network_state->path(),
      /*signal_strength_percent=*/signal_strength->value);
  FetchWifiSignalStrengthRssi(
      base::queue<std::string>({network_state->path()}),
      base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                         std::move(wifi_signal_rssi_cb)));
}

void NetworkEventsObserver::AddObserver() {
  chromeos::cros_healthd::ServiceConnection::GetInstance()->AddNetworkObserver(
      BindNewPipeAndPassRemote());
}

void NetworkEventsObserver::OnSignalStrengthChangedRssiValueReceived(
    const std::string& guid,
    const std::string& service_path,
    int signal_strength_percent,
    base::flat_map<std::string, int> service_path_rssi_map) {
  MetricData metric_data;
  metric_data.mutable_event_data()->set_type(
      MetricEventType::NETWORK_SIGNAL_STRENGTH_CHANGE);
  auto* const network_telemetry = metric_data.mutable_telemetry_data()
                                      ->mutable_networks_telemetry()
                                      ->add_network_telemetry();
  network_telemetry->set_guid(guid);
  network_telemetry->set_signal_strength(signal_strength_percent);
  if (base::Contains(service_path_rssi_map, service_path)) {
    network_telemetry->set_signal_strength_dbm(
        service_path_rssi_map.at(service_path));
  } else {
    DVLOG(1) << "Wifi signal RSSI not found in the service to signal "
                "map for service: "
             << service_path << " with guid: " << guid;
  }
  OnEventObserved(std::move(metric_data));
}

}  // namespace reporting
