// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/device_command_run_routine_job.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/syslog_logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
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
chromeos::cros_healthd::mojom::RunRoutineResponsePtr
MakeInvalidParametersResponse() {
  return chromeos::cros_healthd::mojom::RunRoutineResponse::New(
      chromeos::cros_healthd::mojom::kFailedToStartId,
      chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum::
          kFailedToStart);
}

template <typename T>
bool PopulateMojoEnumValueIfValid(int possible_enum, T* valid_enum_out) {
  DCHECK(valid_enum_out);
  if (!base::IsValueInRangeForNumericType<
          typename std::underlying_type<T>::type>(possible_enum)) {
    return false;
  }
  T enum_to_check = static_cast<T>(possible_enum);
  if (!chromeos::cros_healthd::mojom::IsKnownEnumValue(enum_to_check))
    return false;
  *valid_enum_out = enum_to_check;
  return true;
}

}  // namespace

class DeviceCommandRunRoutineJob::Payload
    : public RemoteCommandJob::ResultPayload {
 public:
  explicit Payload(
      chromeos::cros_healthd::mojom::RunRoutineResponsePtr response);
  Payload(const Payload&) = delete;
  Payload& operator=(const Payload&) = delete;
  ~Payload() override = default;

  // RemoteCommandJob::ResultPayload:
  std::unique_ptr<std::string> Serialize() override;

 private:
  chromeos::cros_healthd::mojom::RunRoutineResponsePtr response_;
};

DeviceCommandRunRoutineJob::Payload::Payload(
    chromeos::cros_healthd::mojom::RunRoutineResponsePtr response)
    : response_(std::move(response)) {}

std::unique_ptr<std::string> DeviceCommandRunRoutineJob::Payload::Serialize() {
  std::string payload;
  base::Value root_dict(base::Value::Type::DICTIONARY);
  root_dict.SetIntKey(kIdFieldName, response_->id);
  root_dict.SetIntKey(kStatusFieldName, static_cast<int>(response_->status));
  base::JSONWriter::Write(root_dict, &payload);
  return std::make_unique<std::string>(std::move(payload));
}

// static
constexpr char DeviceCommandRunRoutineJob::kStunServerHostnameFieldName[];

DeviceCommandRunRoutineJob::DeviceCommandRunRoutineJob() = default;

DeviceCommandRunRoutineJob::~DeviceCommandRunRoutineJob() = default;

em::RemoteCommand_Type DeviceCommandRunRoutineJob::GetType() const {
  return em::RemoteCommand_Type_DEVICE_RUN_DIAGNOSTIC_ROUTINE;
}

bool DeviceCommandRunRoutineJob::ParseCommandPayload(
    const std::string& command_payload) {
  base::Optional<base::Value> root(base::JSONReader::Read(command_payload));
  if (!root.has_value())
    return false;
  if (!root->is_dict())
    return false;

  // Make sure the command payload specified a valid DiagnosticRoutineEnum.
  base::Optional<int> routine_enum = root->FindIntKey(kRoutineEnumFieldName);
  if (!routine_enum.has_value())
    return false;
  if (!PopulateMojoEnumValueIfValid(routine_enum.value(), &routine_enum_)) {
    SYSLOG(ERROR) << "Unknown DiagnosticRoutineEnum in command payload: "
                  << routine_enum.value();
    return false;
  }

  // Make sure there's a dictionary with parameter values for the routine.
  // Validation of routine-specific parameters will be done before running the
  // routine, so here we just check that any dictionary was given to us.
  auto* params_dict = root->FindDictKey(kParamsFieldName);
  if (!params_dict)
    return false;
  params_dict_ = std::move(*params_dict);

  return true;
}

