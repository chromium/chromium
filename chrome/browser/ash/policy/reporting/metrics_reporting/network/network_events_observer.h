// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_EVENTS_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_EVENTS_OBSERVER_H_

#include <string>

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
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_EVENTS_OBSERVER_H_
