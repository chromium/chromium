// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/diagnostics_ui/backend/system/system_routine_controller.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/system/diagnostics/diagnostics_log_controller.h"
#include "ash/system/diagnostics/routine_log.h"
#include "ash/webui/diagnostics_ui/backend/common/histogram_util.h"
#include "ash/webui/diagnostics_ui/backend/common/routine_properties.h"
#include "ash/webui/diagnostics_ui/backend/system/cros_healthd_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/nullable_primitives.mojom.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace ash::diagnostics {

namespace {

namespace healthd = cros_healthd::mojom;

constexpr uint32_t kBatteryDurationInSeconds = 30;
constexpr uint32_t kBatteryChargeMinimumPercent = 0;
constexpr uint32_t kBatteryDischargeMaximumPercent = 100;
constexpr uint32_t kRoutineResultRefreshIntervalInSeconds = 1;

constexpr char kChargePercentKey[] = "chargePercent";
constexpr char kDischargePercentKey[] = "dischargePercent";
constexpr char kResultDetailsKey[] = "resultDetails";

const char kWakeLockReason[] = "DiagnosticsMemoryRoutine";

mojom::RoutineResultInfoPtr ConstructStandardRoutineResultInfoPtr(
    mojom::RoutineType type,
    mojom::StandardRoutineResult result) {
  auto routine_result = mojom::RoutineResult::NewSimpleResult(result);
  return mojom::RoutineResultInfo::New(type, std::move(routine_result));
}

// Converts a cros_healthd::mojom::DiagnosticRoutineStatusEnum to a
// mojom::StandardRoutineResult. Should only be called to construct the final
// response. Should not be called for in-progess statuses.
mojom::StandardRoutineResult TestStatusToResult(
    healthd::DiagnosticRoutineStatusEnum status) {
  switch (status) {
    case healthd::DiagnosticRoutineStatusEnum::kPassed:
      return mojom::StandardRoutineResult::kTestPassed;
    case healthd::DiagnosticRoutineStatusEnum::kFailed:
      return mojom::StandardRoutineResult::kTestFailed;
    case healthd::DiagnosticRoutineStatusEnum::kCancelled:
    case healthd::DiagnosticRoutineStatusEnum::kError:
      return mojom::StandardRoutineResult::kExecutionError;
    case healthd::DiagnosticRoutineStatusEnum::kFailedToStart:
    case healthd::DiagnosticRoutineStatusEnum::kUnsupported:
    case healthd::DiagnosticRoutineStatusEnum::kNotRun:
      return mojom::StandardRoutineResult::kUnableToRun;
    case healthd::DiagnosticRoutineStatusEnum::kReady:
    case healthd::DiagnosticRoutineStatusEnum::kRunning:
    case healthd::DiagnosticRoutineStatusEnum::kWaiting:
    case healthd::DiagnosticRoutineStatusEnum::kRemoved:
    case healthd::DiagnosticRoutineStatusEnum::kCancelling:
    case healthd::DiagnosticRoutineStatusEnum::kUnknown:
      NOTREACHED();
  }
}

mojom::RoutineResultInfoPtr ConstructPowerRoutineResultInfoPtr(
    mojom::RoutineType type,
    mojom::StandardRoutineResult result,
    double percent_change,
    uint32_t seconds_elapsed) {
  auto power_result =
      mojom::PowerRoutineResult::New(result, percent_change, seconds_elapsed);
  auto routine_result =
      mojom::RoutineResult::NewPowerResult(std::move(power_result));
  return mojom::RoutineResultInfo::New(type, std::move(routine_result));
}

bool IsPowerRoutine(mojom::RoutineType routine_type) {
  return routine_type == mojom::RoutineType::kBatteryCharge ||
         routine_type == mojom::RoutineType::kBatteryDischarge;
}

std::string ReadMojoHandleToJsonString(mojo::PlatformHandle handle) {
  base::File file(handle.ReleaseFD());
  std::vector<uint8_t> contents;
  contents.resize(file.GetLength());
  if (!file.ReadAndCheck(0, contents)) {
    return std::string();
  }
  return std::string(contents.begin(), contents.end());
}

bool IsLoggingEnabled() {
  return diagnostics::DiagnosticsLogController::IsInitialized();
}

}  // namespace

