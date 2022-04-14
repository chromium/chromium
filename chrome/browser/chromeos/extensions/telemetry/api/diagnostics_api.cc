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

void OsDiagnosticsGetAvailableRoutinesFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsDiagnosticsGetAvailableRoutinesFunction::OnResult,
                           this);

  remote_diagnostics_service_->GetAvailableRoutines(std::move(cb));
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

// OsDiagnosticsGetRoutineUpdateFunction ---------------------------------------

OsDiagnosticsGetRoutineUpdateFunction::OsDiagnosticsGetRoutineUpdateFunction() =
    default;
OsDiagnosticsGetRoutineUpdateFunction::
    ~OsDiagnosticsGetRoutineUpdateFunction() = default;

void OsDiagnosticsGetRoutineUpdateFunction::RunIfAllowed() {
  std::unique_ptr<api::os_diagnostics::GetRoutineUpdate::Params> params(
      api::os_diagnostics::GetRoutineUpdate::Params::Create(args()));
  if (!params) {
    SetBadMessage();
    Respond(BadMessage());
    return;
  }

  auto cb =
      base::BindOnce(&OsDiagnosticsGetRoutineUpdateFunction::OnResult, this);

  remote_diagnostics_service_->GetRoutineUpdate(
      params->request.id,
      converters::ConvertRoutineCommand(params->request.command),
      /* include_output= */ true, std::move(cb));
}