void DeviceCommandRunRoutineJob::RunImpl(CallbackWithResult succeeded_callback,
                                         CallbackWithResult failed_callback) {
  SYSLOG(INFO) << "Executing RunRoutine command with DiagnosticRoutineEnum "
               << routine_enum_;

  switch (routine_enum_) {
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::
        kBatteryCapacity: {
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunBatteryCapacityRoutine(base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
              std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth: {
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunBatteryHealthRoutine(base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
              std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kUrandom: {
      constexpr char kLengthSecondsFieldName[] = "lengthSeconds";
      base::Optional<int> length_seconds =
          params_dict_.FindIntKey(kLengthSecondsFieldName);
      base::Optional<base::TimeDelta> routine_parameter;
      if (length_seconds.has_value()) {
        // If the optional integer parameter is specified, it must be >= 0.
        int value = length_seconds.value();
        if (value < 0) {
          SYSLOG(ERROR) << "Invalid parameters for Urandom routine.";
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE, base::BindOnce(std::move(failed_callback),
                                        std::make_unique<Payload>(
                                            MakeInvalidParametersResponse())));
          break;
        }
        routine_parameter = base::TimeDelta::FromSeconds(value);
      }
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunUrandomRoutine(
              routine_parameter,
              base::BindOnce(
                  &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
                  weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
                  std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kSmartctlCheck: {
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunSmartctlCheckRoutine(base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
              std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kAcPower: {
      constexpr char kExpectedStatusFieldName[] = "expectedStatus";
      // Note that expectedPowerType is an optional parameter.
      constexpr char kExpectedPowerTypeFieldName[] = "expectedPowerType";
      base::Optional<int> expected_status =
          params_dict_.FindIntKey(kExpectedStatusFieldName);
      std::string* expected_power_type =
          params_dict_.FindStringKey(kExpectedPowerTypeFieldName);
      chromeos::cros_healthd::mojom::AcPowerStatusEnum expected_status_enum;
      // The AC power routine expects a valid ACPowerStatusEnum, and optionally
      // a string.
      if (!expected_status.has_value() ||
          !PopulateMojoEnumValueIfValid(expected_status.value(),
                                        &expected_status_enum)) {
        SYSLOG(ERROR) << "Invalid parameters for AC Power routine.";
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(failed_callback),
                                      std::make_unique<Payload>(
                                          MakeInvalidParametersResponse())));
        break;
      }
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunAcPowerRoutine(
              expected_status_enum,
              expected_power_type
                  ? base::Optional<std::string>(*expected_power_type)
                  : base::nullopt,
              base::BindOnce(
                  &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
                  weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
                  std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuCache: {
      constexpr char kLengthSecondsFieldName[] = "lengthSeconds";
      base::Optional<int> length_seconds =
          params_dict_.FindIntKey(kLengthSecondsFieldName);
      base::Optional<base::TimeDelta> routine_duration;
      if (length_seconds.has_value()) {
        // If the optional integer parameter is specified, it must be >= 0.
        int value = length_seconds.value();
        if (value < 0) {
          SYSLOG(ERROR) << "Invalid parameters for CPU cache routine.";
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE, base::BindOnce(std::move(failed_callback),
                                        std::make_unique<Payload>(
                                            MakeInvalidParametersResponse())));
          break;
        }
        routine_duration = base::TimeDelta::FromSeconds(value);
      }
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunCpuCacheRoutine(
              routine_duration,
              base::BindOnce(
                  &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
                  weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
                  std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuStress: {
      constexpr char kLengthSecondsFieldName[] = "lengthSeconds";
      base::Optional<int> length_seconds =
          params_dict_.FindIntKey(kLengthSecondsFieldName);
      base::Optional<base::TimeDelta> routine_duration;
      if (length_seconds.has_value()) {
        // If the optional integer parameter is specified, it must be >= 0.
        int value = length_seconds.value();
        if (value < 0) {
          SYSLOG(ERROR) << "Invalid parameters for CPU stress routine.";
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE, base::BindOnce(std::move(failed_callback),
                                        std::make_unique<Payload>(
                                            MakeInvalidParametersResponse())));
          break;
        }
        routine_duration = base::TimeDelta::FromSeconds(value);
      }
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunCpuStressRoutine(
              routine_duration,
              base::BindOnce(
                  &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
                  weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
                  std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::
        kFloatingPointAccuracy: {
      constexpr char kLengthSecondsFieldName[] = "lengthSeconds";
      base::Optional<int> length_seconds =
          params_dict_.FindIntKey(kLengthSecondsFieldName);
      base::Optional<base::TimeDelta> routine_duration;
      if (length_seconds.has_value()) {
        // If the optional integer parameter is specified, it must be >= 0.
        int value = length_seconds.value();
        if (value < 0) {
          SYSLOG(ERROR)
              << "Invalid parameters for floating point accuracy routine.";
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE, base::BindOnce(std::move(failed_callback),
                                        std::make_unique<Payload>(
                                            MakeInvalidParametersResponse())));
          break;
        }
        routine_duration = base::TimeDelta::FromSeconds(value);
      }
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunFloatingPointAccuracyRoutine(
              routine_duration,
              base::BindOnce(
                  &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
                  weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
                  std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeWearLevel: {
      constexpr char kWearLevelThresholdFieldName[] = "wearLevelThreshold";
      base::Optional<int> wear_level_threshold =
          params_dict_.FindIntKey(kWearLevelThresholdFieldName);
      // The NVMe wear level routine expects one integer >= 0.
      if (!wear_level_threshold.has_value() ||
          wear_level_threshold.value() < 0) {
        SYSLOG(ERROR) << "Invalid parameters for NVMe wear level routine.";
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(failed_callback),
                                      std::make_unique<Payload>(
                                          MakeInvalidParametersResponse())));
        break;
      }
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunNvmeWearLevelRoutine(
              wear_level_threshold.value(),
              base::BindOnce(
                  &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
                  weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
                  std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeSelfTest: {
      constexpr char kNvmeSelfTestTypeFieldName[] = "nvmeSelfTestType";
      base::Optional<int> nvme_self_test_type =
          params_dict_.FindIntKey(kNvmeSelfTestTypeFieldName);
      chromeos::cros_healthd::mojom::NvmeSelfTestTypeEnum
          nvme_self_test_type_enum;
      // The NVMe self-test routine expects a valid NvmeSelfTestTypeEnum.
      if (!nvme_self_test_type.has_value() ||
          !PopulateMojoEnumValueIfValid(nvme_self_test_type.value(),
                                        &nvme_self_test_type_enum)) {
        SYSLOG(ERROR) << "Invalid parameters for NVMe self-test routine.";
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(failed_callback),
                                      std::make_unique<Payload>(
                                          MakeInvalidParametersResponse())));
        break;
      }
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunNvmeSelfTestRoutine(
              nvme_self_test_type_enum,
              base::BindOnce(
                  &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
                  weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
                  std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead: {
      constexpr char kTypeFieldName[] = "type";
      constexpr char kLengthSecondsFieldName[] = "lengthSeconds";
      constexpr char kFileSizeMbFieldName[] = "fileSizeMb";
      base::Optional<int> type = params_dict_.FindIntKey(kTypeFieldName);
      base::Optional<int> length_seconds =
          params_dict_.FindIntKey(kLengthSecondsFieldName);
      base::Optional<int> file_size_mb =
          params_dict_.FindIntKey(kFileSizeMbFieldName);
      chromeos::cros_healthd::mojom::DiskReadRoutineTypeEnum type_enum;
      if (!length_seconds.has_value() || length_seconds.value() < 0 ||
          !file_size_mb.has_value() || file_size_mb.value() < 0 ||
          !type.has_value() ||
          !PopulateMojoEnumValueIfValid(type.value(), &type_enum)) {
        SYSLOG(ERROR) << "Invalid parameters for disk read routine.";
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(failed_callback),
                                      std::make_unique<Payload>(
                                          MakeInvalidParametersResponse())));
        break;
      }
      auto exec_duration = base::TimeDelta::FromSeconds(length_seconds.value());
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunDiskReadRoutine(
              type_enum, exec_duration, file_size_mb.value(),
              base::BindOnce(
                  &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
                  weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
                  std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch: {
      constexpr char kLengthSecondsFieldName[] = "lengthSeconds";
      base::Optional<int> length_seconds =
          params_dict_.FindIntKey(kLengthSecondsFieldName);
      base::Optional<base::TimeDelta> routine_duration;
      if (length_seconds.has_value()) {
        // If the optional integer parameter is specified, it must be >= 0.
        int value = length_seconds.value();
        if (value < 0) {
          SYSLOG(ERROR) << "Invalid parameters for prime search routine.";
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE, base::BindOnce(std::move(failed_callback),
                                        std::make_unique<Payload>(
                                            MakeInvalidParametersResponse())));
          break;
        }
        routine_duration = base::TimeDelta::FromSeconds(value);
      }
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunPrimeSearchRoutine(
              routine_duration,
              base::BindOnce(
                  &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
                  weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
                  std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::
        kBatteryDischarge: {
      constexpr char kLengthSecondsFieldName[] = "lengthSeconds";
      constexpr char kMaximumDischargePercentAllowedFieldName[] =
          "maximumDischargePercentAllowed";
      base::Optional<int> length_seconds =
          params_dict_.FindIntKey(kLengthSecondsFieldName);
      base::Optional<int> maximum_discharge_percent_allowed =
          params_dict_.FindIntKey(kMaximumDischargePercentAllowedFieldName);
      // The battery discharge routine expects two integers >= 0.
      if (!length_seconds.has_value() ||
          !maximum_discharge_percent_allowed.has_value() ||
          length_seconds.value() < 0 ||
          maximum_discharge_percent_allowed.value() < 0) {
        SYSLOG(ERROR) << "Invalid parameters for BatteryDischarge routine.";
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(failed_callback),
                                      std::make_unique<Payload>(
                                          MakeInvalidParametersResponse())));
        break;
      }
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunBatteryDischargeRoutine(
              base::TimeDelta::FromSeconds(length_seconds.value()),
              maximum_discharge_percent_allowed.value(),
              base::BindOnce(
                  &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
                  weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
                  std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCharge: {
      constexpr char kLengthSecondsFieldName[] = "lengthSeconds";
      constexpr char kMinimumChargePercentRequiredFieldName[] =
          "minimumChargePercentRequired";
      base::Optional<int> length_seconds =
          params_dict_.FindIntKey(kLengthSecondsFieldName);
      base::Optional<int> minimum_charge_percent_required =
          params_dict_.FindIntKey(kMinimumChargePercentRequiredFieldName);
      // The battery charge routine expects two integers >= 0.
      if (!length_seconds.has_value() ||
          !minimum_charge_percent_required.has_value() ||
          length_seconds.value() < 0 ||
          minimum_charge_percent_required.value() < 0) {
        SYSLOG(ERROR) << "Invalid parameters for BatteryCharge routine.";
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(failed_callback),
                                      std::make_unique<Payload>(
                                          MakeInvalidParametersResponse())));
        break;
      }
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunBatteryChargeRoutine(
              base::TimeDelta::FromSeconds(length_seconds.value()),
              minimum_charge_percent_required.value(),
              base::BindOnce(
                  &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
                  weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
                  std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kMemory: {
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunMemoryRoutine(base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
              std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::
        kLanConnectivity: {
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunLanConnectivityRoutine(base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
              std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::
        kSignalStrength: {
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunSignalStrengthRoutine(base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
              std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::
        kGatewayCanBePinged: {
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunGatewayCanBePingedRoutine(base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
              std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::
        kHasSecureWiFiConnection: {
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunHasSecureWiFiConnectionRoutine(base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
              std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::
        kDnsResolverPresent: {
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunDnsResolverPresentRoutine(base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
              std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDnsLatency: {
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunDnsLatencyRoutine(base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
              std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDnsResolution: {
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunDnsResolutionRoutine(base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
              std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kCaptivePortal: {
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunCaptivePortalRoutine(base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
              std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kHttpFirewall: {
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunHttpFirewallRoutine(base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
              std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kHttpsFirewall: {
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunHttpsFirewallRoutine(base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
              std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kHttpsLatency: {
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunHttpsLatencyRoutine(base::BindOnce(
              &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
              weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
              std::move(failed_callback)));
      break;
    }
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::
        kVideoConferencing: {
      std::string* stun_server_hostname =
          params_dict_.FindStringKey(kStunServerHostnameFieldName);
      chromeos::cros_healthd::ServiceConnection::GetInstance()
          ->RunVideoConferencingRoutine(
              stun_server_hostname
                  ? base::make_optional<std::string>(*stun_server_hostname)
                  : base::nullopt,
              base::BindOnce(
                  &DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived,
                  weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
                  std::move(failed_callback)));
      break;
    }
  }
}

void DeviceCommandRunRoutineJob::OnCrosHealthdResponseReceived(
    CallbackWithResult succeeded_callback,
    CallbackWithResult failed_callback,
    chromeos::cros_healthd::mojom::RunRoutineResponsePtr response) {
  if (!response) {
    SYSLOG(ERROR) << "No RunRoutineResponse received from cros_healthd.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(failed_callback), nullptr));
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(succeeded_callback),
                     std::make_unique<Payload>(std::move(response))));
}

}  // namespace policy
