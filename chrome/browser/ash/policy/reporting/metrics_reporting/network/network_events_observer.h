// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_EVENTS_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_EVENTS_OBSERVER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/mojo_service_events_observer_base.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

class NetworkEventsObserver
    : public ::chromeos::network_health::mojom::NetworkEventsObserver,
      public MojoServiceEventsObserverBase<
          ::chromeos::network_health::mojom::NetworkEventsObserver> {
 public:
  NetworkEventsObserver();

  NetworkEventsObserver(const NetworkEventsObserver&) = delete;
  NetworkEventsObserver& operator=(const NetworkEventsObserver&) = delete;

  ~NetworkEventsObserver() override;

  // ::chromeos::network_health::mojom::NetworkEventsObserver:
  void OnConnectionStateChanged(
      const std::string& guid,
      ::chromeos::network_health::mojom::NetworkState state) override;
  void OnSignalStrengthChanged(const std::string& guid,
                               ::chromeos::network_health::mojom::UInt32ValuePtr
                                   signal_strength) override;
  void OnNetworkListChanged(
      std::vector<::chromeos::network_health::mojom::NetworkPtr> networks)
      override;

  // MojoServiceEventsObserverBase:
  void SetReportingEnabled(bool is_enabled) override;

 protected:
  void AddObserver() override;

 private:
  void CheckForSignalStrengthEvent(const ash::NetworkState* network_state);

  void OnSignalStrengthChangedRssiValueReceived(
      const std::string& guid,
      const std::string& service_path,
      base::flat_map<std::string, int> service_path_rssi_map);

  SEQUENCE_CHECKER(sequence_checker_);

  bool low_signal_reported_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // Last reported connection state change data.
  absl::optional<std::string> last_reported_connection_guid_
      GUARDED_BY_CONTEXT(sequence_checker_);
  absl::optional<::chromeos::network_health::mojom::NetworkState>
      last_reported_connection_state_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<NetworkEventsObserver> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_EVENTS_OBSERVER_H_
