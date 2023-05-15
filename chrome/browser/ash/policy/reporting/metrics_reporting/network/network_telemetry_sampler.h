// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_TELEMETRY_SAMPLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_TELEMETRY_SAMPLER_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

// NetworkTelemetrySampler collects the telemetry that describes the networks
// and connections states.
class NetworkTelemetrySampler : public Sampler {
 public:
  static NetworkConnectionState GetNetworkConnectionState(
      const ash::NetworkState* network);

  NetworkTelemetrySampler();

  NetworkTelemetrySampler(const NetworkTelemetrySampler&) = delete;
  NetworkTelemetrySampler& operator=(const NetworkTelemetrySampler&) = delete;

  ~NetworkTelemetrySampler() override;

  void MaybeCollect(OptionalMetricCallback callback) override;

 private:
  void CollectWifiSignalStrengthRssi(
      OptionalMetricCallback callback,
      ash::cros_healthd::mojom::TelemetryInfoPtr cros_healthd_telemetry);

  void CollectNetworksStates(
      OptionalMetricCallback callback,
      ash::cros_healthd::mojom::TelemetryInfoPtr cros_healthd_telemetry,
      base::flat_map<std::string, int> service_path_rssi_map);

  base::WeakPtrFactory<NetworkTelemetrySampler> weak_ptr_factory_{this};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_NETWORK_TELEMETRY_SAMPLER_H_
