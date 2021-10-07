// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_HTTPS_LATENCY_SAMPLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_HTTPS_LATENCY_SAMPLER_H_

#include "base/callback.h"
#include "base/containers/queue.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "components/reporting/metrics/sampler.h"

namespace chromeos {
namespace network_diagnostics {

class HttpsLatencyRoutine;

}  // namespace network_diagnostics
}  // namespace chromeos

namespace reporting {

using HttpsLatencyRoutineGetter = base::RepeatingCallback<
    std::unique_ptr<chromeos::network_diagnostics::HttpsLatencyRoutine>()>;

// `HttpsLatencySampler` collects a sample of the current network latency by
// invoking the `HttpsLatencyRoutine` and parsing its results, no info is
// collected by this sampler only telemetry is collected.
class HttpsLatencySampler : public Sampler {
  using HttpsLatencyRoutine =
      chromeos::network_diagnostics::HttpsLatencyRoutine;
  using RoutineResultPtr =
      chromeos::network_diagnostics::mojom::RoutineResultPtr;

 public:
  HttpsLatencySampler();

  HttpsLatencySampler(const HttpsLatencySampler&) = delete;
  HttpsLatencySampler& operator=(const HttpsLatencySampler&) = delete;

  ~HttpsLatencySampler() override;

  void CollectTelemetry(TelemetryCallback callback) override;

  void SetHttpsLatencyRoutineGetterForTest(
      HttpsLatencyRoutineGetter https_latency_routine_getter);

 private:
  void OnHttpsLatencyRoutineCompleted(RoutineResultPtr routine_result);

  bool is_routine_running_ = false;

  HttpsLatencyRoutineGetter https_latency_routine_getter_;
  std::unique_ptr<HttpsLatencyRoutine> https_latency_routine_;
  base::queue<TelemetryCallback> telemetry_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HttpsLatencySampler> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_HTTPS_LATENCY_SAMPLER_H_