SystemRoutineController::SystemRoutineController() {
  inflight_routine_timer_ = std::make_unique<base::OneShotTimer>();
}

SystemRoutineController::~SystemRoutineController() {
  if (inflight_routine_runner_) {
    // Since SystemRoutineController is torn down at the same time as the
    // frontend, there's no guarantee that the disconnect handler will be
    // called. If there's a routine inflight, cancel it but do not pass a
    // callback.
    BindCrosHealthdDiagnosticsServiceIfNeccessary();
    diagnostics_service_->GetRoutineUpdate(
        inflight_routine_id_, healthd::DiagnosticRoutineCommandEnum::kCancel,
        /*should_include_output=*/false, base::DoNothing());
    if (IsLoggingEnabled() && inflight_routine_type_.has_value()) {
      DiagnosticsLogController::Get()->GetRoutineLog().LogRoutineCancelled(
          inflight_routine_type_.value());
    }
  }

  // Emit the total number of routines run.
  metrics::EmitRoutineRunCount(routine_count_);
}

void SystemRoutineController::RunRoutine(
    mojom::RoutineType type,
    mojo::PendingRemote<mojom::RoutineRunner> runner) {
  if (IsRoutineRunning()) {
    // If a routine is already running, alert the caller that we were unable
    // to start the routine.
    mojo::Remote<mojom::RoutineRunner> routine_runner(std::move(runner));
    auto result = ConstructStandardRoutineResultInfoPtr(
        type, mojom::StandardRoutineResult::kUnableToRun);
    routine_runner->OnRoutineResult(std::move(result));
    return;
  }

  ++routine_count_;

  inflight_routine_runner_ =
      mojo::Remote<mojom::RoutineRunner>(std::move(runner));
  inflight_routine_runner_.set_disconnect_handler(base::BindOnce(
      &SystemRoutineController::OnInflightRoutineRunnerDisconnected,
      base::Unretained(this)));
  ExecuteRoutine(type);
}

