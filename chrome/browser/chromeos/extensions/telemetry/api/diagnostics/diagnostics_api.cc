// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/diagnostics_api.h"

#include <cstddef>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/diagnostics_api_converters.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/remote_diagnostics_service_strategy.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/strings/stringprintf.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

namespace diag = api::os_diagnostics;

// DiagnosticsApiFunctionBase --------------------------------------------------

DiagnosticsApiFunctionBase::DiagnosticsApiFunctionBase()
    : remote_diagnostics_service_strategy_(
          RemoteDiagnosticsServiceStrategy::Create()) {}

DiagnosticsApiFunctionBase::~DiagnosticsApiFunctionBase() = default;

mojo::Remote<crosapi::mojom::DiagnosticsService>&
DiagnosticsApiFunctionBase::GetRemoteService() {
  DCHECK(remote_diagnostics_service_strategy_);
  return remote_diagnostics_service_strategy_->GetRemoteService();
}

template <class Params>
absl::optional<Params> DiagnosticsApiFunctionBase::GetParams() {
  auto params = Params::Create(args());
  if (!params) {
    SetBadMessage();
    Respond(BadMessage());
  }

  return params;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool DiagnosticsApiFunctionBase::IsCrosApiAvailable() {
  return remote_diagnostics_service_strategy_ != nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// OsDiagnosticsGetAvailableRoutinesFunction -----------------------------------

void OsDiagnosticsGetAvailableRoutinesFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsDiagnosticsGetAvailableRoutinesFunction::OnResult,
                           this);

  GetRemoteService()->GetAvailableRoutines(std::move(cb));
}

void OsDiagnosticsGetAvailableRoutinesFunction::OnResult(
    const std::vector<crosapi::mojom::DiagnosticsRoutineEnum>& routines) {
  diag::GetAvailableRoutinesResponse result;
  for (const auto in : routines) {
    diag::RoutineType out;
    if (converters::ConvertMojoRoutine(in, &out)) {
      result.routines.push_back(out);
    }
  }

  Respond(ArgumentList(diag::GetAvailableRoutines::Results::Create(result)));
}

// OsDiagnosticsGetRoutineUpdateFunction ---------------------------------------

void OsDiagnosticsGetRoutineUpdateFunction::RunIfAllowed() {
  const auto params = GetParams<diag::GetRoutineUpdate::Params>();
  if (!params) {
    return;
  }

  auto cb =
      base::BindOnce(&OsDiagnosticsGetRoutineUpdateFunction::OnResult, this);

  GetRemoteService()->GetRoutineUpdate(
      params->request.id,
      converters::ConvertRoutineCommand(params->request.command),
      /* include_output= */ true, std::move(cb));
}

void OsDiagnosticsGetRoutineUpdateFunction::OnResult(
    crosapi::mojom::DiagnosticsRoutineUpdatePtr ptr) {
  if (!ptr) {
    // |ptr| should never be null, otherwise Mojo validation will fail.
    // However it's safer to handle it in case of API changes.
    Respond(Error("API internal error"));
    return;
  }

  diag::GetRoutineUpdateResponse result;
  result.progress_percent = ptr->progress_percent;

  if (ptr->output.has_value() && !ptr->output.value().empty()) {
    result.output = std::move(ptr->output);
  }

  switch (ptr->routine_update_union->which()) {
    case crosapi::mojom::DiagnosticsRoutineUpdateUnion::Tag::
        kNoninteractiveUpdate: {
      auto& routine_update =
          ptr->routine_update_union->get_noninteractive_update();
      result.status = converters::ConvertRoutineStatus(routine_update->status);
      result.status_message = std::move(routine_update->status_message);
      break;
    }
    case crosapi::mojom::DiagnosticsRoutineUpdateUnion::Tag::kInteractiveUpdate:
      // Routine is waiting for user action. Set the status to waiting.
      result.status = diag::RoutineStatus::kWaitingUserAction;
      result.status_message = "Waiting for user action. See user_message";
      result.user_message = converters::ConvertRoutineUserMessage(
          ptr->routine_update_union->get_interactive_update()->user_message);
      break;
  }

  Respond(ArgumentList(diag::GetRoutineUpdate::Results::Create(result)));
}

