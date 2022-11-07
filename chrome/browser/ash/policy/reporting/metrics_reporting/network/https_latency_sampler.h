// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_HTTPS_LATENCY_SAMPLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_HTTPS_LATENCY_SAMPLER_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "components/reporting/metrics/sampler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace reporting {

// `HttpsLatencySampler` collects a sample of the current network latency by
// running https latency test of `NetworkDiagnosticsRoutines` parsing its
// results. A single `HttpsLatencySampler` instance can be shared between
// multiple consumers, and overlapping `Collect` calls will share the same
// routine run and result.
class HttpsLatencySampler : public Sampler {
 public:
  class Delegate {
   public:
    Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    virtual void BindDiagnosticsReceiver(
        mojo::PendingReceiver<
            ::chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
            receiver);
  };

  // Default arg is used in prod and set in test.
  explicit HttpsLatencySampler(
      std::unique_ptr<Delegate> delegate = std::make_unique<Delegate>());

  HttpsLatencySampler(const HttpsLatencySampler&) = delete;
  HttpsLatencySampler& operator=(const HttpsLatencySampler&) = delete;

  ~HttpsLatencySampler() override;

  void MaybeCollect(OptionalMetricCallback callback) override;

 private:
  void OnHttpsLatencyRoutineCompleted(
      ::chromeos::network_diagnostics::mojom::RoutineResultPtr routine_result);

  bool is_routine_running_ = false;

  mojo::Remote<
      ::chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
      network_diagnostics_service_;
  base::queue<OptionalMetricCallback> metric_callbacks_;

  const std::unique_ptr<Delegate> delegate_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HttpsLatencySampler> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_HTTPS_LATENCY_SAMPLER_H_
