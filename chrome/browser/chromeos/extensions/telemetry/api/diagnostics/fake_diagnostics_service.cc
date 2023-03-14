// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/fake_diagnostics_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom-shared.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

FakeDiagnosticsService::FakeDiagnosticsService() : receiver_(this) {}

FakeDiagnosticsService::~FakeDiagnosticsService() {
  // Test if the previously set expectations are met.
  EXPECT_EQ(actual_passed_parameters_, expected_passed_parameters_);
  EXPECT_EQ(actual_called_routine_, expected_called_routine_);
}

void FakeDiagnosticsService::BindPendingReceiver(
    mojo::PendingReceiver<crosapi::mojom::DiagnosticsService> receiver) {
  receiver_.Bind(std::move(receiver));
}

void FakeDiagnosticsService::GetAvailableRoutines(
    GetAvailableRoutinesCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), available_routines_response_));
}

void FakeDiagnosticsService::GetRoutineUpdate(
    int32_t id,
    crosapi::mojom::DiagnosticsRoutineCommandEnum command,
    bool include_output,
    GetRoutineUpdateCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("id", id);
  actual_passed_parameters_.Set("command", static_cast<int32_t>(command));
  actual_passed_parameters_.Set("include_output", include_output);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), routine_update_response_->Clone()));
}

