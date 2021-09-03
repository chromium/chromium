// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/reven_log_source.h"

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

void AddIndentedLogEntry(std::string* log,
                         const std::string& key,
                         const std::string& value) {
  base::StrAppend(log, {kNewlineWithIndent, key, kKeyValueDelimiter, value});
}

void AddLogEntry(std::string* log,
                 const std::string& key,
                 const std::string& value) {
  base::StrAppend(log, {key, kKeyValueDelimiter, value, "\n"});
}

void PopulateBluetoothInfo(std::string* log, const TelemetryInfoPtr& info) {
  if (info->bluetooth_result.is_null() || info->bluetooth_result->is_error()) {
    DVLOG(1) << "BluetoothResult not found in croshealthd response";
    return;
  }
  std::vector<healthd::BluetoothAdapterInfoPtr>& adapters =
      info->bluetooth_result->get_bluetooth_adapter_info();
  base::StrAppend(log, {"bluetoothinfo"});
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

  base::StrAppend(log, {"cpuinfo"});
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
  base::StrAppend(log, {"meminfo"});
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
}

}  // namespace

RevenLogSource::RevenLogSource() : SystemLogsSource("Reven") {
  ash::cros_healthd::ServiceConnection::GetInstance()->GetProbeService(
      probe_service_.BindNewPipeAndPassReceiver());
}

RevenLogSource::~RevenLogSource() = default;

void RevenLogSource::Fetch(SysLogsSourceCallback callback) {
  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kBluetooth, ProbeCategories::kCpu,
       ProbeCategories::kMemory, ProbeCategories::kSystem2},
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
  }

  response->emplace(kRevenLogKey, log_val);
  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs
