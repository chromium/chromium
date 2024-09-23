// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry/telemetry_api.h"

#include <inttypes.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/remote_probe_service_strategy.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry/telemetry_api_converters.h"
#include "chrome/common/chromeos/extensions/api/telemetry.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "extensions/common/permissions/permissions_data.h"

namespace chromeos {

namespace {

namespace cx_telem = api::os_telemetry;
namespace crosapi = ::crosapi::mojom;

}  // namespace

// TelemetryApiFunctionBase ----------------------------------------------------

TelemetryApiFunctionBase::TelemetryApiFunctionBase() {}

TelemetryApiFunctionBase::~TelemetryApiFunctionBase() = default;

crosapi::TelemetryProbeService* TelemetryApiFunctionBase::GetRemoteService() {
  DCHECK(RemoteProbeServiceStrategy::Get()->GetRemoteProbeService());
  return RemoteProbeServiceStrategy::Get()->GetRemoteProbeService();
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool TelemetryApiFunctionBase::IsCrosApiAvailable() {
  return RemoteProbeServiceStrategy::Get()->GetRemoteProbeService() != nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// OsTelemetryGetAudioInfoFunction ---------------------------------------------

void OsTelemetryGetAudioInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetAudioInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo({crosapi::ProbeCategoryEnum::kAudio},
                                         std::move(cb));
}

void OsTelemetryGetAudioInfoFunction::OnResult(
    crosapi::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->audio_result || !ptr->audio_result->is_audio_info()) {
    Respond(Error("API internal error"));
    return;
  }
  auto& audio_info = ptr->audio_result->get_audio_info();

  auto result = converters::telemetry::ConvertPtr(std::move(audio_info));

  Respond(ArgumentList(cx_telem::GetAudioInfo::Results::Create(result)));
}

// OsTelemetryGetBatteryInfoFunction -------------------------------------------

void OsTelemetryGetBatteryInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetBatteryInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo({crosapi::ProbeCategoryEnum::kBattery},
                                         std::move(cb));
}

void OsTelemetryGetBatteryInfoFunction::OnResult(
    crosapi::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->battery_result || !ptr->battery_result->is_battery_info()) {
    Respond(Error("API internal error"));
    return;
  }
  auto& battery_info = ptr->battery_result->get_battery_info();

  const bool has_permission = extension()->permissions_data()->HasAPIPermission(
      extensions::mojom::APIPermissionID::kChromeOSTelemetrySerialNumber);

  cx_telem::BatteryInfo result = converters::telemetry::ConvertPtr(
      std::move(battery_info), has_permission);

  Respond(ArgumentList(cx_telem::GetBatteryInfo::Results::Create(result)));
}

// OsTelemetryGetNonRemovableBlockDevicesInfoFunction --------------------------

void OsTelemetryGetNonRemovableBlockDevicesInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(
      &OsTelemetryGetNonRemovableBlockDevicesInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo(
      {crosapi::ProbeCategoryEnum::kNonRemovableBlockDevices}, std::move(cb));
}

void OsTelemetryGetNonRemovableBlockDevicesInfoFunction::OnResult(
    crosapi::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->block_device_result ||
      !ptr->block_device_result->is_block_device_info()) {
    Respond(Error("API internal error"));
    return;
  }
  auto& block_device_info = ptr->block_device_result->get_block_device_info();

  auto infos = converters::telemetry::ConvertPtrVector<
      cx_telem::NonRemovableBlockDeviceInfo>(std::move(block_device_info));
  cx_telem::NonRemovableBlockDeviceInfoResponse result;
  result.device_infos = std::move(infos);

  Respond(ArgumentList(
      cx_telem::GetNonRemovableBlockDevicesInfo::Results::Create(result)));
}

// OsTelemetryGetCpuInfoFunction -----------------------------------------------

void OsTelemetryGetCpuInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetCpuInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo({crosapi::ProbeCategoryEnum::kCpu},
                                         std::move(cb));
}

void OsTelemetryGetCpuInfoFunction::OnResult(
    crosapi::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->cpu_result || !ptr->cpu_result->is_cpu_info()) {
    Respond(Error("API internal error"));
    return;
  }

  const auto& cpu_info = ptr->cpu_result->get_cpu_info();

  cx_telem::CpuInfo result;
  if (cpu_info->num_total_threads) {
    result.num_total_threads = cpu_info->num_total_threads->value;
  }
  result.architecture = converters::telemetry::Convert(cpu_info->architecture);
  result.physical_cpus =
      converters::telemetry::ConvertPtrVector<cx_telem::PhysicalCpuInfo>(
          std::move(cpu_info->physical_cpus));

  Respond(ArgumentList(cx_telem::GetCpuInfo::Results::Create(result)));
}

