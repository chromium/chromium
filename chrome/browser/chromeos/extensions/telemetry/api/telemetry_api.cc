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
#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry_api_converters.h"
#include "chrome/common/chromeos/extensions/api/telemetry.h"
#include "extensions/common/permissions/permissions_data.h"

namespace chromeos {

TelemetryApiFunctionBase::TelemetryApiFunctionBase()
    : probe_service_(remote_probe_service_.BindNewPipeAndPassReceiver()) {}
TelemetryApiFunctionBase::~TelemetryApiFunctionBase() = default;

// OsTelemetryGetBatteryInfoFunction -------------------------------------------

OsTelemetryGetBatteryInfoFunction::OsTelemetryGetBatteryInfoFunction() =
    default;
OsTelemetryGetBatteryInfoFunction::~OsTelemetryGetBatteryInfoFunction() =
    default;

void OsTelemetryGetBatteryInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetBatteryInfoFunction::OnResult, this);

  remote_probe_service_->ProbeTelemetryInfo(
      {ash::health::mojom::ProbeCategoryEnum::kBattery}, std::move(cb));
}

void OsTelemetryGetBatteryInfoFunction::OnResult(
    ash::health::mojom::TelemetryInfoPtr ptr) {
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

  remote_probe_service_->ProbeTelemetryInfo(
      {ash::health::mojom::ProbeCategoryEnum::kCpu}, std::move(cb));
}

void OsTelemetryGetCpuInfoFunction::OnResult(
    ash::health::mojom::TelemetryInfoPtr ptr) {
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

  remote_probe_service_->ProbeTelemetryInfo(
      {ash::health::mojom::ProbeCategoryEnum::kMemory}, std::move(cb));
}

void OsTelemetryGetMemoryInfoFunction::OnResult(
    ash::health::mojom::TelemetryInfoPtr ptr) {
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

  remote_probe_service_->GetOemData(std::move(cb));
}

void OsTelemetryGetOemDataFunction::OnResult(
    ash::health::mojom::OemDataPtr ptr) {
  if (!ptr || !ptr->oem_data.has_value()) {
    Respond(Error("API internal error"));
    return;
  }

  api::os_telemetry::OemData result;
  result.oem_data =
      std::make_unique<std::string>(std::move(ptr->oem_data.value()));

  Respond(ArgumentList(api::os_telemetry::GetOemData::Results::Create(result)));
}

// OsTelemetryGetVpdInfoFunction -----------------------------------------------

OsTelemetryGetVpdInfoFunction::OsTelemetryGetVpdInfoFunction() = default;
OsTelemetryGetVpdInfoFunction::~OsTelemetryGetVpdInfoFunction() = default;

void OsTelemetryGetVpdInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetVpdInfoFunction::OnResult, this);

  remote_probe_service_->ProbeTelemetryInfo(
      {ash::health::mojom::ProbeCategoryEnum::kCachedVpdData}, std::move(cb));
}

void OsTelemetryGetVpdInfoFunction::OnResult(
    ash::health::mojom::TelemetryInfoPtr ptr) {
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
