// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_FAKE_DIAGNOSTICS_SERVICE_H_
#define ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_FAKE_DIAGNOSTICS_SERVICE_H_

#include <memory>
#include <vector>

#include "ash/webui/telemetry_extension_ui/mojom/diagnostics_service.mojom.h"
#include "ash/webui/telemetry_extension_ui/services/diagnostics_service.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class FakeDiagnosticsService : public health::mojom::DiagnosticsService {
 public:
  class Factory : public ash::DiagnosticsService::Factory {
   public:
    Factory();
    ~Factory() override;

    void SetCreateInstanceResponse(
        std::unique_ptr<FakeDiagnosticsService> fake_service);

   protected:
    // DiagnosticsService::Factory:
    std::unique_ptr<health::mojom::DiagnosticsService> CreateInstance(
        mojo::PendingReceiver<health::mojom::DiagnosticsService> receiver)
        override;

   private:
    std::unique_ptr<FakeDiagnosticsService> fake_service_;
  };

  FakeDiagnosticsService();
  FakeDiagnosticsService(const FakeDiagnosticsService&) = delete;
  FakeDiagnosticsService& operator=(const FakeDiagnosticsService&) = delete;
  ~FakeDiagnosticsService() override;

  // health::mojom::DiagnosticsService overrides.
  void GetAvailableRoutines(GetAvailableRoutinesCallback callback) override;
  void GetRoutineUpdate(int32_t id,
                        health::mojom::DiagnosticRoutineCommandEnum command,
                        bool include_output,
                        GetRoutineUpdateCallback callback) override;
  void RunBatteryCapacityRoutine(
      RunBatteryCapacityRoutineCallback callback) override;
  void RunBatteryHealthRoutine(
      RunBatteryHealthRoutineCallback callback) override;
  void RunSmartctlCheckRoutine(
      RunSmartctlCheckRoutineCallback callback) override;
  void RunAcPowerRoutine(health::mojom::AcPowerStatusEnum expected_status,
                         const absl::optional<std::string>& expected_power_type,
                         RunAcPowerRoutineCallback callback) override;
  void RunCpuCacheRoutine(uint32_t length_seconds,
                          RunCpuCacheRoutineCallback callback) override;
  void RunCpuStressRoutine(uint32_t length_seconds,
                           RunCpuStressRoutineCallback callback) override;
  void RunFloatingPointAccuracyRoutine(
      uint32_t length_seconds,
      RunFloatingPointAccuracyRoutineCallback callback) override;
  void RunNvmeWearLevelRoutine(
      uint32_t wear_level_threshold,
      RunNvmeWearLevelRoutineCallback callback) override;
  void RunNvmeSelfTestRoutine(
      health::mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
      RunNvmeSelfTestRoutineCallback callback) override;
  void RunDiskReadRoutine(health::mojom::DiskReadRoutineTypeEnum type,
                          uint32_t length_seconds,
                          uint32_t file_size_mb,
                          RunDiskReadRoutineCallback callback) override;
  void RunPrimeSearchRoutine(uint32_t length_seconds,
                             RunPrimeSearchRoutineCallback callback) override;
  void RunBatteryDischargeRoutine(
      uint32_t length_seconds,
      uint32_t maximum_discharge_percent_allowed,
      RunBatteryDischargeRoutineCallback callback) override;
  void RunBatteryChargeRoutine(
      uint32_t length_seconds,
      uint32_t minimum_charge_percent_required,
      RunBatteryChargeRoutineCallback callback) override;
  void RunMemoryRoutine(RunMemoryRoutineCallback callback) override;
  void RunLanConnectivityRoutine(
      RunLanConnectivityRoutineCallback callback) override;

  // Sets the return value for |Run*Routine|.
  void SetRunRoutineResponse(
      health::mojom::RunRoutineResponsePtr expected_response);

  // Sets the return value for |GetAvailableRoutines|.
  void SetAvailableRoutines(
      std::vector<health::mojom::DiagnosticRoutineEnum> available_routines);

  // Sets the return value for |GetRoutineUpdate|.
  void SetRoutineUpdateResponse(health::mojom::RoutineUpdatePtr routine_update);

  // Set expectation about the parameter that is passed to a call of
  // |Run*Routine| or |GetAvailableRoutines|.
  void SetExpectedLastPassedParameters(
      base::Value::Dict expected_passed_parameter);

  // Set expectation about the type of routine that is called.
  void SetExpectedLastCalledRoutine(
      health::mojom::DiagnosticRoutineEnum expected_called_routine);

 private:
  void BindPendingReceiver(
      mojo::PendingReceiver<health::mojom::DiagnosticsService> receiver);

  mojo::Receiver<health::mojom::DiagnosticsService> receiver_;

  // Response for a call to |Run*Routine|.
  health::mojom::RunRoutineResponsePtr run_routine_response_;

  // Response for a call to |GetAvailableRoutines|.
  std::vector<health::mojom::DiagnosticRoutineEnum>
      available_routines_response_;

  // Response for a call to |GetRoutineUpdate|.
  health::mojom::RoutineUpdatePtr routine_update_response_;

  // Expectation of the passed parameters to a |Run*Routine| call.
  base::Value::Dict expected_passed_parameters_;
  // Actually passed parameter.
  base::Value::Dict actual_passed_parameters_;

  // Expectation of the called routine.
  health::mojom::DiagnosticRoutineEnum expected_called_routine_{
      health::mojom::DiagnosticRoutineEnum::kUnknown};
  // Actually called routine.
  health::mojom::DiagnosticRoutineEnum actual_called_routine_{
      health::mojom::DiagnosticRoutineEnum::kUnknown};
};
}  // namespace ash

#endif  // ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_FAKE_DIAGNOSTICS_SERVICE_H_
