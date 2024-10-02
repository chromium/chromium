// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/reven_log_source.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-shared.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"

namespace system_logs {

namespace {

namespace healthd = ::ash::cros_healthd::mojom;
using healthd::TelemetryInfo;
using healthd::TelemetryInfoPtr;
using ProbeCategories = healthd::ProbeCategoryEnum;

constexpr char kNotAvailable[] = "<not available>";

constexpr char kRevenAvailableMemoryKey[] = "chromeosflex_available_memory";
constexpr char kRevenBiosVersionKey[] = "chromeosflex_bios_version";
constexpr char kRevenBluetoothDriverKey[] = "chromeosflex_bluetooth_driver";
constexpr char kRevenBluetoothIdKey[] = "chromeosflex_bluetooth_id";
constexpr char kRevenBluetoothNameKey[] = "chromeosflex_bluetooth_name";
constexpr char kRevenCpuNameKey[] = "chromeosflex_cpu_name";
constexpr char kRevenEthernetDriverKey[] = "chromeosflex_ethernet_driver";
constexpr char kRevenEthernetIdKey[] = "chromeosflex_ethernet_id";
constexpr char kRevenEthernetNameKey[] = "chromeosflex_ethernet_name";
constexpr char kRevenFreeMemoryKey[] = "chromeosflex_free_memory";
constexpr char kRevenGlExtensionsKey[] = "chromeosflex_gl_extensions";
constexpr char kRevenGlRendererKey[] = "chromeosflex_gl_renderer";
constexpr char kRevenGlShadingVersionKey[] = "chromeosflex_gl_shading_version";
constexpr char kRevenGlVendorKey[] = "chromeosflex_gl_vendor";
constexpr char kRevenGlVersionKey[] = "chromeosflex_gl_version";
constexpr char kRevenGpuDriverKey[] = "chromeosflex_gpu_driver";
constexpr char kRevenGpuIdKey[] = "chromeosflex_gpu_id";
constexpr char kRevenGpuNameKey[] = "chromeosflex_gpu_name";
constexpr char kRevenProductNameKey[] = "chromeosflex_product_name";
constexpr char kRevenProductVendorKey[] = "chromeosflex_product_vendor";
constexpr char kRevenProductVersionKey[] = "chromeosflex_product_version";
constexpr char kRevenSecurebootKey[] = "chromeosflex_secureboot";
constexpr char kRevenTotalMemoryKey[] = "chromeosflex_total_memory";
constexpr char kRevenTouchpadStack[] = "chromeosflex_touchpad";
constexpr char kRevenTpmAllowListedKey[] = "chromeosflex_tpm_allow_listed";
constexpr char kRevenTpmDidVidKey[] = "chromeosflex_tpm_did_vid";
constexpr char kRevenTpmManufacturerKey[] = "chromeosflex_tpm_manufacturer";
constexpr char kRevenTpmOwnedKey[] = "chromeosflex_tpm_owned";
constexpr char kRevenTpmSpecLevelKey[] = "chromeosflex_tpm_spec_level";
constexpr char kRevenTpmVersionKey[] = "chromeosflex_tpm_version";
constexpr char kRevenUefiKey[] = "chromeosflex_uefi";
constexpr char kRevenWirelessDriverKey[] = "chromeosflex_wireless_driver";
constexpr char kRevenWirelessIdKey[] = "chromeosflex_wireless_id";
constexpr char kRevenWirelessNameKey[] = "chromeosflex_wireless_name";

// Format a gradually-accumulated, comma-separated list.
void StrListAppend(std::string* list, std::string_view value) {
  if (!list->empty()) {
    base::StrAppend(list, {", "});
  }
  base::StrAppend(list, {value});
}

// Format the combination of bus, vendor_id, and product_id or device_id.
std::string FormatDeviceIDs(const std::string& bus,
                            uint16_t vendor_id,
                            uint16_t product_id) {
  return base::StringPrintf("%s:%04x:%04x", bus.c_str(), vendor_id, product_id);
}

// TPM family. We use the TPM 2.0 style encoding, e.g.:
//  * TPM 1.2: "1.2" -> 0x312e3200
//  * TPM 2.0: "2.0" -> 0x322e3000
std::string ToTpmVersionStr(uint32_t tpm_family) {
  if (tpm_family == (uint32_t)0x312e3200) {
    return "1.2";
  } else if (tpm_family == (uint32_t)0x322e3000) {
    return "2.0";
  } else {
    return "unknown";
  }
}

std::string FormatBool(bool value) {
  return value ? "true" : "false";
}

void PopulateCpuInfo(SystemLogsResponse& psd, const TelemetryInfoPtr& info) {
  if (info->cpu_result.is_null() || info->cpu_result->is_error()) {
    DVLOG(1) << "CpuResult not found in croshealthd response";
    return;
  }
  std::vector<healthd::PhysicalCpuInfoPtr>& physical_cpus =
      info->cpu_result->get_cpu_info()->physical_cpus;
  DCHECK_GE(physical_cpus.size(), 1u);

  std::string cpu_names;
  for (const auto& cpu : physical_cpus) {
    // Missing/empty values will be distinguishable in the final list.
    StrListAppend(&cpu_names, cpu->model_name.value_or(kNotAvailable));
  }
  psd.emplace(kRevenCpuNameKey, cpu_names);
}

void PopulateMemoryInfo(SystemLogsResponse& psd, const TelemetryInfoPtr& info) {
  if (info->memory_result.is_null() || info->memory_result->is_error()) {
    DVLOG(1) << "MemoryResult not found in croshealthd response";
    return;
  }

  healthd::MemoryInfoPtr& memory_info = info->memory_result->get_memory_info();

  psd.emplace(kRevenTotalMemoryKey,
              base::NumberToString(memory_info->total_memory_kib));
  psd.emplace(kRevenFreeMemoryKey,
              base::NumberToString(memory_info->free_memory_kib));
  psd.emplace(kRevenAvailableMemoryKey,
              base::NumberToString(memory_info->available_memory_kib));
}

void PopulateSystemInfo(SystemLogsResponse& psd, const TelemetryInfoPtr& info) {
  if (info->system_result.is_null() || info->system_result->is_error()) {
    DVLOG(1) << "SystemResult not found in croshealthd response";
    return;
  }

  healthd::DmiInfoPtr& dmi_info =
      info->system_result->get_system_info()->dmi_info;
  if (!dmi_info.is_null()) {
    psd.emplace(kRevenProductVendorKey,
                dmi_info->sys_vendor.value_or(kNotAvailable));
    psd.emplace(kRevenProductNameKey,
                dmi_info->product_name.value_or(kNotAvailable));
    psd.emplace(kRevenProductVersionKey,
                dmi_info->product_version.value_or(kNotAvailable));
    psd.emplace(kRevenBiosVersionKey,
                dmi_info->bios_version.value_or(kNotAvailable));
  }

  healthd::OsInfoPtr& os_info = info->system_result->get_system_info()->os_info;
  if (!os_info.is_null()) {
    psd.emplace(
        kRevenSecurebootKey,
        FormatBool(os_info->boot_mode == healthd::BootMode::kCrosEfiSecure));
    psd.emplace(
        kRevenUefiKey,
        FormatBool(os_info->boot_mode == healthd::BootMode::kCrosEfi ||
                   os_info->boot_mode == healthd::BootMode::kCrosEfiSecure));
  }
}

// Constructs key names based on the passed label. Collects data from all passed
// devices into each value.
void PopulateBusDevicesInfo(SystemLogsResponse& psd,
                            std::string_view label,
                            const std::vector<healthd::BusDevicePtr>& devices) {
  if (devices.empty()) {
    return;
  }

  const std::string name_key = base::StrCat({"chromeosflex_", label, "_name"});
  std::string names;
  const std::string id_key = base::StrCat({"chromeosflex_", label, "_id"});
  std::string ids;
  const std::string driver_key =
      base::StrCat({"chromeosflex_", label, "_driver"});
  std::string drivers;

  for (const auto& device : devices) {
    StrListAppend(
        &names, base::StrCat({device->vendor_name, " ", device->product_name}));

    if (device->bus_info->is_pci_bus_info()) {
      const healthd::PciBusInfoPtr& pci_info =
          device->bus_info->get_pci_bus_info();

      StrListAppend(&ids, FormatDeviceIDs("pci", pci_info->vendor_id,
                                          pci_info->device_id));

      // Missing/empty values will be distinguishable in the final list.
      StrListAppend(&drivers, pci_info->driver.value_or(kNotAvailable));
    }

    if (device->bus_info->is_usb_bus_info()) {
      const healthd::UsbBusInfoPtr& usb_info =
          device->bus_info->get_usb_bus_info();

      StrListAppend(&ids, FormatDeviceIDs("usb", usb_info->vendor_id,
                                          usb_info->product_id));

      const std::vector<healthd::UsbBusInterfaceInfoPtr>& usb_interfaces =
          usb_info->interfaces;
      for (const auto& interface : usb_interfaces) {
        // Missing/empty values will be distinguishable in the final list.
        StrListAppend(&drivers, interface->driver.value_or(kNotAvailable));
      }
    }
  }

  psd.emplace(name_key, names);
  psd.emplace(id_key, ids);
  psd.emplace(driver_key, drivers);
}

void PopulateBusDevicesInfo(SystemLogsResponse& psd,
                            const TelemetryInfoPtr& info) {
  if (info->bus_result.is_null() || info->bus_result->is_error()) {
    DVLOG(1) << "BusResult not found in croshealthd response";
    return;
  }
  std::vector<healthd::BusDevicePtr>& bus_devices =
      info->bus_result->get_bus_devices();

  // When looking at bus devices there can be multiple of any given class
  // (though in practice we mostly see this with gpu). We need to accumulate
  // these values on a per-class basis, so: sort by class we care about, then
  // process in a group.
  std::vector<healthd::BusDevicePtr> ethernet_devices;
  std::vector<healthd::BusDevicePtr> wireless_devices;
  std::vector<healthd::BusDevicePtr> bluetooth_devices;
  std::vector<healthd::BusDevicePtr> gpu_devices;
  for (auto& device : bus_devices) {
    switch (device->device_class) {
      case healthd::BusDeviceClass::kEthernetController:
        ethernet_devices.push_back(std::move(device));
        break;
      case healthd::BusDeviceClass::kWirelessController:
        wireless_devices.push_back(std::move(device));
        break;
      case healthd::BusDeviceClass::kBluetoothAdapter:
        bluetooth_devices.push_back(std::move(device));
        break;
      case healthd::BusDeviceClass::kDisplayController:
        gpu_devices.push_back(std::move(device));
        break;
      default:
        break;
    }
  }

  // See kRevenEthernet*Key, kRevenWireless*Key,
  // kRevenBluetooth*Key, and kRevenGpu*Key.
  PopulateBusDevicesInfo(psd, "ethernet", ethernet_devices);
  PopulateBusDevicesInfo(psd, "wireless", wireless_devices);
  PopulateBusDevicesInfo(psd, "bluetooth", bluetooth_devices);
  PopulateBusDevicesInfo(psd, "gpu", gpu_devices);
}

void PopulateTpmInfo(SystemLogsResponse& psd, const TelemetryInfoPtr& info) {
  if (info->tpm_result.is_null() || info->tpm_result->is_error()) {
    DVLOG(1) << "TpmResult not found in croshealthd response";
    return;
  }
  const healthd::TpmInfoPtr& tpm_info = info->tpm_result->get_tpm_info();
  if (tpm_info.is_null()) {
    return;
  }

  psd.emplace(kRevenTpmDidVidKey, tpm_info->did_vid.value_or(kNotAvailable));
  psd.emplace(kRevenTpmAllowListedKey,
              FormatBool(tpm_info->supported_features->is_allowed));
  psd.emplace(kRevenTpmOwnedKey, FormatBool(tpm_info->status->owned));

  const healthd::TpmVersionPtr& version = tpm_info->version;
  if (version.is_null()) {
    return;
  }

  psd.emplace(kRevenTpmVersionKey, ToTpmVersionStr(version->family));
  psd.emplace(kRevenTpmSpecLevelKey, base::NumberToString(version->spec_level));
  psd.emplace(kRevenTpmManufacturerKey,
              base::NumberToString(version->manufacturer));
}

void PopulateGraphicsInfo(SystemLogsResponse& psd,
                          const TelemetryInfoPtr& info) {
  if (info->graphics_result.is_null() || info->graphics_result->is_error()) {
    DVLOG(1) << "GraphicsResult not found in croshealthd response";
    return;
  }
  const healthd::GraphicsInfoPtr& graphics_info =
      info->graphics_result->get_graphics_info();

  const healthd::GLESInfoPtr& gles_info = graphics_info->gles_info;
  psd.emplace(kRevenGlVersionKey, gles_info->version);
  psd.emplace(kRevenGlShadingVersionKey, gles_info->shading_version);
  psd.emplace(kRevenGlVendorKey, gles_info->vendor);
  psd.emplace(kRevenGlRendererKey, gles_info->renderer);
  psd.emplace(kRevenGlExtensionsKey,
              base::JoinString(gles_info->extensions, ", "));
}

std::string FetchTouchpadLibraryName() {
  return ash::cros_healthd::ServiceConnection::GetInstance()
      ->FetchTouchpadLibraryName();
}

// For any key not set, set it to "<not available>".
// We use the sentinel value to distinguish between "empty" data and data that
// couldn't be retrieved. We set all values to help keep reporting consistent.
void SetNotAvailablePsd(SystemLogsResponse& psd) {
  psd.try_emplace(kRevenAvailableMemoryKey, kNotAvailable);
  psd.try_emplace(kRevenBiosVersionKey, kNotAvailable);
  psd.try_emplace(kRevenBluetoothDriverKey, kNotAvailable);
  psd.try_emplace(kRevenBluetoothIdKey, kNotAvailable);
  psd.try_emplace(kRevenBluetoothNameKey, kNotAvailable);
  psd.try_emplace(kRevenCpuNameKey, kNotAvailable);
  psd.try_emplace(kRevenEthernetDriverKey, kNotAvailable);
  psd.try_emplace(kRevenEthernetIdKey, kNotAvailable);
  psd.try_emplace(kRevenEthernetNameKey, kNotAvailable);
  psd.try_emplace(kRevenFreeMemoryKey, kNotAvailable);
  psd.try_emplace(kRevenGlExtensionsKey, kNotAvailable);
  psd.try_emplace(kRevenGlRendererKey, kNotAvailable);
  psd.try_emplace(kRevenGlShadingVersionKey, kNotAvailable);
  psd.try_emplace(kRevenGlVendorKey, kNotAvailable);
  psd.try_emplace(kRevenGlVersionKey, kNotAvailable);
  psd.try_emplace(kRevenGpuDriverKey, kNotAvailable);
  psd.try_emplace(kRevenGpuIdKey, kNotAvailable);
  psd.try_emplace(kRevenGpuNameKey, kNotAvailable);
  psd.try_emplace(kRevenProductNameKey, kNotAvailable);
  psd.try_emplace(kRevenProductVendorKey, kNotAvailable);
  psd.try_emplace(kRevenProductVersionKey, kNotAvailable);
  psd.try_emplace(kRevenSecurebootKey, kNotAvailable);
  psd.try_emplace(kRevenTotalMemoryKey, kNotAvailable);
  psd.try_emplace(kRevenTpmAllowListedKey, kNotAvailable);
  psd.try_emplace(kRevenTpmDidVidKey, kNotAvailable);
  psd.try_emplace(kRevenTpmManufacturerKey, kNotAvailable);
  psd.try_emplace(kRevenTpmOwnedKey, kNotAvailable);
  psd.try_emplace(kRevenTpmSpecLevelKey, kNotAvailable);
  psd.try_emplace(kRevenTpmVersionKey, kNotAvailable);
  psd.try_emplace(kRevenUefiKey, kNotAvailable);
  psd.try_emplace(kRevenWirelessDriverKey, kNotAvailable);
  psd.try_emplace(kRevenWirelessIdKey, kNotAvailable);
  psd.try_emplace(kRevenWirelessNameKey, kNotAvailable);
}

}  // namespace

RevenLogSource::RevenLogSource() : SystemLogsSource("Reven") {
  ash::cros_healthd::ServiceConnection::GetInstance()->BindProbeService(
      probe_service_.BindNewPipeAndPassReceiver());
}

RevenLogSource::~RevenLogSource() = default;

void RevenLogSource::Fetch(SysLogsSourceCallback callback) {
  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kBluetooth, ProbeCategories::kBus,
       ProbeCategories::kCpu, ProbeCategories::kGraphics,
       ProbeCategories::kMemory, ProbeCategories::kSystem,
       ProbeCategories::kTpm},
      base::BindOnce(&RevenLogSource::OnTelemetryInfoProbeResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RevenLogSource::OnTelemetryInfoProbeResponse(
    SysLogsSourceCallback callback,
    TelemetryInfoPtr info_ptr) {
  SystemLogsResponse product_specific_data;

  if (info_ptr.is_null()) {
    DVLOG(1) << "Null response from croshealthd::ProbeTelemetryInfo.";
  }

  PopulateSystemInfo(product_specific_data, info_ptr);
  PopulateCpuInfo(product_specific_data, info_ptr);
  PopulateMemoryInfo(product_specific_data, info_ptr);
  PopulateBusDevicesInfo(product_specific_data, info_ptr);
  PopulateTpmInfo(product_specific_data, info_ptr);
  PopulateGraphicsInfo(product_specific_data, info_ptr);

  // Make sure all keys are present.
  SetNotAvailablePsd(product_specific_data);

  base::OnceCallback<void(const std::string&)> reply_cb = base::BindOnce(
      [](SysLogsSourceCallback callback, const SystemLogsResponse& psd,
         const std::string& touchpad_lib_name) {
        auto response = std::make_unique<SystemLogsResponse>(psd);
        // Make sure to overwrite the "not available".
        response->emplace(kRevenTouchpadStack, touchpad_lib_name);
        std::move(callback).Run(std::move(response));
      },
      std::move(callback), product_specific_data);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&FetchTouchpadLibraryName), std::move(reply_cb));
}

}  // namespace system_logs
