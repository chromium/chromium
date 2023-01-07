// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/reven_log_source.h"

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
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

constexpr char kRevenLogKey[] = "CHROMEOSFLEX_HARDWARE_INFO";
constexpr char kNewlineWithIndent[] = "\n  ";
constexpr char kKeyValueDelimiter[] = ": ";

// Format for nested log entry:
// {label}:
//   {key1}: {value1}
//   {key2}: {value2}
void AddIndentedLogEntry(std::string* log,
                         const base::StringPiece key,
                         const base::StringPiece value) {
  base::StrAppend(log, {kNewlineWithIndent, key, kKeyValueDelimiter, value});
}

// Format for regular log entry:
// {key1}: {value1}
void AddLogEntry(std::string* log,
                 const base::StringPiece key,
                 const base::StringPiece value) {
  base::StrAppend(log, {key, kKeyValueDelimiter, value, "\n"});
}

// Format the combination of vendor_id and product_id (or device_id)
std::string FormatDeviceIDs(uint16_t vendor_id, uint16_t product_id) {
  return base::StringPrintf("%04x:%04x", vendor_id, product_id);
}

// TPM family. We use the TPM 2.0 style encoding, e.g.:
//  * TPM 1.2: "1.2" -> 0x312e3200
//  * TPM 2.0: "2.0" -> 0x322e3000
std::string ToTpmVersionStr(uint32_t tpm_family) {
  if (tpm_family == (uint32_t)0x312e3200)
    return "1.2";
  else if (tpm_family == (uint32_t)0x322e3000)
    return "2.0";
  else
    return "unknown";
}

std::string FormatBool(bool value) {
  return value ? "true" : "false";
}

void PopulateCpuInfo(std::string* log, const TelemetryInfoPtr& info) {
  if (info->cpu_result.is_null() || info->cpu_result->is_error()) {
    DVLOG(1) << "CpuResult not found in croshealthd response";
    return;
  }
  std::vector<healthd::PhysicalCpuInfoPtr>& physical_cpus =
      info->cpu_result->get_cpu_info()->physical_cpus;
  DCHECK_GE(physical_cpus.size(), 1u);

  base::StrAppend(log, {"cpuinfo:"});
  for (const auto& cpu : physical_cpus) {
    AddIndentedLogEntry(log, "cpu_name", cpu->model_name.value_or(""));
  }
  base::StrAppend(log, {"\n"});
}

void PopulateMemoryInfo(std::string* log, const TelemetryInfoPtr& info) {
  if (info->memory_result.is_null() || info->memory_result->is_error()) {
    DVLOG(1) << "MemoryResult not found in croshealthd response";
    return;
  }
  healthd::MemoryInfoPtr& memory_info = info->memory_result->get_memory_info();
  base::StrAppend(log, {"meminfo:"});
  AddIndentedLogEntry(log, "total_memory_kib",
                      base::NumberToString(memory_info->total_memory_kib));
  AddIndentedLogEntry(log, "free_memory_kib",
                      base::NumberToString(memory_info->free_memory_kib));
  AddIndentedLogEntry(log, "available_memory_kib",
                      base::NumberToString(memory_info->available_memory_kib));
  base::StrAppend(log, {"\n"});
}

void PopulateSystemInfo(std::string* log, const TelemetryInfoPtr& info) {
  if (info->system_result.is_null() || info->system_result->is_error()) {
    DVLOG(1) << "SystemResult not found in croshealthd response";
    return;
  }
  healthd::DmiInfoPtr& dmi_info =
      info->system_result->get_system_info()->dmi_info;

  if (!dmi_info.is_null()) {
    AddLogEntry(log, "product_vendor", dmi_info->sys_vendor.value_or(""));
    AddLogEntry(log, "product_name", dmi_info->product_name.value_or(""));
    AddLogEntry(log, "product_version", dmi_info->product_version.value_or(""));
  }

  base::StrAppend(log, {"bios_info:"});
  if (!dmi_info.is_null()) {
    AddIndentedLogEntry(log, "bios_version",
                        dmi_info->bios_version.value_or(""));
  }
  healthd::OsInfoPtr& os_info = info->system_result->get_system_info()->os_info;
  if (!os_info.is_null()) {
    AddIndentedLogEntry(
        log, "secureboot",
        FormatBool(os_info->boot_mode == healthd::BootMode::kCrosEfiSecure));
    AddIndentedLogEntry(
        log, "uefi",
        FormatBool(os_info->boot_mode == healthd::BootMode::kCrosEfi ||
                   os_info->boot_mode == healthd::BootMode::kCrosEfiSecure));
  }
  base::StrAppend(log, {"\n"});
}

