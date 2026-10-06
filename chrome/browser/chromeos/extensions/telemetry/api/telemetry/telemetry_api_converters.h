// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_TELEMETRY_API_CONVERTERS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_TELEMETRY_API_CONVERTERS_H_

#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "chrome/common/chromeos/extensions/api/telemetry.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-forward.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom-forward.h"

namespace chromeos::converters::telemetry {

// This file contains helper functions used by telemetry_api.cc to convert its
// types to/from telemetry service types.

namespace unchecked {

// Functions in unchecked namespace do not verify whether input pointer is
// nullptr, they should be called only via ConvertPtr wrapper that checks
// whether input pointer is nullptr.

chromeos::api::os_telemetry::AudioInputNodeInfo
UncheckedConvertInputAudioNodeInfoPtr(
    ash::cros_healthd::mojom::AudioNodeInfoPtr input);

chromeos::api::os_telemetry::AudioOutputNodeInfo
UncheckedConvertOutputAudioNodeInfoPtr(
    ash::cros_healthd::mojom::AudioNodeInfoPtr input);

chromeos::api::os_telemetry::AudioInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::AudioInfoPtr input);

chromeos::api::os_telemetry::CpuCStateInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::CpuCStateInfoPtr input);

chromeos::api::os_telemetry::LogicalCpuInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::LogicalCpuInfoPtr input);

chromeos::api::os_telemetry::PhysicalCpuInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::PhysicalCpuInfoPtr input);

// `serial_number` field should be converted iff `has_serial_number_permission`
// is true.
chromeos::api::os_telemetry::BatteryInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::BatteryInfoPtr input,
    bool has_serial_number_permission);

// The `mac_address` field should be converted iff `has_mac_address_permission`
// is true.
chromeos::api::os_telemetry::NetworkInfo UncheckedConvertPtr(
    chromeos::network_health::mojom::NetworkPtr input,
    bool has_mac_address_permission);

chromeos::api::os_telemetry::InternetConnectivityInfo UncheckedConvertPtr(
    chromeos::network_health::mojom::NetworkHealthStatePtr input,
    bool has_mac_address_permission);

chromeos::api::os_telemetry::NonRemovableBlockDeviceInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr);

chromeos::api::os_telemetry::OsVersionInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::OsVersionPtr input);

chromeos::api::os_telemetry::StatefulPartitionInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::StatefulPartitionInfoPtr input);

chromeos::api::os_telemetry::TpmVersion UncheckedConvertPtr(
    ash::cros_healthd::mojom::TpmVersionPtr input);

chromeos::api::os_telemetry::TpmStatus UncheckedConvertPtr(
    ash::cros_healthd::mojom::TpmStatusPtr input);

chromeos::api::os_telemetry::TpmDictionaryAttack UncheckedConvertPtr(
    ash::cros_healthd::mojom::TpmDictionaryAttackPtr input);

chromeos::api::os_telemetry::TpmInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::TpmInfoPtr input);

chromeos::api::os_telemetry::UsbBusInterfaceInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::UsbBusInterfaceInfoPtr input);

chromeos::api::os_telemetry::FwupdFirmwareVersionInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::FwupdFirmwareVersionInfoPtr input);

chromeos::api::os_telemetry::UsbBusInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::UsbBusInfoPtr input);

// `serial_number` field should be converted iff `has_serial_number_permission`
// is true.
chromeos::api::os_telemetry::VpdInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::VpdInfoPtr input,
    bool has_serial_number_permission);

chromeos::api::os_telemetry::DisplayInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::DisplayInfoPtr input);

chromeos::api::os_telemetry::EmbeddedDisplayInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::EmbeddedDisplayInfoPtr input);

chromeos::api::os_telemetry::ExternalDisplayInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::ExternalDisplayInfoPtr input);

chromeos::api::os_telemetry::ThermalInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::ThermalInfoPtr input);

chromeos::api::os_telemetry::ThermalSensorInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::ThermalSensorInfoPtr input);
}  // namespace unchecked

chromeos::api::os_telemetry::CpuArchitectureEnum Convert(
    ash::cros_healthd::mojom::CpuArchitectureEnum input);

chromeos::api::os_telemetry::NetworkState Convert(
    chromeos::network_health::mojom::NetworkState input);

chromeos::api::os_telemetry::NetworkType Convert(
    chromeos::network_config::mojom::NetworkType input);

chromeos::api::os_telemetry::TpmGSCVersion Convert(
    ash::cros_healthd::mojom::TpmGSCVersion input);

chromeos::api::os_telemetry::FwupdVersionFormat Convert(
    ash::cros_healthd::mojom::FwupdVersionFormat input);

chromeos::api::os_telemetry::UsbVersion Convert(
    ash::cros_healthd::mojom::UsbVersion input);

chromeos::api::os_telemetry::UsbSpecSpeed Convert(
    ash::cros_healthd::mojom::UsbSpecSpeed input);

chromeos::api::os_telemetry::DisplayInputType Convert(
    ash::cros_healthd::mojom::DisplayInputType input);

chromeos::api::os_telemetry::ThermalSensorSource Convert(
    ash::cros_healthd::mojom::ThermalSensorInfo::ThermalSensorSource input);

template <class OutputT, class InputT>
std::vector<OutputT> ConvertPtrVector(std::vector<InputT> input) {
  std::vector<OutputT> output;
  for (auto&& element : input) {
    CHECK(!element.is_null(), base::NotFatalUntil::M161);
    output.push_back(unchecked::UncheckedConvertPtr(std::move(element)));
  }
  return output;
}

template <class InputT,
          class... Types,
          class OutputT = decltype(unchecked::UncheckedConvertPtr(
              std::declval<InputT>(),
              std::declval<Types>()...))>
  requires(std::is_default_constructible_v<OutputT>)
OutputT ConvertPtr(InputT input, Types... args) {
  return (input) ? unchecked::UncheckedConvertPtr(std::move(input), args...)
                 : OutputT();
}

}  // namespace chromeos::converters::telemetry

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_TELEMETRY_API_CONVERTERS_H_
