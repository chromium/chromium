// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_TRAFFIC_COUNTERS_HANDLER_H_
#define CHROME_BROWSER_ASH_NET_TRAFFIC_COUNTERS_HANDLER_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

namespace traffic_counters {

// TrafficCountersHandler is a singleton, owned by ChromeBrowserMainPartsAsh.
// This class handles automatically resetting traffic counters in Shill on a
// date specified by the user. User specified auto reset days that are too
// large for a given month occur on the last day of that month. For example, if
// the user specified day was 29, the actual day of reset for the month of
// February would be February 28th on non-leap years and February 29th on leap
// years. Similarly, if the user specified day was 31, the actual day of reset
// for April would be April 30th.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) TrafficCountersHandler
    : public chromeos::network_config::CrosNetworkConfigObserver {
 public:
  using TimeGetter = base::RepeatingCallback<base::Time()>;

  TrafficCountersHandler();
  TrafficCountersHandler(const TrafficCountersHandler&) = delete;
  TrafficCountersHandler& operator=(const TrafficCountersHandler&) = delete;
  ~TrafficCountersHandler() override;

  // Runs a check to determine whether traffic counters must be reset and starts
  // a timer to do so periodically.
  void Start();

  // CrosNetworkConfigObserver
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          active_networks) override;

  void SetTimeGetterForTest(TimeGetter time_getter);
  void RunForTesting();

 private:
  void RunAll();
  void RunWithFilter(chromeos::network_config::mojom::FilterType filter_type);
  void OnNetworkStateListReceived(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);
  bool GetAutoResetEnabled(std::string guid);
  void OnManagedPropertiesReceived(
      std::string guid,
      chromeos::network_config::mojom::ManagedPropertiesPtr managed_properties);
  bool ShouldReset(std::string guid, base::Time last_reset_time);
  base::Time GetExpectedLastResetTime(
      const base::Time::Exploded& current_time_exploded,
      int user_specified_reset_day);

  // Callback used to get the time. Mocked out in tests.
  TimeGetter time_getter_;
  // Timer used to set the interval at which to run auto reset checks.
  std::unique_ptr<base::RepeatingTimer> timer_;
  // Remote for sending requests to the CrosNetworkConfig service.
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  // Receiver for the CrosNetworkConfigObserver events.
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};

  base::WeakPtrFactory<TrafficCountersHandler> weak_ptr_factory_{this};
};

}  // namespace traffic_counters

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_TRAFFIC_COUNTERS_HANDLER_H_
