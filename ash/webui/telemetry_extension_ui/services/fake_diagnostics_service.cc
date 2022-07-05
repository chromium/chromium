// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "ash/webui/telemetry_extension_ui/mojom/diagnostics_service.mojom.h"
#include "ash/webui/telemetry_extension_ui/services/fake_diagnostics_service.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

FakeDiagnosticsService::Factory::Factory() = default;
FakeDiagnosticsService::Factory::~Factory() = default;

void FakeDiagnosticsService::Factory::SetCreateInstanceResponse(
    std::unique_ptr<FakeDiagnosticsService> fake_service) {
  fake_service_ = std::move(fake_service);
}

std::unique_ptr<health::mojom::DiagnosticsService>
FakeDiagnosticsService::Factory::CreateInstance(
    mojo::PendingReceiver<health::mojom::DiagnosticsService> receiver) {
  DCHECK(fake_service_);
  fake_service_->BindPendingReceiver(std::move(receiver));
  return std::move(fake_service_);
}

FakeDiagnosticsService::FakeDiagnosticsService() : receiver_(this) {}

FakeDiagnosticsService::~FakeDiagnosticsService() {
  // Test if the previously set expectations are met.
  EXPECT_EQ(actual_passed_parameters_, expected_passed_parameters_);
  EXPECT_EQ(actual_called_routine_, expected_called_routine_);
}

void FakeDiagnosticsService::GetAvailableRoutines(
    GetAvailableRoutinesCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), available_routines_response_));
}

void FakeDiagnosticsService::GetRoutineUpdate(
    int32_t id,
    health::mojom::DiagnosticRoutineCommandEnum command,
    bool include_output,
    GetRoutineUpdateCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("id", id);
  actual_passed_parameters_.Set("command", static_cast<int32_t>(command));
  actual_passed_parameters_.Set("include_output", include_output);

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), routine_update_response_->Clone()));
}

void FakeDiagnosticsService::RunBatteryCapacityRoutine(
    RunBatteryCapacityRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_called_routine_ =
      health::mojom::DiagnosticRoutineEnum::kBatteryCapacity;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunBatteryHealthRoutine(
    RunBatteryHealthRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_called_routine_ = health::mojom::DiagnosticRoutineEnum::kBatteryHealth;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunSmartctlCheckRoutine(
    RunSmartctlCheckRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_called_routine_ = health::mojom::DiagnosticRoutineEnum::kSmartctlCheck;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunAcPowerRoutine(
    health::mojom::AcPowerStatusEnum expected_status,
    const absl::optional<std::string>& expected_power_type,
    RunAcPowerRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("expected_status",
                                static_cast<int32_t>(expected_status));
  if (expected_power_type.has_value()) {
    actual_passed_parameters_.Set("expected_power_type",
                                  expected_power_type.value());
  }

  actual_called_routine_ = health::mojom::DiagnosticRoutineEnum::kAcPower;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunCpuCacheRoutine(
    uint32_t length_seconds,
    RunCpuCacheRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("length_seconds",
                                static_cast<int32_t>(length_seconds));

  actual_called_routine_ = health::mojom::DiagnosticRoutineEnum::kCpuCache;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunCpuStressRoutine(
    uint32_t length_seconds,
    RunCpuStressRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("length_seconds",
                                static_cast<int32_t>(length_seconds));

  actual_called_routine_ = health::mojom::DiagnosticRoutineEnum::kCpuStress;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunFloatingPointAccuracyRoutine(
    uint32_t length_seconds,
    RunFloatingPointAccuracyRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("length_seconds",
                                static_cast<int32_t>(length_seconds));

  actual_called_routine_ =
      health::mojom::DiagnosticRoutineEnum::kFloatingPointAccuracy;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunNvmeWearLevelRoutine(
    uint32_t wear_level_threshold,
    RunNvmeWearLevelRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("wear_level_threshold",
                                static_cast<int32_t>(wear_level_threshold));

  actual_called_routine_ = health::mojom::DiagnosticRoutineEnum::kNvmeWearLevel;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunNvmeSelfTestRoutine(
    health::mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
    RunNvmeSelfTestRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("nvme_self_test_type",
                                static_cast<int32_t>(nvme_self_test_type));

  actual_called_routine_ = health::mojom::DiagnosticRoutineEnum::kNvmeSelfTest;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunDiskReadRoutine(
    health::mojom::DiskReadRoutineTypeEnum type,
    uint32_t length_seconds,
    uint32_t file_size_mb,
    RunDiskReadRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("type", static_cast<int32_t>(type));
  actual_passed_parameters_.Set("length_seconds",
                                static_cast<int32_t>(length_seconds));
  actual_passed_parameters_.Set("file_size_mb",
                                static_cast<int32_t>(file_size_mb));

  actual_called_routine_ = health::mojom::DiagnosticRoutineEnum::kDiskRead;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunPrimeSearchRoutine(
    uint32_t length_seconds,
    RunPrimeSearchRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_passed_parameters_.Set("length_seconds",
                                static_cast<int32_t>(length_seconds));

  actual_called_routine_ = health::mojom::DiagnosticRoutineEnum::kPrimeSearch;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
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
      health::mojom::DiagnosticRoutineEnum::kBatteryDischarge;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
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

  actual_called_routine_ = health::mojom::DiagnosticRoutineEnum::kBatteryCharge;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunMemoryRoutine(
    RunMemoryRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_called_routine_ = health::mojom::DiagnosticRoutineEnum::kMemory;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::RunLanConnectivityRoutine(
    RunLanConnectivityRoutineCallback callback) {
  actual_passed_parameters_.clear();
  actual_called_routine_ =
      health::mojom::DiagnosticRoutineEnum::kLanConnectivity;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), run_routine_response_->Clone()));
}

void FakeDiagnosticsService::SetRunRoutineResponse(
    health::mojom::RunRoutineResponsePtr response) {
  run_routine_response_ = std::move(response);
}

void FakeDiagnosticsService::SetAvailableRoutines(
    std::vector<health::mojom::DiagnosticRoutineEnum> available_routines) {
  available_routines_response_ = available_routines;
}

void FakeDiagnosticsService::SetRoutineUpdateResponse(
    health::mojom::RoutineUpdatePtr routine_update) {
  routine_update_response_ = std::move(routine_update);
}

void FakeDiagnosticsService::SetExpectedLastPassedParameters(
    base::Value::Dict expected_passed_parameter) {
  expected_passed_parameters_ = std::move(expected_passed_parameter);
}

void FakeDiagnosticsService::SetExpectedLastCalledRoutine(
    health::mojom::DiagnosticRoutineEnum expected_called_routine) {
  expected_called_routine_ = expected_called_routine;
}

void FakeDiagnosticsService::BindPendingReceiver(
    mojo::PendingReceiver<health::mojom::DiagnosticsService> receiver) {
  receiver_.Bind(std::move(receiver));
}

}  // namespace ash
