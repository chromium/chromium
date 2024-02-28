// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_TRAFFIC_COUNTERS_HANDLER_H_
#define CHROME_BROWSER_ASH_NET_TRAFFIC_COUNTERS_HANDLER_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/network/network_metadata_observer.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class NetworkMetadataStore;

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
    : public chromeos::network_config::CrosNetworkConfigObserver,
      public NetworkMetadataObserver {
 public:
  using TimeGetter = base::RepeatingCallback<base::Time()>;

  TrafficCountersHandler();
  TrafficCountersHandler(const TrafficCountersHandler&) = delete;
  TrafficCountersHandler& operator=(const TrafficCountersHandler&) = delete;
  ~TrafficCountersHandler() override;

  // Runs a check to determine whether traffic counters must be reset and starts
  // a timer to do so periodically.
  void Start();

  // CrosNetworkConfigObserver:
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          active_networks) override;

  // NetworkMetadataObserver:
  void OnNetworkUpdate(const std::string& guid,
                       const base::Value::Dict* set_properties) override;

  void SetTimeGetterForTest(TimeGetter time_getter);
  void RunForTesting();

 private:
  void RunActive();
  void OnNetworkStateListReceived(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);
  void OnManagedPropertiesReceived(
      std::string guid,
      chromeos::network_config::mojom::ManagedPropertiesPtr managed_properties);
  // Returns a base::Time::Exploded object representing the current time. This
  // allows easy modification of the date values, which we can use to compare
  // different dates.
  base::Time::Exploded CurrentDateExploded();

  // Callback used to get the time. Mocked out in tests.
  TimeGetter time_getter_;
  // Timer used to set the interval at which to run auto reset checks.
  std::unique_ptr<base::RepeatingTimer> timer_;
  // Remote for sending requests to the CrosNetworkConfig service.
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  // Pointer to access network metadata.
  base::WeakPtr<NetworkMetadataStore> network_metadata_store_;
  // Receiver for the CrosNetworkConfigObserver events.
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};

  base::WeakPtrFactory<TrafficCountersHandler> weak_ptr_factory_{this};
};

}  // namespace traffic_counters

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_TRAFFIC_COUNTERS_HANDLER_H_
