// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry/telemetry_api_converters.h"

#include <inttypes.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chromeos/ash/experiences/extensions/common/telemetry/api/telemetry.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom.h"

namespace chromeos::converters::telemetry {

namespace {

namespace cx_telem = ::chromeos::api::os_telemetry;

uint64_t UserHz() {
  const long user_hz = sysconf(_SC_CLK_TCK);
  CHECK_GE(user_hz, 0);
  return user_hz;
}

}  // namespace

namespace unchecked {

cx_telem::AudioInputNodeInfo UncheckedConvertInputAudioNodeInfoPtr(
    ash::cros_healthd::mojom::AudioNodeInfoPtr input) {
  cx_telem::AudioInputNodeInfo result;

  result.id = input->id;
  result.name = input->name;
  result.device_name = input->device_name;
  result.active = input->active;
  result.node_gain = input->input_node_gain;

  return result;
}

cx_telem::AudioOutputNodeInfo UncheckedConvertOutputAudioNodeInfoPtr(
    ash::cros_healthd::mojom::AudioNodeInfoPtr input) {
  cx_telem::AudioOutputNodeInfo result;

  result.id = input->id;
  result.name = input->name;
  result.device_name = input->device_name;
  result.active = input->active;
  result.node_volume = input->node_volume;

  return result;
}

cx_telem::AudioInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::AudioInfoPtr input) {
  cx_telem::AudioInfo result;

  result.output_mute = input->output_mute;
  result.input_mute = input->input_mute;
  result.underruns = input->underruns;
  result.severe_underruns = input->severe_underruns;

  if (input->output_nodes) {
    for (auto& node : input->output_nodes.value()) {
      if (node) {
        result.output_nodes.push_back(
            UncheckedConvertOutputAudioNodeInfoPtr(std::move(node)));
      } else {
        result.output_nodes.emplace_back();
      }
    }
  }
  if (input->input_nodes) {
    for (auto& node : input->input_nodes.value()) {
      if (node) {
        result.input_nodes.push_back(
            UncheckedConvertInputAudioNodeInfoPtr(std::move(node)));
      } else {
        result.input_nodes.emplace_back();
      }
    }
  }

  return result;
}

cx_telem::CpuCStateInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::CpuCStateInfoPtr input) {
  cx_telem::CpuCStateInfo result;
  result.name = input->name;
  result.time_in_state_since_last_boot_us =
      input->time_in_state_since_last_boot_us;
  return result;
}

cx_telem::LogicalCpuInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::LogicalCpuInfoPtr input) {
  const auto user_hz = UserHz();
  CHECK_GT(user_hz, 0u);

  cx_telem::LogicalCpuInfo result;

  result.max_clock_speed_khz = input->max_clock_speed_khz;
  result.scaling_max_frequency_khz = input->scaling_max_frequency_khz;
  result.scaling_current_frequency_khz = input->scaling_current_frequency_khz;
  result.idle_time_ms =
      input->idle_time_user_hz * base::Time::kMillisecondsPerSecond / user_hz;

  result.c_states =
      ConvertPtrVector<cx_telem::CpuCStateInfo>(std::move(input->c_states));
  result.core_id = input->core_id;

  return result;
}

cx_telem::PhysicalCpuInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::PhysicalCpuInfoPtr input) {
  cx_telem::PhysicalCpuInfo result;
  result.model_name = input->model_name;
  result.logical_cpus = ConvertPtrVector<cx_telem::LogicalCpuInfo>(
      std::move(input->logical_cpus));
  return result;
}

cx_telem::BatteryInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::BatteryInfoPtr input,
    bool has_serial_number_permission) {
  cx_telem::BatteryInfo result;
  result.vendor = std::move(input->vendor);
  result.model_name = std::move(input->model_name);
  result.technology = std::move(input->technology);
  result.status = std::move(input->status);
  result.cycle_count = input->cycle_count;
  result.voltage_now = input->voltage_now;
  result.charge_full_design = input->charge_full_design;
  result.charge_full = input->charge_full;
  result.voltage_min_design = input->voltage_min_design;
  result.charge_now = input->charge_now;
  result.current_now = input->current_now;
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
    ash::cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr input) {
  cx_telem::NonRemovableBlockDeviceInfo result;
  result.size = input->size;
  result.name = input->name;
  result.type = input->type;

  return result;
}

cx_telem::OsVersionInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::OsVersionPtr input) {
  cx_telem::OsVersionInfo result;

  result.release_milestone = input->release_milestone;
  result.build_number = input->build_number;
  result.patch_number = input->patch_number;
  result.release_channel = input->release_channel;

  return result;
}