// Log bus devices info with the following format:
// {label}_info:
//   {label}_name: ...
//   {label}_id: ...
//   {label}_bus: ...
//   {label}_driver: ...
//
// where label is one of the following:
//   ethernet_adapter
//   wireless_adapter
//   bluetooth_adapter
//   gpu
void PopulateBusDevicesInfo(std::string* log,
                            const base::StringPiece prefix,
                            const std::vector<healthd::BusDevicePtr>& devices) {
  if (devices.empty())
    return;
  base::StrAppend(log, {prefix, "_info:"});

  for (const auto& device : devices) {
    AddIndentedLogEntry(
        log, base::StrCat({prefix, "_name"}),
        base::StrCat({device->vendor_name, " ", device->product_name}));

    if (device->bus_info->is_pci_bus_info()) {
      const healthd::PciBusInfoPtr& pci_info =
          device->bus_info->get_pci_bus_info();

      AddIndentedLogEntry(
          log, base::StrCat({prefix, "_id"}),
          FormatDeviceIDs(pci_info->vendor_id, pci_info->device_id));
      AddIndentedLogEntry(log, base::StrCat({prefix, "_bus"}), "pci");
      AddIndentedLogEntry(log, base::StrCat({prefix, "_driver"}),
                          pci_info->driver.value_or(""));
    }

    if (device->bus_info->is_usb_bus_info()) {
      const healthd::UsbBusInfoPtr& usb_info =
          device->bus_info->get_usb_bus_info();

      AddIndentedLogEntry(
          log, base::StrCat({prefix, "_id"}),
          FormatDeviceIDs(usb_info->vendor_id, usb_info->product_id));
      AddIndentedLogEntry(log, base::StrCat({prefix, "_bus"}), "usb");
      const std::vector<healthd::UsbBusInterfaceInfoPtr>& usb_interfaces =
          usb_info->interfaces;
      for (const auto& interface : usb_interfaces) {
        AddIndentedLogEntry(log, base::StrCat({prefix, "_driver"}),
                            interface->driver.value_or(""));
      }
    }

    base::StrAppend(log, {"\n"});
  }
}

void PopulateBusDevicesInfo(std::string* log, const TelemetryInfoPtr& info) {
  if (info->bus_result.is_null() || info->bus_result->is_error()) {
    DVLOG(1) << "BusResult not found in croshealthd response";
    return;
  }
  std::vector<healthd::BusDevicePtr>& bus_devices =
      info->bus_result->get_bus_devices();

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
  PopulateBusDevicesInfo(log, "ethernet_adapter", ethernet_devices);
  PopulateBusDevicesInfo(log, "wireless_adapter", wireless_devices);
  PopulateBusDevicesInfo(log, "bluetooth_adapter", bluetooth_devices);
  PopulateBusDevicesInfo(log, "gpu", gpu_devices);
}

void PopulateTpmInfo(std::string* log, const TelemetryInfoPtr& info) {
  if (info->tpm_result.is_null() || info->tpm_result->is_error()) {
    DVLOG(1) << "TpmResult not found in croshealthd response";
    return;
  }
  const healthd::TpmInfoPtr& dmi_info = info->tpm_result->get_tpm_info();
  base::StrAppend(log, {"tpm_info:"});

  const healthd::TpmVersionPtr& version = dmi_info->version;
  AddIndentedLogEntry(log, "tpm_version", ToTpmVersionStr(version->family));
  AddIndentedLogEntry(log, "spec_level",
                      base::NumberToString(version->spec_level));
  AddIndentedLogEntry(log, "manufacturer",
                      base::NumberToString(version->manufacturer));
  AddIndentedLogEntry(log, "did_vid", dmi_info->did_vid.value_or(""));

  AddIndentedLogEntry(log, "tpm_allow_listed",
                      FormatBool(dmi_info->supported_features->is_allowed));
  AddIndentedLogEntry(log, "tpm_owned", FormatBool(dmi_info->status->owned));

  base::StrAppend(log, {"\n"});
}

void PopulateGraphicsInfo(std::string* log, const TelemetryInfoPtr& info) {
  if (info->graphics_result.is_null() || info->graphics_result->is_error()) {
    DVLOG(1) << "GraphicsResult not found in croshealthd response";
    return;
  }
  const healthd::GraphicsInfoPtr& graphics_info =
      info->graphics_result->get_graphics_info();

  base::StrAppend(log, {"graphics_info:"});
  const healthd::GLESInfoPtr& gles_info = graphics_info->gles_info;
  AddIndentedLogEntry(log, "gl_version", gles_info->version);
  AddIndentedLogEntry(log, "gl_shading_version", gles_info->shading_version);
  AddIndentedLogEntry(log, "gl_vendor", gles_info->vendor);
  AddIndentedLogEntry(log, "gl_renderer", gles_info->renderer);
  AddIndentedLogEntry(log, "gl_extensions",
                      base::JoinString(gles_info->extensions, ", "));

  base::StrAppend(log, {"\n"});
}

std::string FetchTouchpadLibraryName() {
  return ash::cros_healthd::ServiceConnection::GetInstance()
      ->FetchTouchpadLibraryName();
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
  std::string log_val;

  if (info_ptr.is_null()) {
    DVLOG(1) << "Null response from croshealthd::ProbeTelemetryInfo.";
    base::StrAppend(&log_val, {"<not available>"});
  } else {
    PopulateSystemInfo(&log_val, info_ptr);
    PopulateCpuInfo(&log_val, info_ptr);
    PopulateMemoryInfo(&log_val, info_ptr);
    PopulateBusDevicesInfo(&log_val, info_ptr);
    PopulateTpmInfo(&log_val, info_ptr);
    PopulateGraphicsInfo(&log_val, info_ptr);
  }

  base::OnceCallback<void(const std::string&)> reply_cb = base::BindOnce(
      [](SysLogsSourceCallback callback, const std::string& log_val,
         const std::string& touchpad_lib_name) {
        std::string log_final = log_val;
        AddLogEntry(&log_final, "touchpad_stack", touchpad_lib_name);

        auto response = std::make_unique<SystemLogsResponse>();
        response->emplace(kRevenLogKey, std::move(log_final));

        std::move(callback).Run(std::move(response));
      },
      std::move(callback), log_val);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&FetchTouchpadLibraryName), std::move(reply_cb));
}

}  // namespace system_logs
