// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_events_observer.h"

#include <utility>

#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {
namespace {

bool IsConnectedWifiNetwork(const std::string& guid) {
  const auto* network_state = ::ash::NetworkHandler::Get()
                                  ->network_state_handler()
                                  ->GetNetworkStateFromGuid(guid);
  if (!network_state) {
    return false;
  }
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
  if (!IsConnectedWifiNetwork(guid)) {
    return;
  }

  MetricData metric_data;
  metric_data.mutable_event_data()->set_type(
      MetricEventType::NETWORK_SIGNAL_STRENGTH_CHANGE);
  auto* const network_telemetry = metric_data.mutable_telemetry_data()
                                      ->mutable_networks_telemetry()
                                      ->add_network_telemetry();
  network_telemetry->set_guid(guid);
  network_telemetry->set_signal_strength(signal_strength->value);
  OnEventObserved(std::move(metric_data));
}

void NetworkEventsObserver::AddObserver() {
  chromeos::cros_healthd::ServiceConnection::GetInstance()->AddNetworkObserver(
      BindNewPipeAndPassRemote());
}

}  // namespace reporting