// OsTelemetryGetDisplayInfoFunction
// -----------------------------------------------

void OsTelemetryGetDisplayInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetDisplayInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo({crosapi::ProbeCategoryEnum::kDisplay},
                                         std::move(cb));
}

void OsTelemetryGetDisplayInfoFunction::OnResult(
    crosapi::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->display_result || !ptr->display_result->is_display_info()) {
    Respond(Error("API internal error"));
    return;
  }

  cx_telem::DisplayInfo result;
  result = converters::telemetry::ConvertPtr(
      std::move(ptr->display_result->get_display_info()));

  Respond(ArgumentList(cx_telem::GetDisplayInfo::Results::Create(result)));
}

// OsTelemetryGetInternetConnectivityInfoFunction ------------------------------

void OsTelemetryGetInternetConnectivityInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(
      &OsTelemetryGetInternetConnectivityInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo({crosapi::ProbeCategoryEnum::kNetwork},
                                         std::move(cb));
}

void OsTelemetryGetInternetConnectivityInfoFunction::OnResult(
    crosapi::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->network_result ||
      !ptr->network_result->is_network_health()) {
    Respond(Error("API internal error"));
    return;
  }
  auto& network_info = ptr->network_result->get_network_health();

  const bool has_permission = extension()->permissions_data()->HasAPIPermission(
      extensions::mojom::APIPermissionID::kChromeOSTelemetryNetworkInformation);
  auto result = converters::telemetry::ConvertPtr(std::move(network_info),
                                                  has_permission);

  Respond(ArgumentList(
      cx_telem::GetInternetConnectivityInfo::Results::Create(result)));
}

// OsTelemetryGetMarketingInfoFunction -----------------------------------------

void OsTelemetryGetMarketingInfoFunction::RunIfAllowed() {
  auto cb =
      base::BindOnce(&OsTelemetryGetMarketingInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo({crosapi::ProbeCategoryEnum::kSystem},
                                         std::move(cb));
}

void OsTelemetryGetMarketingInfoFunction::OnResult(
    crosapi::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->system_result || !ptr->system_result->is_system_info()) {
    Respond(Error("API internal error"));
    return;
  }

  auto& system_info = ptr->system_result->get_system_info();

  if (!system_info->os_info) {
    Respond(Error("API internal error"));
    return;
  }

  cx_telem::MarketingInfo result;
  result.marketing_name = system_info->os_info->marketing_name;

  Respond(ArgumentList(cx_telem::GetMarketingInfo::Results::Create(result)));
}

// OsTelemetryGetMemoryInfoFunction --------------------------------------------

void OsTelemetryGetMemoryInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetMemoryInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo({crosapi::ProbeCategoryEnum::kMemory},
                                         std::move(cb));
}

void OsTelemetryGetMemoryInfoFunction::OnResult(
    crosapi::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->memory_result || !ptr->memory_result->is_memory_info()) {
    Respond(Error("API internal error"));
    return;
  }

  cx_telem::MemoryInfo result;

  const auto& memory_info = ptr->memory_result->get_memory_info();
  if (memory_info->total_memory_kib) {
    result.total_memory_ki_b = memory_info->total_memory_kib->value;
  }
  if (memory_info->free_memory_kib) {
    result.free_memory_ki_b = memory_info->free_memory_kib->value;
  }
  if (memory_info->available_memory_kib) {
    result.available_memory_ki_b = memory_info->available_memory_kib->value;
  }
  if (memory_info->page_faults_since_last_boot) {
    result.page_faults_since_last_boot =
        memory_info->page_faults_since_last_boot->value;
  }

  Respond(ArgumentList(cx_telem::GetMemoryInfo::Results::Create(result)));
}

// OsTelemetryGetOemDataFunction -----------------------------------------------

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

void OsTelemetryGetOemDataFunction::OnResult(crosapi::ProbeOemDataPtr ptr) {
  if (!ptr || !ptr->oem_data.has_value()) {
    Respond(Error("API internal error"));
    return;
  }

  cx_telem::OemData result;
  result.oem_data = std::move(ptr->oem_data);

  Respond(ArgumentList(cx_telem::GetOemData::Results::Create(result)));
}

// OsTelemetryGetOsVersionInfoFunction -----------------------------------------

