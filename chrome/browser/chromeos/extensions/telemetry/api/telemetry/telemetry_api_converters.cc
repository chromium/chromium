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

namespace chromeos::converters::telemetry {

namespace {

namespace cx_telem = ::chromeos::api::os_telemetry;
namespace crosapi = ::crosapi::mojom;

}  // namespace

namespace unchecked {
cx_telem::AudioInputNodeInfo UncheckedConvertPtr(
    crosapi::ProbeAudioInputNodeInfoPtr input) {
  cx_telem::AudioInputNodeInfo result;

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

cx_telem::AudioOutputNodeInfo UncheckedConvertPtr(
    crosapi::ProbeAudioOutputNodeInfoPtr input) {
  cx_telem::AudioOutputNodeInfo result;

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

cx_telem::AudioInfo UncheckedConvertPtr(crosapi::ProbeAudioInfoPtr input) {
  cx_telem::AudioInfo result;

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
    result.output_nodes = ConvertPtrVector<cx_telem::AudioOutputNodeInfo>(
        std::move(input->output_nodes.value()));
  }
  if (input->input_nodes) {
    result.input_nodes = ConvertPtrVector<cx_telem::AudioInputNodeInfo>(
        std::move(input->input_nodes.value()));
  }

  return result;
}

cx_telem::CpuCStateInfo UncheckedConvertPtr(
    crosapi::ProbeCpuCStateInfoPtr input) {
  cx_telem::CpuCStateInfo result;
  result.name = input->name;
  if (input->time_in_state_since_last_boot_us) {
    result.time_in_state_since_last_boot_us =
        input->time_in_state_since_last_boot_us->value;
  }
  return result;
}

cx_telem::LogicalCpuInfo UncheckedConvertPtr(
    crosapi::ProbeLogicalCpuInfoPtr input) {
  cx_telem::LogicalCpuInfo result;
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
  result.c_states =
      ConvertPtrVector<cx_telem::CpuCStateInfo>(std::move(input->c_states));
  if (input->core_id) {
    result.core_id = input->core_id->value;
  }
  return result;
}

cx_telem::PhysicalCpuInfo UncheckedConvertPtr(
    crosapi::ProbePhysicalCpuInfoPtr input) {
  cx_telem::PhysicalCpuInfo result;
  result.model_name = input->model_name;
  result.logical_cpus = ConvertPtrVector<cx_telem::LogicalCpuInfo>(
      std::move(input->logical_cpus));
  return result;
}

cx_telem::BatteryInfo UncheckedConvertPtr(crosapi::ProbeBatteryInfoPtr input,
                                          bool has_serial_number_permission) {
  cx_telem::BatteryInfo result;
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

  if (has_serial_number_permission) {
    result.serial_number = std::move(input->serial_number);
  }

  return result;
}

cx_telem::NonRemovableBlockDeviceInfo UncheckedConvertPtr(
    crosapi::ProbeNonRemovableBlockDeviceInfoPtr input) {
  cx_telem::NonRemovableBlockDeviceInfo result;

  if (input->size) {
    result.size = input->size->value;
  }

  result.name = input->name.value();
  result.type = input->type.value();

  return result;
}

cx_telem::OsVersionInfo UncheckedConvertPtr(crosapi::ProbeOsVersionPtr input) {
  cx_telem::OsVersionInfo result;

  result.release_milestone = input->release_milestone;
  result.build_number = input->build_number;
  result.patch_number = input->patch_number;
  result.release_channel = input->release_channel;

  return result;
}

cx_telem::StatefulPartitionInfo UncheckedConvertPtr(
    crosapi::ProbeStatefulPartitionInfoPtr input) {
  cx_telem::StatefulPartitionInfo result;
  if (input->available_space) {
    result.available_space = input->available_space->value;
  }
  if (input->total_space) {
    result.total_space = input->total_space->value;
  }

  return result;
}

cx_telem::NetworkInfo UncheckedConvertPtr(
    chromeos::network_health::mojom::NetworkPtr input,
    bool has_mac_address_permission) {
  cx_telem::NetworkInfo result;

  result.type = Convert(input->type);
  result.state = Convert(input->state);

  if (input->ipv4_address.has_value()) {
    result.ipv4_address = input->ipv4_address.value();
  }
  result.ipv6_addresses = input->ipv6_addresses;
  if (input->signal_strength) {
    result.signal_strength = input->signal_strength->value;
  }
  if (has_mac_address_permission) {
    result.mac_address = std::move(input->mac_address);
  }

  return result;
}

cx_telem::InternetConnectivityInfo UncheckedConvertPtr(
    chromeos::network_health::mojom::NetworkHealthStatePtr input,
    bool has_mac_address_permission) {
  cx_telem::InternetConnectivityInfo result;
  for (auto& network : input->networks) {
    auto converted_network =
        ConvertPtr(std::move(network), has_mac_address_permission);

    // Don't include networks with an undefined type.
    if (converted_network.type != cx_telem::NetworkType::kNone) {
      result.networks.push_back(std::move(converted_network));
    }
  }

  return result;
}

cx_telem::TpmVersion UncheckedConvertPtr(crosapi::ProbeTpmVersionPtr input) {
  cx_telem::TpmVersion result;

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

cx_telem::TpmStatus UncheckedConvertPtr(crosapi::ProbeTpmStatusPtr input) {
  cx_telem::TpmStatus result;

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

cx_telem::TpmDictionaryAttack UncheckedConvertPtr(
    crosapi::ProbeTpmDictionaryAttackPtr input) {
  cx_telem::TpmDictionaryAttack result;

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

cx_telem::TpmInfo UncheckedConvertPtr(crosapi::ProbeTpmInfoPtr input) {
  cx_telem::TpmInfo result;

  if (input->version) {
    result.version = ConvertPtr(std::move(input->version));
  }
  if (input->status) {
    result.status = ConvertPtr(std::move(input->status));
  }
  if (input->dictionary_attack) {
    result.dictionary_attack = ConvertPtr(std::move(input->dictionary_attack));
  }

  return result;
}

cx_telem::UsbBusInterfaceInfo UncheckedConvertPtr(
    crosapi::ProbeUsbBusInterfaceInfoPtr input) {
  cx_telem::UsbBusInterfaceInfo result;

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

cx_telem::FwupdFirmwareVersionInfo UncheckedConvertPtr(
    crosapi::ProbeFwupdFirmwareVersionInfoPtr input) {
  cx_telem::FwupdFirmwareVersionInfo result;

  result.version = input->version;
  result.version_format = Convert(input->version_format);

  return result;
}

cx_telem::UsbBusInfo UncheckedConvertPtr(crosapi::ProbeUsbBusInfoPtr input) {
  cx_telem::UsbBusInfo result;

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
    result.interfaces = ConvertPtrVector<cx_telem::UsbBusInterfaceInfo>(
        std::move(input->interfaces.value()));
  }
  result.fwupd_firmware_version_info =
      ConvertPtr(std::move(input->fwupd_firmware_version_info));
  result.version = Convert(input->version);
  result.spec_speed = Convert(input->spec_speed);

  return result;
}

chromeos::api::os_telemetry::VpdInfo UncheckedConvertPtr(
    crosapi::ProbeCachedVpdInfoPtr input,
    bool has_serial_number_permission) {
  cx_telem::VpdInfo result;

  result.activate_date = std::move(input->first_power_date);
  result.model_name = std::move(input->model_name);
  result.sku_number = std::move(input->sku_number);

  if (has_serial_number_permission) {
    result.serial_number = std::move(input->serial_number);
  }

  return result;
}

cx_telem::DisplayInfo UncheckedConvertPtr(crosapi::ProbeDisplayInfoPtr input) {
  cx_telem::DisplayInfo result;

  result.embedded_display =
      converters::telemetry::ConvertPtr(std::move(input->embedded_display));
  if (input->external_displays.has_value()) {
    result.external_displays =
        converters::telemetry::ConvertPtrVector<cx_telem::ExternalDisplayInfo>(
            std::move(input->external_displays.value()));
  }

  return result;
}

cx_telem::EmbeddedDisplayInfo UncheckedConvertPtr(
    crosapi::ProbeEmbeddedDisplayInfoPtr input) {
  cx_telem::EmbeddedDisplayInfo result;

  result.privacy_screen_supported = std::move(input->privacy_screen_supported);
  result.privacy_screen_enabled = std::move(input->privacy_screen_enabled);
  result.display_width = std::move(input->display_width);
  result.display_height = std::move(input->display_height);
  result.resolution_horizontal = std::move(input->resolution_horizontal);
  result.resolution_vertical = std::move(input->resolution_vertical);
  result.refresh_rate = std::move(input->refresh_rate);
  result.manufacturer = std::move(input->manufacturer);
  result.model_id = std::move(input->model_id);
  // Not reporting serial_number for now until we get Privacy's approval.
  // result.serial_number = std::move(input->serial_number);
  result.manufacture_week = std::move(input->manufacture_week);
  result.manufacture_year = std::move(input->manufacture_year);
  result.edid_version = std::move(input->edid_version);
  result.input_type = Convert(input->input_type);
  result.display_name = (input->display_name);

  return result;
}

cx_telem::ExternalDisplayInfo UncheckedConvertPtr(
    crosapi::ProbeExternalDisplayInfoPtr input) {
  cx_telem::ExternalDisplayInfo result;

  result.display_width = std::move(input->display_width);
  result.display_height = std::move(input->display_height);
  result.resolution_horizontal = std::move(input->resolution_horizontal);
  result.resolution_vertical = std::move(input->resolution_vertical);
  result.refresh_rate = std::move(input->refresh_rate);
  result.manufacturer = std::move(input->manufacturer);
  result.model_id = std::move(input->model_id);
  // Not reporting serial_number for now until we get Privacy's approval.
  // result.serial_number = std::move(input->serial_number);
  result.manufacture_week = std::move(input->manufacture_week);
  result.manufacture_year = std::move(input->manufacture_year);
  result.edid_version = std::move(input->edid_version);
  result.input_type = Convert(input->input_type);
  result.display_name = std::move(input->display_name);

  return result;
}

cx_telem::ThermalInfo UncheckedConvertPtr(crosapi::ProbeThermalInfoPtr input) {
  cx_telem::ThermalInfo result;

  result.thermal_sensors =
      converters::telemetry::ConvertPtrVector<cx_telem::ThermalSensorInfo>(
          std::move(input->thermal_sensors));

  return result;
}

cx_telem::ThermalSensorInfo UncheckedConvertPtr(
    crosapi::ProbeThermalSensorInfoPtr input) {
  cx_telem::ThermalSensorInfo result;

  result.name = input->name;
  result.temperature_celsius = input->temperature_celsius;
  result.source = Convert(input->source);

  return result;
}

}  // namespace unchecked

cx_telem::CpuArchitectureEnum Convert(crosapi::ProbeCpuArchitectureEnum input) {
  switch (input) {
    case crosapi::ProbeCpuArchitectureEnum::kUnknown:
      return cx_telem::CpuArchitectureEnum::kUnknown;
    case crosapi::ProbeCpuArchitectureEnum::kX86_64:
      return cx_telem::CpuArchitectureEnum::kX86_64;
    case crosapi::ProbeCpuArchitectureEnum::kAArch64:
      return cx_telem::CpuArchitectureEnum::kAarch64;
    case crosapi::ProbeCpuArchitectureEnum::kArmv7l:
      return cx_telem::CpuArchitectureEnum::kArmv7l;
  }
  NOTREACHED();
}

cx_telem::NetworkState Convert(
    chromeos::network_health::mojom::NetworkState input) {
  switch (input) {
    case network_health::mojom::NetworkState::kUninitialized:
      return cx_telem::NetworkState::kUninitialized;
    case network_health::mojom::NetworkState::kDisabled:
      return cx_telem::NetworkState::kDisabled;
    case network_health::mojom::NetworkState::kProhibited:
      return cx_telem::NetworkState::kProhibited;
    case network_health::mojom::NetworkState::kNotConnected:
      return cx_telem::NetworkState::kNotConnected;
    case network_health::mojom::NetworkState::kConnecting:
      return cx_telem::NetworkState::kConnecting;
    case network_health::mojom::NetworkState::kPortal:
      return cx_telem::NetworkState::kPortal;
    case network_health::mojom::NetworkState::kConnected:
      return cx_telem::NetworkState::kConnected;
    case network_health::mojom::NetworkState::kOnline:
      return cx_telem::NetworkState::kOnline;
  }
  NOTREACHED();
}

cx_telem::NetworkType Convert(
    chromeos::network_config::mojom::NetworkType input) {
  // Cases kAll, kMobile and kWireless are only used for querying
  // the network_config daemon and are not returned by the cros_healthd
  // interface we are calling. For this reason we return NONE in those
  // cases.
  switch (input) {
    case network_config::mojom::NetworkType::kAll:
      return cx_telem::NetworkType::kNone;
    case network_config::mojom::NetworkType::kCellular:
      return cx_telem::NetworkType::kCellular;
    case network_config::mojom::NetworkType::kEthernet:
      return cx_telem::NetworkType::kEthernet;
    case network_config::mojom::NetworkType::kMobile:
      return cx_telem::NetworkType::kNone;
    case network_config::mojom::NetworkType::kTether:
      return cx_telem::NetworkType::kTether;
    case network_config::mojom::NetworkType::kVPN:
      return cx_telem::NetworkType::kVpn;
    case network_config::mojom::NetworkType::kWireless:
      return cx_telem::NetworkType::kNone;
    case network_config::mojom::NetworkType::kWiFi:
      return cx_telem::NetworkType::kWifi;
  }
  NOTREACHED();
}

cx_telem::TpmGSCVersion Convert(crosapi::ProbeTpmGSCVersion input) {
  switch (input) {
    case crosapi::ProbeTpmGSCVersion::kNotGSC:
      return cx_telem::TpmGSCVersion::kNotGsc;
    case crosapi::ProbeTpmGSCVersion::kCr50:
      return cx_telem::TpmGSCVersion::kCr50;
    case crosapi::ProbeTpmGSCVersion::kTi50:
      return cx_telem::TpmGSCVersion::kTi50;
  }
  NOTREACHED();
}

cx_telem::FwupdVersionFormat Convert(crosapi::ProbeFwupdVersionFormat input) {
  switch (input) {
    case crosapi::ProbeFwupdVersionFormat::kUnknown:
      return cx_telem::FwupdVersionFormat::kPlain;
    case crosapi::ProbeFwupdVersionFormat::kPlain:
      return cx_telem::FwupdVersionFormat::kPlain;
    case crosapi::ProbeFwupdVersionFormat::kNumber:
      return cx_telem::FwupdVersionFormat::kNumber;
    case crosapi::ProbeFwupdVersionFormat::kPair:
      return cx_telem::FwupdVersionFormat::kPair;
    case crosapi::ProbeFwupdVersionFormat::kTriplet:
      return cx_telem::FwupdVersionFormat::kTriplet;
    case crosapi::ProbeFwupdVersionFormat::kQuad:
      return cx_telem::FwupdVersionFormat::kQuad;
    case crosapi::ProbeFwupdVersionFormat::kBcd:
      return cx_telem::FwupdVersionFormat::kBcd;
    case crosapi::ProbeFwupdVersionFormat::kIntelMe:
      return cx_telem::FwupdVersionFormat::kIntelMe;
    case crosapi::ProbeFwupdVersionFormat::kIntelMe2:
      return cx_telem::FwupdVersionFormat::kIntelMe2;
    case crosapi::ProbeFwupdVersionFormat::kSurfaceLegacy:
      return cx_telem::FwupdVersionFormat::kSurfaceLegacy;
    case crosapi::ProbeFwupdVersionFormat::kSurface:
      return cx_telem::FwupdVersionFormat::kSurface;
    case crosapi::ProbeFwupdVersionFormat::kDellBios:
      return cx_telem::FwupdVersionFormat::kDellBios;
    case crosapi::ProbeFwupdVersionFormat::kHex:
      return cx_telem::FwupdVersionFormat::kHex;
  }
  NOTREACHED();
}

cx_telem::UsbVersion Convert(crosapi::ProbeUsbVersion input) {
  switch (input) {
    case crosapi::ProbeUsbVersion::kUnknown:
      return cx_telem::UsbVersion::kUnknown;
    case crosapi::ProbeUsbVersion::kUsb1:
      return cx_telem::UsbVersion::kUsb1;
    case crosapi::ProbeUsbVersion::kUsb2:
      return cx_telem::UsbVersion::kUsb2;
    case crosapi::ProbeUsbVersion::kUsb3:
      return cx_telem::UsbVersion::kUsb3;
  }
  NOTREACHED();
}

cx_telem::UsbSpecSpeed Convert(crosapi::ProbeUsbSpecSpeed input) {
  switch (input) {
    case crosapi::ProbeUsbSpecSpeed::kUnknown:
      return cx_telem::UsbSpecSpeed::kUnknown;
    case crosapi::ProbeUsbSpecSpeed::k1_5Mbps:
      return cx_telem::UsbSpecSpeed::kN1_5mbps;
    case crosapi::ProbeUsbSpecSpeed::k12Mbps:
      return cx_telem::UsbSpecSpeed::kN12Mbps;
    case crosapi::ProbeUsbSpecSpeed::k480Mbps:
      return cx_telem::UsbSpecSpeed::kN480Mbps;
    case crosapi::ProbeUsbSpecSpeed::k5Gbps:
      return cx_telem::UsbSpecSpeed::kN5Gbps;
    case crosapi::ProbeUsbSpecSpeed::k10Gbps:
      return cx_telem::UsbSpecSpeed::kN10Gbps;
    case crosapi::ProbeUsbSpecSpeed::k20Gbps:
      return cx_telem::UsbSpecSpeed::kN20Gbps;
  }
  NOTREACHED();
}

cx_telem::DisplayInputType Convert(crosapi::ProbeDisplayInputType input) {
  switch (input) {
    case crosapi::ProbeDisplayInputType::kUnmappedEnumField:
      return cx_telem::DisplayInputType::kUnknown;
    case crosapi::ProbeDisplayInputType::kDigital:
      return cx_telem::DisplayInputType::kDigital;
    case crosapi::ProbeDisplayInputType::kAnalog:
      return cx_telem::DisplayInputType::kAnalog;
  }
  NOTREACHED();
}

cx_telem::ThermalSensorSource Convert(crosapi::ProbeThermalSensorSource input) {
  switch (input) {
    case crosapi::ProbeThermalSensorSource::kUnmappedEnumField:
      return cx_telem::ThermalSensorSource::kUnknown;
    case crosapi::ProbeThermalSensorSource::kEc:
      return cx_telem::ThermalSensorSource::kEc;
    case crosapi::ProbeThermalSensorSource::kSysFs:
      return cx_telem::ThermalSensorSource::kSysFs;
  }
  NOTREACHED();
}

}  // namespace chromeos::converters::telemetry
