// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/telemetry_extension_ui/services/probe_service_converters.h"

#include <unistd.h>
#include <utility>

#include "ash/webui/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"

namespace ash {
namespace converters {

namespace {

namespace cros_healthd = ::ash::cros_healthd;

cros_healthd::mojom::ProbeCategoryEnum Convert(
    health::mojom::ProbeCategoryEnum input) {
  switch (input) {
    case health::mojom::ProbeCategoryEnum::kUnknown:
      return cros_healthd::mojom::ProbeCategoryEnum::kUnknown;
    case health::mojom::ProbeCategoryEnum::kBattery:
      return cros_healthd::mojom::ProbeCategoryEnum::kBattery;
    case health::mojom::ProbeCategoryEnum::kNonRemovableBlockDevices:
      return cros_healthd::mojom::ProbeCategoryEnum::kNonRemovableBlockDevices;
    case health::mojom::ProbeCategoryEnum::kCachedVpdData:
      return cros_healthd::mojom::ProbeCategoryEnum::kSystem;
    case health::mojom::ProbeCategoryEnum::kCpu:
      return cros_healthd::mojom::ProbeCategoryEnum::kCpu;
    case health::mojom::ProbeCategoryEnum::kTimezone:
      return cros_healthd::mojom::ProbeCategoryEnum::kTimezone;
    case health::mojom::ProbeCategoryEnum::kMemory:
      return cros_healthd::mojom::ProbeCategoryEnum::kMemory;
    case health::mojom::ProbeCategoryEnum::kBacklight:
      return cros_healthd::mojom::ProbeCategoryEnum::kBacklight;
    case health::mojom::ProbeCategoryEnum::kFan:
      return cros_healthd::mojom::ProbeCategoryEnum::kFan;
    case health::mojom::ProbeCategoryEnum::kStatefulPartition:
      return cros_healthd::mojom::ProbeCategoryEnum::kStatefulPartition;
    case health::mojom::ProbeCategoryEnum::kBluetooth:
      return cros_healthd::mojom::ProbeCategoryEnum::kBluetooth;
    case health::mojom::ProbeCategoryEnum::kSystem:
      return cros_healthd::mojom::ProbeCategoryEnum::kSystem2;
  }
  NOTREACHED();
}

}  // namespace

namespace unchecked {

health::mojom::ProbeErrorPtr UncheckedConvertPtr(
    cros_healthd::mojom::ProbeErrorPtr input) {
  return health::mojom::ProbeError::New(Convert(input->type),
                                        std::move(input->msg));
}

health::mojom::UInt64ValuePtr UncheckedConvertPtr(
    cros_healthd::mojom::NullableUint64Ptr input) {
  return health::mojom::UInt64Value::New(input->value);
}

health::mojom::BatteryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BatteryInfoPtr input) {
  return health::mojom::BatteryInfo::New(
      Convert(input->cycle_count), Convert(input->voltage_now),
      std::move(input->vendor), std::move(input->serial_number),
      Convert(input->charge_full_design), Convert(input->charge_full),
      Convert(input->voltage_min_design), std::move(input->model_name),
      Convert(input->charge_now), Convert(input->current_now),
      std::move(input->technology), std::move(input->status),
      std::move(input->manufacture_date),
      ConvertProbePtr(std::move(input->temperature)));
}

health::mojom::BatteryResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BatteryResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::BatteryResult::Tag::kBatteryInfo:
      return health::mojom::BatteryResult::NewBatteryInfo(
          ConvertProbePtr(std::move(input->get_battery_info())));
    case cros_healthd::mojom::BatteryResult::Tag::kError:
      return health::mojom::BatteryResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

health::mojom::NonRemovableBlockDeviceInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr input) {
  return health::mojom::NonRemovableBlockDeviceInfo::New(
      std::move(input->path), Convert(input->size), std::move(input->type),
      Convert(static_cast<uint32_t>(input->manufacturer_id)),
      std::move(input->name), base::NumberToString(input->serial),
      Convert(input->bytes_read_since_last_boot),
      Convert(input->bytes_written_since_last_boot),
      Convert(input->read_time_seconds_since_last_boot),
      Convert(input->write_time_seconds_since_last_boot),
      Convert(input->io_time_seconds_since_last_boot),
      ConvertProbePtr(std::move(input->discard_time_seconds_since_last_boot)));
}

health::mojom::NonRemovableBlockDeviceResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::NonRemovableBlockDeviceResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::NonRemovableBlockDeviceResult::Tag::
        kBlockDeviceInfo:
      return health::mojom::NonRemovableBlockDeviceResult::NewBlockDeviceInfo(
          ConvertPtrVector<health::mojom::NonRemovableBlockDeviceInfoPtr>(
              std::move(input->get_block_device_info())));
    case cros_healthd::mojom::NonRemovableBlockDeviceResult::Tag::kError:
      return health::mojom::NonRemovableBlockDeviceResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

health::mojom::CachedVpdInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::SystemInfoPtr input) {
  return health::mojom::CachedVpdInfo::New(
      std::move(input->first_power_date), std::move(input->product_sku_number),
      std::move(input->product_serial_number),
      std::move(input->product_model_name));
}