void OsTelemetryGetOsVersionInfoFunction::RunIfAllowed() {
  auto cb =
      base::BindOnce(&OsTelemetryGetOsVersionInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo({crosapi::ProbeCategoryEnum::kSystem},
                                         std::move(cb));
}

void OsTelemetryGetOsVersionInfoFunction::OnResult(
    crosapi::ProbeTelemetryInfoPtr ptr) {
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

  cx_telem::OsVersionInfo result = converters::telemetry::ConvertPtr(
      std::move(system_info->os_info->os_version));

  Respond(ArgumentList(cx_telem::GetOsVersionInfo::Results::Create(result)));
}

// OsTelemetryGetStatefulPartitionInfoFunction ---------------------------------

void OsTelemetryGetStatefulPartitionInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(
      &OsTelemetryGetStatefulPartitionInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo(
      {crosapi::ProbeCategoryEnum::kStatefulPartition}, std::move(cb));
}

void OsTelemetryGetStatefulPartitionInfoFunction::OnResult(
    crosapi::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->stateful_partition_result ||
      !ptr->stateful_partition_result->is_partition_info()) {
    Respond(Error("API internal error"));
    return;
  }
  auto& stateful_part_info =
      ptr->stateful_partition_result->get_partition_info();

  cx_telem::StatefulPartitionInfo result =
      converters::telemetry::ConvertPtr(std::move(stateful_part_info));

  Respond(ArgumentList(
      cx_telem::GetStatefulPartitionInfo::Results::Create(result)));
}

// OsTelemetryGetThermalInfoFunction
// -----------------------------------------------

void OsTelemetryGetThermalInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetThermalInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo({crosapi::ProbeCategoryEnum::kThermal},
                                         std::move(cb));
}

void OsTelemetryGetThermalInfoFunction::OnResult(
    crosapi::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->thermal_result || !ptr->thermal_result->is_thermal_info()) {
    Respond(Error("API internal error"));
    return;
  }

  cx_telem::ThermalInfo result;
  result = converters::telemetry::ConvertPtr(
      std::move(ptr->thermal_result->get_thermal_info()));

  Respond(ArgumentList(cx_telem::GetThermalInfo::Results::Create(result)));
}

// OsTelemetryGetTpmInfoFunction -----------------------------------------------

void OsTelemetryGetTpmInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetTpmInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo({crosapi::ProbeCategoryEnum::kTpm},
                                         std::move(cb));
}

void OsTelemetryGetTpmInfoFunction::OnResult(
    crosapi::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->tpm_result || !ptr->tpm_result->is_tpm_info()) {
    Respond(Error("API internal error"));
    return;
  }
  auto& tpm_info = ptr->tpm_result->get_tpm_info();

  cx_telem::TpmInfo result =
      converters::telemetry::ConvertPtr(std::move(tpm_info));

  Respond(ArgumentList(cx_telem::GetTpmInfo::Results::Create(result)));
}

// OsTelemetryGetUsbBusInfoFunction --------------------------------------------

void OsTelemetryGetUsbBusInfoFunction::RunIfAllowed() {
  // USB info is guarded by the `os.attached_device_info` permission.
  if (!extension()->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::kChromeOSAttachedDeviceInfo)) {
    Respond(Error(
        "Unauthorized access to chrome.os.telemetry.getUsbBusInfo. Extension"
        " doesn't have the permission."));
    return;
  }

  auto cb = base::BindOnce(&OsTelemetryGetUsbBusInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo({crosapi::ProbeCategoryEnum::kBus},
                                         std::move(cb));
}

void OsTelemetryGetUsbBusInfoFunction::OnResult(
    crosapi::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->bus_result || !ptr->bus_result->is_bus_devices_info()) {
    Respond(Error("API internal error"));
    return;
  }

  cx_telem::UsbBusDevices result;
  auto bus_infos = std::move(ptr->bus_result->get_bus_devices_info());
  for (auto& info : bus_infos) {
    if (info->is_usb_bus_info()) {
      result.devices.push_back(converters::telemetry::ConvertPtr(
          std::move(info->get_usb_bus_info())));
    }
  }

  Respond(ArgumentList(cx_telem::GetUsbBusInfo::Results::Create(result)));
}

// OsTelemetryGetVpdInfoFunction -----------------------------------------------

void OsTelemetryGetVpdInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetVpdInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo(
      {crosapi::ProbeCategoryEnum::kCachedVpdData}, std::move(cb));
}

void OsTelemetryGetVpdInfoFunction::OnResult(
    crosapi::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->vpd_result || !ptr->vpd_result->is_vpd_info()) {
    Respond(Error("API internal error"));
    return;
  }

  const bool has_permission = extension()->permissions_data()->HasAPIPermission(
      extensions::mojom::APIPermissionID::kChromeOSTelemetrySerialNumber);
  auto result = converters::telemetry::ConvertPtr(
      std::move(ptr->vpd_result->get_vpd_info()), has_permission);

  Respond(ArgumentList(cx_telem::GetVpdInfo::Results::Create(result)));
}

}  // namespace chromeos
