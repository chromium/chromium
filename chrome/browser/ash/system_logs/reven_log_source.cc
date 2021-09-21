// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/reven_log_source.h"

#include <base/strings/stringprintf.h>
#include "base/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-shared.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"

namespace system_logs {

namespace {

namespace healthd = ::chromeos::cros_healthd::mojom;
using healthd::TelemetryInfo;
using healthd::TelemetryInfoPtr;
using ProbeCategories = healthd::ProbeCategoryEnum;

// TODO(xiangdongkong): replace cloudready with the official branding name
constexpr char kRevenLogKey[] = "CLOUDREADY_HARDWARE_INFO";
constexpr char kNewlineWithIndent[] = "\n  ";
constexpr char kKeyValueDelimiter[] = ": ";

// Format for nested log entry:
// {label}:
//   {key1}: {value1}
//   {key2}: {value2}
void AddIndentedLogEntry(std::string* log,
                         const std::string& key,
                         const std::string& value) {
  base::StrAppend(log, {kNewlineWithIndent, key, kKeyValueDelimiter, value});
}

// Format for regular log entry:
// {key1}: {value1}
void AddLogEntry(std::string* log,
                 const std::string& key,
                 const std::string& value) {
  base::StrAppend(log, {key, kKeyValueDelimiter, value, "\n"});
}

// Format the combination of vendor_id and product_id (or device_id)
std::string FormatDeviceIDs(uint16_t vendor_id, uint16_t product_id) {
  return base::StringPrintf("%04x:%04x", vendor_id, product_id);
}

void PopulateBluetoothInfo(std::string* log, const TelemetryInfoPtr& info) {
  if (info->bluetooth_result.is_null() || info->bluetooth_result->is_error()) {
    DVLOG(1) << "BluetoothResult not found in croshealthd response";
    return;
  }
  std::vector<healthd::BluetoothAdapterInfoPtr>& adapters =
      info->bluetooth_result->get_bluetooth_adapter_info();
  base::StrAppend(log, {"bluetoothinfo:"});
  for (const auto& adapter : adapters) {
    AddIndentedLogEntry(log, "bluetooth_adapter_name", adapter->name);
    AddIndentedLogEntry(log, "powered", (adapter->powered ? "on" : "off"));
  }
  base::StrAppend(log, {"\n"});
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
  if (info->system_result_v2.is_null() || info->system_result_v2->is_error()) {
    DVLOG(1) << "SystemResult2 not found in croshealthd response";
    return;
  }
  healthd::DmiInfoPtr& dmi_info =
      info->system_result_v2->get_system_info_v2()->dmi_info;

  if (!dmi_info.is_null()) {
    AddLogEntry(log, "product_vendor", dmi_info->sys_vendor.value_or(""));
    AddLogEntry(log, "product_name", dmi_info->product_name.value_or(""));
    AddLogEntry(log, "product_version", dmi_info->product_version.value_or(""));
  }

  base::StrAppend(log, {"biosinfo:"});
  if (!dmi_info.is_null()) {
    AddIndentedLogEntry(log, "bios_version",
                        dmi_info->bios_version.value_or(""));
  }
  healthd::OsInfoPtr& os_info =
      info->system_result_v2->get_system_info_v2()->os_info;
  if (!os_info.is_null()) {
    AddIndentedLogEntry(
        log, "uefi",
        (os_info->boot_mode == healthd::BootMode::kCrosEfi) ? "true" : "false");
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
void PopulateBusDevicesInfo(std::string* log,
                            const std::string& prefix,
                            const std::vector<healthd::BusDevicePtr>& devices) {
  if (devices.empty())
    return;
  base::StrAppend(log, {prefix, "_info:"});

  for (auto& device : devices) {
    AddIndentedLogEntry(
        log, base::StrCat({prefix, "_name"}),
        base::StrCat({device->vendor_name, " ", device->product_name}));

    if (device->bus_info->is_pci_bus_info()) {
      healthd::PciBusInfoPtr& pci_info = device->bus_info->get_pci_bus_info();

      AddIndentedLogEntry(
          log, base::StrCat({prefix, "_id"}),
          FormatDeviceIDs(pci_info->vendor_id, pci_info->device_id));
      AddIndentedLogEntry(log, base::StrCat({prefix, "_bus"}), "pci");
      AddIndentedLogEntry(log, base::StrCat({prefix, "_driver"}),
                          pci_info->driver.value_or(""));
    }

    if (device->bus_info->is_usb_bus_info()) {
      healthd::UsbBusInfoPtr& usb_info = device->bus_info->get_usb_bus_info();

      AddIndentedLogEntry(
          log, base::StrCat({prefix, "_id"}),
          FormatDeviceIDs(usb_info->vendor_id, usb_info->product_id));
      AddIndentedLogEntry(log, base::StrCat({prefix, "_bus"}), "usb");
      std::vector<healthd::UsbBusInterfaceInfoPtr>& usb_interfaces =
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
  for (auto& device : bus_devices) {
    switch (device->device_class) {
      case healthd::BusDeviceClass::kEthernetController:
        ethernet_devices.push_back(std::move(device));
        break;
      case healthd::BusDeviceClass::kWirelessController:
        wireless_devices.push_back(std::move(device));
        break;
      default:
        break;
    }
  }
  PopulateBusDevicesInfo(log, "ethernet_adapter", ethernet_devices);
  PopulateBusDevicesInfo(log, "wireless_adapter", wireless_devices);
}

}  // namespace

RevenLogSource::RevenLogSource() : SystemLogsSource("Reven") {
  ash::cros_healthd::ServiceConnection::GetInstance()->GetProbeService(
      probe_service_.BindNewPipeAndPassReceiver());
}

RevenLogSource::~RevenLogSource() = default;

void RevenLogSource::Fetch(SysLogsSourceCallback callback) {
  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kBluetooth, ProbeCategories::kBus,
       ProbeCategories::kCpu, ProbeCategories::kMemory,
       ProbeCategories::kSystem2},
      base::BindOnce(&RevenLogSource::OnTelemetryInfoProbeResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RevenLogSource::OnTelemetryInfoProbeResponse(
    SysLogsSourceCallback callback,
    TelemetryInfoPtr info_ptr) {
  auto response = std::make_unique<SystemLogsResponse>();

  std::string log_val;

  if (info_ptr.is_null()) {
    DVLOG(1) << "Null response from croshealthd::ProbeTelemetryInfo.";
    base::StrAppend(&log_val, {"<not available>"});
  } else {
    PopulateBluetoothInfo(&log_val, info_ptr);
    PopulateCpuInfo(&log_val, info_ptr);
    PopulateMemoryInfo(&log_val, info_ptr);
    PopulateSystemInfo(&log_val, info_ptr);
    PopulateBusDevicesInfo(&log_val, info_ptr);
  }

  response->emplace(kRevenLogKey, log_val);
  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs
