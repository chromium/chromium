// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/remote_diagnostic_routines_service_strategy.h"

#include <memory>

#include "base/notreached.h"
#include "chromeos/ash/components/telemetry_extension/routines/telemetry_diagnostic_routine_service_ash.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

class RemoteDiagnosticRoutineServiceStrategyAsh
    : public RemoteDiagnosticRoutineServiceStrategy {
 public:
  RemoteDiagnosticRoutineServiceStrategyAsh()
      : routine_service_(
            ash::TelemetryDiagnosticsRoutineServiceAsh::Factory::Create(
                remote_diagnostic_service_.BindNewPipeAndPassReceiver())) {}

  ~RemoteDiagnosticRoutineServiceStrategyAsh() override = default;

  // `RemoteDiagnosticRoutineServiceStrategy`:
  mojo::Remote<crosapi::TelemetryDiagnosticRoutinesService>& GetRemoteService()
      override {
    return remote_diagnostic_service_;
  }

 private:
  mojo::Remote<crosapi::TelemetryDiagnosticRoutinesService>
      remote_diagnostic_service_;

  std::unique_ptr<crosapi::TelemetryDiagnosticRoutinesService> routine_service_;
};

}  // namespace

// static
std::unique_ptr<RemoteDiagnosticRoutineServiceStrategy>
RemoteDiagnosticRoutineServiceStrategy::Create() {
  return std::make_unique<RemoteDiagnosticRoutineServiceStrategyAsh>();
}

RemoteDiagnosticRoutineServiceStrategy::
    RemoteDiagnosticRoutineServiceStrategy() = default;
RemoteDiagnosticRoutineServiceStrategy::
    ~RemoteDiagnosticRoutineServiceStrategy() = default;

}  // namespace chromeos