health::mojom::CachedVpdResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::SystemResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::SystemResult::Tag::kSystemInfo:
      return health::mojom::CachedVpdResult::NewVpdInfo(
          ConvertProbePtr(std::move(input->get_system_info())));
    case cros_healthd::mojom::SystemResult::Tag::kError:
      return health::mojom::CachedVpdResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

health::mojom::CpuCStateInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuCStateInfoPtr input) {
  return health::mojom::CpuCStateInfo::New(
      std::move(input->name), Convert(input->time_in_state_since_last_boot_us));
}

namespace {

uint64_t UserHz() {
  const long user_hz = sysconf(_SC_CLK_TCK);
  DCHECK(user_hz >= 0);
  return user_hz;
}

}  // namespace

health::mojom::LogicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::LogicalCpuInfoPtr input) {
  return UncheckedConvertPtr(std::move(input), UserHz());
}

health::mojom::LogicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::LogicalCpuInfoPtr input,
    uint64_t user_hz) {
  constexpr uint64_t kMillisecondsInSecond = 1000;
  uint64_t idle_time_user_hz = static_cast<uint64_t>(input->idle_time_user_hz);

  DCHECK(user_hz != 0);

  return health::mojom::LogicalCpuInfo::New(
      Convert(input->max_clock_speed_khz),
      Convert(input->scaling_max_frequency_khz),
      Convert(input->scaling_current_frequency_khz),
      Convert(idle_time_user_hz * kMillisecondsInSecond / user_hz),
      ConvertPtrVector<health::mojom::CpuCStateInfoPtr>(
          std::move(input->c_states)));
}

health::mojom::PhysicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::PhysicalCpuInfoPtr input) {
  return health::mojom::PhysicalCpuInfo::New(
      std::move(input->model_name),
      ConvertPtrVector<health::mojom::LogicalCpuInfoPtr>(
          std::move(input->logical_cpus)));
}

health::mojom::CpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuInfoPtr input) {
  return health::mojom::CpuInfo::New(
      Convert(input->num_total_threads), Convert(input->architecture),
      ConvertPtrVector<health::mojom::PhysicalCpuInfoPtr>(
          std::move(input->physical_cpus)));
}

health::mojom::CpuResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::CpuResult::Tag::kCpuInfo:
      return health::mojom::CpuResult::NewCpuInfo(
          ConvertProbePtr(std::move(input->get_cpu_info())));
    case cros_healthd::mojom::CpuResult::Tag::kError:
      return health::mojom::CpuResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

health::mojom::TimezoneInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TimezoneInfoPtr input) {
  return health::mojom::TimezoneInfo::New(input->posix, input->region);
}

