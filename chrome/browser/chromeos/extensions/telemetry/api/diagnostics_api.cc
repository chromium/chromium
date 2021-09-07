// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics_api_converters.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"

namespace chromeos {

// DiagnosticsApiFunctionBase --------------------------------------------------

DiagnosticsApiFunctionBase::DiagnosticsApiFunctionBase()
    : diagnostics_service_(
          remote_diagnostics_service_.BindNewPipeAndPassReceiver()) {}
DiagnosticsApiFunctionBase::~DiagnosticsApiFunctionBase() = default;

// OsDiagnosticsGetAvailableRoutinesFunction -----------------------------------

OsDiagnosticsGetAvailableRoutinesFunction::
    OsDiagnosticsGetAvailableRoutinesFunction() = default;
OsDiagnosticsGetAvailableRoutinesFunction::
    ~OsDiagnosticsGetAvailableRoutinesFunction() = default;

ExtensionFunction::ResponseAction
OsDiagnosticsGetAvailableRoutinesFunction::Run() {
  auto cb = base::BindOnce(&OsDiagnosticsGetAvailableRoutinesFunction::OnResult,
                           this);

  remote_diagnostics_service_->GetAvailableRoutines(std::move(cb));

  return RespondLater();
}

void OsDiagnosticsGetAvailableRoutinesFunction::OnResult(
    const std::vector<ash::health::mojom::DiagnosticRoutineEnum>& routines) {
  api::os_diagnostics::GetAvailableRoutinesResponse result;
  for (const auto in : routines) {
    api::os_diagnostics::RoutineType out;
    if (converters::ConvertMojoRoutine(in, &out)) {
      result.routines.push_back(out);
    }
  }

  Respond(ArgumentList(
      api::os_diagnostics::GetAvailableRoutines::Results::Create(result)));
}

// DiagnosticsApiRunRoutineFunctionBase ----------------------------------------

DiagnosticsApiRunRoutineFunctionBase::DiagnosticsApiRunRoutineFunctionBase() =
    default;
DiagnosticsApiRunRoutineFunctionBase::~DiagnosticsApiRunRoutineFunctionBase() =
    default;

void DiagnosticsApiRunRoutineFunctionBase::OnResult(
    ash::health::mojom::RunRoutineResponsePtr ptr) {
  if (!ptr) {
    // |ptr| should never be null, otherwise Mojo validation will fail.
    // However it's safer to handle it in case of API changes.
    Respond(Error("API internal error"));
    return;
  }

  api::os_diagnostics::RunRoutineResponse result;
  result.id = ptr->id;
  result.status = converters::ConvertRoutineStatus(ptr->status);
  Respond(OneArgument(base::Value::FromUniquePtrValue(result.ToValue())));
}

// OsDiagnosticsRunBatteryCapacityRoutineFunction ------------------------------

OsDiagnosticsRunBatteryCapacityRoutineFunction::
    OsDiagnosticsRunBatteryCapacityRoutineFunction() = default;
OsDiagnosticsRunBatteryCapacityRoutineFunction::
    ~OsDiagnosticsRunBatteryCapacityRoutineFunction() = default;

ExtensionFunction::ResponseAction
OsDiagnosticsRunBatteryCapacityRoutineFunction::Run() {
  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunBatteryCapacityRoutine(std::move(cb));

  return RespondLater();
}

// OsDiagnosticsRunBatteryChargeRoutineFunction --------------------------------

OsDiagnosticsRunBatteryChargeRoutineFunction::
    OsDiagnosticsRunBatteryChargeRoutineFunction() = default;
OsDiagnosticsRunBatteryChargeRoutineFunction::
    ~OsDiagnosticsRunBatteryChargeRoutineFunction() = default;

ExtensionFunction::ResponseAction
OsDiagnosticsRunBatteryChargeRoutineFunction::Run() {
  std::unique_ptr<api::os_diagnostics::RunBatteryChargeRoutine::Params> params(
      api::os_diagnostics::RunBatteryChargeRoutine::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunBatteryChargeRoutine(
      params->request.length_seconds,
      params->request.minimum_charge_percent_required, std::move(cb));

  return RespondLater();
}

// OsDiagnosticsRunBatteryDischargeRoutineFunction -----------------------------

OsDiagnosticsRunBatteryDischargeRoutineFunction::
    OsDiagnosticsRunBatteryDischargeRoutineFunction() = default;
OsDiagnosticsRunBatteryDischargeRoutineFunction::
    ~OsDiagnosticsRunBatteryDischargeRoutineFunction() = default;

ExtensionFunction::ResponseAction
OsDiagnosticsRunBatteryDischargeRoutineFunction::Run() {
  std::unique_ptr<api::os_diagnostics::RunBatteryDischargeRoutine::Params>
      params(api::os_diagnostics::RunBatteryDischargeRoutine::Params::Create(
          args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunBatteryDischargeRoutine(
      params->request.length_seconds,
      params->request.maximum_discharge_percent_allowed, std::move(cb));

  return RespondLater();
}

// OsDiagnosticsRunBatteryHealthRoutineFunction --------------------------------

OsDiagnosticsRunBatteryHealthRoutineFunction::
    OsDiagnosticsRunBatteryHealthRoutineFunction() = default;
OsDiagnosticsRunBatteryHealthRoutineFunction::
    ~OsDiagnosticsRunBatteryHealthRoutineFunction() = default;

ExtensionFunction::ResponseAction
OsDiagnosticsRunBatteryHealthRoutineFunction::Run() {
  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunBatteryHealthRoutine(std::move(cb));

  return RespondLater();
}

// OsDiagnosticsRunCpuCacheRoutineFunction -------------------------------------

OsDiagnosticsRunCpuCacheRoutineFunction::
    OsDiagnosticsRunCpuCacheRoutineFunction() = default;
OsDiagnosticsRunCpuCacheRoutineFunction::
    ~OsDiagnosticsRunCpuCacheRoutineFunction() = default;

ExtensionFunction::ResponseAction
OsDiagnosticsRunCpuCacheRoutineFunction::Run() {
  std::unique_ptr<api::os_diagnostics::RunCpuCacheRoutine::Params> params(
      api::os_diagnostics::RunCpuCacheRoutine::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunCpuCacheRoutine(
      params->request.length_seconds, std::move(cb));

  return RespondLater();
}

// OsDiagnosticsRunCpuStressRoutineFunction ------------------------------------

OsDiagnosticsRunCpuStressRoutineFunction::
    OsDiagnosticsRunCpuStressRoutineFunction() = default;
OsDiagnosticsRunCpuStressRoutineFunction::
    ~OsDiagnosticsRunCpuStressRoutineFunction() = default;

ExtensionFunction::ResponseAction
OsDiagnosticsRunCpuStressRoutineFunction::Run() {
  std::unique_ptr<api::os_diagnostics::RunCpuStressRoutine::Params> params(
      api::os_diagnostics::RunCpuStressRoutine::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunCpuStressRoutine(
      params->request.length_seconds, std::move(cb));

  return RespondLater();
}

// OsDiagnosticsRunMemoryRoutineFunction ---------------------------------------

OsDiagnosticsRunMemoryRoutineFunction::OsDiagnosticsRunMemoryRoutineFunction() =
    default;
OsDiagnosticsRunMemoryRoutineFunction::
    ~OsDiagnosticsRunMemoryRoutineFunction() = default;

ExtensionFunction::ResponseAction OsDiagnosticsRunMemoryRoutineFunction::Run() {
  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunMemoryRoutine(std::move(cb));

  return RespondLater();
}

}  // namespace chromeos
