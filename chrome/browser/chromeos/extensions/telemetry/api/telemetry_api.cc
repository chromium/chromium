// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry_api.h"

#include <inttypes.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/remote_probe_service_strategy.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry_api_converters.h"
#include "chrome/common/chromeos/extensions/api/telemetry.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "extensions/common/permissions/permissions_data.h"

namespace chromeos {

// TelemetryApiFunctionBase ----------------------------------------------------

TelemetryApiFunctionBase::TelemetryApiFunctionBase()
    : remote_probe_service_strategy_(RemoteProbeServiceStrategy::Create()) {}

TelemetryApiFunctionBase::~TelemetryApiFunctionBase() = default;

mojo::Remote<crosapi::mojom::TelemetryProbeService>&
TelemetryApiFunctionBase::GetRemoteService() {
  DCHECK(remote_probe_service_strategy_);
  return remote_probe_service_strategy_->GetRemoteService();
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool TelemetryApiFunctionBase::IsCrosApiAvailable() {
  return remote_probe_service_strategy_ != nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// OsTelemetryGetBatteryInfoFunction -------------------------------------------

OsTelemetryGetBatteryInfoFunction::OsTelemetryGetBatteryInfoFunction() =
    default;
OsTelemetryGetBatteryInfoFunction::~OsTelemetryGetBatteryInfoFunction() =
    default;

void OsTelemetryGetBatteryInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetBatteryInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo(
      {crosapi::mojom::ProbeCategoryEnum::kBattery}, std::move(cb));
}

void OsTelemetryGetBatteryInfoFunction::OnResult(
    crosapi::mojom::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->battery_result || !ptr->battery_result->is_battery_info()) {
    Respond(Error("API internal error"));
    return;
  }
  auto& battery_info = ptr->battery_result->get_battery_info();

  // Protect accessing the serial number by a permission.
  std::unique_ptr<std::string> serial_number;
  if (extension()->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::kChromeOSTelemetrySerialNumber) &&
      battery_info->serial_number.has_value()) {
    serial_number = std::make_unique<std::string>(
        std::move(battery_info->serial_number.value()));
  }

  api::os_telemetry::BatteryInfo result =
      converters::ConvertPtr<api::os_telemetry::BatteryInfo>(
          std::move(battery_info));

  if (serial_number && !serial_number->empty()) {
    result.serial_number = std::move(serial_number);
  }

  Respond(
      ArgumentList(api::os_telemetry::GetBatteryInfo::Results::Create(result)));
}

// OsTelemetryGetCpuInfoFunction -----------------------------------------------

OsTelemetryGetCpuInfoFunction::OsTelemetryGetCpuInfoFunction() = default;
OsTelemetryGetCpuInfoFunction::~OsTelemetryGetCpuInfoFunction() = default;

void OsTelemetryGetCpuInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetCpuInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo(
      {crosapi::mojom::ProbeCategoryEnum::kCpu}, std::move(cb));
}

void OsTelemetryGetCpuInfoFunction::OnResult(
    crosapi::mojom::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->cpu_result || !ptr->cpu_result->is_cpu_info()) {
    Respond(Error("API internal error"));
    return;
  }

  const auto& cpu_info = ptr->cpu_result->get_cpu_info();

  api::os_telemetry::CpuInfo result;
  if (cpu_info->num_total_threads) {
    result.num_total_threads =
        std::make_unique<int32_t>(cpu_info->num_total_threads->value);
  }
  result.architecture = converters::Convert(cpu_info->architecture);
  result.physical_cpus =
      converters::ConvertPtrVector<api::os_telemetry::PhysicalCpuInfo>(
          std::move(cpu_info->physical_cpus));

  Respond(ArgumentList(api::os_telemetry::GetCpuInfo::Results::Create(result)));
}

// OsTelemetryGetMemoryInfoFunction --------------------------------------------

OsTelemetryGetMemoryInfoFunction::OsTelemetryGetMemoryInfoFunction() = default;
OsTelemetryGetMemoryInfoFunction::~OsTelemetryGetMemoryInfoFunction() = default;

void OsTelemetryGetMemoryInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetMemoryInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo(
      {crosapi::mojom::ProbeCategoryEnum::kMemory}, std::move(cb));
}

void OsTelemetryGetMemoryInfoFunction::OnResult(
    crosapi::mojom::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->memory_result || !ptr->memory_result->is_memory_info()) {
    Respond(Error("API internal error"));
    return;
  }

  api::os_telemetry::MemoryInfo result;

  const auto& memory_info = ptr->memory_result->get_memory_info();
  if (memory_info->total_memory_kib) {
    result.total_memory_ki_b =
        std::make_unique<int32_t>(memory_info->total_memory_kib->value);
  }
  if (memory_info->free_memory_kib) {
    result.free_memory_ki_b =
        std::make_unique<int32_t>(memory_info->free_memory_kib->value);
  }
  if (memory_info->available_memory_kib) {
    result.available_memory_ki_b =
        std::make_unique<int32_t>(memory_info->available_memory_kib->value);
  }
  if (memory_info->page_faults_since_last_boot) {
    result.page_faults_since_last_boot = std::make_unique<double_t>(
        memory_info->page_faults_since_last_boot->value);
  }

  Respond(
      ArgumentList(api::os_telemetry::GetMemoryInfo::Results::Create(result)));
}

// OsTelemetryGetOemDataFunction -----------------------------------------------

