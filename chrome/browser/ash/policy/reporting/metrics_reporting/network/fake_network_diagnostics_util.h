// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_FAKE_NETWORK_DIAGNOSTICS_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_FAKE_NETWORK_DIAGNOSTICS_UTIL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/https_latency_sampler.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"

using ::ash::network_diagnostics::NetworkDiagnostics;
using ::chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines;
using ::chromeos::network_diagnostics::mojom::RoutineResult;

using HttpsLatencyProblemMojom =
    ::chromeos::network_diagnostics::mojom::HttpsLatencyProblem;
using RoutineCallSourceMojom =
    ::chromeos::network_diagnostics::mojom::RoutineCallSource;

namespace reporting {

class FakeNetworkDiagnostics : public NetworkDiagnostics {
 public:
  FakeNetworkDiagnostics();

  FakeNetworkDiagnostics(const FakeNetworkDiagnostics&) = delete;

  FakeNetworkDiagnostics& operator=(const FakeNetworkDiagnostics&) = delete;

  ~FakeNetworkDiagnostics() override;

  void RunHttpsLatency(
      std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
          source,
      RunHttpsLatencyCallback callback) override;

  void ExecuteCallback();

  void SetReceiver(
      mojo::PendingReceiver<NetworkDiagnosticsRoutines> pending_receiver);

  void SetResultNoProblem(int latency_ms);

  void SetResultProblem(HttpsLatencyProblemMojom problem);

  void SetResultProblemLatency(HttpsLatencyProblemMojom problem,
                               int latency_ms);

 private:
  RoutineResult routine_result_;

  std::unique_ptr<mojo::Receiver<NetworkDiagnosticsRoutines>> receiver_;

  RunHttpsLatencyCallback callback_;

  ash::FakeDebugDaemonClient fake_debug_daemon_client_;
};

class FakeHttpsLatencyDelegate : public HttpsLatencySampler::Delegate {
 public:
  explicit FakeHttpsLatencyDelegate(FakeNetworkDiagnostics* fake_diagnostics)
      : fake_diagnostics_(fake_diagnostics) {}

  FakeHttpsLatencyDelegate(const FakeHttpsLatencyDelegate&) = delete;
  FakeHttpsLatencyDelegate& operator=(const FakeHttpsLatencyDelegate&) = delete;
  ~FakeHttpsLatencyDelegate() override = default;

  void BindDiagnosticsReceiver(mojo::PendingReceiver<NetworkDiagnosticsRoutines>
                                   pending_receiver) override;

 private:
  const raw_ptr<FakeNetworkDiagnostics> fake_diagnostics_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_FAKE_NETWORK_DIAGNOSTICS_UTIL_H_
