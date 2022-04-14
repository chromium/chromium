// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_DIAGNOSTICS_SERVICE_H_
#define ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_DIAGNOSTICS_SERVICE_H_

#include "ash/webui/telemetry_extension_ui/mojom/diagnostics_service.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// TODO(https://crbug.com/1164001): Remove if cros_healthd::mojom moved to ash.
namespace cros_healthd {
namespace mojom = ::chromeos::cros_healthd::mojom;
}  // namespace cros_healthd

class DiagnosticsService : public health::mojom::DiagnosticsService {
 public:
  explicit DiagnosticsService(
      mojo::PendingReceiver<health::mojom::DiagnosticsService> receiver);
  DiagnosticsService(const DiagnosticsService&) = delete;
  DiagnosticsService& operator=(const DiagnosticsService&) = delete;
  ~DiagnosticsService() override;

 private:
  // Ensures that |service_| created and connected to the
  // CrosHealthdDiagnosticsService.
  cros_healthd::mojom::CrosHealthdDiagnosticsService* GetService();

  void OnDisconnect();

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

  // Pointer to real implementation.
  mojo::Remote<cros_healthd::mojom::CrosHealthdDiagnosticsService> service_;

  // We must destroy |receiver_| before destroying |service_|, so we will close
  // interface pipe before destroying pending response callbacks owned by
  // |service_|. It is an error to drop response callbacks which still
  // correspond to an open interface pipe.
  mojo::Receiver<health::mojom::DiagnosticsService> receiver_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromeOS code migration is done.
namespace chromeos {
using ::ash::DiagnosticsService;
}  // namespace chromeos

#endif  // ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_DIAGNOSTICS_SERVICE_H_