cx_telem::StatefulPartitionInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::StatefulPartitionInfoPtr input) {
  cx_telem::StatefulPartitionInfo result;

  result.available_space = input->available_space;
  result.total_space = input->total_space;

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

cx_telem::TpmVersion UncheckedConvertPtr(
    ash::cros_healthd::mojom::TpmVersionPtr input) {
  cx_telem::TpmVersion result;

  result.gsc_version = Convert(input->gsc_version);
  result.family = input->family;
  result.spec_level = input->spec_level;
  result.manufacturer = input->manufacturer;
  result.tpm_model = input->tpm_model;
  result.firmware_version = input->firmware_version;
  result.vendor_specific = input->vendor_specific;

  return result;
}

cx_telem::TpmStatus UncheckedConvertPtr(
    ash::cros_healthd::mojom::TpmStatusPtr input) {
  cx_telem::TpmStatus result;

  result.enabled = input->enabled;
  result.owned = input->owned;
  result.owner_password_is_present = input->owner_password_is_present;

  return result;
}

cx_telem::TpmDictionaryAttack UncheckedConvertPtr(
    ash::cros_healthd::mojom::TpmDictionaryAttackPtr input) {
  cx_telem::TpmDictionaryAttack result;

  result.counter = input->counter;
  result.threshold = input->threshold;
  result.lockout_in_effect = input->lockout_in_effect;
  result.lockout_seconds_remaining = input->lockout_seconds_remaining;

  return result;
}

cx_telem::TpmInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::TpmInfoPtr input) {
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
    ash::cros_healthd::mojom::UsbBusInterfaceInfoPtr input) {
  cx_telem::UsbBusInterfaceInfo result;

  result.interface_number = input->interface_number;
  result.class_id = input->class_id;
  result.subclass_id = input->subclass_id;
  result.protocol_id = input->protocol_id;
  result.driver = input->driver;

  return result;
}

cx_telem::FwupdFirmwareVersionInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::FwupdFirmwareVersionInfoPtr input) {
  cx_telem::FwupdFirmwareVersionInfo result;

  result.version = input->version;
  result.version_format = Convert(input->version_format);

  return result;
}

cx_telem::UsbBusInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::UsbBusInfoPtr input) {
  cx_telem::UsbBusInfo result;

  result.class_id = input->class_id;
  result.subclass_id = input->subclass_id;
  result.protocol_id = input->protocol_id;
  result.vendor_id = input->vendor_id;
  result.product_id = input->product_id;

  result.interfaces = ConvertPtrVector<cx_telem::UsbBusInterfaceInfo>(
      std::move(input->interfaces));
  result.fwupd_firmware_version_info =
      ConvertPtr(std::move(input->fwupd_firmware_version_info));
  result.version = Convert(input->version);
  result.spec_speed = Convert(input->spec_speed);

  return result;
}

chromeos::api::os_telemetry::VpdInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::VpdInfoPtr input,
    bool has_serial_number_permission) {
  cx_telem::VpdInfo result;

  result.activate_date = std::move(input->activate_date);
  result.model_name = std::move(input->model_name);
  result.sku_number = std::move(input->sku_number);

  if (has_serial_number_permission) {
    result.serial_number = std::move(input->serial_number);
  }

  return result;
}

cx_telem::DisplayInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::DisplayInfoPtr input) {
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
    ash::cros_healthd::mojom::EmbeddedDisplayInfoPtr input) {
  cx_telem::EmbeddedDisplayInfo result;

  result.privacy_screen_supported = input->privacy_screen_supported;
  result.privacy_screen_enabled = input->privacy_screen_enabled;
  if (input->display_width) {
    result.display_width = input->display_width->value;
  }
  if (input->display_height) {
    result.display_height = input->display_height->value;
  }
  if (input->resolution_horizontal) {
    result.resolution_horizontal = input->resolution_horizontal->value;
  }
  if (input->resolution_vertical) {
    result.resolution_vertical = input->resolution_vertical->value;
  }
  if (input->refresh_rate) {
    result.refresh_rate = input->refresh_rate->value;
  }
  if (input->manufacturer) {
    result.manufacturer = std::move(*input->manufacturer);
  }
  if (input->model_id) {
    result.model_id = input->model_id->value;
  }
  // Not reporting serial_number for now until we get Privacy's approval.
  // result.serial_number = std::move(input->serial_number);
  if (input->manufacture_week) {
    result.manufacture_week = input->manufacture_week->value;
  }
  if (input->manufacture_year) {
    result.manufacture_year = input->manufacture_year->value;
  }
  if (input->edid_version) {
    result.edid_version = std::move(*input->edid_version);
  }
  result.input_type = Convert(input->input_type);
  if (input->display_name) {
    result.display_name = std::move(*input->display_name);
  }

  return result;
}