health::mojom::TimezoneResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::TimezoneResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::TimezoneResult::Tag::kTimezoneInfo:
      return health::mojom::TimezoneResult::NewTimezoneInfo(
          ConvertProbePtr(std::move(input->get_timezone_info())));
    case cros_healthd::mojom::TimezoneResult::Tag::kError:
      return health::mojom::TimezoneResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

health::mojom::MemoryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::MemoryInfoPtr input) {
  return health::mojom::MemoryInfo::New(
      Convert(input->total_memory_kib), Convert(input->free_memory_kib),
      Convert(input->available_memory_kib),
      Convert(static_cast<uint64_t>(input->page_faults_since_last_boot)));
}

health::mojom::MemoryResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::MemoryResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::MemoryResult::Tag::kMemoryInfo:
      return health::mojom::MemoryResult::NewMemoryInfo(
          ConvertProbePtr(std::move(input->get_memory_info())));
    case cros_healthd::mojom::MemoryResult::Tag::kError:
      return health::mojom::MemoryResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

health::mojom::BacklightInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BacklightInfoPtr input) {
  return health::mojom::BacklightInfo::New(std::move(input->path),
                                           Convert(input->max_brightness),
                                           Convert(input->brightness));
}

health::mojom::BacklightResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BacklightResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::BacklightResult::Tag::kBacklightInfo:
      return health::mojom::BacklightResult::NewBacklightInfo(
          ConvertPtrVector<health::mojom::BacklightInfoPtr>(
              std::move(input->get_backlight_info())));
    case cros_healthd::mojom::BacklightResult::Tag::kError:
      return health::mojom::BacklightResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

health::mojom::FanInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::FanInfoPtr input) {
  return health::mojom::FanInfo::New(Convert(input->speed_rpm));
}

health::mojom::FanResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::FanResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::FanResult::Tag::kFanInfo:
      return health::mojom::FanResult::NewFanInfo(
          ConvertPtrVector<health::mojom::FanInfoPtr>(
              std::move(input->get_fan_info())));
    case cros_healthd::mojom::FanResult::Tag::kError:
      return health::mojom::FanResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

health::mojom::StatefulPartitionInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::StatefulPartitionInfoPtr input) {
  constexpr uint64_t k100MiB = 100 * 1024 * 1024;
  return health::mojom::StatefulPartitionInfo::New(
      Convert(input->available_space / k100MiB * k100MiB),
      Convert(input->total_space));
}

health::mojom::StatefulPartitionResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::StatefulPartitionResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::StatefulPartitionResult::Tag::kPartitionInfo:
      return health::mojom::StatefulPartitionResult::NewPartitionInfo(
          ConvertProbePtr(std::move(input->get_partition_info())));
    case cros_healthd::mojom::StatefulPartitionResult::Tag::kError:
      return health::mojom::StatefulPartitionResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

health::mojom::BluetoothAdapterInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BluetoothAdapterInfoPtr input) {
  return health::mojom::BluetoothAdapterInfo::New(
      std::move(input->name), std::move(input->address),
      Convert(input->powered), Convert(input->num_connected_devices));
}

health::mojom::BluetoothResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BluetoothResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::BluetoothResult::Tag::kBluetoothAdapterInfo:
      return health::mojom::BluetoothResult::NewBluetoothAdapterInfo(
          ConvertPtrVector<health::mojom::BluetoothAdapterInfoPtr>(
              std::move(input->get_bluetooth_adapter_info())));
    case cros_healthd::mojom::BluetoothResult::Tag::kError:
      return health::mojom::BluetoothResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

health::mojom::OsInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::OsInfoPtr input) {
  return health::mojom::OsInfo::New(std::move(input->oem_name));
}

health::mojom::SystemInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::SystemInfoV2Ptr input) {
  return health::mojom::SystemInfo::New(
      ConvertProbePtr(std::move(input->os_info)));
}