OsTelemetryGetOemDataFunction::OsTelemetryGetOemDataFunction() = default;
OsTelemetryGetOemDataFunction::~OsTelemetryGetOemDataFunction() = default;

void OsTelemetryGetOemDataFunction::RunIfAllowed() {
  // Protect accessing the serial number by a permission.
  if (!extension()->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::kChromeOSTelemetrySerialNumber)) {
    Respond(
        Error("Unauthorized access to chrome.os.telemetry.getOemData. Extension"
              " doesn't have the permission."));
    return;
  }

  auto cb = base::BindOnce(&OsTelemetryGetOemDataFunction::OnResult, this);

  GetRemoteService()->GetOemData(std::move(cb));
}

void OsTelemetryGetOemDataFunction::OnResult(
    crosapi::mojom::ProbeOemDataPtr ptr) {
  if (!ptr || !ptr->oem_data.has_value()) {
    Respond(Error("API internal error"));
    return;
  }

  api::os_telemetry::OemData result;
  result.oem_data =
      std::make_unique<std::string>(std::move(ptr->oem_data.value()));

  Respond(ArgumentList(api::os_telemetry::GetOemData::Results::Create(result)));
}

// OsTelemetryGetOsVersionInfoFunction -----------------------------------------

OsTelemetryGetOsVersionInfoFunction::OsTelemetryGetOsVersionInfoFunction() =
    default;
OsTelemetryGetOsVersionInfoFunction::~OsTelemetryGetOsVersionInfoFunction() =
    default;

void OsTelemetryGetOsVersionInfoFunction::RunIfAllowed() {
  auto cb =
      base::BindOnce(&OsTelemetryGetOsVersionInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo(
      {crosapi::mojom::ProbeCategoryEnum::kSystem}, std::move(cb));
}

void OsTelemetryGetOsVersionInfoFunction::OnResult(
    crosapi::mojom::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->system_result || !ptr->system_result->is_system_info()) {
    Respond(Error("API internal error"));
    return;
  }
  auto& system_info = ptr->system_result->get_system_info();

  // os_version is an optional value and might not be present.
  // TODO(b/234338704): check how to test this.
  if (!system_info->os_info || !system_info->os_info->os_version) {
    Respond(Error("API internal error"));
    return;
  }

  api::os_telemetry::OsVersionInfo result =
      converters::ConvertPtr<api::os_telemetry::OsVersionInfo>(
          std::move(system_info->os_info->os_version));

  Respond(ArgumentList(
      api::os_telemetry::GetOsVersionInfo::Results::Create(result)));
}

// OsTelemetryGetStatefulPartitionInfoFunction ---------------------------------

OsTelemetryGetStatefulPartitionInfoFunction::
    OsTelemetryGetStatefulPartitionInfoFunction() = default;
OsTelemetryGetStatefulPartitionInfoFunction::
    ~OsTelemetryGetStatefulPartitionInfoFunction() = default;

void OsTelemetryGetStatefulPartitionInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(
      &OsTelemetryGetStatefulPartitionInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo(
      {crosapi::mojom::ProbeCategoryEnum::kStatefulPartition}, std::move(cb));
}

void OsTelemetryGetStatefulPartitionInfoFunction::OnResult(
    crosapi::mojom::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->stateful_partition_result ||
      !ptr->stateful_partition_result->is_partition_info()) {
    Respond(Error("API internal error"));
    return;
  }
  auto& stateful_part_info =
      ptr->stateful_partition_result->get_partition_info();

  api::os_telemetry::StatefulPartitionInfo result =
      converters::ConvertPtr<api::os_telemetry::StatefulPartitionInfo>(
          std::move(stateful_part_info));

  Respond(ArgumentList(
      api::os_telemetry::GetStatefulPartitionInfo::Results::Create(result)));
}

// OsTelemetryGetVpdInfoFunction -----------------------------------------------

OsTelemetryGetVpdInfoFunction::OsTelemetryGetVpdInfoFunction() = default;
OsTelemetryGetVpdInfoFunction::~OsTelemetryGetVpdInfoFunction() = default;

void OsTelemetryGetVpdInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetVpdInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo(
      {crosapi::mojom::ProbeCategoryEnum::kCachedVpdData}, std::move(cb));
}

void OsTelemetryGetVpdInfoFunction::OnResult(
    crosapi::mojom::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->vpd_result || !ptr->vpd_result->is_vpd_info()) {
    Respond(Error("API internal error"));
    return;
  }

  api::os_telemetry::VpdInfo result;

  const auto& vpd_info = ptr->vpd_result->get_vpd_info();
  if (vpd_info->first_power_date.has_value()) {
    result.activate_date =
        std::make_unique<std::string>(vpd_info->first_power_date.value());
  }
  if (vpd_info->model_name.has_value()) {
    result.model_name =
        std::make_unique<std::string>(vpd_info->model_name.value());
  }
  if (vpd_info->sku_number.has_value()) {
    result.sku_number =
        std::make_unique<std::string>(vpd_info->sku_number.value());
  }

  // Protect accessing the serial number by a permission.
  if (extension()->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::kChromeOSTelemetrySerialNumber) &&
      vpd_info->serial_number.has_value()) {
    result.serial_number =
        std::make_unique<std::string>(vpd_info->serial_number.value());
  }

  Respond(ArgumentList(api::os_telemetry::GetVpdInfo::Results::Create(result)));
}

}  // namespace chromeos
