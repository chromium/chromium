// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"

namespace chromeos {

// DiagnosticsApiFunctionBase --------------------------------------------------

DiagnosticsApiFunctionBase::DiagnosticsApiFunctionBase()
    : diagnostics_service_(
          remote_diagnostics_service_.BindNewPipeAndPassReceiver()) {}
DiagnosticsApiFunctionBase::~DiagnosticsApiFunctionBase() = default;

// getAvailableRoutines --------------------------------------------------------

namespace {

bool ConvertMojoRoutine(ash::health::mojom::DiagnosticRoutineEnum in,
                        api::os_diagnostics::RoutineType* out) {
  DCHECK(out);
  switch (in) {
    case ash::health::mojom::DiagnosticRoutineEnum::kBatteryCapacity:
      *out = api::os_diagnostics::RoutineType::ROUTINE_TYPE_BATTERY_CAPACITY;
      return true;
    case ash::health::mojom::DiagnosticRoutineEnum::kBatteryCharge:
      *out = api::os_diagnostics::RoutineType::ROUTINE_TYPE_BATTERY_CHARGE;
      return true;
    case ash::health::mojom::DiagnosticRoutineEnum::kBatteryDischarge:
      *out = api::os_diagnostics::RoutineType::ROUTINE_TYPE_BATTERY_DISCHARGE;
      return true;
    case ash::health::mojom::DiagnosticRoutineEnum::kBatteryHealth:
      *out = api::os_diagnostics::RoutineType::ROUTINE_TYPE_BATTERY_HEALTH;
      return true;
    case ash::health::mojom::DiagnosticRoutineEnum::kCpuStress:
      *out = api::os_diagnostics::RoutineType::ROUTINE_TYPE_CPU_STRESS;
      return true;
    default:
      return false;
  }
}

}  // namespace

OsDiagnosticsGetAvailableRoutinesFunction::
    OsDiagnosticsGetAvailableRoutinesFunction() = default;
OsDiagnosticsGetAvailableRoutinesFunction::
    ~OsDiagnosticsGetAvailableRoutinesFunction() = default;

ExtensionFunction::ResponseAction
OsDiagnosticsGetAvailableRoutinesFunction::Run() {
  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
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
    if (ConvertMojoRoutine(in, &out)) {
      result.routines.push_back(out);
    }
  }

  Respond(ArgumentList(
      api::os_diagnostics::GetAvailableRoutines::Results::Create(result)));
}

// DiagnosticsApiRunRoutineFunctionBase ----------------------------------------

namespace {

api::os_diagnostics::RoutineStatus ConvertRoutineStatus(
    ash::health::mojom::DiagnosticRoutineStatusEnum status) {
  namespace health = ::ash::health;
  switch (status) {
    case ash::health::mojom::DiagnosticRoutineStatusEnum::kReady:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_READY;
    case ash::health::mojom::DiagnosticRoutineStatusEnum::kRunning:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_RUNNING;
    case ash::health::mojom::DiagnosticRoutineStatusEnum::kWaiting:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_WAITING;
    case ash::health::mojom::DiagnosticRoutineStatusEnum::kPassed:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_PASSED;
    case ash::health::mojom::DiagnosticRoutineStatusEnum::kFailed:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_FAILED;
    case ash::health::mojom::DiagnosticRoutineStatusEnum::kError:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_ERROR;
    case ash::health::mojom::DiagnosticRoutineStatusEnum::kCancelled:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_CANCELLED;
    case ash::health::mojom::DiagnosticRoutineStatusEnum::kFailedToStart:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_FAILED_TO_START;
    case ash::health::mojom::DiagnosticRoutineStatusEnum::kRemoved:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_REMOVED;
    case ash::health::mojom::DiagnosticRoutineStatusEnum::kCancelling:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_CANCELLING;
    case ash::health::mojom::DiagnosticRoutineStatusEnum::kUnsupported:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_UNSUPPORTED;
    case ash::health::mojom::DiagnosticRoutineStatusEnum::kNotRun:
      return api::os_diagnostics::RoutineStatus::ROUTINE_STATUS_NOT_RUN;
  }
}

}  // namespace

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
  result.status = ConvertRoutineStatus(ptr->status);
  Respond(OneArgument(base::Value::FromUniquePtrValue(result.ToValue())));
}

// runBatteryCapacityRoutine ---------------------------------------------------

OsDiagnosticsRunBatteryCapacityRoutineFunction::
    OsDiagnosticsRunBatteryCapacityRoutineFunction() = default;
OsDiagnosticsRunBatteryCapacityRoutineFunction::
    ~OsDiagnosticsRunBatteryCapacityRoutineFunction() = default;

ExtensionFunction::ResponseAction
OsDiagnosticsRunBatteryCapacityRoutineFunction::Run() {
  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunBatteryCapacityRoutine(std::move(cb));

  return RespondLater();
}

// runBatteryChargeRoutine -----------------------------------------------------

OsDiagnosticsRunBatteryChargeRoutineFunction::
    OsDiagnosticsRunBatteryChargeRoutineFunction() = default;
OsDiagnosticsRunBatteryChargeRoutineFunction::
    ~OsDiagnosticsRunBatteryChargeRoutineFunction() = default;

ExtensionFunction::ResponseAction
OsDiagnosticsRunBatteryChargeRoutineFunction::Run() {
  std::unique_ptr<api::os_diagnostics::RunBatteryChargeRoutine::Params> params(
      api::os_diagnostics::RunBatteryChargeRoutine::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunBatteryChargeRoutine(
      params->request.length_seconds,
      params->request.minimum_charge_percent_required, std::move(cb));

  return RespondLater();
}

// runBatteryDischargeRoutine --------------------------------------------------

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

  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunBatteryDischargeRoutine(
      params->request.length_seconds,
      params->request.maximum_discharge_percent_allowed, std::move(cb));

  return RespondLater();
}

// runBatteryHealthRoutine -----------------------------------------------------

OsDiagnosticsRunBatteryHealthRoutineFunction::
    OsDiagnosticsRunBatteryHealthRoutineFunction() = default;
OsDiagnosticsRunBatteryHealthRoutineFunction::
    ~OsDiagnosticsRunBatteryHealthRoutineFunction() = default;

ExtensionFunction::ResponseAction
OsDiagnosticsRunBatteryHealthRoutineFunction::Run() {
  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunBatteryHealthRoutine(std::move(cb));

  return RespondLater();
}

// runCpuStressRoutine ---------------------------------------------------------

OsDiagnosticsRunCpuStressRoutineFunction::
    OsDiagnosticsRunCpuStressRoutineFunction() = default;
OsDiagnosticsRunCpuStressRoutineFunction::
    ~OsDiagnosticsRunCpuStressRoutineFunction() = default;

ExtensionFunction::ResponseAction
OsDiagnosticsRunCpuStressRoutineFunction::Run() {
  std::unique_ptr<api::os_diagnostics::RunCpuStressRoutine::Params> params(
      api::os_diagnostics::RunCpuStressRoutine::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunCpuStressRoutine(
      params->request.length_seconds, std::move(cb));

  return RespondLater();
}

}  // namespace chromeos
