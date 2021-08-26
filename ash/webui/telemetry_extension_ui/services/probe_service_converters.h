// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_PROBE_SERVICE_CONVERTERS_H_
#define ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_PROBE_SERVICE_CONVERTERS_H_

#include <cstdint>
#include <vector>

#include "ash/webui/telemetry_extension_ui/mojom/probe_service.mojom-forward.h"
#include "base/check.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"

namespace chromeos {
namespace converters {

// This file contains helper functions used by ProbeService to convert its
// types to/from cros_healthd ProbeService types.

namespace unchecked {

// Functions in unchecked namespace do not verify whether input pointer is
// nullptr, they should be called only via ConvertPtr wrapper that checks
// whether input pointer is nullptr.

ash::health::mojom::ProbeErrorPtr UncheckedConvertPtr(
    cros_healthd::mojom::ProbeErrorPtr input);

ash::health::mojom::UInt64ValuePtr UncheckedConvertPtr(
    cros_healthd::mojom::NullableUint64Ptr input);

ash::health::mojom::BatteryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BatteryInfoPtr input);

ash::health::mojom::BatteryResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BatteryResultPtr input);

ash::health::mojom::NonRemovableBlockDeviceInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr input);

ash::health::mojom::NonRemovableBlockDeviceResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::NonRemovableBlockDeviceResultPtr input);

ash::health::mojom::CachedVpdInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::SystemInfoPtr input);

ash::health::mojom::CachedVpdResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::SystemResultPtr input);

ash::health::mojom::CpuCStateInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuCStateInfoPtr input);

ash::health::mojom::LogicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::LogicalCpuInfoPtr input);

ash::health::mojom::LogicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::LogicalCpuInfoPtr input,
    uint64_t user_hz);

ash::health::mojom::PhysicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::PhysicalCpuInfoPtr input);

ash::health::mojom::CpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuInfoPtr input);

ash::health::mojom::CpuResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuResultPtr input);

ash::health::mojom::TimezoneInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TimezoneInfoPtr input);

ash::health::mojom::TimezoneResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::TimezoneResultPtr input);

ash::health::mojom::MemoryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::MemoryInfoPtr input);

ash::health::mojom::MemoryResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::MemoryResultPtr input);

ash::health::mojom::BacklightInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BacklightInfoPtr input);

ash::health::mojom::BacklightResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BacklightResultPtr input);

ash::health::mojom::FanInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::FanInfoPtr input);

ash::health::mojom::FanResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::FanResultPtr input);

ash::health::mojom::StatefulPartitionInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::StatefulPartitionInfoPtr input);

ash::health::mojom::StatefulPartitionResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::StatefulPartitionResultPtr input);

ash::health::mojom::BluetoothAdapterInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BluetoothAdapterInfoPtr input);

ash::health::mojom::BluetoothResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BluetoothResultPtr input);

ash::health::mojom::TelemetryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TelemetryInfoPtr input);

}  // namespace unchecked

ash::health::mojom::ErrorType Convert(cros_healthd::mojom::ErrorType type);

ash::health::mojom::CpuArchitectureEnum Convert(
    cros_healthd::mojom::CpuArchitectureEnum input);

ash::health::mojom::BoolValuePtr Convert(bool input);

ash::health::mojom::DoubleValuePtr Convert(double input);

ash::health::mojom::Int64ValuePtr Convert(int64_t input);

ash::health::mojom::UInt32ValuePtr Convert(uint32_t input);

ash::health::mojom::UInt64ValuePtr Convert(uint64_t input);

template <class OutputT, class InputT>
std::vector<OutputT> ConvertPtrVector(std::vector<InputT> input) {
  std::vector<OutputT> output;
  for (auto&& element : input) {
    DCHECK(!element.is_null());
    output.push_back(unchecked::UncheckedConvertPtr(std::move(element)));
  }
  return output;
}

std::vector<cros_healthd::mojom::ProbeCategoryEnum> ConvertCategoryVector(
    const std::vector<ash::health::mojom::ProbeCategoryEnum>& input);

}  // namespace converters
}  // namespace chromeos

#endif  // ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_PROBE_SERVICE_CONVERTERS_H_
