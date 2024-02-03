// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/fake_network_diagnostics_util.h"

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/fake_network_diagnostics_util.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"

using ::ash::network_diagnostics::NetworkDiagnostics;
using ::chromeos::network_diagnostics::mojom::HttpsLatencyResultValue;
using ::chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines;
using ::chromeos::network_diagnostics::mojom::RoutineProblems;
using ::chromeos::network_diagnostics::mojom::RoutineResult;
using ::chromeos::network_diagnostics::mojom::RoutineResultValue;

using HttpsLatencyProblemMojom =
    ::chromeos::network_diagnostics::mojom::HttpsLatencyProblem;
using RoutineVerdictMojom =
    ::chromeos::network_diagnostics::mojom::RoutineVerdict;

namespace reporting {

FakeNetworkDiagnostics::FakeNetworkDiagnostics()
    : NetworkDiagnostics(&fake_debug_daemon_client_) {}

FakeNetworkDiagnostics::~FakeNetworkDiagnostics() = default;

void FakeNetworkDiagnostics::RunHttpsLatency(
    std::optional<chromeos::network_diagnostics::mojom::RoutineCallSource>
        source,
    RunHttpsLatencyCallback callback) {
  callback_ = std::move(callback);
}

void FakeNetworkDiagnostics::ExecuteCallback() {
  // Block until all previously posted tasks are executed to make sure
  // `RunHttpsLatency` is called and `callback_` is set.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  // callback_ is null for tests which report nothing.
  if (callback_.is_null()) {
    return;
  }
  std::move(callback_).Run(routine_result_.Clone());
}

void FakeNetworkDiagnostics::SetReceiver(
    mojo::PendingReceiver<NetworkDiagnosticsRoutines> pending_receiver) {
  receiver_ = std::make_unique<mojo::Receiver<NetworkDiagnosticsRoutines>>(
      this, std::move(pending_receiver));
}

void FakeNetworkDiagnostics::SetResultNoProblem(int latency_ms) {
  routine_result_.result_value = RoutineResultValue::NewHttpsLatencyResultValue(
      HttpsLatencyResultValue::New(base::Milliseconds(latency_ms)));
  routine_result_.verdict = RoutineVerdictMojom::kNoProblem;
  routine_result_.problems = RoutineProblems::NewHttpsLatencyProblems({});
}

void FakeNetworkDiagnostics::SetResultProblem(
    HttpsLatencyProblemMojom problem) {
  routine_result_.problems =
      RoutineProblems::NewHttpsLatencyProblems({problem});
  routine_result_.verdict = RoutineVerdictMojom::kProblem;
}

void FakeNetworkDiagnostics::SetResultProblemLatency(
    HttpsLatencyProblemMojom problem,
    int latency_ms) {
  routine_result_.result_value = RoutineResultValue::NewHttpsLatencyResultValue(
      HttpsLatencyResultValue::New(base::Milliseconds(latency_ms)));
  SetResultProblem(problem);
}

void FakeHttpsLatencyDelegate::BindDiagnosticsReceiver(
    mojo::PendingReceiver<NetworkDiagnosticsRoutines> pending_receiver) {
  fake_diagnostics_->SetReceiver(std::move(pending_receiver));
}

}  // namespace reporting
