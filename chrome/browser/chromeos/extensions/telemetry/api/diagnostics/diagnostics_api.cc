// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/diagnostics_api.h"

#include <cstddef>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/diagnostics_api_converters.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/diagnostics_api_metrics.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/remote_diagnostics_service_strategy.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_manager.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/permissions/permissions_data.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/strings/stringprintf.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

namespace {

namespace cx_diag = api::os_diagnostics;

base::expected<cx_diag::RoutineSupportStatusInfo, std::string>
ParseRoutineArgumentSupportResult(
    crosapi::mojom::TelemetryExtensionSupportStatusPtr result) {
  switch (result->which()) {
    case crosapi::mojom::TelemetryExtensionSupportStatus::Tag::
        kUnmappedUnionField:
      return base::unexpected("API internal error.");
    case crosapi::mojom::TelemetryExtensionSupportStatus::Tag::kException:
      return base::unexpected(result->get_exception()->debug_message);
    case crosapi::mojom::TelemetryExtensionSupportStatus::Tag::kSupported: {
      cx_diag::RoutineSupportStatusInfo info;
      info.status = cx_diag::RoutineSupportStatus::kSupported;

      return base::ok(std::move(info));
    }
    case crosapi::mojom::TelemetryExtensionSupportStatus::Tag::kUnsupported: {
      cx_diag::RoutineSupportStatusInfo info;
      info.status = cx_diag::RoutineSupportStatus::kUnsupported;

      return base::ok(std::move(info));
    }
  }
  NOTREACHED();
}

bool IsPendingApprovalRoutine(
    const crosapi::mojom::TelemetryDiagnosticRoutineArgumentPtr& arg) {
  return false;
}

}  // namespace

// DiagnosticsApiFunctionV1AndV2Base -------------------------------------------