void SystemRoutineController::GetSupportedRoutines(
    GetSupportedRoutinesCallback callback) {
  if (!supported_routines_.empty()) {
    std::move(callback).Run(supported_routines_);
    return;
  }
  BindCrosHealthdDiagnosticsServiceIfNeccessary();
  diagnostics_service_->GetAvailableRoutines(
      base::BindOnce(&SystemRoutineController::OnAvailableRoutinesFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SystemRoutineController::BindInterface(
    mojo::PendingReceiver<mojom::SystemRoutineController> pending_receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&SystemRoutineController::OnBoundInterfaceDisconnect,
                     base::Unretained(this)));
}

bool SystemRoutineController::IsReceiverBoundForTesting() {
  return receiver_.is_bound();
}

void SystemRoutineController::OnBoundInterfaceDisconnect() {
  receiver_.reset();
}

void SystemRoutineController::OnAvailableRoutinesFetched(
    GetSupportedRoutinesCallback callback,
    const std::vector<healthd::DiagnosticRoutineEnum>& available_routines) {
  base::flat_set<healthd::DiagnosticRoutineEnum> healthd_routines(
      available_routines);
  for (size_t i = 0; i < kRoutinePropertiesLength; i++) {
    const RoutineProperties& routine = kRoutineProperties[i];
    if (base::Contains(healthd_routines, routine.healthd_type)) {
      supported_routines_.push_back(routine.type);
    }
  }
  std::move(callback).Run(supported_routines_);
}

void SystemRoutineController::ExecuteRoutine(mojom::RoutineType routine_type) {
  BindCrosHealthdDiagnosticsServiceIfNeccessary();

  switch (routine_type) {
    case mojom::RoutineType::kArcDnsResolution:
      diagnostics_service_->RunArcDnsResolutionRoutine(
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;

    case mojom::RoutineType::kArcHttp:
      diagnostics_service_->RunArcHttpRoutine(
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;

    case mojom::RoutineType::kArcPing:
      diagnostics_service_->RunArcPingRoutine(
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;

    case mojom::RoutineType::kBatteryCharge:
      diagnostics_service_->RunBatteryChargeRoutine(
          GetExpectedRoutineDurationInSeconds(routine_type),
          kBatteryChargeMinimumPercent,
          base::BindOnce(&SystemRoutineController::OnPowerRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));

      break;
    case mojom::RoutineType::kBatteryDischarge:
      diagnostics_service_->RunBatteryDischargeRoutine(
          GetExpectedRoutineDurationInSeconds(routine_type),
          kBatteryDischargeMaximumPercent,
          base::BindOnce(&SystemRoutineController::OnPowerRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));

      break;
    case mojom::RoutineType::kCaptivePortal:
      diagnostics_service_->RunCaptivePortalRoutine(
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;
    case mojom::RoutineType::kCpuCache:
      diagnostics_service_->RunCpuCacheRoutine(
          healthd::NullableUint32::New(
              GetExpectedRoutineDurationInSeconds(routine_type)),
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;
    case mojom::RoutineType::kCpuFloatingPoint:
      diagnostics_service_->RunFloatingPointAccuracyRoutine(
          healthd::NullableUint32::New(
              GetExpectedRoutineDurationInSeconds(routine_type)),
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;
    case mojom::RoutineType::kCpuPrime:
      diagnostics_service_->RunPrimeSearchRoutine(
          healthd::NullableUint32::New(
              GetExpectedRoutineDurationInSeconds(routine_type)),
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;
    case mojom::RoutineType::kCpuStress:
      diagnostics_service_->RunCpuStressRoutine(
          healthd::NullableUint32::New(
              GetExpectedRoutineDurationInSeconds(routine_type)),
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;
    case mojom::RoutineType::kDnsLatency:
      diagnostics_service_->RunDnsLatencyRoutine(
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;
    case mojom::RoutineType::kDnsResolution:
      diagnostics_service_->RunDnsResolutionRoutine(
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;
    case mojom::RoutineType::kDnsResolverPresent:
      diagnostics_service_->RunDnsResolverPresentRoutine(
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;
    case mojom::RoutineType::kGatewayCanBePinged:
      diagnostics_service_->RunGatewayCanBePingedRoutine(
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;
    case mojom::RoutineType::kHasSecureWiFiConnection:
      diagnostics_service_->RunHasSecureWiFiConnectionRoutine(
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;
    case mojom::RoutineType::kHttpFirewall:
      diagnostics_service_->RunHttpFirewallRoutine(
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;
    case mojom::RoutineType::kHttpsFirewall:
      diagnostics_service_->RunHttpsFirewallRoutine(
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;
    case mojom::RoutineType::kHttpsLatency:
      diagnostics_service_->RunHttpsLatencyRoutine(
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;
    case mojom::RoutineType::kLanConnectivity:
      diagnostics_service_->RunLanConnectivityRoutine(
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;
    case mojom::RoutineType::kMemory:
      AcquireWakeLock();
      diagnostics_service_->RunMemoryRoutine(
          std::nullopt,
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      memory_routine_start_timestamp_ = base::Time::Now();
      break;
    case mojom::RoutineType::kSignalStrength:
      diagnostics_service_->RunSignalStrengthRoutine(
          base::BindOnce(&SystemRoutineController::OnRoutineStarted,
                         weak_factory_.GetWeakPtr(), routine_type));
      break;
  }
  if (IsLoggingEnabled()) {
    DiagnosticsLogController::Get()->GetRoutineLog().LogRoutineStarted(
        routine_type);
  }
}

void SystemRoutineController::OnRoutineStarted(
    mojom::RoutineType routine_type,
    healthd::RunRoutineResponsePtr response_ptr) {
  DCHECK(!IsPowerRoutine(routine_type));
  // Check for error conditions.
  // TODO(baileyberro): Handle additional statuses.
  if (response_ptr->status ==
          healthd::DiagnosticRoutineStatusEnum::kFailedToStart ||
      response_ptr->id == healthd::kFailedToStartId) {
    OnStandardRoutineResult(routine_type,
                            TestStatusToResult(response_ptr->status));
    return;
  }
  DCHECK_EQ(healthd::DiagnosticRoutineStatusEnum::kRunning,
            response_ptr->status);

  DCHECK_EQ(kInvalidRoutineId, inflight_routine_id_);
  inflight_routine_id_ = response_ptr->id;
  inflight_routine_type_ = routine_type;

  // Sleep for the length of the test using a one-shot timer, then start
  // querying again for status.
  ScheduleCheckRoutineStatus(GetExpectedRoutineDurationInSeconds(routine_type),
                             routine_type);
}

void SystemRoutineController::OnPowerRoutineStarted(
    mojom::RoutineType routine_type,
    healthd::RunRoutineResponsePtr response_ptr) {
  DCHECK(IsPowerRoutine(routine_type));
  // TODO(baileyberro): Handle additional statuses.
  if (response_ptr->status != healthd::DiagnosticRoutineStatusEnum::kWaiting) {
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    return;
  }

  DCHECK_EQ(kInvalidRoutineId, inflight_routine_id_);
  inflight_routine_id_ = response_ptr->id;
  inflight_routine_type_ = routine_type;

  ContinuePowerRoutine(routine_type);
}

void SystemRoutineController::ContinuePowerRoutine(
    mojom::RoutineType routine_type) {
  DCHECK(IsPowerRoutine(routine_type));

  BindCrosHealthdDiagnosticsServiceIfNeccessary();
  diagnostics_service_->GetRoutineUpdate(
      inflight_routine_id_, healthd::DiagnosticRoutineCommandEnum::kContinue,
      /*should_include_output=*/true,
      base::BindOnce(&SystemRoutineController::OnPowerRoutineContinued,
                     weak_factory_.GetWeakPtr(), routine_type));
}

void SystemRoutineController::OnPowerRoutineContinued(
    mojom::RoutineType routine_type,
    healthd::RoutineUpdatePtr update_ptr) {
  DCHECK(IsPowerRoutine(routine_type));

  const healthd::NonInteractiveRoutineUpdate* update =
      GetNonInteractiveRoutineUpdate(*update_ptr);

  if (!update ||
      update->status != healthd::DiagnosticRoutineStatusEnum::kRunning) {
    DVLOG(2) << "Failed to resume power routine.";
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    return;
  }

  ScheduleCheckRoutineStatus(GetExpectedRoutineDurationInSeconds(routine_type),
                             routine_type);
}

void SystemRoutineController::CheckRoutineStatus(
    mojom::RoutineType routine_type) {
  DCHECK_NE(kInvalidRoutineId, inflight_routine_id_);
  BindCrosHealthdDiagnosticsServiceIfNeccessary();
  const bool should_include_output = IsPowerRoutine(routine_type);
  diagnostics_service_->GetRoutineUpdate(
      inflight_routine_id_, healthd::DiagnosticRoutineCommandEnum::kGetStatus,
      should_include_output,
      base::BindOnce(&SystemRoutineController::OnRoutineStatusUpdated,
                     weak_factory_.GetWeakPtr(), routine_type));
}

void SystemRoutineController::OnRoutineStatusUpdated(
    mojom::RoutineType routine_type,
    healthd::RoutineUpdatePtr update_ptr) {
  if (IsPowerRoutine(routine_type)) {
    HandlePowerRoutineStatusUpdate(routine_type, std::move(update_ptr));
    return;
  }

  const healthd::NonInteractiveRoutineUpdate* update =
      GetNonInteractiveRoutineUpdate(*update_ptr);

  if (!update) {
    DVLOG(2) << "Invalid routine update";
    OnStandardRoutineResult(routine_type,
                            mojom::StandardRoutineResult::kExecutionError);
    return;
  }

  const healthd::DiagnosticRoutineStatusEnum status = update->status;

  switch (status) {
    case healthd::DiagnosticRoutineStatusEnum::kRunning:
      // If still running, continue to repoll until it is finished.
      // TODO(baileyberro): Consider adding a timeout mechanism.
      ScheduleCheckRoutineStatus(kRoutineResultRefreshIntervalInSeconds,
                                 routine_type);
      return;
    case healthd::DiagnosticRoutineStatusEnum::kPassed:
    case healthd::DiagnosticRoutineStatusEnum::kFailed:
      OnStandardRoutineResult(routine_type, TestStatusToResult(status));
      return;
    case healthd::DiagnosticRoutineStatusEnum::kCancelled:
    case healthd::DiagnosticRoutineStatusEnum::kError:
    case healthd::DiagnosticRoutineStatusEnum::kFailedToStart:
    case healthd::DiagnosticRoutineStatusEnum::kUnsupported:
    case healthd::DiagnosticRoutineStatusEnum::kReady:
    case healthd::DiagnosticRoutineStatusEnum::kWaiting:
    case healthd::DiagnosticRoutineStatusEnum::kRemoved:
    case healthd::DiagnosticRoutineStatusEnum::kCancelling:
    case healthd::DiagnosticRoutineStatusEnum::kNotRun:
    case healthd::DiagnosticRoutineStatusEnum::kUnknown:
      // Any other reason, report failure.
      DVLOG(2) << "Routine failed: " << update->status_message;
      OnStandardRoutineResult(routine_type, TestStatusToResult(status));
      return;
  }
}

void SystemRoutineController::HandlePowerRoutineStatusUpdate(
    mojom ::RoutineType routine_type,
    healthd::RoutineUpdatePtr update_ptr) {
  DCHECK(IsPowerRoutine(routine_type));

  const healthd::NonInteractiveRoutineUpdate* update =
      GetNonInteractiveRoutineUpdate(*update_ptr);

  if (!update) {
    DVLOG(2) << "Invalid routine update";
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    return;
  }

  const healthd::DiagnosticRoutineStatusEnum status = update->status;

  // If still running, continue to repoll until it is finished.
  // TODO(baileyberro): Consider adding a timeout mechanism.
  if (status == healthd::DiagnosticRoutineStatusEnum::kRunning) {
    ScheduleCheckRoutineStatus(kRoutineResultRefreshIntervalInSeconds,
                               routine_type);
    return;
  }

  // If test passed, report result.
  if (status == healthd::DiagnosticRoutineStatusEnum::kPassed) {
    ParsePowerRoutineResult(routine_type,
                            mojom::StandardRoutineResult::kTestPassed,
                            std::move(update_ptr->output));
    return;
  }

  // If test failed, report result.
  if (status == healthd::DiagnosticRoutineStatusEnum::kFailed) {
    ParsePowerRoutineResult(routine_type,
                            mojom::StandardRoutineResult::kTestFailed,
                            std::move(update_ptr->output));
    return;
  }

  // Any other reason, report failure.
  DVLOG(2) << "Routine failed: " << update->status_message;
  OnPowerRoutineResult(routine_type,
                       mojom::StandardRoutineResult::kExecutionError,
                       /*percent_change=*/0, /*seconds_elapsed=*/0);
}

bool SystemRoutineController::IsRoutineRunning() const {
  return inflight_routine_runner_.is_bound();
}

void SystemRoutineController::ScheduleCheckRoutineStatus(
    uint32_t duration_in_seconds,
    mojom::RoutineType routine_type) {
  inflight_routine_timer_->Start(
      FROM_HERE, base::Seconds(duration_in_seconds),
      base::BindOnce(&SystemRoutineController::CheckRoutineStatus,
                     weak_factory_.GetWeakPtr(), routine_type));
}

void SystemRoutineController::ParsePowerRoutineResult(
    mojom::RoutineType routine_type,
    mojom::StandardRoutineResult result,
    mojo::ScopedHandle output_handle) {
  if (!output_handle.is_valid()) {
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    return;
  }

  mojo::PlatformHandle platform_handle =
      mojo::UnwrapPlatformHandle(std::move(output_handle));
  if (!platform_handle.is_valid()) {
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ReadMojoHandleToJsonString, std::move(platform_handle)),
      base::BindOnce(&SystemRoutineController::OnPowerRoutineResultFetched,
                     weak_factory_.GetWeakPtr(), routine_type));
}

void SystemRoutineController::OnPowerRoutineResultFetched(
    mojom::RoutineType routine_type,
    const std::string& file_contents) {
  if (file_contents.empty()) {
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    DVLOG(2) << "Empty Power Routine Result File.";
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      file_contents,
      base::BindOnce(&SystemRoutineController::OnPowerRoutineJsonParsed,
                     weak_factory_.GetWeakPtr(), routine_type));
  return;
}

void SystemRoutineController::OnPowerRoutineJsonParsed(
    mojom::RoutineType routine_type,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    DVLOG(2) << "JSON parsing failed: " << result.error();
    return;
  }

  if (!result->is_dict()) {
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    DVLOG(2) << "Malformed Routine Result File.";
    return;
  }

  const base::Value::Dict& parsed_json = result->GetDict();
  const base::Value::Dict* result_details_dict =
      parsed_json.FindDict(kResultDetailsKey);
  if (!result_details_dict) {
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    DVLOG(2) << "Malformed Routine Result File.";
    return;
  }

  std::optional<double> charge_percent_opt =
      routine_type == mojom::RoutineType::kBatteryCharge
          ? result_details_dict->FindDouble(kChargePercentKey)
          : result_details_dict->FindDouble(kDischargePercentKey);
  if (!charge_percent_opt.has_value()) {
    OnPowerRoutineResult(routine_type,
                         mojom::StandardRoutineResult::kExecutionError,
                         /*percent_change=*/0, /*seconds_elapsed=*/0);
    DVLOG(2) << "Malformed Routine Result File.";
    return;
  }

  OnPowerRoutineResult(routine_type, mojom::StandardRoutineResult::kTestPassed,
                       *charge_percent_opt, kBatteryDurationInSeconds);
}

void SystemRoutineController::OnStandardRoutineResult(
    mojom::RoutineType routine_type,
    mojom::StandardRoutineResult result) {
  DCHECK(IsRoutineRunning());
  auto result_info =
      ConstructStandardRoutineResultInfoPtr(routine_type, result);
  SendRoutineResult(std::move(result_info));
  metrics::EmitRoutineResult(routine_type, result);
  if (routine_type == mojom::RoutineType::kMemory) {
    ReleaseWakeLock();
    metrics::EmitMemoryRoutineDuration(base::Time::Now() -
                                       memory_routine_start_timestamp_);
  }
  if (IsLoggingEnabled()) {
    DiagnosticsLogController::Get()->GetRoutineLog().LogRoutineCompleted(
        routine_type, result);
  }
}

void SystemRoutineController::OnPowerRoutineResult(
    mojom::RoutineType routine_type,
    mojom::StandardRoutineResult result,
    double percent_change,
    uint32_t seconds_elapsed) {
  DCHECK(IsRoutineRunning());
  auto result_info = ConstructPowerRoutineResultInfoPtr(
      routine_type, result, percent_change, seconds_elapsed);
  SendRoutineResult(std::move(result_info));
  metrics::EmitRoutineResult(routine_type, result);
  if (IsLoggingEnabled()) {
    DiagnosticsLogController::Get()->GetRoutineLog().LogRoutineCompleted(
        routine_type, result);
  }
}

void SystemRoutineController::SendRoutineResult(
    mojom::RoutineResultInfoPtr result_info) {
  if (inflight_routine_runner_ && !result_info.is_null() &&
      !result_info->result.is_null()) {
    inflight_routine_runner_->OnRoutineResult(std::move(result_info));
  } else {
    LOG(ERROR) << (inflight_routine_runner_
                       ? "Do not send routine result since it's null."
                       : "Not able to call OnRoutineResult() since the "
                         "inflight_routine_runner_ is null.");
  }

  inflight_routine_runner_.reset();
  inflight_routine_id_ = kInvalidRoutineId;
  inflight_routine_type_.reset();
}

void SystemRoutineController::BindCrosHealthdDiagnosticsServiceIfNeccessary() {
  if (!diagnostics_service_ || !diagnostics_service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->BindDiagnosticsService(
        diagnostics_service_.BindNewPipeAndPassReceiver());
    diagnostics_service_.set_disconnect_handler(base::BindOnce(
        &SystemRoutineController::OnDiagnosticsServiceDisconnected,
        weak_factory_.GetWeakPtr()));
  }
}

void SystemRoutineController::OnDiagnosticsServiceDisconnected() {
  diagnostics_service_.reset();
}

void SystemRoutineController::OnInflightRoutineRunnerDisconnected() {
  // Reset `inflight_routine_runner_` since the other side of the pipe is
  // already disconnected.
  inflight_routine_runner_.reset();

  // Stop `inflight_routine_timer_` so that we do not attempt to fetch the
  // status of a cancelled routine.
  inflight_routine_timer_->Stop();

  // Release `wake_lock_` if necessary.
  if (wake_lock_) {
    ReleaseWakeLock();
  }

  // Make a best effort attempt to remove the routine.
  BindCrosHealthdDiagnosticsServiceIfNeccessary();
  diagnostics_service_->GetRoutineUpdate(
      inflight_routine_id_, healthd::DiagnosticRoutineCommandEnum::kCancel,
      /*should_include_output=*/false,
      base::BindOnce(&SystemRoutineController::OnRoutineCancelAttempted,
                     weak_factory_.GetWeakPtr()));

  // Reset `inflight_routine_id_` to maintain invariant.
  inflight_routine_id_ = kInvalidRoutineId;

  if (IsLoggingEnabled() && inflight_routine_type_.has_value()) {
    DiagnosticsLogController::Get()->GetRoutineLog().LogRoutineCancelled(
        inflight_routine_type_.value());
  }
}

void SystemRoutineController::OnRoutineCancelAttempted(
    healthd::RoutineUpdatePtr update_ptr) {
  const healthd::NonInteractiveRoutineUpdate* update =
      GetNonInteractiveRoutineUpdate(*update_ptr);

  if (!update ||
      update->status != healthd::DiagnosticRoutineStatusEnum::kCancelled) {
    DVLOG(2) << "Failed to cancel routine.";
    return;
  }
}

void SystemRoutineController::AcquireWakeLock() {
  if (!wake_lock_) {
    if (!wake_lock_provider_) {
      content::GetDeviceService().BindWakeLockProvider(
          wake_lock_provider_.BindNewPipeAndPassReceiver());
    }

    wake_lock_provider_->GetWakeLockWithoutContext(
        device::mojom::WakeLockType::kPreventDisplaySleepAllowDimming,
        device::mojom::WakeLockReason::kOther, kWakeLockReason,
        wake_lock_.BindNewPipeAndPassReceiver());
  }

  wake_lock_->RequestWakeLock();
}

void SystemRoutineController::ReleaseWakeLock() {
  DCHECK(wake_lock_);
  wake_lock_->CancelWakeLock();
}

}  // namespace ash::diagnostics
