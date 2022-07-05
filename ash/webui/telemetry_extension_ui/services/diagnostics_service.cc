// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/telemetry_extension_ui/services/diagnostics_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/webui/telemetry_extension_ui/mojom/diagnostics_service.mojom.h"
#include "ash/webui/telemetry_extension_ui/services/diagnostics_service_converters.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/nullable_primitives.mojom.h"

namespace ash {

// static
DiagnosticsService::Factory* DiagnosticsService::Factory::test_factory_ =
    nullptr;

// static
std::unique_ptr<health::mojom::DiagnosticsService>
DiagnosticsService::Factory::Create(
    mojo::PendingReceiver<health::mojom::DiagnosticsService> receiver) {
  if (test_factory_) {
    return test_factory_->CreateInstance(std::move(receiver));
  }

  return base::WrapUnique<DiagnosticsService>(
      new DiagnosticsService(std::move(receiver)));
}

// static
void DiagnosticsService::Factory::SetForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

DiagnosticsService::Factory::~Factory() = default;

DiagnosticsService::DiagnosticsService(
    mojo::PendingReceiver<health::mojom::DiagnosticsService> receiver)
    : receiver_(this, std::move(receiver)) {}

DiagnosticsService::~DiagnosticsService() = default;

cros_healthd::mojom::CrosHealthdDiagnosticsService*
DiagnosticsService::GetService() {
  if (!service_ || !service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->GetDiagnosticsService(
        service_.BindNewPipeAndPassReceiver());
    service_.set_disconnect_handler(base::BindOnce(
        &DiagnosticsService::OnDisconnect, base::Unretained(this)));
  }
  return service_.get();
}

void DiagnosticsService::OnDisconnect() {
  service_.reset();
}

void DiagnosticsService::GetAvailableRoutines(
    GetAvailableRoutinesCallback callback) {
  GetService()->GetAvailableRoutines(base::BindOnce(
      [](health::mojom::DiagnosticsService::GetAvailableRoutinesCallback
             callback,
         const std::vector<cros_healthd::mojom::DiagnosticRoutineEnum>&
             routines) {
        std::move(callback).Run(converters::Convert(routines));
      },
      std::move(callback)));
}

void DiagnosticsService::GetRoutineUpdate(
    int32_t id,
    health::mojom::DiagnosticRoutineCommandEnum command,
    bool include_output,
    GetRoutineUpdateCallback callback) {
  GetService()->GetRoutineUpdate(
      id, converters::Convert(command), include_output,
      base::BindOnce(
          [](health::mojom::DiagnosticsService::GetRoutineUpdateCallback
                 callback,
             cros_healthd::mojom::RoutineUpdatePtr ptr) {
            std::move(callback).Run(
                converters::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsService::RunBatteryCapacityRoutine(
    RunBatteryCapacityRoutineCallback callback) {
  GetService()->RunBatteryCapacityRoutine(base::BindOnce(
      [](health::mojom::DiagnosticsService::RunBatteryCapacityRoutineCallback
             callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsService::RunBatteryHealthRoutine(
    RunBatteryHealthRoutineCallback callback) {
  GetService()->RunBatteryHealthRoutine(base::BindOnce(
      [](health::mojom::DiagnosticsService::RunBatteryHealthRoutineCallback
             callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsService::RunSmartctlCheckRoutine(
    RunSmartctlCheckRoutineCallback callback) {
  GetService()->RunSmartctlCheckRoutine(base::BindOnce(
      [](health::mojom::DiagnosticsService::RunSmartctlCheckRoutineCallback
             callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsService::RunAcPowerRoutine(
    health::mojom::AcPowerStatusEnum expected_status,
    const absl::optional<std::string>& expected_power_type,
    RunAcPowerRoutineCallback callback) {
  GetService()->RunAcPowerRoutine(
      converters::Convert(expected_status), expected_power_type,
      base::BindOnce(
          [](health::mojom::DiagnosticsService::RunAcPowerRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsService::RunCpuCacheRoutine(
    uint32_t length_seconds,
    RunCpuCacheRoutineCallback callback) {
  GetService()->RunCpuCacheRoutine(
      cros_healthd::mojom::NullableUint32::New(length_seconds),
      base::BindOnce(
          [](health::mojom::DiagnosticsService::RunCpuCacheRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsService::RunCpuStressRoutine(
    uint32_t length_seconds,
    RunCpuStressRoutineCallback callback) {
  GetService()->RunCpuStressRoutine(
      cros_healthd::mojom::NullableUint32::New(length_seconds),
      base::BindOnce(
          [](health::mojom::DiagnosticsService::RunCpuStressRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsService::RunFloatingPointAccuracyRoutine(
    uint32_t length_seconds,
    RunFloatingPointAccuracyRoutineCallback callback) {
  GetService()->RunFloatingPointAccuracyRoutine(
      cros_healthd::mojom::NullableUint32::New(length_seconds),
      base::BindOnce(
          [](health::mojom::DiagnosticsService::
                 RunFloatingPointAccuracyRoutineCallback callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsService::RunNvmeWearLevelRoutine(
    uint32_t wear_level_threshold,
    RunNvmeWearLevelRoutineCallback callback) {
  GetService()->RunNvmeWearLevelRoutine(
      wear_level_threshold,
      base::BindOnce(
          [](health::mojom::DiagnosticsService::RunNvmeWearLevelRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsService::RunNvmeSelfTestRoutine(
    health::mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
    RunNvmeSelfTestRoutineCallback callback) {
  GetService()->RunNvmeSelfTestRoutine(
      converters::Convert(nvme_self_test_type),
      base::BindOnce(
          [](health::mojom::DiagnosticsService::RunNvmeSelfTestRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsService::RunDiskReadRoutine(
    health::mojom::DiskReadRoutineTypeEnum type,
    uint32_t length_seconds,
    uint32_t file_size_mb,
    RunDiskReadRoutineCallback callback) {
  GetService()->RunDiskReadRoutine(
      converters::Convert(type), length_seconds, file_size_mb,
      base::BindOnce(
          [](health::mojom::DiagnosticsService::RunDiskReadRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsService::RunPrimeSearchRoutine(
    uint32_t length_seconds,
    RunPrimeSearchRoutineCallback callback) {
  GetService()->RunPrimeSearchRoutine(
      cros_healthd::mojom::NullableUint32::New(length_seconds),
      base::BindOnce(
          [](health::mojom::DiagnosticsService::RunPrimeSearchRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsService::RunBatteryDischargeRoutine(
    uint32_t length_seconds,
    uint32_t maximum_discharge_percent_allowed,
    RunBatteryDischargeRoutineCallback callback) {
  GetService()->RunBatteryDischargeRoutine(
      length_seconds, maximum_discharge_percent_allowed,
      base::BindOnce(
          [](health::mojom::DiagnosticsService::
                 RunBatteryDischargeRoutineCallback callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsService::RunBatteryChargeRoutine(
    uint32_t length_seconds,
    uint32_t minimum_charge_percent_required,
    RunBatteryChargeRoutineCallback callback) {
  GetService()->RunBatteryChargeRoutine(
      length_seconds, minimum_charge_percent_required,
      base::BindOnce(
          [](health::mojom::DiagnosticsService::RunBatteryChargeRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsService::RunMemoryRoutine(RunMemoryRoutineCallback callback) {
  GetService()->RunMemoryRoutine(base::BindOnce(
      [](health::mojom::DiagnosticsService::RunMemoryRoutineCallback callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsService::RunLanConnectivityRoutine(
    RunLanConnectivityRoutineCallback callback) {
  GetService()->RunLanConnectivityRoutine(base::BindOnce(
      [](health::mojom::DiagnosticsService::RunLanConnectivityRoutineCallback
             callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

}  // namespace ash