void OsDiagnosticsGetRoutineUpdateFunction::OnResult(
    ash::health::mojom::RoutineUpdatePtr ptr) {
  if (!ptr) {
    // |ptr| should never be null, otherwise Mojo validation will fail.
    // However it's safer to handle it in case of API changes.
    Respond(Error("API internal error"));
    return;
  }

  api::os_diagnostics::GetRoutineUpdateResponse result;
  result.progress_percent = ptr->progress_percent;

  if (ptr->output.has_value() && !ptr->output.value().empty()) {
    result.output =
        std::make_unique<std::string>(std::move(ptr->output.value()));
  }

  switch (ptr->routine_update_union->which()) {
    case ash::health::mojom::RoutineUpdateUnion::Tag::NONINTERACTIVE_UPDATE: {
      auto& routine_update =
          ptr->routine_update_union->get_noninteractive_update();
      result.status = converters::ConvertRoutineStatus(routine_update->status);
      result.status_message = std::move(routine_update->status_message);
      break;
    }
    case ash::health::mojom::RoutineUpdateUnion::Tag::INTERACTIVE_UPDATE:
      // Routine is waiting for user action. Set the status to waiting.
      result.status = api::os_diagnostics::RoutineStatus::
          ROUTINE_STATUS_WAITING_USER_ACTION;
      result.status_message = "Waiting for user action. See user_message";
      result.user_message = converters::ConvertRoutineUserMessage(
          ptr->routine_update_union->get_interactive_update()->user_message);
      break;
  }

  Respond(ArgumentList(
      api::os_diagnostics::GetRoutineUpdate::Results::Create(result)));
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

void OsDiagnosticsRunBatteryCapacityRoutineFunction::RunIfAllowed() {
  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunBatteryCapacityRoutine(std::move(cb));
}

// OsDiagnosticsRunBatteryChargeRoutineFunction --------------------------------

OsDiagnosticsRunBatteryChargeRoutineFunction::
    OsDiagnosticsRunBatteryChargeRoutineFunction() = default;
OsDiagnosticsRunBatteryChargeRoutineFunction::
    ~OsDiagnosticsRunBatteryChargeRoutineFunction() = default;

void OsDiagnosticsRunBatteryChargeRoutineFunction::RunIfAllowed() {
  std::unique_ptr<api::os_diagnostics::RunBatteryChargeRoutine::Params> params(
      api::os_diagnostics::RunBatteryChargeRoutine::Params::Create(args()));
  if (!params) {
    SetBadMessage();
    Respond(BadMessage());
    return;
  }

  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunBatteryChargeRoutine(
      params->request.length_seconds,
      params->request.minimum_charge_percent_required, std::move(cb));
}

// OsDiagnosticsRunBatteryDischargeRoutineFunction -----------------------------

OsDiagnosticsRunBatteryDischargeRoutineFunction::
    OsDiagnosticsRunBatteryDischargeRoutineFunction() = default;
OsDiagnosticsRunBatteryDischargeRoutineFunction::
    ~OsDiagnosticsRunBatteryDischargeRoutineFunction() = default;

void OsDiagnosticsRunBatteryDischargeRoutineFunction::RunIfAllowed() {
  std::unique_ptr<api::os_diagnostics::RunBatteryDischargeRoutine::Params>
      params(api::os_diagnostics::RunBatteryDischargeRoutine::Params::Create(
          args()));
  if (!params) {
    SetBadMessage();
    Respond(BadMessage());
    return;
  }

  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunBatteryDischargeRoutine(
      params->request.length_seconds,
      params->request.maximum_discharge_percent_allowed, std::move(cb));
}

// OsDiagnosticsRunBatteryHealthRoutineFunction --------------------------------

OsDiagnosticsRunBatteryHealthRoutineFunction::
    OsDiagnosticsRunBatteryHealthRoutineFunction() = default;
OsDiagnosticsRunBatteryHealthRoutineFunction::
    ~OsDiagnosticsRunBatteryHealthRoutineFunction() = default;

void OsDiagnosticsRunBatteryHealthRoutineFunction::RunIfAllowed() {
  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunBatteryHealthRoutine(std::move(cb));
}

// OsDiagnosticsRunCpuCacheRoutineFunction -------------------------------------

OsDiagnosticsRunCpuCacheRoutineFunction::
    OsDiagnosticsRunCpuCacheRoutineFunction() = default;
OsDiagnosticsRunCpuCacheRoutineFunction::
    ~OsDiagnosticsRunCpuCacheRoutineFunction() = default;

void OsDiagnosticsRunCpuCacheRoutineFunction::RunIfAllowed() {
  std::unique_ptr<api::os_diagnostics::RunCpuCacheRoutine::Params> params(
      api::os_diagnostics::RunCpuCacheRoutine::Params::Create(args()));
  if (!params) {
    SetBadMessage();
    Respond(BadMessage());
    return;
  }

  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunCpuCacheRoutine(
      params->request.length_seconds, std::move(cb));
}

// OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction ---------------------

OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction::
    OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction() = default;
OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction::
    ~OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction() = default;

void OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction::RunIfAllowed() {
  std::unique_ptr<
      api::os_diagnostics::RunCpuFloatingPointAccuracyRoutine::Params>
      params(api::os_diagnostics::RunCpuFloatingPointAccuracyRoutine::Params::
                 Create(args()));
  if (!params) {
    SetBadMessage();
    Respond(BadMessage());
    return;
  }

  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunFloatingPointAccuracyRoutine(
      params->request.length_seconds, std::move(cb));
}

// OsDiagnosticsRunCpuPrimeSearchRoutineFunction -------------------------------

OsDiagnosticsRunCpuPrimeSearchRoutineFunction::
    OsDiagnosticsRunCpuPrimeSearchRoutineFunction() = default;
OsDiagnosticsRunCpuPrimeSearchRoutineFunction::
    ~OsDiagnosticsRunCpuPrimeSearchRoutineFunction() = default;

void OsDiagnosticsRunCpuPrimeSearchRoutineFunction::RunIfAllowed() {
  std::unique_ptr<api::os_diagnostics::RunCpuPrimeSearchRoutine::Params> params(
      api::os_diagnostics::RunCpuPrimeSearchRoutine::Params::Create(args()));
  if (!params) {
    SetBadMessage();
    Respond(BadMessage());
    return;
  }

  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunPrimeSearchRoutine(
      params->request.length_seconds, std::move(cb));
}

// OsDiagnosticsRunCpuStressRoutineFunction ------------------------------------

OsDiagnosticsRunCpuStressRoutineFunction::
    OsDiagnosticsRunCpuStressRoutineFunction() = default;
OsDiagnosticsRunCpuStressRoutineFunction::
    ~OsDiagnosticsRunCpuStressRoutineFunction() = default;

void OsDiagnosticsRunCpuStressRoutineFunction::RunIfAllowed() {
  std::unique_ptr<api::os_diagnostics::RunCpuStressRoutine::Params> params(
      api::os_diagnostics::RunCpuStressRoutine::Params::Create(args()));
  if (!params) {
    SetBadMessage();
    Respond(BadMessage());
    return;
  }

  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunCpuStressRoutine(
      params->request.length_seconds, std::move(cb));
}

// OsDiagnosticsRunDiskReadRoutineFunction -------------------------------------

OsDiagnosticsRunDiskReadRoutineFunction::
    OsDiagnosticsRunDiskReadRoutineFunction() = default;
OsDiagnosticsRunDiskReadRoutineFunction::
    ~OsDiagnosticsRunDiskReadRoutineFunction() = default;

void OsDiagnosticsRunDiskReadRoutineFunction::RunIfAllowed() {
  std::unique_ptr<api::os_diagnostics::RunDiskReadRoutine::Params> params(
      api::os_diagnostics::RunDiskReadRoutine::Params::Create(args()));
  if (!params) {
    SetBadMessage();
    Respond(BadMessage());
    return;
  }

  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunDiskReadRoutine(
      converters::ConvertDiskReadRoutineType(params->request.type),
      params->request.length_seconds, params->request.file_size_mb,
      std::move(cb));
}

// OsDiagnosticsRunLanConnectivityRoutineFunction ------------------------------

OsDiagnosticsRunLanConnectivityRoutineFunction::
    OsDiagnosticsRunLanConnectivityRoutineFunction() = default;
OsDiagnosticsRunLanConnectivityRoutineFunction::
    ~OsDiagnosticsRunLanConnectivityRoutineFunction() = default;

void OsDiagnosticsRunLanConnectivityRoutineFunction::RunIfAllowed() {
  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunLanConnectivityRoutine(std::move(cb));
}

// OsDiagnosticsRunMemoryRoutineFunction ---------------------------------------

OsDiagnosticsRunMemoryRoutineFunction::OsDiagnosticsRunMemoryRoutineFunction() =
    default;
OsDiagnosticsRunMemoryRoutineFunction::
    ~OsDiagnosticsRunMemoryRoutineFunction() = default;

void OsDiagnosticsRunMemoryRoutineFunction::RunIfAllowed() {
  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunMemoryRoutine(std::move(cb));
}

// OsDiagnosticsRunNvmeWearLevelRoutineFunction --------------------------------

OsDiagnosticsRunNvmeWearLevelRoutineFunction::
    OsDiagnosticsRunNvmeWearLevelRoutineFunction() = default;
OsDiagnosticsRunNvmeWearLevelRoutineFunction::
    ~OsDiagnosticsRunNvmeWearLevelRoutineFunction() = default;

void OsDiagnosticsRunNvmeWearLevelRoutineFunction::RunIfAllowed() {
  std::unique_ptr<api::os_diagnostics::RunNvmeWearLevelRoutine::Params> params(
      api::os_diagnostics::RunNvmeWearLevelRoutine::Params::Create(args()));
  if (!params) {
    SetBadMessage();
    Respond(BadMessage());
    return;
  }

  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunNvmeWearLevelRoutine(
      params->request.wear_level_threshold, std::move(cb));
}

// OsDiagnosticsRunSmartctlCheckRoutineFunction --------------------------------

OsDiagnosticsRunSmartctlCheckRoutineFunction::
    OsDiagnosticsRunSmartctlCheckRoutineFunction() = default;
OsDiagnosticsRunSmartctlCheckRoutineFunction::
    ~OsDiagnosticsRunSmartctlCheckRoutineFunction() = default;

void OsDiagnosticsRunSmartctlCheckRoutineFunction::RunIfAllowed() {
  auto cb =
      base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);

  remote_diagnostics_service_->RunSmartctlCheckRoutine(std::move(cb));
}

}  // namespace chromeos