void FakeDiagnosticsService::RunAcPowerRoutine(
    crosapi::mojom::DiagnosticsAcPowerStatusEnum expected_status,
    const absl::optional<std::string>& expected_power_type,
    RunAcPowerRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("expected_status",
                                static_cast<int32_t>(expected_status));
  if (expected_power_type.has_value()) {
    actual_passed_parameters_.Set("expected_power_type",
                                  expected_power_type.value());
  }

  actual_called_routine_ = crosapi::mojom::DiagnosticsRoutineEnum::kAcPower;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunBatteryCapacityRoutine(
    RunBatteryCapacityRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_called_routine_ =
      crosapi::mojom::DiagnosticsRoutineEnum::kBatteryCapacity;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunBatteryChargeRoutine(
    uint32_t length_seconds,
    uint32_t minimum_charge_percent_required,
    RunBatteryChargeRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("length_seconds",
                                static_cast<int32_t>(length_seconds));
  actual_passed_parameters_.Set(
      "minimum_charge_percent_required",
      static_cast<int32_t>(minimum_charge_percent_required));

  actual_called_routine_ =
      crosapi::mojom::DiagnosticsRoutineEnum::kBatteryCharge;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunBatteryDischargeRoutine(
    uint32_t length_seconds,
    uint32_t maximum_discharge_percent_allowed,
    RunBatteryDischargeRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("length_seconds",
                                static_cast<int32_t>(length_seconds));
  actual_passed_parameters_.Set(
      "maximum_discharge_percent_allowed",
      static_cast<int32_t>(maximum_discharge_percent_allowed));

  actual_called_routine_ =
      crosapi::mojom::DiagnosticsRoutineEnum::kBatteryDischarge;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunBatteryHealthRoutine(
    RunBatteryHealthRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_called_routine_ =
      crosapi::mojom::DiagnosticsRoutineEnum::kBatteryHealth;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunCpuCacheRoutine(
    uint32_t length_seconds,
    RunCpuCacheRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("length_seconds",
                                static_cast<int32_t>(length_seconds));

  actual_called_routine_ = crosapi::mojom::DiagnosticsRoutineEnum::kCpuCache;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunCpuStressRoutine(
    uint32_t length_seconds,
    RunCpuStressRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("length_seconds",
                                static_cast<int32_t>(length_seconds));

  actual_called_routine_ = crosapi::mojom::DiagnosticsRoutineEnum::kCpuStress;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunDiskReadRoutine(
    crosapi::mojom::DiagnosticsDiskReadRoutineTypeEnum type,
    uint32_t length_seconds,
    uint32_t file_size_mb,
    RunDiskReadRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("type", static_cast<int32_t>(type));
  actual_passed_parameters_.Set("length_seconds",
                                static_cast<int32_t>(length_seconds));
  actual_passed_parameters_.Set("file_size_mb",
                                static_cast<int32_t>(file_size_mb));

  actual_called_routine_ = crosapi::mojom::DiagnosticsRoutineEnum::kDiskRead;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunDnsResolutionRoutine(
    RunDnsResolutionRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_called_routine_ =
      crosapi::mojom::DiagnosticsRoutineEnum::kDnsResolution;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunDnsResolverPresentRoutine(
    RunDnsResolverPresentRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_called_routine_ =
      crosapi::mojom::DiagnosticsRoutineEnum::kDnsResolverPresent;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()));
}

void FakeDiagnosticsService::RunEmmcLifetimeRoutine(
    RunEmmcLifetimeRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_called_routine_ =
      crosapi::mojom::DiagnosticsRoutineEnum::kEmmcLifetime;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()));
}

void FakeDiagnosticsService::RunFloatingPointAccuracyRoutine(
    uint32_t length_seconds,
    RunFloatingPointAccuracyRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("length_seconds",
                                static_cast<int32_t>(length_seconds));

  actual_called_routine_ =
      crosapi::mojom::DiagnosticsRoutineEnum::kFloatingPointAccuracy;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunFingerprintAliveRoutine(
    RunFingerprintAliveRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_called_routine_ =
      crosapi::mojom::DiagnosticsRoutineEnum::kFingerprintAlive;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()));
}

void FakeDiagnosticsService::RunGatewayCanBePingedRoutine(
    RunGatewayCanBePingedRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_called_routine_ =
      crosapi::mojom::DiagnosticsRoutineEnum::kGatewayCanBePinged;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_.Clone()));
}

void FakeDiagnosticsService::RunLanConnectivityRoutine(
    RunLanConnectivityRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_called_routine_ =
      crosapi::mojom::DiagnosticsRoutineEnum::kLanConnectivity;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunMemoryRoutine(
    RunMemoryRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_called_routine_ = crosapi::mojom::DiagnosticsRoutineEnum::kMemory;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunNvmeSelfTestRoutine(
    crosapi::mojom::DiagnosticsNvmeSelfTestTypeEnum nvme_self_test_type,
    RunNvmeSelfTestRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("test_type",
                                static_cast<int32_t>(nvme_self_test_type));

  actual_called_routine_ =
      crosapi::mojom::DiagnosticsRoutineEnum::kNvmeSelfTest;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunNvmeWearLevelRoutine(
    uint32_t wear_level_threshold,
    RunNvmeWearLevelRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("wear_level_threshold",
                                static_cast<int32_t>(wear_level_threshold));

  actual_called_routine_ =
      crosapi::mojom::DiagnosticsRoutineEnum::kNvmeWearLevel;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunPrimeSearchRoutine(
    uint32_t length_seconds,
    RunPrimeSearchRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("length_seconds",
                                static_cast<int32_t>(length_seconds));

  actual_called_routine_ = crosapi::mojom::DiagnosticsRoutineEnum::kPrimeSearch;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunSensitiveSensorRoutine(
    RunSensitiveSensorRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_called_routine_ =
      crosapi::mojom::DiagnosticsRoutineEnum::kSensitiveSensor;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunSignalStrengthRoutine(
    RunSignalStrengthRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_called_routine_ =
      crosapi::mojom::DiagnosticsRoutineEnum::kSignalStrength;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunSmartctlCheckRoutine(
    crosapi::mojom::UInt32ValuePtr percentage_used_threshold,
    RunSmartctlCheckRoutineCallback callback) {
  actual_passed_parameters_.clear();

  if (percentage_used_threshold) {
    actual_passed_parameters_.Set(
        "percentage_used_threshold",
        static_cast<int32_t>(percentage_used_threshold->value));
    actual_called_routine_ = crosapi::mojom::DiagnosticsRoutineEnum::
        kSmartctlCheckWithPercentageUsed;
  } else {
    actual_called_routine_ =
        crosapi::mojom::DiagnosticsRoutineEnum::kSmartctlCheck;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::SetRunRoutineResponse(
    crosapi::mojom::DiagnosticsRunRoutineResponsePtr response) {
  run_routine_response_ = std::move(response);
}

void FakeDiagnosticsService::SetAvailableRoutines(
    std::vector<crosapi::mojom::DiagnosticsRoutineEnum> available_routines) {
  available_routines_response_ = available_routines;
}

void FakeDiagnosticsService::SetRoutineUpdateResponse(
    crosapi::mojom::DiagnosticsRoutineUpdatePtr routine_update) {
  routine_update_response_ = std::move(routine_update);
}

void FakeDiagnosticsService::SetExpectedLastPassedParameters(
    base::Value::Dict expected_passed_parameter) {
  expected_passed_parameters_ = std::move(expected_passed_parameter);
}

void FakeDiagnosticsService::SetExpectedLastCalledRoutine(
    crosapi::mojom::DiagnosticsRoutineEnum expected_called_routine) {
  expected_called_routine_ = expected_called_routine;
}

}  // namespace chromeos