template <class Params>
std::optional<Params> DiagnosticsApiFunctionV1AndV2Base::GetParams() {
  auto params = Params::Create(args());
  if (!params) {
    SetBadMessage();
    Respond(BadMessage());
  }

  return params;
}

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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool DiagnosticsApiFunctionBase::IsCrosApiAvailable() {
  return remote_diagnostics_service_strategy_ != nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// DiagnosticsApiFunctionBaseV2 ------------------------------------------------

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool DiagnosticsApiFunctionBaseV2::IsCrosApiAvailable() {
  return LacrosService::Get() &&
         LacrosService::Get()
             ->IsAvailable<
                 crosapi::mojom::TelemetryDiagnosticRoutinesService>();
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
  cx_diag::GetAvailableRoutinesResponse result;
  for (const auto in : routines) {
    cx_diag::RoutineType out;
    if (converters::diagnostics::ConvertMojoRoutine(in, &out)) {
      result.routines.push_back(out);
    }
  }

  Respond(ArgumentList(cx_diag::GetAvailableRoutines::Results::Create(result)));
}

// OsDiagnosticsGetRoutineUpdateFunction ---------------------------------------

void OsDiagnosticsGetRoutineUpdateFunction::RunIfAllowed() {
  const auto params = GetParams<cx_diag::GetRoutineUpdate::Params>();
  if (!params) {
    return;
  }

  auto cb =
      base::BindOnce(&OsDiagnosticsGetRoutineUpdateFunction::OnResult, this);

  GetRemoteService()->GetRoutineUpdate(
      params->request.id,
      converters::diagnostics::ConvertRoutineCommand(params->request.command),
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

  cx_diag::GetRoutineUpdateResponse result;
  result.progress_percent = ptr->progress_percent;

  if (ptr->output.has_value() && !ptr->output.value().empty()) {
    result.output = std::move(ptr->output);
  }

  switch (ptr->routine_update_union->which()) {
    case crosapi::mojom::DiagnosticsRoutineUpdateUnion::Tag::
        kNoninteractiveUpdate: {
      auto& routine_update =
          ptr->routine_update_union->get_noninteractive_update();
      result.status =
          converters::diagnostics::ConvertRoutineStatus(routine_update->status);
      result.status_message = std::move(routine_update->status_message);
      break;
    }
    case crosapi::mojom::DiagnosticsRoutineUpdateUnion::Tag::kInteractiveUpdate:
      // Routine is waiting for user action. Set the status to waiting.
      result.status = cx_diag::RoutineStatus::kWaitingUserAction;
      result.status_message = "Waiting for user action. See user_message";
      result.user_message = converters::diagnostics::ConvertRoutineUserMessage(
          ptr->routine_update_union->get_interactive_update()->user_message);
      break;
  }

  Respond(ArgumentList(cx_diag::GetRoutineUpdate::Results::Create(result)));
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

  cx_diag::RunRoutineResponse result;
  result.id = ptr->id;
  result.status = converters::diagnostics::ConvertRoutineStatus(ptr->status);
  Respond(WithArguments(result.ToValue()));
}

base::OnceCallback<void(crosapi::mojom::DiagnosticsRunRoutineResponsePtr)>
DiagnosticsApiRunRoutineFunctionBase::GetOnResult() {
  return base::BindOnce(&DiagnosticsApiRunRoutineFunctionBase::OnResult, this);
}

// OsDiagnosticsRunAcPowerRoutineFunction ------------------------------

void OsDiagnosticsRunAcPowerRoutineFunction::RunIfAllowed() {
  const auto params = GetParams<cx_diag::RunAcPowerRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunAcPowerRoutine(
      converters::diagnostics::ConvertAcPowerStatusRoutineType(
          params->request.expected_status),
      params->request.expected_power_type, GetOnResult());
}

// OsDiagnosticsRunBatteryCapacityRoutineFunction ------------------------------
void OsDiagnosticsRunBatteryCapacityRoutineFunction::RunIfAllowed() {
  GetRemoteService()->RunBatteryCapacityRoutine(GetOnResult());
}

// OsDiagnosticsRunBatteryChargeRoutineFunction --------------------------------

void OsDiagnosticsRunBatteryChargeRoutineFunction::RunIfAllowed() {
  const auto params = GetParams<cx_diag::RunBatteryChargeRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunBatteryChargeRoutine(
      params->request.length_seconds,
      params->request.minimum_charge_percent_required, GetOnResult());
}

// OsDiagnosticsRunBatteryDischargeRoutineFunction -----------------------------

void OsDiagnosticsRunBatteryDischargeRoutineFunction::RunIfAllowed() {
  const auto params = GetParams<cx_diag::RunBatteryDischargeRoutine::Params>();
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

// OsDiagnosticsRunBluetoothDiscoveryRoutineFunction ---------------------------

void OsDiagnosticsRunBluetoothDiscoveryRoutineFunction::RunIfAllowed() {
  GetRemoteService()->RunBluetoothDiscoveryRoutine(GetOnResult());
}

// OsDiagnosticsRunBluetoothPairingRoutineFunction -----------------------------

void OsDiagnosticsRunBluetoothPairingRoutineFunction::RunIfAllowed() {
  // Pairing Routine is guarded by `os.bluetooth_peripherals_info` permission.
  if (!extension()->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::
              kChromeOSBluetoothPeripheralsInfo)) {
    Respond(
        Error("Unauthorized access to "
              "chrome.os.diagnostics.runBluetoothPairingRoutine. Extension "
              "doesn't have the permission."));
    return;
  }

  const auto params = GetParams<cx_diag::RunBluetoothPairingRoutine::Params>();
  if (!params) {
    return;
  }
  GetRemoteService()->RunBluetoothPairingRoutine(params->request.peripheral_id,
                                                 GetOnResult());
}

// OsDiagnosticsRunBluetoothPowerRoutineFunction -------------------------------

void OsDiagnosticsRunBluetoothPowerRoutineFunction::RunIfAllowed() {
  GetRemoteService()->RunBluetoothPowerRoutine(GetOnResult());
}

// OsDiagnosticsRunBluetoothScanningRoutineFunction ----------------------------

void OsDiagnosticsRunBluetoothScanningRoutineFunction::RunIfAllowed() {
  // Scanning Routine is guarded by `os.bluetooth_peripherals_info` permission.
  if (!extension()->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::
              kChromeOSBluetoothPeripheralsInfo)) {
    Respond(
        Error("Unauthorized access to "
              "chrome.os.diagnostics.runBluetoothScanningRoutine. Extension"
              " doesn't have the permission."));
    return;
  }

  const auto params = GetParams<cx_diag::RunBluetoothScanningRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunBluetoothScanningRoutine(
      params->request.length_seconds, GetOnResult());
}

// OsDiagnosticsRunCpuCacheRoutineFunction -------------------------------------

void OsDiagnosticsRunCpuCacheRoutineFunction::RunIfAllowed() {
  const auto params = GetParams<cx_diag::RunCpuCacheRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunCpuCacheRoutine(params->request.length_seconds,
                                         GetOnResult());
}

// OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction ---------------------

void OsDiagnosticsRunCpuFloatingPointAccuracyRoutineFunction::RunIfAllowed() {
  const auto params =
      GetParams<cx_diag::RunCpuFloatingPointAccuracyRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunFloatingPointAccuracyRoutine(
      params->request.length_seconds, GetOnResult());
}

// OsDiagnosticsRunCpuPrimeSearchRoutineFunction -------------------------------

void OsDiagnosticsRunCpuPrimeSearchRoutineFunction::RunIfAllowed() {
  const auto params = GetParams<cx_diag::RunCpuPrimeSearchRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunPrimeSearchRoutine(params->request.length_seconds,
                                            GetOnResult());
}

// OsDiagnosticsRunCpuStressRoutineFunction ------------------------------------

void OsDiagnosticsRunCpuStressRoutineFunction::RunIfAllowed() {
  const auto params = GetParams<cx_diag::RunCpuStressRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunCpuStressRoutine(params->request.length_seconds,
                                          GetOnResult());
}

// OsDiagnosticsRunDiskReadRoutineFunction -------------------------------------

void OsDiagnosticsRunDiskReadRoutineFunction::RunIfAllowed() {
  const auto params = GetParams<cx_diag::RunDiskReadRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunDiskReadRoutine(
      converters::diagnostics::ConvertDiskReadRoutineType(params->request.type),
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
  auto params = GetParams<cx_diag::RunNvmeSelfTestRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunNvmeSelfTestRoutine(
      converters::diagnostics::ConvertNvmeSelfTestRoutineType(
          std::move(params->request)),
      GetOnResult());
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
  std::optional<cx_diag::RunSmartctlCheckRoutine::Params> params(
      cx_diag::RunSmartctlCheckRoutine::Params::Create(args()));

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

// OsDiagnosticsRunUfsLifetimeRoutineFunction -------------------------------

void OsDiagnosticsRunUfsLifetimeRoutineFunction::RunIfAllowed() {
  GetRemoteService()->RunUfsLifetimeRoutine(GetOnResult());
}

// OsDiagnosticsRunPowerButtonRoutineFunction -----------------------------

void OsDiagnosticsRunPowerButtonRoutineFunction::RunIfAllowed() {
  const auto params = GetParams<cx_diag::RunPowerButtonRoutine::Params>();
  if (!params) {
    return;
  }

  GetRemoteService()->RunPowerButtonRoutine(params->request.timeout_seconds,
                                            GetOnResult());
}

// OsDiagnosticsRunAudioDriverRoutineFunction -------------------------------

void OsDiagnosticsRunAudioDriverRoutineFunction::RunIfAllowed() {
  GetRemoteService()->RunAudioDriverRoutine(GetOnResult());
}

// OsDiagnosticsRunFanRoutineFunction -------------------------------

void OsDiagnosticsRunFanRoutineFunction::RunIfAllowed() {
  GetRemoteService()->RunFanRoutine(GetOnResult());
}

// OsDiagnosticsCreateRoutineFunction ------------------------------------

void OsDiagnosticsCreateRoutineFunction::RunIfAllowed() {
  std::optional<cx_diag::CreateRoutine::Params> params(
      cx_diag::CreateRoutine::Params::Create(args()));
  if (!params.has_value()) {
    Respond(BadMessage());
    return;
  }

  std::optional<crosapi::mojom::TelemetryDiagnosticRoutineArgumentPtr>
      mojo_arg = converters::diagnostics::ConvertRoutineArgumentsUnion(
          std::move(params->args));
  if (!mojo_arg.has_value()) {
    RespondWithError("Routine arguments are invalid.");
    return;
  }

  // Block unreleased features behind the feature flag.
  if (IsPendingApprovalRoutine(mojo_arg.value()) &&
      !base::FeatureList::IsEnabled(
          extensions_features::kTelemetryExtensionPendingApprovalApi)) {
    mojo_arg = crosapi::mojom::TelemetryDiagnosticRoutineArgument::
        NewUnrecognizedArgument(false);
  }

  RecordRoutineCreation(mojo_arg.value()->which());

  // Network bandwidth routine is guarded by `os.diagnostics.network_info_mlab`
  // permission.
  if (mojo_arg.value()->is_network_bandwidth() &&
      !extension()->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::
              kChromeOSDiagnosticsNetworkInfoForMlab)) {
    RespondWithError(
        "Unauthorized access to chrome.os.diagnostics.CreateRoutine with "
        "networkBandwidth argument. Extension doesn't have the permission.");
    return;
  }

  auto* routines_manager = DiagnosticRoutineManager::Get(browser_context());
  auto result = routines_manager->CreateRoutine(extension_id(),
                                                std::move(mojo_arg.value()));

  if (!result.has_value()) {
    switch (result.error()) {
      case DiagnosticRoutineManager::kAppUiClosed:
        Respond(Error("Companion app UI is not open."));
        break;
      case DiagnosticRoutineManager::kExtensionUnloaded:
        Respond(Error("Extension has been unloaded."));
        break;
    }
    return;
  }

  cx_diag::CreateRoutineResponse response;
  response.uuid = result->AsLowercaseString();
  Respond(ArgumentList(cx_diag::CreateRoutine::Results::Create(response)));
}

// OsDiagnosticsCreateMemoryRoutineFunction ------------------------------------

void OsDiagnosticsCreateMemoryRoutineFunction::RunIfAllowed() {
  std::optional<cx_diag::CreateMemoryRoutine::Params> params(
      cx_diag::CreateMemoryRoutine::Params::Create(args()));

  if (!params.has_value() ||
      (params.value().args.max_testing_mem_kib.has_value() &&
       params.value().args.max_testing_mem_kib < 0)) {
    SetBadMessage();
    Respond(BadMessage());
    return;
  }

  auto memory_arg =
      crosapi::mojom::TelemetryDiagnosticMemoryRoutineArgument::New();
  if (params.value().args.max_testing_mem_kib.has_value()) {
    memory_arg->max_testing_mem_kib = params.value().args.max_testing_mem_kib;
  }

  auto* routines_manager = DiagnosticRoutineManager::Get(browser_context());
  auto result = routines_manager->CreateRoutine(
      extension_id(),
      crosapi::mojom::TelemetryDiagnosticRoutineArgument::NewMemory(
          std::move(memory_arg)));

  if (!result.has_value()) {
    switch (result.error()) {
      case DiagnosticRoutineManager::kAppUiClosed:
        Respond(Error("Companion app UI is not open."));
        break;
      case DiagnosticRoutineManager::kExtensionUnloaded:
        Respond(Error("Extension has been unloaded."));
        break;
    }
    return;
  }

  cx_diag::CreateRoutineResponse response;
  response.uuid = result->AsLowercaseString();
  Respond(
      ArgumentList(cx_diag::CreateMemoryRoutine::Results::Create(response)));
}

// OsDiagnosticsCreateVolumeButtonRoutineFunction
// ------------------------------------

void OsDiagnosticsCreateVolumeButtonRoutineFunction::RunIfAllowed() {
  std::optional<cx_diag::CreateVolumeButtonRoutine::Params> params(
      cx_diag::CreateVolumeButtonRoutine::Params::Create(args()));

  if (!params.has_value() || params.value().args.timeout_seconds <= 0 ||
      params.value().args.button_type == cx_diag::VolumeButtonType::kNone) {
    SetBadMessage();
    Respond(BadMessage());
    return;
  }

  auto volume_button_arg =
      crosapi::mojom::TelemetryDiagnosticVolumeButtonRoutineArgument::New();
  volume_button_arg->type =
      converters::diagnostics::ConvertVolumeButtonRoutineButtonType(
          params.value().args.button_type);
  volume_button_arg->timeout =
      base::Seconds(params.value().args.timeout_seconds);

  auto* routines_manager = DiagnosticRoutineManager::Get(browser_context());
  auto result = routines_manager->CreateRoutine(
      extension_id(),
      crosapi::mojom::TelemetryDiagnosticRoutineArgument::NewVolumeButton(
          std::move(volume_button_arg)));

  if (!result.has_value()) {
    switch (result.error()) {
      case DiagnosticRoutineManager::kAppUiClosed:
        Respond(Error("Companion app UI is not open."));
        break;
      case DiagnosticRoutineManager::kExtensionUnloaded:
        Respond(Error("Extension has been unloaded."));
        break;
    }
    return;
  }

  cx_diag::CreateRoutineResponse response;
  response.uuid = result->AsLowercaseString();
  Respond(ArgumentList(
      cx_diag::CreateVolumeButtonRoutine::Results::Create(response)));
}

// OsDiagnosticsCreateFanRoutineFunction ------------------------------------

void OsDiagnosticsCreateFanRoutineFunction::RunIfAllowed() {
  std::optional<cx_diag::CreateFanRoutine::Params> params(
      cx_diag::CreateFanRoutine::Params::Create(args()));

  if (!params.has_value()) {
    SetBadMessage();
    Respond(BadMessage());
    return;
  }

  auto fan_arg = crosapi::mojom::TelemetryDiagnosticFanRoutineArgument::New();

  auto* routines_manager = DiagnosticRoutineManager::Get(browser_context());
  auto result = routines_manager->CreateRoutine(
      extension_id(),
      crosapi::mojom::TelemetryDiagnosticRoutineArgument::NewFan(
          std::move(fan_arg)));

  if (!result.has_value()) {
    switch (result.error()) {
      case DiagnosticRoutineManager::kAppUiClosed:
        Respond(Error("Companion app UI is not open."));
        break;
      case DiagnosticRoutineManager::kExtensionUnloaded:
        Respond(Error("Extension has been unloaded."));
        break;
    }
    return;
  }

  cx_diag::CreateRoutineResponse response;
  response.uuid = result->AsLowercaseString();
  Respond(ArgumentList(cx_diag::CreateFanRoutine::Results::Create(response)));
}

// OsDiagnosticsStartRoutineFunction -------------------------------------------

void OsDiagnosticsStartRoutineFunction::RunIfAllowed() {
  auto params = GetParams<cx_diag::StartRoutine::Params>();
  if (!params.has_value()) {
    return;
  }

  auto* routines_manager = DiagnosticRoutineManager::Get(browser_context());
  bool result = routines_manager->StartRoutineForExtension(
      extension_id(), base::Uuid::ParseLowercase(params.value().request.uuid));

  if (!result) {
    RespondWithError("Unknown routine id.");
    return;
  }

  Respond(NoArguments());
}

// OsDiagnosticsCancelRoutineFunction ------------------------------------------

void OsDiagnosticsCancelRoutineFunction::RunIfAllowed() {
  auto params = GetParams<cx_diag::CancelRoutine::Params>();
  if (!params.has_value()) {
    return;
  }

  auto* routines_manager = DiagnosticRoutineManager::Get(browser_context());
  routines_manager->CancelRoutineForExtension(
      extension_id(), base::Uuid::ParseLowercase(params.value().request.uuid));

  Respond(NoArguments());
}

// OsDiagnosticsReplyToRoutineInquiryFunction ----------------------------------

void OsDiagnosticsReplyToRoutineInquiryFunction::RunIfAllowed() {
  auto params = cx_diag::ReplyToRoutineInquiry::Params::Create(args());
  if (!params.has_value()) {
    Respond(BadMessage());
    return;
  }

  std::optional<crosapi::mojom::TelemetryDiagnosticRoutineInquiryReplyPtr>
      mojo_reply = converters::diagnostics::ConvertRoutineInquiryReplyUnion(
          std::move(params->request.reply));
  if (!mojo_reply.has_value()) {
    RespondWithError("Inquiry reply is invalid.");
    return;
  }

  auto* routines_manager = DiagnosticRoutineManager::Get(browser_context());
  bool result = routines_manager->ReplyToRoutineInquiryForExtension(
      extension_id(), base::Uuid::ParseLowercase(params.value().request.uuid),
      std::move(mojo_reply.value()));

  if (!result) {
    RespondWithError("Unknown routine id.");
    return;
  }

  Respond(NoArguments());
}

// OsDiagnosticsIsRoutineArgumentSupportedFunction -----------------------

void OsDiagnosticsIsRoutineArgumentSupportedFunction::RunIfAllowed() {
  auto params = GetParams<cx_diag::IsRoutineArgumentSupported::Params>();
  if (!params.has_value()) {
    return;
  }

  std::optional<crosapi::mojom::TelemetryDiagnosticRoutineArgumentPtr>
      mojo_arg = converters::diagnostics::ConvertRoutineArgumentsUnion(
          std::move(params->args));
  if (!mojo_arg.has_value()) {
    RespondWithError("Routine arguments are invalid.");
    return;
  }

  // Block unreleased features behind the feature flag.
  if (IsPendingApprovalRoutine(mojo_arg.value()) &&
      !base::FeatureList::IsEnabled(
          extensions_features::kTelemetryExtensionPendingApprovalApi)) {
    mojo_arg = crosapi::mojom::TelemetryDiagnosticRoutineArgument::
        NewUnrecognizedArgument(false);
  }

  RecordRoutineSupportedStatusQuery(mojo_arg.value()->which());

  // Network bandwidth routine is guarded by `os.diagnostics.network_info_mlab`
  // permission.
  if (mojo_arg.value()->is_network_bandwidth() &&
      !extension()->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::
              kChromeOSDiagnosticsNetworkInfoForMlab)) {
    RespondWithError(
        "Unauthorized access to "
        "chrome.os.diagnostics.isRoutineArgumentSupported with "
        "networkBandwidth argument. Extension doesn't have the permission.");
    return;
  }

  auto* routines_manager = DiagnosticRoutineManager::Get(browser_context());
  routines_manager->IsRoutineArgumentSupported(
      std::move(mojo_arg.value()),
      base::BindOnce(&OsDiagnosticsIsRoutineArgumentSupportedFunction::OnResult,
                     this));
}

void OsDiagnosticsIsRoutineArgumentSupportedFunction::OnResult(
    crosapi::mojom::TelemetryExtensionSupportStatusPtr result) {
  if (result.is_null()) {
    RespondWithError("API internal error.");
    return;
  }

  auto response = ParseRoutineArgumentSupportResult(std::move(result));

  if (!response.has_value()) {
    RespondWithError(response.error());
    return;
  }

  Respond(ArgumentList(
      cx_diag::IsRoutineArgumentSupported::Results::Create(response.value())));
}

// OsDiagnosticsIsMemoryRoutineArgumentSupportedFunction -----------------------

void OsDiagnosticsIsMemoryRoutineArgumentSupportedFunction::RunIfAllowed() {
  auto params = GetParams<cx_diag::IsMemoryRoutineArgumentSupported::Params>();
  if (!params.has_value()) {
    return;
  }

  auto* routines_manager = DiagnosticRoutineManager::Get(browser_context());
  auto mem_args =
      crosapi::mojom::TelemetryDiagnosticMemoryRoutineArgument::New();
  mem_args->max_testing_mem_kib = params.value().args.max_testing_mem_kib;

  auto args = crosapi::mojom::TelemetryDiagnosticRoutineArgument::NewMemory(
      std::move(mem_args));
  routines_manager->IsRoutineArgumentSupported(
      std::move(args),
      base::BindOnce(
          &OsDiagnosticsIsMemoryRoutineArgumentSupportedFunction::OnResult,
          this));
}

void OsDiagnosticsIsMemoryRoutineArgumentSupportedFunction::OnResult(
    crosapi::mojom::TelemetryExtensionSupportStatusPtr result) {
  if (result.is_null()) {
    RespondWithError("API internal error.");
    return;
  }

  auto response = ParseRoutineArgumentSupportResult(std::move(result));

  if (!response.has_value()) {
    RespondWithError(response.error());
    return;
  }

  Respond(
      ArgumentList(cx_diag::IsMemoryRoutineArgumentSupported::Results::Create(
          response.value())));
}

// OsDiagnosticsIsVolumeButtonRoutineArgumentSupportedFunction
// -----------------------

void OsDiagnosticsIsVolumeButtonRoutineArgumentSupportedFunction::
    RunIfAllowed() {
  auto params =
      GetParams<cx_diag::IsVolumeButtonRoutineArgumentSupported::Params>();
  if (!params.has_value() || params.value().args.timeout_seconds <= 0 ||
      params.value().args.button_type == cx_diag::VolumeButtonType::kNone) {
    return;
  }

  auto* routines_manager = DiagnosticRoutineManager::Get(browser_context());
  auto volume_button_args =
      crosapi::mojom::TelemetryDiagnosticVolumeButtonRoutineArgument::New();
  volume_button_args->type =
      converters::diagnostics::ConvertVolumeButtonRoutineButtonType(
          params.value().args.button_type);
  volume_button_args->timeout =
      base::Seconds(params.value().args.timeout_seconds);

  auto args =
      crosapi::mojom::TelemetryDiagnosticRoutineArgument::NewVolumeButton(
          std::move(volume_button_args));
  routines_manager->IsRoutineArgumentSupported(
      std::move(args),
      base::BindOnce(
          &OsDiagnosticsIsVolumeButtonRoutineArgumentSupportedFunction::
              OnResult,
          this));
}

void OsDiagnosticsIsVolumeButtonRoutineArgumentSupportedFunction::OnResult(
    crosapi::mojom::TelemetryExtensionSupportStatusPtr result) {
  if (result.is_null()) {
    RespondWithError("API internal error.");
    return;
  }

  auto response = ParseRoutineArgumentSupportResult(std::move(result));

  if (!response.has_value()) {
    RespondWithError(response.error());
    return;
  }

  Respond(ArgumentList(
      cx_diag::IsVolumeButtonRoutineArgumentSupported::Results::Create(
          response.value())));
}

// OsDiagnosticsIsFanRoutineArgumentSupportedFunction -----------------------

void OsDiagnosticsIsFanRoutineArgumentSupportedFunction::RunIfAllowed() {
  auto params = GetParams<cx_diag::IsFanRoutineArgumentSupported::Params>();
  if (!params.has_value()) {
    return;
  }

  auto* routines_manager = DiagnosticRoutineManager::Get(browser_context());
  auto fan_args = crosapi::mojom::TelemetryDiagnosticFanRoutineArgument::New();

  auto args = crosapi::mojom::TelemetryDiagnosticRoutineArgument::NewFan(
      std::move(fan_args));
  routines_manager->IsRoutineArgumentSupported(
      std::move(args),
      base::BindOnce(
          &OsDiagnosticsIsFanRoutineArgumentSupportedFunction::OnResult, this));
}

void OsDiagnosticsIsFanRoutineArgumentSupportedFunction::OnResult(
    crosapi::mojom::TelemetryExtensionSupportStatusPtr result) {
  if (result.is_null()) {
    RespondWithError("API internal error.");
    return;
  }

  auto response = ParseRoutineArgumentSupportResult(std::move(result));

  if (!response.has_value()) {
    RespondWithError(response.error());
    return;
  }

  Respond(ArgumentList(cx_diag::IsFanRoutineArgumentSupported::Results::Create(
      response.value())));
}

}  // namespace chromeos
