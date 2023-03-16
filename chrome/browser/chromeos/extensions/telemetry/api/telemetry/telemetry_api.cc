// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry/telemetry_api.h"

#include <inttypes.h>

#include <memory>
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
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

namespace telemetry = api::os_telemetry;

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

// OsTelemetryGetAudioInfoFunction ---------------------------------------------

void OsTelemetryGetAudioInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetAudioInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo(
      {crosapi::mojom::ProbeCategoryEnum::kAudio}, std::move(cb));
}

void OsTelemetryGetAudioInfoFunction::OnResult(
    crosapi::mojom::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->audio_result || !ptr->audio_result->is_audio_info()) {
    Respond(Error("API internal error"));
    return;
  }
  auto& audio_info = ptr->audio_result->get_audio_info();

  auto result =
      converters::ConvertPtr<telemetry::AudioInfo>(std::move(audio_info));

  Respond(ArgumentList(telemetry::GetAudioInfo::Results::Create(result)));
}

// OsTelemetryGetBatteryInfoFunction -------------------------------------------

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
  absl::optional<std::string> serial_number;
  if (extension()->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::kChromeOSTelemetrySerialNumber)) {
    serial_number = std::move(battery_info->serial_number);
  }

  telemetry::BatteryInfo result =
      converters::ConvertPtr<telemetry::BatteryInfo>(std::move(battery_info));

  if (serial_number && !serial_number->empty()) {
    result.serial_number = std::move(serial_number);
  }

  Respond(ArgumentList(telemetry::GetBatteryInfo::Results::Create(result)));
}

// OsTelemetryGetNonRemovableBlockDevicesInfoFunction --------------------------

void OsTelemetryGetNonRemovableBlockDevicesInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(
      &OsTelemetryGetNonRemovableBlockDevicesInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo(
      {crosapi::mojom::ProbeCategoryEnum::kNonRemovableBlockDevices},
      std::move(cb));
}

void OsTelemetryGetNonRemovableBlockDevicesInfoFunction::OnResult(
    crosapi::mojom::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->block_device_result ||
      !ptr->block_device_result->is_block_device_info()) {
    Respond(Error("API internal error"));
    return;
  }
  auto& block_device_info = ptr->block_device_result->get_block_device_info();

  auto infos =
      converters::ConvertPtrVector<telemetry::NonRemovableBlockDeviceInfo>(
          std::move(block_device_info));
  telemetry::NonRemovableBlockDeviceInfoResponse result;
  result.device_infos = std::move(infos);

  Respond(ArgumentList(
      telemetry::GetNonRemovableBlockDevicesInfo::Results::Create(result)));
}

// OsTelemetryGetCpuInfoFunction -----------------------------------------------

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

  telemetry::CpuInfo result;
  if (cpu_info->num_total_threads) {
    result.num_total_threads = cpu_info->num_total_threads->value;
  }
  result.architecture = converters::Convert(cpu_info->architecture);
  result.physical_cpus =
      converters::ConvertPtrVector<telemetry::PhysicalCpuInfo>(
          std::move(cpu_info->physical_cpus));

  Respond(ArgumentList(telemetry::GetCpuInfo::Results::Create(result)));
}

// OsTelemetryGetInternetConnectivityInfoFunction ------------------------------

void OsTelemetryGetInternetConnectivityInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(
      &OsTelemetryGetInternetConnectivityInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo(
      {crosapi::mojom::ProbeCategoryEnum::kNetwork}, std::move(cb));
}

void OsTelemetryGetInternetConnectivityInfoFunction::OnResult(
    crosapi::mojom::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->network_result ||
      !ptr->network_result->is_network_health()) {
    Respond(Error("API internal error"));
    return;
  }
  auto& network_info = ptr->network_result->get_network_health();

  telemetry::InternetConnectivityInfo result;
  for (auto& network : network_info->networks) {
    absl::optional<std::string> mac_address;
    if (extension()->permissions_data()->HasAPIPermission(
            extensions::mojom::APIPermissionID::
                kChromeOSTelemetryNetworkInformation)) {
      mac_address = std::move(network->mac_address);
    }

    auto converted_network =
        converters::ConvertPtr<telemetry::NetworkInfo>(std::move(network));

    if (mac_address && !mac_address->empty()) {
      converted_network.mac_address = std::move(mac_address);
    }

    // Don't include networks with an undefined type.
    if (converted_network.type != telemetry::NetworkType::kNone) {
      result.networks.push_back(std::move(converted_network));
    }
  }

  Respond(ArgumentList(
      telemetry::GetInternetConnectivityInfo::Results::Create(result)));
}