cx_telem::ExternalDisplayInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::ExternalDisplayInfoPtr input) {
  cx_telem::ExternalDisplayInfo result;

  if (input->display_width) {
    result.display_width = input->display_width->value;
  }
  if (input->display_height) {
    result.display_height = input->display_height->value;
  }
  if (input->resolution_horizontal) {
    result.resolution_horizontal = input->resolution_horizontal->value;
  }
  if (input->resolution_vertical) {
    result.resolution_vertical = input->resolution_vertical->value;
  }
  if (input->refresh_rate) {
    result.refresh_rate = input->refresh_rate->value;
  }
  if (input->manufacturer) {
    result.manufacturer = std::move(*input->manufacturer);
  }
  if (input->model_id) {
    result.model_id = input->model_id->value;
  }
  // Not reporting serial_number for now until we get Privacy's approval.
  // result.serial_number = std::move(input->serial_number);
  if (input->manufacture_week) {
    result.manufacture_week = input->manufacture_week->value;
  }
  if (input->manufacture_year) {
    result.manufacture_year = input->manufacture_year->value;
  }
  if (input->edid_version) {
    result.edid_version = std::move(*input->edid_version);
  }
  result.input_type = Convert(input->input_type);
  if (input->display_name) {
    result.display_name = std::move(*input->display_name);
  }

  return result;
}

cx_telem::ThermalInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::ThermalInfoPtr input) {
  cx_telem::ThermalInfo result;

  result.thermal_sensors =
      converters::telemetry::ConvertPtrVector<cx_telem::ThermalSensorInfo>(
          std::move(input->thermal_sensors));

  return result;
}

cx_telem::ThermalSensorInfo UncheckedConvertPtr(
    ash::cros_healthd::mojom::ThermalSensorInfoPtr input) {
  cx_telem::ThermalSensorInfo result;

  result.name = input->name;
  result.temperature_celsius = input->temperature_celsius;
  result.source = Convert(input->source);

  return result;
}

}  // namespace unchecked

