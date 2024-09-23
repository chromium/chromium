// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_FAKE_DIAGNOSTICS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_FAKE_DIAGNOSTICS_SERVICE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/values.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

class FakeDiagnosticsService : public crosapi::mojom::DiagnosticsService {
 public:
  FakeDiagnosticsService();
  FakeDiagnosticsService(const FakeDiagnosticsService&) = delete;
  FakeDiagnosticsService& operator=(const FakeDiagnosticsService&) = delete;
  ~FakeDiagnosticsService() override;

  void BindPendingReceiver(
      mojo::PendingReceiver<crosapi::mojom::DiagnosticsService> receiver);

  mojo::PendingRemote<crosapi::mojom::DiagnosticsService>
  BindNewPipeAndPassRemote();

  // crosapi::health::mojom::DiagnosticsService overrides.
  void GetAvailableRoutines(GetAvailableRoutinesCallback callback) override;
  void GetRoutineUpdate(int32_t id,
                        crosapi::mojom::DiagnosticsRoutineCommandEnum command,
                        bool include_output,
                        GetRoutineUpdateCallback callback) override;
  void RunAcPowerRoutine(
      crosapi::mojom::DiagnosticsAcPowerStatusEnum expected_status,
      const std::optional<std::string>& expected_power_type,
      RunAcPowerRoutineCallback callback) override;
  void RunBatteryCapacityRoutine(
      RunBatteryCapacityRoutineCallback callback) override;
  void RunBatteryChargeRoutine(
      uint32_t length_seconds,
      uint32_t minimum_charge_percent_required,
      RunBatteryChargeRoutineCallback callback) override;
  void RunBatteryDischargeRoutine(
      uint32_t length_seconds,
      uint32_t maximum_discharge_percent_allowed,
      RunBatteryDischargeRoutineCallback callback) override;
  void RunBatteryHealthRoutine(
      RunBatteryHealthRoutineCallback callback) override;
  void RunBluetoothDiscoveryRoutine(
      RunBluetoothDiscoveryRoutineCallback callback) override;
  void RunBluetoothPairingRoutine(
      const std::string& peripheral_id,
      RunBluetoothPairingRoutineCallback callback) override;
  void RunBluetoothPowerRoutine(
      RunBluetoothPowerRoutineCallback callback) override;
  void RunBluetoothScanningRoutine(
      uint32_t length_seconds,
      RunBluetoothScanningRoutineCallback callback) override;
  void RunCpuCacheRoutine(uint32_t length_seconds,
                          RunCpuCacheRoutineCallback callback) override;
  void RunCpuStressRoutine(uint32_t length_seconds,
                           RunCpuStressRoutineCallback callback) override;
  void RunDiskReadRoutine(
      crosapi::mojom::DiagnosticsDiskReadRoutineTypeEnum type,
      uint32_t length_seconds,
      uint32_t file_size_mb,
      RunDiskReadRoutineCallback callback) override;
  void RunDnsResolutionRoutine(
      RunDnsResolutionRoutineCallback callback) override;
  void RunDnsResolverPresentRoutine(
      RunDnsResolverPresentRoutineCallback callback) override;
  void RunEmmcLifetimeRoutine(RunEmmcLifetimeRoutineCallback callback) override;
  void RunFloatingPointAccuracyRoutine(
      uint32_t length_seconds,
      RunFloatingPointAccuracyRoutineCallback callback) override;
  void RunFingerprintAliveRoutine(
      RunFingerprintAliveRoutineCallback callback) override;
  void RunGatewayCanBePingedRoutine(
      RunGatewayCanBePingedRoutineCallback callback) override;
  void RunLanConnectivityRoutine(
      RunLanConnectivityRoutineCallback callback) override;
  void RunMemoryRoutine(RunMemoryRoutineCallback callback) override;
  void RunNvmeSelfTestRoutine(
      crosapi::mojom::DiagnosticsNvmeSelfTestTypeEnum nvme_self_test_type,
      RunNvmeSelfTestRoutineCallback callback) override;
  void DEPRECATED_RunNvmeWearLevelRoutine(
      uint32_t wear_level_threshold,
      DEPRECATED_RunNvmeWearLevelRoutineCallback callback) override;
  void RunPrimeSearchRoutine(uint32_t length_seconds,
                             RunPrimeSearchRoutineCallback callback) override;
  void RunSensitiveSensorRoutine(
      RunSensitiveSensorRoutineCallback callback) override;
  void RunSignalStrengthRoutine(
      RunSignalStrengthRoutineCallback callback) override;
  void RunSmartctlCheckRoutine(
      crosapi::mojom::UInt32ValuePtr percentage_used_threshold,
      RunSmartctlCheckRoutineCallback callback) override;
  void RunUfsLifetimeRoutine(RunUfsLifetimeRoutineCallback callback) override;
  void RunPowerButtonRoutine(uint32_t timeout_seconds,
                             RunPowerButtonRoutineCallback callback) override;
  void RunAudioDriverRoutine(RunAudioDriverRoutineCallback callback) override;
  void RunFanRoutine(RunFanRoutineCallback callback) override;

  // Sets the return value for |Run*Routine|.
  void SetRunRoutineResponse(
      crosapi::mojom::DiagnosticsRunRoutineResponsePtr expected_response);

  // Sets the return value for |GetAvailableRoutines|.
  void SetAvailableRoutines(
      std::vector<crosapi::mojom::DiagnosticsRoutineEnum> available_routines);

  // Sets the return value for |GetRoutineUpdate|.
  void SetRoutineUpdateResponse(
      crosapi::mojom::DiagnosticsRoutineUpdatePtr routine_update);

  // Set expectation about the parameter that is passed to a call of
  // |Run*Routine| or |GetAvailableRoutines|.
  void SetExpectedLastPassedParameters(
      base::Value::Dict expected_passed_parameter);

  // Set expectation about the type of routine that is called.
  void SetExpectedLastCalledRoutine(
      crosapi::mojom::DiagnosticsRoutineEnum expected_called_routine);

 private:
  mojo::Receiver<crosapi::mojom::DiagnosticsService> receiver_;

  // Response for a call to |Run*Routine|.
  crosapi::mojom::DiagnosticsRunRoutineResponsePtr run_routine_response_;

  // Response for a call to |GetAvailableRoutines|.
  std::vector<crosapi::mojom::DiagnosticsRoutineEnum>
      available_routines_response_;

  // Response for a call to |GetRoutineUpdate|.
  crosapi::mojom::DiagnosticsRoutineUpdatePtr routine_update_response_;

  // Expectation of the passed parameters to a |Run*Routine| call.
  base::Value::Dict expected_passed_parameters_;
  // Actually passed parameter.
  base::Value::Dict actual_passed_parameters_;

  // Expectation of the called routine.
  crosapi::mojom::DiagnosticsRoutineEnum expected_called_routine_{
      crosapi::mojom::DiagnosticsRoutineEnum::kUnknown};
  // Actually called routine.
  crosapi::mojom::DiagnosticsRoutineEnum actual_called_routine_{
      crosapi::mojom::DiagnosticsRoutineEnum::kUnknown};
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_FAKE_DIAGNOSTICS_SERVICE_H_
