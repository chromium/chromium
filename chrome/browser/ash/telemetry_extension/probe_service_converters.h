// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_PROBE_SERVICE_CONVERTERS_H_
#define CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_PROBE_SERVICE_CONVERTERS_H_

#include <cstdint>
#include <vector>

#include "base/check.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-forward.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/nullable_primitives.mojom-forward.h"
#include "chromeos/crosapi/mojom/probe_service.mojom-forward.h"

namespace ash {

// TODO(https://crbug.com/1164001): Remove if cros_healthd::mojom moved to ash.
namespace cros_healthd {
namespace mojom = ::chromeos::cros_healthd::mojom;
}  // namespace cros_healthd

namespace converters {

// This file contains helper functions used by ProbeService to convert its
// types to/from cros_healthd ProbeService types.

namespace unchecked {

// Functions in unchecked namespace do not verify whether input pointer is
// nullptr, they should be called only via ConvertPtr wrapper that checks
// whether input pointer is nullptr.

health::mojom::ProbeErrorPtr UncheckedConvertPtr(
    cros_healthd::mojom::ProbeErrorPtr input);

health::mojom::UInt64ValuePtr UncheckedConvertPtr(
    cros_healthd::mojom::NullableUint64Ptr input);

health::mojom::BatteryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BatteryInfoPtr input);

health::mojom::BatteryResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BatteryResultPtr input);

health::mojom::NonRemovableBlockDeviceInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr input);

health::mojom::NonRemovableBlockDeviceResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::NonRemovableBlockDeviceResultPtr input);

health::mojom::CachedVpdInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::VpdInfoPtr input);

health::mojom::CpuCStateInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuCStateInfoPtr input);

health::mojom::LogicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::LogicalCpuInfoPtr input);

health::mojom::LogicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::LogicalCpuInfoPtr input,
    uint64_t user_hz);

health::mojom::PhysicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::PhysicalCpuInfoPtr input);

health::mojom::CpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuInfoPtr input);

health::mojom::CpuResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuResultPtr input);

health::mojom::TimezoneInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TimezoneInfoPtr input);

health::mojom::TimezoneResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::TimezoneResultPtr input);

health::mojom::MemoryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::MemoryInfoPtr input);

health::mojom::MemoryResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::MemoryResultPtr input);

health::mojom::BacklightInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BacklightInfoPtr input);

health::mojom::BacklightResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BacklightResultPtr input);

health::mojom::FanInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::FanInfoPtr input);

health::mojom::FanResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::FanResultPtr input);

health::mojom::StatefulPartitionInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::StatefulPartitionInfoPtr input);

health::mojom::StatefulPartitionResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::StatefulPartitionResultPtr input);

health::mojom::BluetoothAdapterInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BluetoothAdapterInfoPtr input);

health::mojom::BluetoothResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BluetoothResultPtr input);

health::mojom::SystemInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::OsInfoPtr input);

health::mojom::OsVersionPtr UncheckedConvertPtr(
    cros_healthd::mojom::OsVersionPtr);

std::pair<health::mojom::CachedVpdInfoPtr, health::mojom::SystemInfoPtr>
UncheckedConvertPairPtr(cros_healthd::mojom::SystemInfoV2Ptr input);

std::pair<health::mojom::CachedVpdResultPtr, health::mojom::SystemResultPtr>
UncheckedConvertPairPtr(cros_healthd::mojom::SystemResultV2Ptr input);

health::mojom::TelemetryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TelemetryInfoPtr input);

}  // namespace unchecked

health::mojom::ErrorType Convert(cros_healthd::mojom::ErrorType type);

health::mojom::CpuArchitectureEnum Convert(
    cros_healthd::mojom::CpuArchitectureEnum input);

health::mojom::BoolValuePtr Convert(bool input);

health::mojom::DoubleValuePtr Convert(double input);

health::mojom::Int64ValuePtr Convert(int64_t input);

health::mojom::UInt32ValuePtr Convert(uint32_t input);

health::mojom::UInt64ValuePtr Convert(uint64_t input);

template <class OutputT, class InputT>
std::vector<OutputT> ConvertPtrVector(std::vector<InputT> input) {
  std::vector<OutputT> output;
  for (auto&& element : input) {
    DCHECK(!element.is_null());
    output.push_back(unchecked::UncheckedConvertPtr(std::move(element)));
  }
  return output;
}

template <class InputT>
auto ConvertProbePtr(InputT input) {
  return (!input.is_null()) ? unchecked::UncheckedConvertPtr(std::move(input))
                            : nullptr;
}

template <class InputT>
auto ConvertProbePairPtr(InputT input) {
  return (!input.is_null())
             ? unchecked::UncheckedConvertPairPtr(std::move(input))
             : std::make_pair(nullptr, nullptr);
}

std::vector<cros_healthd::mojom::ProbeCategoryEnum> ConvertCategoryVector(
    const std::vector<health::mojom::ProbeCategoryEnum>& input);

}  // namespace converters
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_PROBE_SERVICE_CONVERTERS_H_