health::mojom::SystemResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::SystemResultV2Ptr input) {
  switch (input->which()) {
    case cros_healthd::mojom::SystemResultV2::Tag::kSystemInfoV2:
      return health::mojom::SystemResult::NewSystemInfo(
          ConvertProbePtr(std::move(input->get_system_info_v2())));
    case cros_healthd::mojom::SystemResultV2::Tag::kError:
      return health::mojom::SystemResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

health::mojom::TelemetryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TelemetryInfoPtr input) {
  return health::mojom::TelemetryInfo::New(
      ConvertProbePtr(std::move(input->battery_result)),
      ConvertProbePtr(std::move(input->block_device_result)),
      ConvertProbePtr(std::move(input->system_result)),
      ConvertProbePtr(std::move(input->cpu_result)),
      ConvertProbePtr(std::move(input->timezone_result)),
      ConvertProbePtr(std::move(input->memory_result)),
      ConvertProbePtr(std::move(input->backlight_result)),
      ConvertProbePtr(std::move(input->fan_result)),
      ConvertProbePtr(std::move(input->stateful_partition_result)),
      ConvertProbePtr(std::move(input->bluetooth_result)),
      ConvertProbePtr(std::move(input->system_result_v2)));
}

}  // namespace unchecked

health::mojom::ErrorType Convert(cros_healthd::mojom::ErrorType input) {
  switch (input) {
    case cros_healthd::mojom::ErrorType::kUnknown:
      return health::mojom::ErrorType::kUnknown;
    case cros_healthd::mojom::ErrorType::kFileReadError:
      return health::mojom::ErrorType::kFileReadError;
    case cros_healthd::mojom::ErrorType::kParseError:
      return health::mojom::ErrorType::kParseError;
    case cros_healthd::mojom::ErrorType::kSystemUtilityError:
      return health::mojom::ErrorType::kSystemUtilityError;
    case cros_healthd::mojom::ErrorType::kServiceUnavailable:
      return health::mojom::ErrorType::kServiceUnavailable;
  }
  NOTREACHED();
}

health::mojom::CpuArchitectureEnum Convert(
    cros_healthd::mojom::CpuArchitectureEnum input) {
  switch (input) {
    case cros_healthd::mojom::CpuArchitectureEnum::kUnknown:
      return health::mojom::CpuArchitectureEnum::kUnknown;
    case cros_healthd::mojom::CpuArchitectureEnum::kX86_64:
      return health::mojom::CpuArchitectureEnum::kX86_64;
    case cros_healthd::mojom::CpuArchitectureEnum::kAArch64:
      return health::mojom::CpuArchitectureEnum::kAArch64;
    case cros_healthd::mojom::CpuArchitectureEnum::kArmv7l:
      return health::mojom::CpuArchitectureEnum::kArmv7l;
  }
  NOTREACHED();
}

health::mojom::BoolValuePtr Convert(bool input) {
  return health::mojom::BoolValue::New(input);
}

health::mojom::DoubleValuePtr Convert(double input) {
  return health::mojom::DoubleValue::New(input);
}

health::mojom::Int64ValuePtr Convert(int64_t input) {
  return health::mojom::Int64Value::New(input);
}

health::mojom::UInt32ValuePtr Convert(uint32_t input) {
  return health::mojom::UInt32Value::New(input);
}

health::mojom::UInt64ValuePtr Convert(uint64_t input) {
  return health::mojom::UInt64Value::New(input);
}

std::vector<cros_healthd::mojom::ProbeCategoryEnum> ConvertCategoryVector(
    const std::vector<health::mojom::ProbeCategoryEnum>& input) {
  std::vector<cros_healthd::mojom::ProbeCategoryEnum> output;
  for (const auto element : input) {
    output.push_back(Convert(element));
  }
  return output;
}

}  // namespace converters
}  // namespace ash
