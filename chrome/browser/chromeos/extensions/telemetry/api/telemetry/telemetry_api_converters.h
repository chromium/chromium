// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_TELEMETRY_API_CONVERTERS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_TELEMETRY_API_CONVERTERS_H_

#include <utility>
#include <vector>

#include "base/check.h"
#include "chrome/common/chromeos/extensions/api/telemetry.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-forward.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom-forward.h"

namespace chromeos {

namespace converters {

// This file contains helper functions used by telemetry_api.cc to convert its
// types to/from telemetry service types.

namespace unchecked {

// Functions in unchecked namespace do not verify whether input pointer is
// nullptr, they should be called only via ConvertPtr wrapper that checks
// whether input pointer is nullptr.

chromeos::api::os_telemetry::AudioInputNodeInfo UncheckedConvertPtr(
    crosapi::mojom::ProbeAudioInputNodeInfoPtr input);

chromeos::api::os_telemetry::AudioOutputNodeInfo UncheckedConvertPtr(
    crosapi::mojom::ProbeAudioOutputNodeInfoPtr input);

chromeos::api::os_telemetry::AudioInfo UncheckedConvertPtr(
    crosapi::mojom::ProbeAudioInfoPtr input);

chromeos::api::os_telemetry::CpuCStateInfo UncheckedConvertPtr(
    crosapi::mojom::ProbeCpuCStateInfoPtr input);

chromeos::api::os_telemetry::LogicalCpuInfo UncheckedConvertPtr(
    crosapi::mojom::ProbeLogicalCpuInfoPtr input);

chromeos::api::os_telemetry::PhysicalCpuInfo UncheckedConvertPtr(
    crosapi::mojom::ProbePhysicalCpuInfoPtr input);

// Note: Battery's serial number is not converted in this function because it is
// guarded by a permission.
chromeos::api::os_telemetry::BatteryInfo UncheckedConvertPtr(
    crosapi::mojom::ProbeBatteryInfoPtr input);

chromeos::api::os_telemetry::NetworkInfo UncheckedConvertPtr(
    chromeos::network_health::mojom::NetworkPtr input);

chromeos::api::os_telemetry::NonRemovableBlockDeviceInfo UncheckedConvertPtr(
    crosapi::mojom::ProbeNonRemovableBlockDeviceInfoPtr);

chromeos::api::os_telemetry::OsVersionInfo UncheckedConvertPtr(
    crosapi::mojom::ProbeOsVersionPtr input);

chromeos::api::os_telemetry::StatefulPartitionInfo UncheckedConvertPtr(
    crosapi::mojom::ProbeStatefulPartitionInfoPtr input);

chromeos::api::os_telemetry::TpmVersion UncheckedConvertPtr(
    crosapi::mojom::ProbeTpmVersionPtr input);

chromeos::api::os_telemetry::TpmStatus UncheckedConvertPtr(
    crosapi::mojom::ProbeTpmStatusPtr input);

chromeos::api::os_telemetry::TpmDictionaryAttack UncheckedConvertPtr(
    crosapi::mojom::ProbeTpmDictionaryAttackPtr input);

chromeos::api::os_telemetry::TpmInfo UncheckedConvertPtr(
    crosapi::mojom::ProbeTpmInfoPtr input);

chromeos::api::os_telemetry::UsbBusInterfaceInfo UncheckedConvertPtr(
    crosapi::mojom::ProbeUsbBusInterfaceInfoPtr input);

chromeos::api::os_telemetry::FwupdFirmwareVersionInfo UncheckedConvertPtr(
    crosapi::mojom::ProbeFwupdFirmwareVersionInfoPtr input);

chromeos::api::os_telemetry::UsbBusInfo UncheckedConvertPtr(
    crosapi::mojom::ProbeUsbBusInfoPtr input);

}  // namespace unchecked

chromeos::api::os_telemetry::CpuArchitectureEnum Convert(
    crosapi::mojom::ProbeCpuArchitectureEnum input);

chromeos::api::os_telemetry::NetworkState Convert(
    chromeos::network_health::mojom::NetworkState input);

chromeos::api::os_telemetry::NetworkType Convert(
    chromeos::network_config::mojom::NetworkType input);

chromeos::api::os_telemetry::TpmGSCVersion Convert(
    crosapi::mojom::ProbeTpmGSCVersion input);

chromeos::api::os_telemetry::FwupdVersionFormat Convert(
    crosapi::mojom::ProbeFwupdVersionFormat input);

chromeos::api::os_telemetry::UsbVersion Convert(
    crosapi::mojom::ProbeUsbVersion input);

chromeos::api::os_telemetry::UsbSpecSpeed Convert(
    crosapi::mojom::ProbeUsbSpecSpeed input);

template <class OutputT, class InputT>
std::vector<OutputT> ConvertPtrVector(std::vector<InputT> input) {
  std::vector<OutputT> output;
  for (auto&& element : input) {
    DCHECK(!element.is_null());
    output.push_back(unchecked::UncheckedConvertPtr(std::move(element)));
  }
  return output;
}

template <class OutputT, class InputT>
OutputT ConvertPtr(InputT input) {
  return (!input.is_null()) ? unchecked::UncheckedConvertPtr(std::move(input))
                            : OutputT();
}

}  // namespace converters
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_TELEMETRY_API_CONVERTERS_H_