// DiagnosticsApiRunRoutineFunctionBase ----------------------------------------

void DiagnosticsApiRunRoutineFunctionBase::OnResult(
    crosapi::mojom::DiagnosticsRunRoutineResponsePtr ptr) {
  if (!ptr) {
    // |ptr| should never be null, otherwise Mojo validation will fail.
    // However it's safer to handle it in case of API changes.
    Respond(Error("API internal error"));
    return;
  }

  diag::RunRoutineResponse result;
  result.id = ptr->id;
  result.status = converters::ConvertRoutineStatus(ptr->status);
  Respond(WithArguments(result.ToValue()));
}

base::OnceCallback<void(crosapi::mojom::DiagnosticsRunRoutineResponsePtr)>
DiagnosticsApiRunRoutineFunctionBase::GetOnResult() {
  return base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);
}

// OsDiagnosticsRunAcPowerRoutineFunction ------------------------------

void OsDiagnosticsRunAcPowerRoutineFunction::RunIfAllowed() {
  const auto params = GetParams<diag::RunAcPowerRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunAcPowerRoutine(
      converters::ConvertAcPowerStatusRoutineType(
          params->request.expected_status),
      params->request.expected_power_type, GetOnResult());
}

// OsDiagnosticsRunBatteryCapacityRoutineFunction ------------------------------
void OsDiagnosticsRunBatteryCapacityRoutineFunction::RunIfAllowed() {
  GetRemoteService()->RunBatteryCapacityRoutine(GetOnResult());
}

// OsDiagnosticsRunBatteryChargeRoutineFunction --------------------------------

void OsDiagnosticsRunBatteryChargeRoutineFunction::RunIfAllowed() {
  const auto params = GetParams<diag::RunBatteryChargeRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunBatteryChargeRoutine(
      params->request.length_seconds,
      params->request.minimum_charge_percent_required, GetOnResult());
}

// OsDiagnosticsRunBatteryDischargeRoutineFunction -----------------------------

void OsDiagnosticsRunBatteryDischargeRoutineFunction::RunIfAllowed() {
  const auto params = GetParams<diag::RunBatteryDischargeRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunBatteryDischargeRoutine(
      params->request.length_seconds,
      params->request.maximum_discharge_percent_allowed, GetOnResult());
}

// OsDiagnosticsRunBatteryHealthRoutineFunction --------------------------------

void OsDiagnosticsRunBatteryHealthRoutineFunction::RunIfAllowed() {
  GetRemoteService()->RunBatteryHealthRoutine(GetOnResult());
}

// OsDiagnosticsRunCpuCacheRoutineFunction -------------------------------------

void OsDiagnosticsRunCpuCacheRoutineFunction::RunIfAllowed() {
  const auto params = GetParams<diag::RunCpuCacheRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunCpuCacheRoutine(params->request.length_seconds,
                                         GetOnResult());
}

// OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction ---------------------

void OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction::RunIfAllowed() {
  const auto params =
      GetParams<diag::RunCpuFloatingPointAccuracyRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunFloatingPointAccuracyRoutine(
      params->request.length_seconds, GetOnResult());
}

// OsDiagnosticsRunCpuPrimeSearchRoutineFunction -------------------------------

void OsDiagnosticsRunCpuPrimeSearchRoutineFunction::RunIfAllowed() {
  const auto params = GetParams<diag::RunCpuPrimeSearchRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunPrimeSearchRoutine(params->request.length_seconds,
                                            GetOnResult());
}

// OsDiagnosticsRunCpuStressRoutineFunction ------------------------------------

void OsDiagnosticsRunCpuStressRoutineFunction::RunIfAllowed() {
  const auto params = GetParams<diag::RunCpuStressRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunCpuStressRoutine(params->request.length_seconds,
                                          GetOnResult());
}

// OsDiagnosticsRunDiskReadRoutineFunction -------------------------------------

void OsDiagnosticsRunDiskReadRoutineFunction::RunIfAllowed() {
  const auto params = GetParams<diag::RunDiskReadRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunDiskReadRoutine(
      converters::ConvertDiskReadRoutineType(params->request.type),
      params->request.length_seconds, params->request.file_size_mb,
      GetOnResult());
}

