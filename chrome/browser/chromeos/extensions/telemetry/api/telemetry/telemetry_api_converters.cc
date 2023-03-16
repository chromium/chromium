// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry/telemetry_api_converters.h"

#include <inttypes.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "chrome/common/chromeos/extensions/api/telemetry.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom.h"

namespace chromeos {
namespace converters {

namespace {

namespace telemetry_api = ::chromeos::api::os_telemetry;
namespace telemetry_service = ::crosapi::mojom;

}  // namespace

namespace unchecked {
chromeos::api::os_telemetry::AudioInputNodeInfo UncheckedConvertPtr(
    crosapi::mojom::ProbeAudioInputNodeInfoPtr input) {
  telemetry_api::AudioInputNodeInfo result;

  if (input->id) {
    result.id = input->id->value;
  }
  result.name = input->name;
  result.device_name = input->device_name;
  if (input->active) {
    result.active = input->active->value;
  }
  if (input->node_gain) {
    result.node_gain = input->node_gain->value;
  }

  return result;
}

chromeos::api::os_telemetry::AudioOutputNodeInfo UncheckedConvertPtr(
    crosapi::mojom::ProbeAudioOutputNodeInfoPtr input) {
  telemetry_api::AudioOutputNodeInfo result;

  if (input->id) {
    result.id = input->id->value;
  }
  result.name = input->name;
  result.device_name = input->device_name;
  if (input->active) {
    result.active = input->active->value;
  }
  if (input->node_volume) {
    result.node_volume = input->node_volume->value;
  }

  return result;
}

telemetry_api::AudioInfo UncheckedConvertPtr(
    telemetry_service::ProbeAudioInfoPtr input) {
  telemetry_api::AudioInfo result;

  if (input->output_mute) {
    result.output_mute = input->output_mute->value;
  }
  if (input->input_mute) {
    result.input_mute = input->input_mute->value;
  }
  if (input->underruns) {
    result.underruns = input->underruns->value;
  }
  if (input->severe_underruns) {
    result.severe_underruns = input->severe_underruns->value;
  }
  if (input->output_nodes) {
    result.output_nodes = ConvertPtrVector<telemetry_api::AudioOutputNodeInfo>(
        std::move(input->output_nodes.value()));
  }
  if (input->input_nodes) {
    result.input_nodes = ConvertPtrVector<telemetry_api::AudioInputNodeInfo>(
        std::move(input->input_nodes.value()));
  }

  return result;
}

telemetry_api::CpuCStateInfo UncheckedConvertPtr(
    telemetry_service::ProbeCpuCStateInfoPtr input) {
  telemetry_api::CpuCStateInfo result;
  result.name = input->name;
  if (input->time_in_state_since_last_boot_us) {
    result.time_in_state_since_last_boot_us =
        input->time_in_state_since_last_boot_us->value;
  }
  return result;
}

telemetry_api::LogicalCpuInfo UncheckedConvertPtr(
    telemetry_service::ProbeLogicalCpuInfoPtr input) {
  telemetry_api::LogicalCpuInfo result;
  if (input->max_clock_speed_khz) {
    result.max_clock_speed_khz = input->max_clock_speed_khz->value;
  }
  if (input->scaling_max_frequency_khz) {
    result.scaling_max_frequency_khz = input->scaling_max_frequency_khz->value;
  }
  if (input->scaling_current_frequency_khz) {
    result.scaling_current_frequency_khz =
        input->scaling_current_frequency_khz->value;
  }
  if (input->idle_time_ms) {
    result.idle_time_ms = input->idle_time_ms->value;
  }
  result.c_states = ConvertPtrVector<telemetry_api::CpuCStateInfo>(
      std::move(input->c_states));
  return result;
}

telemetry_api::PhysicalCpuInfo UncheckedConvertPtr(
    telemetry_service::ProbePhysicalCpuInfoPtr input) {
  telemetry_api::PhysicalCpuInfo result;
  result.model_name = input->model_name;
  result.logical_cpus = ConvertPtrVector<telemetry_api::LogicalCpuInfo>(
      std::move(input->logical_cpus));
  return result;
}

telemetry_api::BatteryInfo UncheckedConvertPtr(
    telemetry_service::ProbeBatteryInfoPtr input) {
  telemetry_api::BatteryInfo result;
  result.vendor = std::move(input->vendor);
  result.model_name = std::move(input->model_name);
  result.technology = std::move(input->technology);
  result.status = std::move(input->status);
  if (input->cycle_count) {
    result.cycle_count = input->cycle_count->value;
  }
  if (input->voltage_now) {
    result.voltage_now = input->voltage_now->value;
  }
  if (input->charge_full_design) {
    result.charge_full_design = input->charge_full_design->value;
  }
  if (input->charge_full) {
    result.charge_full = input->charge_full->value;
  }
  if (input->voltage_min_design) {
    result.voltage_min_design = input->voltage_min_design->value;
  }
  if (input->charge_now) {
    result.charge_now = input->charge_now->value;
  }
  if (input->current_now) {
    result.current_now = input->current_now->value;
  }
  if (input->temperature) {
    result.temperature = input->temperature->value;
  }
  result.manufacture_date = std::move(input->manufacture_date);

  return result;
}

telemetry_api::NonRemovableBlockDeviceInfo UncheckedConvertPtr(
    telemetry_service::ProbeNonRemovableBlockDeviceInfoPtr input) {
  telemetry_api::NonRemovableBlockDeviceInfo result;

  if (input->size) {
    result.size = input->size->value;
  }

  result.name = input->name.value();
  result.type = input->type.value();

  return result;
}

telemetry_api::OsVersionInfo UncheckedConvertPtr(
    telemetry_service::ProbeOsVersionPtr input) {
  telemetry_api::OsVersionInfo result;

  result.release_milestone = input->release_milestone;
  result.build_number = input->build_number;
  result.patch_number = input->patch_number;
  result.release_channel = input->release_channel;

  return result;
}

telemetry_api::StatefulPartitionInfo UncheckedConvertPtr(
    telemetry_service::ProbeStatefulPartitionInfoPtr input) {
  telemetry_api::StatefulPartitionInfo result;
  if (input->available_space) {
    result.available_space = input->available_space->value;
  }
  if (input->total_space) {
    result.total_space = input->total_space->value;
  }

  return result;
}

telemetry_api::NetworkInfo UncheckedConvertPtr(
    chromeos::network_health::mojom::NetworkPtr input) {
  telemetry_api::NetworkInfo result;

  result.type = Convert(input->type);
  result.state = Convert(input->state);

  if (input->ipv4_address.has_value()) {
    result.ipv4_address = input->ipv4_address.value();
  }
  result.ipv6_addresses = input->ipv6_addresses;
  if (input->signal_strength) {
    result.signal_strength = input->signal_strength->value;
  }

  return result;
}

telemetry_api::TpmVersion UncheckedConvertPtr(
    telemetry_service::ProbeTpmVersionPtr input) {
  telemetry_api::TpmVersion result;

  result.gsc_version = Convert(input->gsc_version);
  if (input->family) {
    result.family = input->family->value;
  }
  if (input->spec_level) {
    result.spec_level = input->spec_level->value;
  }
  if (input->manufacturer) {
    result.manufacturer = input->manufacturer->value;
  }
  if (input->tpm_model) {
    result.tpm_model = input->tpm_model->value;
  }
  if (input->firmware_version) {
    result.firmware_version = input->firmware_version->value;
  }
  result.vendor_specific = input->vendor_specific;

  return result;
}

telemetry_api::TpmStatus UncheckedConvertPtr(
    telemetry_service::ProbeTpmStatusPtr input) {
  telemetry_api::TpmStatus result;

  if (input->enabled) {
    result.enabled = input->enabled->value;
  }
  if (input->owned) {
    result.owned = input->owned->value;
  }
  if (input->owner_password_is_present) {
    result.owner_password_is_present = input->owner_password_is_present->value;
  }

  return result;
}

telemetry_api::TpmDictionaryAttack UncheckedConvertPtr(
    telemetry_service::ProbeTpmDictionaryAttackPtr input) {
  telemetry_api::TpmDictionaryAttack result;

  if (input->counter) {
    result.counter = input->counter->value;
  }
  if (input->threshold) {
    result.threshold = input->threshold->value;
  }
  if (input->lockout_in_effect) {
    result.lockout_in_effect = input->lockout_in_effect->value;
  }
  if (input->lockout_seconds_remaining) {
    result.lockout_seconds_remaining = input->lockout_seconds_remaining->value;
  }

  return result;
}

telemetry_api::TpmInfo UncheckedConvertPtr(
    telemetry_service::ProbeTpmInfoPtr input) {
  telemetry_api::TpmInfo result;

  if (input->version) {
    result.version =
        ConvertPtr<telemetry_api::TpmVersion>(std::move(input->version));
  }
  if (input->status) {
    result.status =
        ConvertPtr<telemetry_api::TpmStatus>(std::move(input->status));
  }
  if (input->dictionary_attack) {
    result.dictionary_attack = ConvertPtr<telemetry_api::TpmDictionaryAttack>(
        std::move(input->dictionary_attack));
  }

  return result;
}

telemetry_api::UsbBusInterfaceInfo UncheckedConvertPtr(
    telemetry_service::ProbeUsbBusInterfaceInfoPtr input) {
  telemetry_api::UsbBusInterfaceInfo result;

  if (input->interface_number) {
    result.interface_number = input->interface_number->value;
  }
  if (input->class_id) {
    result.class_id = input->class_id->value;
  }
  if (input->subclass_id) {
    result.subclass_id = input->subclass_id->value;
  }
  if (input->protocol_id) {
    result.protocol_id = input->protocol_id->value;
  }
  result.driver = input->driver;

  return result;
}

telemetry_api::FwupdFirmwareVersionInfo UncheckedConvertPtr(
    telemetry_service::ProbeFwupdFirmwareVersionInfoPtr input) {
  telemetry_api::FwupdFirmwareVersionInfo result;

  result.version = input->version;
  result.version_format = Convert(input->version_format);

  return result;
}

telemetry_api::UsbBusInfo UncheckedConvertPtr(
    telemetry_service::ProbeUsbBusInfoPtr input) {
  telemetry_api::UsbBusInfo result;

  if (input->class_id) {
    result.class_id = input->class_id->value;
  }
  if (input->subclass_id) {
    result.subclass_id = input->subclass_id->value;
  }
  if (input->protocol_id) {
    result.protocol_id = input->protocol_id->value;
  }
  if (input->vendor_id) {
    result.vendor_id = input->vendor_id->value;
  }
  if (input->product_id) {
    result.product_id = input->product_id->value;
  }
  if (input->interfaces) {
    result.interfaces = ConvertPtrVector<telemetry_api::UsbBusInterfaceInfo>(
        std::move(input->interfaces.value()));
  }
  result.fwupd_firmware_version_info =
      ConvertPtr<telemetry_api::FwupdFirmwareVersionInfo>(
          std::move(input->fwupd_firmware_version_info));
  result.version = Convert(input->version);
  result.spec_speed = Convert(input->spec_speed);

  return result;
}

}  // namespace unchecked

telemetry_api::CpuArchitectureEnum Convert(
    telemetry_service::ProbeCpuArchitectureEnum input) {
  switch (input) {
    case telemetry_service::ProbeCpuArchitectureEnum::kUnknown:
      return telemetry_api::CpuArchitectureEnum::kUnknown;
    case telemetry_service::ProbeCpuArchitectureEnum::kX86_64:
      return telemetry_api::CpuArchitectureEnum::kX8664;
    case telemetry_service::ProbeCpuArchitectureEnum::kAArch64:
      return telemetry_api::CpuArchitectureEnum::kAarch64;
    case telemetry_service::ProbeCpuArchitectureEnum::kArmv7l:
      return telemetry_api::CpuArchitectureEnum::kArmv7l;
  }
  NOTREACHED();
}

telemetry_api::NetworkState Convert(
    chromeos::network_health::mojom::NetworkState input) {
  switch (input) {
    case network_health::mojom::NetworkState::kUninitialized:
      return telemetry_api::NetworkState::kUninitialized;
    case network_health::mojom::NetworkState::kDisabled:
      return telemetry_api::NetworkState::kDisabled;
    case network_health::mojom::NetworkState::kProhibited:
      return telemetry_api::NetworkState::kProhibited;
    case network_health::mojom::NetworkState::kNotConnected:
      return telemetry_api::NetworkState::kNotConnected;
    case network_health::mojom::NetworkState::kConnecting:
      return telemetry_api::NetworkState::kConnecting;
    case network_health::mojom::NetworkState::kPortal:
      return telemetry_api::NetworkState::kPortal;
    case network_health::mojom::NetworkState::kConnected:
      return telemetry_api::NetworkState::kConnected;
    case network_health::mojom::NetworkState::kOnline:
      return telemetry_api::NetworkState::kOnline;
  }
  NOTREACHED();
}

telemetry_api::NetworkType Convert(
    chromeos::network_config::mojom::NetworkType input) {
  // Cases kAll, kMobile and kWireless are only used for querying
  // the network_config daemon and are not returned by the cros_healthd
  // interface we are calling. For this reason we return NONE in those
  // cases.
  switch (input) {
    case network_config::mojom::NetworkType::kAll:
      return telemetry_api::NetworkType::kNone;
    case network_config::mojom::NetworkType::kCellular:
      return telemetry_api::NetworkType::kCellular;
    case network_config::mojom::NetworkType::kEthernet:
      return telemetry_api::NetworkType::kEthernet;
    case network_config::mojom::NetworkType::kMobile:
      return telemetry_api::NetworkType::kNone;
    case network_config::mojom::NetworkType::kTether:
      return telemetry_api::NetworkType::kTether;
    case network_config::mojom::NetworkType::kVPN:
      return telemetry_api::NetworkType::kVpn;
    case network_config::mojom::NetworkType::kWireless:
      return telemetry_api::NetworkType::kNone;
    case network_config::mojom::NetworkType::kWiFi:
      return telemetry_api::NetworkType::kWifi;
  }
  NOTREACHED();
}

chromeos::api::os_telemetry::TpmGSCVersion Convert(
    crosapi::mojom::ProbeTpmGSCVersion input) {
  switch (input) {
    case telemetry_service::ProbeTpmGSCVersion::kNotGSC:
      return telemetry_api::TpmGSCVersion::kNotGsc;
    case telemetry_service::ProbeTpmGSCVersion::kCr50:
      return telemetry_api::TpmGSCVersion::kCr50;
    case telemetry_service::ProbeTpmGSCVersion::kTi50:
      return telemetry_api::TpmGSCVersion::kTi50;
  }
  NOTREACHED();
}

telemetry_api::FwupdVersionFormat Convert(
    telemetry_service::ProbeFwupdVersionFormat input) {
  switch (input) {
    case crosapi::mojom::ProbeFwupdVersionFormat::kUnknown:
      return telemetry_api::FwupdVersionFormat::kPlain;
    case crosapi::mojom::ProbeFwupdVersionFormat::kPlain:
      return telemetry_api::FwupdVersionFormat::kPlain;
    case crosapi::mojom::ProbeFwupdVersionFormat::kNumber:
      return telemetry_api::FwupdVersionFormat::kNumber;
    case crosapi::mojom::ProbeFwupdVersionFormat::kPair:
      return telemetry_api::FwupdVersionFormat::kPair;
    case crosapi::mojom::ProbeFwupdVersionFormat::kTriplet:
      return telemetry_api::FwupdVersionFormat::kTriplet;
    case crosapi::mojom::ProbeFwupdVersionFormat::kQuad:
      return telemetry_api::FwupdVersionFormat::kQuad;
    case crosapi::mojom::ProbeFwupdVersionFormat::kBcd:
      return telemetry_api::FwupdVersionFormat::kBcd;
    case crosapi::mojom::ProbeFwupdVersionFormat::kIntelMe:
      return telemetry_api::FwupdVersionFormat::kIntelMe;
    case crosapi::mojom::ProbeFwupdVersionFormat::kIntelMe2:
      return telemetry_api::FwupdVersionFormat::kIntelMe2;
    case crosapi::mojom::ProbeFwupdVersionFormat::kSurfaceLegacy:
      return telemetry_api::FwupdVersionFormat::kSurfaceLegacy;
    case crosapi::mojom::ProbeFwupdVersionFormat::kSurface:
      return telemetry_api::FwupdVersionFormat::kSurface;
    case crosapi::mojom::ProbeFwupdVersionFormat::kDellBios:
      return telemetry_api::FwupdVersionFormat::kDellBios;
    case crosapi::mojom::ProbeFwupdVersionFormat::kHex:
      return telemetry_api::FwupdVersionFormat::kHex;
  }
  NOTREACHED();
}

telemetry_api::UsbVersion Convert(telemetry_service::ProbeUsbVersion input) {
  switch (input) {
    case crosapi::mojom::ProbeUsbVersion::kUnknown:
      return telemetry_api::UsbVersion::kUnknown;
    case crosapi::mojom::ProbeUsbVersion::kUsb1:
      return telemetry_api::UsbVersion::kUsb1;
    case crosapi::mojom::ProbeUsbVersion::kUsb2:
      return telemetry_api::UsbVersion::kUsb2;
    case crosapi::mojom::ProbeUsbVersion::kUsb3:
      return telemetry_api::UsbVersion::kUsb3;
  }
  NOTREACHED();
}

telemetry_api::UsbSpecSpeed Convert(
    telemetry_service::ProbeUsbSpecSpeed input) {
  switch (input) {
    case crosapi::mojom::ProbeUsbSpecSpeed::kUnknown:
      return telemetry_api::UsbSpecSpeed::kUnknown;
    case crosapi::mojom::ProbeUsbSpecSpeed::k1_5Mbps:
      return telemetry_api::UsbSpecSpeed::kN15mbps;
    case crosapi::mojom::ProbeUsbSpecSpeed::k12Mbps:
      return telemetry_api::UsbSpecSpeed::kN12Mbps;
    case crosapi::mojom::ProbeUsbSpecSpeed::k480Mbps:
      return telemetry_api::UsbSpecSpeed::kN480Mbps;
    case crosapi::mojom::ProbeUsbSpecSpeed::k5Gbps:
      return telemetry_api::UsbSpecSpeed::kN5Gbps;
    case crosapi::mojom::ProbeUsbSpecSpeed::k10Gbps:
      return telemetry_api::UsbSpecSpeed::kN10Gbps;
    case crosapi::mojom::ProbeUsbSpecSpeed::k20Gbps:
      return telemetry_api::UsbSpecSpeed::kN20Gbps;
  }
  NOTREACHED();
}

}  // namespace converters
}  // namespace chromeos
