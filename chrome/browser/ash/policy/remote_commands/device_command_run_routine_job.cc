// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_run_routine_job.h"

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/nullable_primitives.mojom.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

namespace em = enterprise_management;

namespace {

// String constant identifying the id field in the result payload.
constexpr char kIdFieldName[] = "id";
// String constant identifying the status field in the result payload.
constexpr char kStatusFieldName[] = "status";

// String constant identifying the routine enum field in the command payload.
constexpr char kRoutineEnumFieldName[] = "routineEnum";
// String constant identifying the parameter dictionary field in the command
// payload.
constexpr char kParamsFieldName[] = "params";

// Returns a RunRoutineResponse with an id of kFailedToStartId and a status of
// kFailedToStart.
ash::cros_healthd::mojom::RunRoutineResponsePtr
MakeInvalidParametersResponse() {
  return ash::cros_healthd::mojom::RunRoutineResponse::New(
      ash::cros_healthd::mojom::kFailedToStartId,
      ash::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kFailedToStart);
}

template <typename T>
bool PopulateMojoEnumValueIfValid(int possible_enum, T* valid_enum_out) {
  DCHECK(valid_enum_out);
  if (!base::IsValueInRangeForNumericType<
          typename std::underlying_type<T>::type>(possible_enum)) {
    return false;
  }
  T enum_to_check = static_cast<T>(possible_enum);
  if (!ash::cros_healthd::mojom::IsKnownEnumValue(enum_to_check)) {
    return false;
  }
  *valid_enum_out = enum_to_check;
  return true;
}

std::string CreatePayload(
    ash::cros_healthd::mojom::RunRoutineResponsePtr response) {
  auto root_dict =
      base::Value::Dict()
          .Set(kIdFieldName, response->id)
          .Set(kStatusFieldName, static_cast<int>(response->status));

  std::string payload;
  base::JSONWriter::Write(root_dict, &payload);
  return payload;
}
}  // namespace

// static
constexpr char DeviceCommandRunRoutineJob::kStunServerHostnameFieldName[];

DeviceCommandRunRoutineJob::DeviceCommandRunRoutineJob() = default;

DeviceCommandRunRoutineJob::~DeviceCommandRunRoutineJob() = default;

em::RemoteCommand_Type DeviceCommandRunRoutineJob::GetType() const {
  return em::RemoteCommand_Type_DEVICE_RUN_DIAGNOSTIC_ROUTINE;
}

bool DeviceCommandRunRoutineJob::ParseCommandPayload(
    const std::string& command_payload) {
  std::optional<base::Value> root(base::JSONReader::Read(command_payload));
  if (!root.has_value()) {
    return false;
  }
  if (!root->is_dict()) {
    return false;
  }

  base::Value::Dict& dict = root->GetDict();
  // Make sure the command payload specified a valid DiagnosticRoutineEnum.
  std::optional<int> routine_enum = dict.FindInt(kRoutineEnumFieldName);
  if (!routine_enum.has_value()) {
    return false;
  }
  if (!PopulateMojoEnumValueIfValid(routine_enum.value(), &routine_enum_)) {
    SYSLOG(ERROR) << "Unknown DiagnosticRoutineEnum in command payload: "
                  << routine_enum.value();
    return false;
  }

  // Make sure there's a dictionary with parameter values for the routine.
  // Validation of routine-specific parameters will be done before running the
  // routine, so here we just check that any dictionary was given to us.
  auto* params_dict = dict.FindDict(kParamsFieldName);
  if (!params_dict) {
    return false;
  }
  params_dict_ = std::move(*params_dict);

  return true;
}

