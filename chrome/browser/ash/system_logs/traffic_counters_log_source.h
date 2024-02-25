// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_TRAFFIC_COUNTERS_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_TRAFFIC_COUNTERS_LOG_SOURCE_H_

#include <map>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace system_logs {

// Gathers traffic counters information for system logs/feedback reports.
class TrafficCountersLogSource : public SystemLogsSource {
 public:
  TrafficCountersLogSource();
  TrafficCountersLogSource(const TrafficCountersLogSource&) = delete;
  TrafficCountersLogSource& operator=(const TrafficCountersLogSource&) = delete;
  ~TrafficCountersLogSource() override;

  // SystemLogsSource:
  void Fetch(SysLogsSourceCallback request) override;

 private:
  void OnGetRecentlyActiveNetworks(const std::vector<std::string>& guids);

  void OnTrafficCountersReceived(
      const std::string& guid,
      std::vector<chromeos::network_config::mojom::TrafficCounterPtr>
          traffic_counters);

  void OnGetManagedProperties(
      const std::string& guid,
      std::vector<chromeos::network_config::mojom::TrafficCounterPtr>
          traffic_counters,
      chromeos::network_config::mojom::ManagedPropertiesPtr managed_properties);

  void SendResponseIfDone();

  SysLogsSourceCallback callback_;

  mojo::Remote<chromeos::network_health::mojom::NetworkHealthService>
      network_health_service_;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;

  int total_guids_ = 0;
  base::Value::Dict traffic_counters_;

  base::WeakPtrFactory<TrafficCountersLogSource> weak_factory_{this};
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_TRAFFIC_COUNTERS_LOG_SOURCE_H_