cx_telem::CpuArchitectureEnum Convert(
    ash::cros_healthd::mojom::CpuArchitectureEnum input) {
  switch (input) {
    case ash::cros_healthd::mojom::CpuArchitectureEnum::kUnknown:
      return cx_telem::CpuArchitectureEnum::kUnknown;
    case ash::cros_healthd::mojom::CpuArchitectureEnum::kX86_64:
      return cx_telem::CpuArchitectureEnum::kX86_64;
    case ash::cros_healthd::mojom::CpuArchitectureEnum::kAArch64:
      return cx_telem::CpuArchitectureEnum::kAarch64;
    case ash::cros_healthd::mojom::CpuArchitectureEnum::kArmv7l:
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

cx_telem::TpmGSCVersion Convert(ash::cros_healthd::mojom::TpmGSCVersion input) {
  switch (input) {
    case ash::cros_healthd::mojom::TpmGSCVersion::kNotGSC:
      return cx_telem::TpmGSCVersion::kNotGsc;
    case ash::cros_healthd::mojom::TpmGSCVersion::kCr50:
      return cx_telem::TpmGSCVersion::kCr50;
    case ash::cros_healthd::mojom::TpmGSCVersion::kTi50:
      return cx_telem::TpmGSCVersion::kTi50;
  }
  NOTREACHED();
}

cx_telem::FwupdVersionFormat Convert(
    ash::cros_healthd::mojom::FwupdVersionFormat input) {
  switch (input) {
    case ash::cros_healthd::mojom::FwupdVersionFormat::kUnmappedEnumField:
    case ash::cros_healthd::mojom::FwupdVersionFormat::kUnknown:
    case ash::cros_healthd::mojom::FwupdVersionFormat::kPlain:
      return cx_telem::FwupdVersionFormat::kPlain;
    case ash::cros_healthd::mojom::FwupdVersionFormat::kNumber:
      return cx_telem::FwupdVersionFormat::kNumber;
    case ash::cros_healthd::mojom::FwupdVersionFormat::kPair:
      return cx_telem::FwupdVersionFormat::kPair;
    case ash::cros_healthd::mojom::FwupdVersionFormat::kTriplet:
      return cx_telem::FwupdVersionFormat::kTriplet;
    case ash::cros_healthd::mojom::FwupdVersionFormat::kQuad:
      return cx_telem::FwupdVersionFormat::kQuad;
    case ash::cros_healthd::mojom::FwupdVersionFormat::kBcd:
      return cx_telem::FwupdVersionFormat::kBcd;
    case ash::cros_healthd::mojom::FwupdVersionFormat::kIntelMe:
      return cx_telem::FwupdVersionFormat::kIntelMe;
    case ash::cros_healthd::mojom::FwupdVersionFormat::kIntelMe2:
      return cx_telem::FwupdVersionFormat::kIntelMe2;
    case ash::cros_healthd::mojom::FwupdVersionFormat::kSurfaceLegacy:
      return cx_telem::FwupdVersionFormat::kSurfaceLegacy;
    case ash::cros_healthd::mojom::FwupdVersionFormat::kSurface:
      return cx_telem::FwupdVersionFormat::kSurface;
    case ash::cros_healthd::mojom::FwupdVersionFormat::kDellBios:
      return cx_telem::FwupdVersionFormat::kDellBios;
    case ash::cros_healthd::mojom::FwupdVersionFormat::kHex:
      return cx_telem::FwupdVersionFormat::kHex;
  }
  NOTREACHED();
}

cx_telem::UsbVersion Convert(ash::cros_healthd::mojom::UsbVersion input) {
  switch (input) {
    case ash::cros_healthd::mojom::UsbVersion::kUnmappedEnumField:
    case ash::cros_healthd::mojom::UsbVersion::kUnknown:
      return cx_telem::UsbVersion::kUnknown;
    case ash::cros_healthd::mojom::UsbVersion::kUsb1:
      return cx_telem::UsbVersion::kUsb1;
    case ash::cros_healthd::mojom::UsbVersion::kUsb2:
      return cx_telem::UsbVersion::kUsb2;
    case ash::cros_healthd::mojom::UsbVersion::kUsb3:
      return cx_telem::UsbVersion::kUsb3;
  }
  NOTREACHED();
}

cx_telem::UsbSpecSpeed Convert(ash::cros_healthd::mojom::UsbSpecSpeed input) {
  switch (input) {
    case ash::cros_healthd::mojom::UsbSpecSpeed::kUnmappedEnumField:
    case ash::cros_healthd::mojom::UsbSpecSpeed::kDeprecatedSpeed:
    case ash::cros_healthd::mojom::UsbSpecSpeed::kUnknown:
      return cx_telem::UsbSpecSpeed::kUnknown;
    case ash::cros_healthd::mojom::UsbSpecSpeed::k1_5Mbps:
      return cx_telem::UsbSpecSpeed::kN1_5mbps;
    case ash::cros_healthd::mojom::UsbSpecSpeed::k12Mbps:
      return cx_telem::UsbSpecSpeed::kN12Mbps;
    case ash::cros_healthd::mojom::UsbSpecSpeed::k480Mbps:
      return cx_telem::UsbSpecSpeed::kN480Mbps;
    case ash::cros_healthd::mojom::UsbSpecSpeed::k5Gbps:
      return cx_telem::UsbSpecSpeed::kN5Gbps;
    case ash::cros_healthd::mojom::UsbSpecSpeed::k10Gbps:
      return cx_telem::UsbSpecSpeed::kN10Gbps;
    case ash::cros_healthd::mojom::UsbSpecSpeed::k20Gbps:
      return cx_telem::UsbSpecSpeed::kN20Gbps;
  }
  NOTREACHED();
}

cx_telem::DisplayInputType Convert(
    ash::cros_healthd::mojom::DisplayInputType input) {
  switch (input) {
    case ash::cros_healthd::mojom::DisplayInputType::kUnmappedEnumField:
      return cx_telem::DisplayInputType::kUnknown;
    case ash::cros_healthd::mojom::DisplayInputType::kDigital:
      return cx_telem::DisplayInputType::kDigital;
    case ash::cros_healthd::mojom::DisplayInputType::kAnalog:
      return cx_telem::DisplayInputType::kAnalog;
  }
  NOTREACHED();
}

cx_telem::ThermalSensorSource Convert(
    ash::cros_healthd::mojom::ThermalSensorInfo::ThermalSensorSource input) {
  switch (input) {
    case ash::cros_healthd::mojom::ThermalSensorInfo::ThermalSensorSource::
        kUnmappedEnumField:
      return cx_telem::ThermalSensorSource::kUnknown;
    case ash::cros_healthd::mojom::ThermalSensorInfo::ThermalSensorSource::kEc:
      return cx_telem::ThermalSensorSource::kEc;
    case ash::cros_healthd::mojom::ThermalSensorInfo::ThermalSensorSource::
        kSysFs:
      return cx_telem::ThermalSensorSource::kSysFs;
  }
  NOTREACHED();
}

}  // namespace chromeos::converters::telemetry