// OsDiagnosticsRunDnsResolutionRoutineFunction --------------------------------

void OsDiagnosticsRunDnsResolutionRoutineFunction::RunIfAllowed() {
  GetRemoteService()->RunDnsResolutionRoutine(GetOnResult());
}

// OsDiagnosticsRunDnsResolverPresentRoutineFunction ---------------------------
void OsDiagnosticsRunDnsResolverPresentRoutineFunction::RunIfAllowed() {
  GetRemoteService()->RunDnsResolverPresentRoutine(GetOnResult());
}

// OsDiagnosticsRunEmmcLifetimeRoutineFunction ---------------------------

void OsDiagnosticsRunEmmcLifetimeRoutineFunction::RunIfAllowed() {
  GetRemoteService()->RunEmmcLifetimeRoutine(GetOnResult());
}

// OsDiagnosticsRunGatewayCanBePingedRoutineFunction ---------------------------

void OsDiagnosticsRunGatewayCanBePingedRoutineFunction::RunIfAllowed() {
  GetRemoteService()->RunGatewayCanBePingedRoutine(GetOnResult());
}

// OsDiagnosticsRunFingerprintAliveRoutineFunction -----------------------------

void OsDiagnosticsRunFingerprintAliveRoutineFunction::RunIfAllowed() {
  GetRemoteService()->RunFingerprintAliveRoutine(GetOnResult());
}

// OsDiagnosticsRunLanConnectivityRoutineFunction ------------------------------

void OsDiagnosticsRunLanConnectivityRoutineFunction::RunIfAllowed() {
  GetRemoteService()->RunLanConnectivityRoutine(GetOnResult());
}

// OsDiagnosticsRunMemoryRoutineFunction ---------------------------------------

void OsDiagnosticsRunMemoryRoutineFunction::RunIfAllowed() {
  GetRemoteService()->RunMemoryRoutine(GetOnResult());
}

// OsDiagnosticsRunNvmeSelfTestRoutineFunction ---------------------------------

void OsDiagnosticsRunNvmeSelfTestRoutineFunction::RunIfAllowed() {
  auto params = GetParams<diag::RunNvmeSelfTestRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunNvmeSelfTestRoutine(
      converters::ConvertNvmeSelfTestRoutineType(std::move(params->request)),
      GetOnResult());
}

// OsDiagnosticsRunNvmeWearLevelRoutineFunction --------------------------------

void OsDiagnosticsRunNvmeWearLevelRoutineFunction::RunIfAllowed() {
  const auto params = GetParams<diag::RunNvmeWearLevelRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunNvmeWearLevelRoutine(
      params->request.wear_level_threshold, GetOnResult());
}

// OsDiagnosticsRunSensitiveSensorRoutineFunction -----------------------------

void OsDiagnosticsRunSensitiveSensorRoutineFunction::RunIfAllowed() {
  GetRemoteService()->RunSensitiveSensorRoutine(GetOnResult());
}

// OsDiagnosticsRunSignalStrengthRoutineFunction -------------------------------

void OsDiagnosticsRunSignalStrengthRoutineFunction::RunIfAllowed() {
  GetRemoteService()->RunSignalStrengthRoutine(GetOnResult());
}

// OsDiagnosticsRunSmartctlCheckRoutineFunction --------------------------------

void OsDiagnosticsRunSmartctlCheckRoutineFunction::RunIfAllowed() {
  absl::optional<api::os_diagnostics::RunSmartctlCheckRoutine::Params> params(
      api::os_diagnostics::RunSmartctlCheckRoutine::Params::Create(args()));

  crosapi::mojom::UInt32ValuePtr percentage_used;
  if (params && params->request && params->request->percentage_used_threshold) {
    percentage_used = crosapi::mojom::UInt32Value::New(
        params->request->percentage_used_threshold.value());
  }

  // Backwards compatibility: Calling the routine with an null parameter
  // results in the same behaviour as the former `RunSmartctlCheckRoutine`
  // without any parameters.
  GetRemoteService()->RunSmartctlCheckRoutine(std::move(percentage_used),
                                              GetOnResult());
}

}  // namespace chromeos