void DeviceCommandRunRoutineJob::RunImpl(CallbackWithResult result_callback) {
  SYSLOG(INFO) << "Executing RunRoutine command with DiagnosticRoutineEnum "
               << routine_enum_;
  auto* diagnostics_service =
      ash::cros_healthd::ServiceConnection::GetInstance()
          ->GetDiagnosticsService();

  switch (routine_enum_) {
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kUnknown: {
      NOTREACHED_IN_MIGRATION() << "This default value should not be used.";
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity: {
      diagnostics_service->RunBatteryCapacityRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth: {
      diagnostics_service->RunBatteryHealthRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kUrandom: {
      constexpr char kLengthSecondsFieldName[] = "lengthSeconds";
      std::optional<int> length_seconds =
          params_dict_.FindInt(kLengthSecondsFieldName);
      ash::cros_healthd::mojom::NullableUint32Ptr routine_duration;
      if (length_seconds.has_value()) {
        // If the optional integer parameter is specified, it must be >= 0.
        int value = length_seconds.value();
        if (value < 0) {
          SYSLOG(ERROR) << "Invalid parameters for Urandom routine.";
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(result_callback), ResultType::kFailure,
                             CreatePayload(MakeInvalidParametersResponse())));
          break;
        }
        routine_duration = ash::cros_healthd::mojom::NullableUint32::New(value);
      }
      diagnostics_service->RunUrandomRoutine(
          std::move(routine_duration),
          base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kSmartctlCheck: {
      diagnostics_service->RunSmartctlCheckRoutine(
          ash::cros_healthd::mojom::NullableUint32Ptr(),
          base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::
        kSmartctlCheckWithPercentageUsed: {
      constexpr char kPercentageUsedThresholdFieldName[] =
          "percentageUsedThreshold";
      std::optional<int> percentage_used_threshold =
          params_dict_.FindInt(kPercentageUsedThresholdFieldName);
      ash::cros_healthd::mojom::NullableUint32Ptr input_threshold;
      // The smartctl check routine expects one optional integer >= 0.
      if (percentage_used_threshold.has_value()) {
        // If the optional integer parameter is specified, it must be [0, 255].
        int value = percentage_used_threshold.value();
        if (value < 0 || value > 255) {
          SYSLOG(ERROR) << "Invalid parameters for smartctl check routine.";
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(result_callback), ResultType::kFailure,
                             CreatePayload(MakeInvalidParametersResponse())));
          break;
        }
        input_threshold = ash::cros_healthd::mojom::NullableUint32::New(value);
      }
      diagnostics_service->RunSmartctlCheckRoutine(
          std::move(input_threshold),
          base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kAcPower: {
      constexpr char kExpectedStatusFieldName[] = "expectedStatus";
      // Note that expectedPowerType is an optional parameter.
      constexpr char kExpectedPowerTypeFieldName[] = "expectedPowerType";
      std::optional<int> expected_status =
          params_dict_.FindInt(kExpectedStatusFieldName);
      std::string* expected_power_type =
          params_dict_.FindString(kExpectedPowerTypeFieldName);
      ash::cros_healthd::mojom::AcPowerStatusEnum expected_status_enum;
      // The AC power routine expects a valid ACPowerStatusEnum, and optionally
      // a string.
      if (!expected_status.has_value() ||
          !PopulateMojoEnumValueIfValid(expected_status.value(),
                                        &expected_status_enum)) {
        SYSLOG(ERROR) << "Invalid parameters for AC Power routine.";
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(result_callback), ResultType::kFailure,
                           CreatePayload(MakeInvalidParametersResponse())));
        break;
      }
      diagnostics_service->RunAcPowerRoutine(
          expected_status_enum,
          expected_power_type ? std::optional<std::string>(*expected_power_type)
                              : std::nullopt,
          base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuCache: {
      constexpr char kLengthSecondsFieldName[] = "lengthSeconds";
      std::optional<int> length_seconds =
          params_dict_.FindInt(kLengthSecondsFieldName);
      ash::cros_healthd::mojom::NullableUint32Ptr routine_duration;
      if (length_seconds.has_value()) {
        // If the optional integer parameter is specified, it must be >= 0.
        int value = length_seconds.value();
        if (value < 0) {
          SYSLOG(ERROR) << "Invalid parameters for CPU cache routine.";
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(result_callback), ResultType::kFailure,
                             CreatePayload(MakeInvalidParametersResponse())));
          break;
        }
        routine_duration = ash::cros_healthd::mojom::NullableUint32::New(value);
      }
      diagnostics_service->RunCpuCacheRoutine(
          std::move(routine_duration),
          base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuStress: {
      constexpr char kLengthSecondsFieldName[] = "lengthSeconds";
      std::optional<int> length_seconds =
          params_dict_.FindInt(kLengthSecondsFieldName);
      ash::cros_healthd::mojom::NullableUint32Ptr routine_duration;
      if (length_seconds.has_value()) {
        // If the optional integer parameter is specified, it must be >= 0.
        int value = length_seconds.value();
        if (value < 0) {
          SYSLOG(ERROR) << "Invalid parameters for CPU stress routine.";
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(result_callback), ResultType::kFailure,
                             CreatePayload(MakeInvalidParametersResponse())));
          break;
        }
        routine_duration = ash::cros_healthd::mojom::NullableUint32::New(value);
      }
      diagnostics_service->RunCpuStressRoutine(
          std::move(routine_duration),
          base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::
        kFloatingPointAccuracy: {
      constexpr char kLengthSecondsFieldName[] = "lengthSeconds";
      std::optional<int> length_seconds =
          params_dict_.FindInt(kLengthSecondsFieldName);
      ash::cros_healthd::mojom::NullableUint32Ptr routine_duration;
      if (length_seconds.has_value()) {
        // If the optional integer parameter is specified, it must be >= 0.
        int value = length_seconds.value();
        if (value < 0) {
          SYSLOG(ERROR)
              << "Invalid parameters for floating point accuracy routine.";
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(result_callback), ResultType::kFailure,
                             CreatePayload(MakeInvalidParametersResponse())));
          break;
        }
        routine_duration = ash::cros_healthd::mojom::NullableUint32::New(value);
      }
      diagnostics_service->RunFloatingPointAccuracyRoutine(
          std::move(routine_duration),
          base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::
        DEPRECATED_kNvmeWearLevel: {
      NOTIMPLEMENTED();
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeSelfTest: {
      constexpr char kNvmeSelfTestTypeFieldName[] = "nvmeSelfTestType";
      std::optional<int> nvme_self_test_type =
          params_dict_.FindInt(kNvmeSelfTestTypeFieldName);
      ash::cros_healthd::mojom::NvmeSelfTestTypeEnum nvme_self_test_type_enum;
      // The NVMe self-test routine expects a valid NvmeSelfTestTypeEnum.
      if (!nvme_self_test_type.has_value() ||
          !PopulateMojoEnumValueIfValid(nvme_self_test_type.value(),
                                        &nvme_self_test_type_enum)) {
        SYSLOG(ERROR) << "Invalid parameters for NVMe self-test routine.";
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(result_callback), ResultType::kFailure,
                           CreatePayload(MakeInvalidParametersResponse())));
        break;
      }
      diagnostics_service->RunNvmeSelfTestRoutine(
          nvme_self_test_type_enum,
          base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead: {
      constexpr char kTypeFieldName[] = "type";
      constexpr char kLengthSecondsFieldName[] = "lengthSeconds";
      constexpr char kFileSizeMbFieldName[] = "fileSizeMb";
      std::optional<int> type = params_dict_.FindInt(kTypeFieldName);
      std::optional<int> length_seconds =
          params_dict_.FindInt(kLengthSecondsFieldName);
      std::optional<int> file_size_mb =
          params_dict_.FindInt(kFileSizeMbFieldName);
      ash::cros_healthd::mojom::DiskReadRoutineTypeEnum type_enum;
      if (!length_seconds.has_value() || length_seconds.value() < 0 ||
          !file_size_mb.has_value() || file_size_mb.value() < 0 ||
          !type.has_value() ||
          !PopulateMojoEnumValueIfValid(type.value(), &type_enum)) {
        SYSLOG(ERROR) << "Invalid parameters for disk read routine.";
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(result_callback), ResultType::kFailure,
                           CreatePayload(MakeInvalidParametersResponse())));
        break;
      }
      diagnostics_service->RunDiskReadRoutine(
          type_enum, length_seconds.value(), file_size_mb.value(),
          base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch: {
      constexpr char kLengthSecondsFieldName[] = "lengthSeconds";
      std::optional<int> length_seconds =
          params_dict_.FindInt(kLengthSecondsFieldName);
      ash::cros_healthd::mojom::NullableUint32Ptr routine_duration;
      if (length_seconds.has_value()) {
        // If the optional integer parameter is specified, it must be >= 0.
        int value = length_seconds.value();
        if (value < 0) {
          SYSLOG(ERROR) << "Invalid parameters for prime search routine.";
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(result_callback), ResultType::kFailure,
                             CreatePayload(MakeInvalidParametersResponse())));
          break;
        }
        routine_duration = ash::cros_healthd::mojom::NullableUint32::New(value);
      }
      diagnostics_service->RunPrimeSearchRoutine(
          std::move(routine_duration),
          base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge: {
      constexpr char kLengthSecondsFieldName[] = "lengthSeconds";
      constexpr char kMaximumDischargePercentAllowedFieldName[] =
          "maximumDischargePercentAllowed";
      std::optional<int> length_seconds =
          params_dict_.FindInt(kLengthSecondsFieldName);
      std::optional<int> maximum_discharge_percent_allowed =
          params_dict_.FindInt(kMaximumDischargePercentAllowedFieldName);
      // The battery discharge routine expects two integers >= 0.
      if (!length_seconds.has_value() ||
          !maximum_discharge_percent_allowed.has_value() ||
          length_seconds.value() < 0 ||
          maximum_discharge_percent_allowed.value() < 0) {
        SYSLOG(ERROR) << "Invalid parameters for BatteryDischarge routine.";
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(result_callback), ResultType::kFailure,
                           CreatePayload(MakeInvalidParametersResponse())));
        break;
      }
      diagnostics_service->RunBatteryDischargeRoutine(
          length_seconds.value(), maximum_discharge_percent_allowed.value(),
          base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCharge: {
      constexpr char kLengthSecondsFieldName[] = "lengthSeconds";
      constexpr char kMinimumChargePercentRequiredFieldName[] =
          "minimumChargePercentRequired";
      std::optional<int> length_seconds =
          params_dict_.FindInt(kLengthSecondsFieldName);
      std::optional<int> minimum_charge_percent_required =
          params_dict_.FindInt(kMinimumChargePercentRequiredFieldName);
      // The battery charge routine expects two integers >= 0.
      if (!length_seconds.has_value() ||
          !minimum_charge_percent_required.has_value() ||
          length_seconds.value() < 0 ||
          minimum_charge_percent_required.value() < 0) {
        SYSLOG(ERROR) << "Invalid parameters for BatteryCharge routine.";
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(result_callback), ResultType::kFailure,
                           CreatePayload(MakeInvalidParametersResponse())));
        break;
      }
      diagnostics_service->RunBatteryChargeRoutine(
          length_seconds.value(), minimum_charge_percent_required.value(),
          base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kMemory: {
      diagnostics_service->RunMemoryRoutine(
          std::nullopt,
          base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kLanConnectivity: {
      diagnostics_service->RunLanConnectivityRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kSignalStrength: {
      diagnostics_service->RunSignalStrengthRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kGatewayCanBePinged: {
      diagnostics_service->RunGatewayCanBePingedRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::
        kHasSecureWiFiConnection: {
      diagnostics_service->RunHasSecureWiFiConnectionRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kDnsResolverPresent: {
      diagnostics_service->RunDnsResolverPresentRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kDnsLatency: {
      diagnostics_service->RunDnsLatencyRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kDnsResolution: {
      diagnostics_service->RunDnsResolutionRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kCaptivePortal: {
      diagnostics_service->RunCaptivePortalRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kHttpFirewall: {
      diagnostics_service->RunHttpFirewallRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kHttpsFirewall: {
      diagnostics_service->RunHttpsFirewallRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kHttpsLatency: {
      diagnostics_service->RunHttpsLatencyRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kVideoConferencing: {
      std::string* stun_server_hostname =
          params_dict_.FindString(kStunServerHostnameFieldName);
      diagnostics_service->RunVideoConferencingRoutine(
          stun_server_hostname
              ? std::make_optional<std::string>(*stun_server_hostname)
              : std::nullopt,
          base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kArcHttp: {
      diagnostics_service->RunArcHttpRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kArcPing: {
      diagnostics_service->RunArcPingRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kArcDnsResolution: {
      diagnostics_service->RunArcDnsResolutionRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kSensitiveSensor: {
      diagnostics_service->RunSensitiveSensorRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kFingerprint: {
      diagnostics_service->RunFingerprintRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kFingerprintAlive: {
      diagnostics_service->RunFingerprintAliveRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kPrivacyScreen: {
      constexpr char kPrivacyScreenTargetState[] = "targetState";
      std::optional<bool> target_state =
          params_dict_.FindBool(kPrivacyScreenTargetState);
      diagnostics_service->RunPrivacyScreenRoutine(
          target_state.value_or(true),
          base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::
        DEPRECATED_kLedLitUp: {
      NOTIMPLEMENTED();
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kEmmcLifetime: {
      diagnostics_service->RunEmmcLifetimeRoutine(base::BindOnce(
          &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::
        DEPRECATED_kAudioSetVolume: {
      NOTIMPLEMENTED();
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::
        DEPRECATED_kAudioSetGain: {
      NOTIMPLEMENTED();
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBluetoothPower: {
      NOTIMPLEMENTED();
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBluetoothDiscovery: {
      NOTIMPLEMENTED();
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBluetoothScanning: {
      NOTIMPLEMENTED();
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kBluetoothPairing: {
      NOTIMPLEMENTED();
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kPowerButton: {
      NOTIMPLEMENTED();
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kAudioDriver: {
      NOTIMPLEMENTED();
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kUfsLifetime: {
      NOTIMPLEMENTED();
      break;
    }
    case ash::cros_healthd::mojom::DiagnosticRoutineEnum::kFan: {
      NOTIMPLEMENTED();
      break;
    }
  }
}

void DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived(
    CallbackWithResult result_callback,
    ash::cros_healthd::mojom::RunRoutineResponsePtr response) {
  if (!response) {
    SYSLOG(ERROR) << "No RunRoutineResponse received from cros_healthd.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback),
                                  ResultType::kFailure, std::nullopt));
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(result_callback), ResultType::kSuccess,
                     CreatePayload(std::move(response))));
}

}  // namespace policy
