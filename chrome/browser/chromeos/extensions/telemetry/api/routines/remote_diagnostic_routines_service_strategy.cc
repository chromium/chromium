// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/remote_diagnostic_routines_service_strategy.h"

#include <memory>

#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/telemetry_extension/routines/telemetry_diagnostic_routine_service_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG (IS_CHROMEOS_LACROS)

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class RemoteDiagnosticRoutineServiceStrategyLacros
    : public RemoteDiagnosticRoutineServiceStrategy {
 public:
  RemoteDiagnosticRoutineServiceStrategyLacros() = default;

  ~RemoteDiagnosticRoutineServiceStrategyLacros() override = default;

  // `RemoteDiagnosticRoutineServiceStrategy`:
  mojo::Remote<crosapi::TelemetryDiagnosticRoutinesService>& GetRemoteService()
      override {
    return LacrosService::Get()
        ->GetRemote<crosapi::TelemetryDiagnosticRoutinesService>();
  }
};
#endif  // BUILDFLAG (IS_CHROMEOS_LACROS)

}  // namespace

// static
std::unique_ptr<RemoteDiagnosticRoutineServiceStrategy>
RemoteDiagnosticRoutineServiceStrategy::Create() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<RemoteDiagnosticRoutineServiceStrategyAsh>();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!LacrosService::Get()
           ->IsAvailable<crosapi::TelemetryDiagnosticRoutinesService>()) {
    return nullptr;
  }
  return std::make_unique<RemoteDiagnosticRoutineServiceStrategyLacros>();
#else  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  NOTREACHED();
#endif
}

RemoteDiagnosticRoutineServiceStrategy::
    RemoteDiagnosticRoutineServiceStrategy() = default;
RemoteDiagnosticRoutineServiceStrategy::
    ~RemoteDiagnosticRoutineServiceStrategy() = default;

}  // namespace chromeos