// OsTelemetryGetMarketingInfoFunction -----------------------------------------

void OsTelemetryGetMarketingInfoFunction::RunIfAllowed() {
  auto cb =
      base::BindOnce(&OsTelemetryGetMarketingInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo(
      {crosapi::mojom::ProbeCategoryEnum::kSystem}, std::move(cb));
}

void OsTelemetryGetMarketingInfoFunction::OnResult(
    crosapi::mojom::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->system_result || !ptr->system_result->is_system_info()) {
    Respond(Error("API internal error"));
    return;
  }

  auto& system_info = ptr->system_result->get_system_info();

  if (!system_info->os_info) {
    Respond(Error("API internal error"));
    return;
  }

  telemetry::MarketingInfo result;
  result.marketing_name = system_info->os_info->marketing_name;

  Respond(ArgumentList(telemetry::GetMarketingInfo::Results::Create(result)));
}

// OsTelemetryGetMemoryInfoFunction --------------------------------------------

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

  telemetry::MemoryInfo result;

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

  Respond(ArgumentList(telemetry::GetMemoryInfo::Results::Create(result)));
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

void OsTelemetryGetOemDataFunction::OnResult(
    crosapi::mojom::ProbeOemDataPtr ptr) {
  if (!ptr || !ptr->oem_data.has_value()) {
    Respond(Error("API internal error"));
    return;
  }

  telemetry::OemData result;
  result.oem_data = std::move(ptr->oem_data);

  Respond(ArgumentList(telemetry::GetOemData::Results::Create(result)));
}

// OsTelemetryGetOsVersionInfoFunction -----------------------------------------

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

  telemetry::OsVersionInfo result =
      converters::ConvertPtr<telemetry::OsVersionInfo>(
          std::move(system_info->os_info->os_version));

  Respond(ArgumentList(telemetry::GetOsVersionInfo::Results::Create(result)));
}

// OsTelemetryGetStatefulPartitionInfoFunction ---------------------------------

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

  telemetry::StatefulPartitionInfo result =
      converters::ConvertPtr<telemetry::StatefulPartitionInfo>(
          std::move(stateful_part_info));

  Respond(ArgumentList(
      telemetry::GetStatefulPartitionInfo::Results::Create(result)));
}

// OsTelemetryGetTpmInfoFunction -----------------------------------------------

void OsTelemetryGetTpmInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetTpmInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo(
      {crosapi::mojom::ProbeCategoryEnum::kTpm}, std::move(cb));
}

void OsTelemetryGetTpmInfoFunction::OnResult(
    crosapi::mojom::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->tpm_result || !ptr->tpm_result->is_tpm_info()) {
    Respond(Error("API internal error"));
    return;
  }
  auto& tpm_info = ptr->tpm_result->get_tpm_info();

  telemetry::TpmInfo result =
      converters::ConvertPtr<telemetry::TpmInfo>(std::move(tpm_info));

  Respond(ArgumentList(telemetry::GetTpmInfo::Results::Create(result)));
}

// OsTelemetryGetUsbBusInfoFunction --------------------------------------------

void OsTelemetryGetUsbBusInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetUsbBusInfoFunction::OnResult, this);

  GetRemoteService()->ProbeTelemetryInfo(
      {crosapi::mojom::ProbeCategoryEnum::kBus}, std::move(cb));
}

void OsTelemetryGetUsbBusInfoFunction::OnResult(
    crosapi::mojom::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->bus_result || !ptr->bus_result->is_bus_devices_info()) {
    Respond(Error("API internal error"));
    return;
  }

  telemetry::UsbBusDevices result;
  auto bus_infos = std::move(ptr->bus_result->get_bus_devices_info());
  for (auto& info : bus_infos) {
    if (info->is_usb_bus_info()) {
      result.devices.push_back(converters::ConvertPtr<telemetry::UsbBusInfo>(
          std::move(info->get_usb_bus_info())));
    }
  }

  Respond(ArgumentList(telemetry::GetUsbBusInfo::Results::Create(result)));
}

// OsTelemetryGetVpdInfoFunction -----------------------------------------------

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

  telemetry::VpdInfo result;

  const auto& vpd_info = ptr->vpd_result->get_vpd_info();
  result.activate_date = vpd_info->first_power_date;
  result.model_name = vpd_info->model_name;
  result.sku_number = vpd_info->sku_number;

  // Protect accessing the serial number by a permission.
  if (extension()->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::kChromeOSTelemetrySerialNumber)) {
    result.serial_number = vpd_info->serial_number;
  }

  Respond(ArgumentList(telemetry::GetVpdInfo::Results::Create(result)));
}

}  // namespace chromeos
