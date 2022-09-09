// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_EVENTS_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_EVENTS_OBSERVER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_events_observer_base.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"

namespace reporting {

class NetworkEventsObserver
    : public ::chromeos::network_health::mojom::NetworkEventsObserver,
      public CrosHealthdEventsObserverBase<
          ::chromeos::network_health::mojom::NetworkEventsObserver> {
 public:
  NetworkEventsObserver();

  NetworkEventsObserver(const NetworkEventsObserver&) = delete;
  NetworkEventsObserver& operator=(const NetworkEventsObserver&) = delete;

  ~NetworkEventsObserver() override;

  // ash::network_health::mojom::NetworkEventsObserver:
  void OnConnectionStateChanged(
      const std::string& guid,
      ::chromeos::network_health::mojom::NetworkState state) override;
  void OnSignalStrengthChanged(const std::string& guid,
                               ::chromeos::network_health::mojom::UInt32ValuePtr
                                   signal_strength) override;

 protected:
  void AddObserver() override;

 private:
  void OnSignalStrengthChangedRssiValueReceived(
      const std::string& guid,
      const std::string& service_path,
      int signal_strength_percent,
      base::flat_map<std::string, int> service_path_rssi_map);

  base::WeakPtrFactory<NetworkEventsObserver> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_EVENTS_OBSERVER_H_
