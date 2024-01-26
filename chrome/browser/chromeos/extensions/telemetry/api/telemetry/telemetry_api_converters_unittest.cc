// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry/telemetry_api_converters.h"
#include "chrome/common/chromeos/extensions/api/telemetry.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::converters::telemetry {

namespace {

namespace cx_telem = ::chromeos::api::os_telemetry;
namespace crosapi = ::crosapi::mojom;

}  // namespace

TEST(TelemetryApiConverters, AudioInputNodeInfo) {
  constexpr uint64_t kId = 42;
  constexpr char kName[] = "Internal Mic";
  constexpr char kDeviceName[] = "HDA Intel PCH: CA0132 Analog:0,0";
  constexpr bool kActive = true;
  constexpr uint8_t kNodeGain = 1;

  auto input = crosapi::ProbeAudioInputNodeInfo::New();
  input->id = crosapi::UInt64Value::New(kId);
  input->name = kName;
  input->device_name = kDeviceName;
  input->active = crosapi::BoolValue::New(kActive);
  input->node_gain = crosapi::UInt8Value::New(kNodeGain);

  auto result = ConvertPtr(std::move(input));

  ASSERT_TRUE(result.id);
  EXPECT_EQ(kId, static_cast<uint64_t>(*result.id));

  ASSERT_TRUE(result.name);
  EXPECT_EQ(kName, *result.name);

  ASSERT_TRUE(result.device_name);
  EXPECT_EQ(kDeviceName, *result.device_name);

  ASSERT_TRUE(result.active);
  EXPECT_EQ(kActive, *result.active);

  ASSERT_TRUE(result.node_gain);
  EXPECT_EQ(kNodeGain, static_cast<uint8_t>(*result.node_gain));
}

TEST(TelemetryApiConverters, AudioOutputNodeInfo) {
  constexpr uint64_t kId = 42;
  constexpr char kName[] = "Internal Speaker";
  constexpr char kDeviceName[] = "HDA Intel PCH: CA0132 Analog:0,0";
  constexpr bool kActive = true;
  constexpr uint8_t kNodeVolume = 242;

  auto input = crosapi::ProbeAudioOutputNodeInfo::New();
  input->id = crosapi::UInt64Value::New(kId);
  input->name = kName;
  input->device_name = kDeviceName;
  input->active = crosapi::BoolValue::New(kActive);
  input->node_volume = crosapi::UInt8Value::New(kNodeVolume);

  auto result = ConvertPtr(std::move(input));

  ASSERT_TRUE(result.id);
  EXPECT_EQ(kId, static_cast<uint64_t>(*result.id));

  ASSERT_TRUE(result.name);
  EXPECT_EQ(kName, *result.name);

  ASSERT_TRUE(result.device_name);
  EXPECT_EQ(kDeviceName, *result.device_name);

  ASSERT_TRUE(result.active);
  EXPECT_EQ(kActive, *result.active);

  ASSERT_TRUE(result.node_volume);
  EXPECT_EQ(kNodeVolume, static_cast<uint8_t>(*result.node_volume));
}

TEST(TelemetryApiConverters, AudioInfo) {
  constexpr bool kOutputMute = true;
  constexpr bool kInputMute = false;
  constexpr uint32_t kUnderruns = 56;
  constexpr uint32_t kSevereUnderruns = 3;

  constexpr uint64_t kIdInput = 42;
  constexpr char kNameInput[] = "Internal Speaker";
  constexpr char kDeviceNameInput[] = "HDA Intel PCH: CA0132 Analog:0,0";
  constexpr bool kActiveInput = true;
  constexpr uint8_t kNodeGainInput = 1;

  constexpr uint64_t kIdOutput = 43;
  constexpr char kNameOutput[] = "Extenal Speaker";
  constexpr char kDeviceNameOutput[] = "HDA Intel PCH: CA0132 Analog:1,0";
  constexpr bool kActiveOutput = false;
  constexpr uint8_t kNodeVolumeOutput = 212;

  std::vector<crosapi::ProbeAudioInputNodeInfoPtr> input_node_info;
  auto input_node = crosapi::ProbeAudioInputNodeInfo::New();
  input_node->id = crosapi::UInt64Value::New(kIdInput);
  input_node->name = kNameInput;
  input_node->device_name = kDeviceNameInput;
  input_node->active = crosapi::BoolValue::New(kActiveInput);
  input_node->node_gain = crosapi::UInt8Value::New(kNodeGainInput);
  input_node_info.push_back(std::move(input_node));

  std::vector<crosapi::ProbeAudioOutputNodeInfoPtr> output_node_info;
  auto output_node = crosapi::ProbeAudioOutputNodeInfo::New();
  output_node->id = crosapi::UInt64Value::New(kIdOutput);
  output_node->name = kNameOutput;
  output_node->device_name = kDeviceNameOutput;
  output_node->active = crosapi::BoolValue::New(kActiveOutput);
  output_node->node_volume = crosapi::UInt8Value::New(kNodeVolumeOutput);
  output_node_info.push_back(std::move(output_node));

  auto input = crosapi::ProbeAudioInfo::New();
  input->output_mute = crosapi::BoolValue::New(kOutputMute);
  input->input_mute = crosapi::BoolValue::New(kInputMute);
  input->underruns = crosapi::UInt32Value::New(kUnderruns);
  input->severe_underruns = crosapi::UInt32Value::New(kSevereUnderruns);
  input->output_nodes = std::move(output_node_info);
  input->input_nodes = std::move(input_node_info);

  auto result = ConvertPtr(std::move(input));

  ASSERT_TRUE(result.output_mute);
  EXPECT_EQ(kOutputMute, *result.output_mute);

  ASSERT_TRUE(result.input_mute);
  EXPECT_EQ(kInputMute, *result.input_mute);

  ASSERT_TRUE(result.underruns);
  EXPECT_EQ(kUnderruns, static_cast<uint32_t>(*result.underruns));

  ASSERT_TRUE(result.severe_underruns);
  EXPECT_EQ(kSevereUnderruns, static_cast<uint32_t>(*result.severe_underruns));

  auto result_output_nodes = std::move(result.output_nodes);
  ASSERT_EQ(result_output_nodes.size(), 1UL);

  ASSERT_TRUE(result_output_nodes[0].id);
  EXPECT_EQ(kIdOutput, static_cast<uint64_t>(*result_output_nodes[0].id));

  ASSERT_TRUE(result_output_nodes[0].name);
  EXPECT_EQ(kNameOutput, *result_output_nodes[0].name);

  ASSERT_TRUE(result_output_nodes[0].device_name);
  EXPECT_EQ(kDeviceNameOutput, *result_output_nodes[0].device_name);

  ASSERT_TRUE(result_output_nodes[0].active);
  EXPECT_EQ(kActiveOutput, *result_output_nodes[0].active);

  ASSERT_TRUE(result_output_nodes[0].node_volume);
  EXPECT_EQ(kNodeVolumeOutput,
            static_cast<uint8_t>(*result_output_nodes[0].node_volume));

  auto result_input_nodes = std::move(result.input_nodes);
  ASSERT_EQ(result_input_nodes.size(), 1UL);

  ASSERT_TRUE(result_input_nodes[0].id);
  EXPECT_EQ(kIdInput, static_cast<uint64_t>(*result_input_nodes[0].id));

  ASSERT_TRUE(result_input_nodes[0].name);
  EXPECT_EQ(kNameInput, *result_input_nodes[0].name);

  ASSERT_TRUE(result_input_nodes[0].device_name);
  EXPECT_EQ(kDeviceNameInput, *result_input_nodes[0].device_name);

  ASSERT_TRUE(result_input_nodes[0].active);
  EXPECT_EQ(kActiveInput, *result_input_nodes[0].active);

  ASSERT_TRUE(result_input_nodes[0].node_gain);
  EXPECT_EQ(kNodeGainInput,
            static_cast<uint8_t>(*result_input_nodes[0].node_gain));
}

TEST(TelemetryApiConverters, CpuArchitectureEnum) {
  EXPECT_EQ(cx_telem::CpuArchitectureEnum::kUnknown,
            Convert(crosapi::ProbeCpuArchitectureEnum::kUnknown));
  EXPECT_EQ(cx_telem::CpuArchitectureEnum::kX86_64,
            Convert(crosapi::ProbeCpuArchitectureEnum::kX86_64));
  EXPECT_EQ(cx_telem::CpuArchitectureEnum::kAarch64,
            Convert(crosapi::ProbeCpuArchitectureEnum::kAArch64));
  EXPECT_EQ(cx_telem::CpuArchitectureEnum::kArmv7l,
            Convert(crosapi::ProbeCpuArchitectureEnum::kArmv7l));
}

TEST(TelemetryApiConverters, CpuCStateInfo) {
  constexpr char kName[] = "C0";
  constexpr uint64_t kTimeInStateSinceLastBootUs = 123456;

  auto input = crosapi::ProbeCpuCStateInfo::New(
      kName, crosapi::UInt64Value::New(kTimeInStateSinceLastBootUs));

  auto result = ConvertPtr(std::move(input));
  ASSERT_TRUE(result.name);
  EXPECT_EQ(kName, *result.name);

  ASSERT_TRUE(result.time_in_state_since_last_boot_us);
  EXPECT_EQ(kTimeInStateSinceLastBootUs,
            *result.time_in_state_since_last_boot_us);
}

TEST(TelemetryApiConverters, LogicalCpuInfo) {
  constexpr char kCpuCStateName[] = "C1";
  constexpr uint64_t kCpuCStateTime = (1 << 27) + 50000;

  std::vector<crosapi::ProbeCpuCStateInfoPtr> expected_c_states;
  expected_c_states.push_back(crosapi::ProbeCpuCStateInfo::New(
      kCpuCStateName, crosapi::UInt64Value::New(kCpuCStateTime)));

  constexpr uint32_t kMaxClockSpeedKhz = (1 << 30) + 10000;
  constexpr uint32_t kScalingMaxFrequencyKhz = (1 << 30) + 20000;
  constexpr uint32_t kScalingCurrentFrequencyKhz = (1 << 29) + 30000;
  constexpr uint64_t kIdleTime = (1ULL << 52) + 40000;
  constexpr uint32_t kCoreId = 200;

  auto input = crosapi::ProbeLogicalCpuInfo::New(
      crosapi::UInt32Value::New(kMaxClockSpeedKhz),
      crosapi::UInt32Value::New(kScalingMaxFrequencyKhz),
      crosapi::UInt32Value::New(kScalingCurrentFrequencyKhz),
      crosapi::UInt64Value::New(kIdleTime), std::move(expected_c_states),
      crosapi::UInt32Value::New(kCoreId));

  auto result = ConvertPtr(std::move(input));
  ASSERT_TRUE(result.max_clock_speed_khz);
  EXPECT_EQ(kMaxClockSpeedKhz,
            static_cast<uint32_t>(*result.max_clock_speed_khz));

  ASSERT_TRUE(result.scaling_max_frequency_khz);
  EXPECT_EQ(kScalingMaxFrequencyKhz,
            static_cast<uint32_t>(*result.scaling_max_frequency_khz));

  ASSERT_TRUE(result.scaling_current_frequency_khz);
  EXPECT_EQ(kScalingCurrentFrequencyKhz,
            static_cast<uint32_t>(*result.scaling_current_frequency_khz));

  ASSERT_TRUE(result.idle_time_ms);
  EXPECT_EQ(kIdleTime, *result.idle_time_ms);

  ASSERT_EQ(1u, result.c_states.size());

  ASSERT_TRUE(result.c_states[0].name);
  EXPECT_EQ(kCpuCStateName, *result.c_states[0].name);

  ASSERT_TRUE(result.c_states[0].time_in_state_since_last_boot_us);
  EXPECT_EQ(kCpuCStateTime,
            *result.c_states[0].time_in_state_since_last_boot_us);
  ASSERT_TRUE(result.core_id);
  EXPECT_EQ(kCoreId, static_cast<uint32_t>(*result.core_id));
}

TEST(TelemetryApiConverters, PhysicalCpuInfo) {
  constexpr char kCpuCStateName[] = "C2";
  constexpr uint64_t kCpuCStateTime = (1 << 27) + 90000;

  std::vector<crosapi::ProbeCpuCStateInfoPtr> expected_c_states;
  expected_c_states.push_back(crosapi::ProbeCpuCStateInfo::New(
      kCpuCStateName, crosapi::UInt64Value::New(kCpuCStateTime)));

  constexpr uint32_t kMaxClockSpeedKhz = (1 << 30) + 80000;
  constexpr uint32_t kScalingMaxFrequencyKhz = (1 << 30) + 70000;
  constexpr uint32_t kScalingCurrentFrequencyKhz = (1 << 29) + 60000;
  constexpr uint64_t kIdleTime = (1ULL << 52) + 50000;
  constexpr uint32_t kCoreId = 200;

  std::vector<crosapi::ProbeLogicalCpuInfoPtr> logical_cpus;
  logical_cpus.push_back(crosapi::ProbeLogicalCpuInfo::New(
      crosapi::UInt32Value::New(kMaxClockSpeedKhz),
      crosapi::UInt32Value::New(kScalingMaxFrequencyKhz),
      crosapi::UInt32Value::New(kScalingCurrentFrequencyKhz),
      crosapi::UInt64Value::New(kIdleTime), std::move(expected_c_states),
      crosapi::UInt32Value::New(kCoreId)));

  constexpr char kModelName[] = "i9";

  auto input =
      crosapi::ProbePhysicalCpuInfo::New(kModelName, std::move(logical_cpus));

  auto result = ConvertPtr(std::move(input));
  ASSERT_TRUE(result.model_name);
  EXPECT_EQ(kModelName, *result.model_name);

  ASSERT_EQ(1u, result.logical_cpus.size());

  ASSERT_TRUE(result.logical_cpus[0].max_clock_speed_khz);
  EXPECT_EQ(kMaxClockSpeedKhz,
            static_cast<uint32_t>(*result.logical_cpus[0].max_clock_speed_khz));

  ASSERT_TRUE(result.logical_cpus[0].scaling_max_frequency_khz);
  EXPECT_EQ(
      kScalingMaxFrequencyKhz,
      static_cast<uint32_t>(*result.logical_cpus[0].scaling_max_frequency_khz));

  ASSERT_TRUE(result.logical_cpus[0].scaling_current_frequency_khz);
  EXPECT_EQ(kScalingCurrentFrequencyKhz,
            static_cast<uint32_t>(
                *result.logical_cpus[0].scaling_current_frequency_khz));

  ASSERT_TRUE(result.logical_cpus[0].idle_time_ms);
  EXPECT_EQ(kIdleTime, *result.logical_cpus[0].idle_time_ms);

  ASSERT_EQ(1u, result.logical_cpus[0].c_states.size());

  ASSERT_TRUE(result.logical_cpus[0].c_states[0].name);
  EXPECT_EQ(kCpuCStateName, *result.logical_cpus[0].c_states[0].name);

  ASSERT_TRUE(
      result.logical_cpus[0].c_states[0].time_in_state_since_last_boot_us);
  EXPECT_EQ(
      kCpuCStateTime,
      *result.logical_cpus[0].c_states[0].time_in_state_since_last_boot_us);
  ASSERT_TRUE(result.logical_cpus[0].core_id);
  EXPECT_EQ(kCoreId, static_cast<uint32_t>(*result.logical_cpus[0].core_id));
}

TEST(TelemetryApiConverters, BatteryInfoWithoutSerialNumberPermission) {
  constexpr int64_t kCycleCount = 100000000000000;
  constexpr double_t kVoltageNow = 1234567890.123456;
  constexpr char kVendor[] = "Google";
  constexpr char kSerialNumber[] = "abcdef";
  constexpr double_t kChargeFullDesign = 3000000000000000;
  constexpr double_t kChargeFull = 9000000000000000;
  constexpr double_t kVoltageMinDesign = 1000000000.1001;
  constexpr char kModelName[] = "Google Battery";
  constexpr double_t kChargeNow = 7777777777.777;
  constexpr double_t kCurrentNow = 0.9999999999999;
  constexpr char kTechnology[] = "Li-ion";
  constexpr char kStatus[] = "Charging";
  constexpr char kManufacturerDate[] = "2020-07-30";
  constexpr double_t kTemperature = 7777777777777777;

  crosapi::ProbeBatteryInfoPtr input = crosapi::ProbeBatteryInfo::New(
      crosapi::Int64Value::New(kCycleCount),
      crosapi::DoubleValue::New(kVoltageNow), kVendor, kSerialNumber,
      crosapi::DoubleValue::New(kChargeFullDesign),
      crosapi::DoubleValue::New(kChargeFull),
      crosapi::DoubleValue::New(kVoltageMinDesign), kModelName,
      crosapi::DoubleValue::New(kChargeNow),
      crosapi::DoubleValue::New(kCurrentNow), kTechnology, kStatus,
      kManufacturerDate, crosapi::UInt64Value::New(kTemperature));

  auto result =
      ConvertPtr(std::move(input), /* has_serial_number_permission= */ false);
  ASSERT_TRUE(result.cycle_count);
  EXPECT_EQ(kCycleCount, static_cast<int64_t>(*result.cycle_count));

  ASSERT_TRUE(result.voltage_now);
  EXPECT_EQ(kVoltageNow, static_cast<double_t>(*result.voltage_now));

  ASSERT_TRUE(result.vendor);
  EXPECT_EQ(kVendor, *result.vendor);
  // serial_number is not converted in ConvertPtr() without permission.
  EXPECT_FALSE(result.serial_number);

  ASSERT_TRUE(result.charge_full_design);
  EXPECT_EQ(kChargeFullDesign,
            static_cast<double_t>(*result.charge_full_design));

  ASSERT_TRUE(result.charge_full);
  EXPECT_EQ(kChargeFull, static_cast<double_t>(*result.charge_full));

  ASSERT_TRUE(result.voltage_min_design);
  EXPECT_EQ(kVoltageMinDesign,
            static_cast<double_t>(*result.voltage_min_design));

  ASSERT_TRUE(result.model_name);
  EXPECT_EQ(kModelName, *result.model_name);

  ASSERT_TRUE(result.charge_now);
  EXPECT_EQ(kChargeNow, static_cast<double_t>(*result.charge_now));

  ASSERT_TRUE(result.current_now);
  EXPECT_EQ(kCurrentNow, static_cast<double_t>(*result.current_now));

  ASSERT_TRUE(result.technology);
  EXPECT_EQ(kTechnology, *result.technology);

  ASSERT_TRUE(result.status);
  EXPECT_EQ(kStatus, *result.status);

  ASSERT_TRUE(result.manufacture_date);
  EXPECT_EQ(kManufacturerDate, *result.manufacture_date);

  ASSERT_TRUE(result.temperature);
  EXPECT_EQ(kTemperature, static_cast<uint64_t>(*result.temperature));
}

TEST(TelemetryApiConverters, BatteryInfoWithSerialNumberPermission) {
  constexpr char kSerialNumber[] = "abcdef";

  crosapi::ProbeBatteryInfoPtr input = crosapi::ProbeBatteryInfo::New();
  input->serial_number = kSerialNumber;

  auto result =
      ConvertPtr(std::move(input), /* has_serial_number_permission= */ true);

  ASSERT_TRUE(result.serial_number);
  EXPECT_EQ(kSerialNumber, result.serial_number);
}

TEST(TelemetryApiConverters, NonRemovableBlockDevice) {
  constexpr uint64_t kSize1 = 100000000000;
  constexpr char kName1[] = "TestName1";
  constexpr char kType1[] = "TestType1";

  constexpr uint64_t kSize2 = 200000000000;
  constexpr char kName2[] = "TestName2";
  constexpr char kType2[] = "TestType2";

  auto first_element = crosapi::ProbeNonRemovableBlockDeviceInfo::New();
  first_element->size = crosapi::UInt64Value::New(kSize1);
  first_element->name = kName1;
  first_element->type = kType1;

  auto second_element = crosapi::ProbeNonRemovableBlockDeviceInfo::New();
  second_element->size = crosapi::UInt64Value::New(kSize2);
  second_element->name = kName2;
  second_element->type = kType2;

  std::vector<crosapi::ProbeNonRemovableBlockDeviceInfoPtr> input;
  input.push_back(std::move(first_element));
  input.push_back(std::move(second_element));

  auto result =
      ConvertPtrVector<cx_telem::NonRemovableBlockDeviceInfo>(std::move(input));

  ASSERT_EQ(result.size(), 2ul);

  ASSERT_TRUE(result[0].size);
  EXPECT_EQ(*result[0].size, kSize1);
  ASSERT_TRUE(result[0].name);
  EXPECT_EQ(*result[0].name, kName1);
  ASSERT_TRUE(result[0].type);
  EXPECT_EQ(*result[0].type, kType1);

  ASSERT_TRUE(result[1].size);
  EXPECT_EQ(*result[1].size, kSize2);
  ASSERT_TRUE(result[1].name);
  EXPECT_EQ(*result[1].name, kName2);
  ASSERT_TRUE(result[1].type);
  EXPECT_EQ(*result[1].type, kType2);
}

TEST(TelemetryApiConverters, OsVersion) {
  constexpr char kReleaseMilestone[] = "87";
  constexpr char kBuildNumber[] = "13544";
  constexpr char kPatchNumber[] = "59.0";
  constexpr char kReleaseChannel[] = "stable-channel";

  auto input = crosapi::ProbeOsVersion::New(kReleaseMilestone, kBuildNumber,
                                            kPatchNumber, kReleaseChannel);

  auto result = ConvertPtr(std::move(input));
  ASSERT_TRUE(result.release_milestone);
  EXPECT_EQ(*result.release_milestone, kReleaseMilestone);

  ASSERT_TRUE(result.build_number);
  EXPECT_EQ(*result.build_number, kBuildNumber);

  ASSERT_TRUE(result.patch_number);
  EXPECT_EQ(*result.patch_number, kPatchNumber);

  ASSERT_TRUE(result.release_channel);
  EXPECT_EQ(*result.release_channel, kReleaseChannel);
}

TEST(TelemetryApiConverters, StatefulPartitionInfo) {
  constexpr uint64_t kAvailableSpace = 3000000000000000;
  constexpr uint64_t kTotalSpace = 9000000000000000;

  crosapi::ProbeStatefulPartitionInfoPtr input =
      crosapi::ProbeStatefulPartitionInfo::New(
          crosapi::UInt64Value::New(kAvailableSpace),
          crosapi::UInt64Value::New(kTotalSpace));

  auto result = ConvertPtr(std::move(input));
  ASSERT_TRUE(result.available_space);
  EXPECT_EQ(kAvailableSpace, *result.available_space);

  ASSERT_TRUE(result.total_space);
  EXPECT_EQ(kTotalSpace, *result.total_space);
}

TEST(TelemetryApiConverters, StatefulPartitionInfoNullFields) {
  crosapi::ProbeStatefulPartitionInfoPtr input =
      crosapi::ProbeStatefulPartitionInfo::New<crosapi::UInt64ValuePtr,
                                               crosapi::UInt64ValuePtr>(
          nullptr, nullptr);

  auto result = ConvertPtr(std::move(input));
  ASSERT_FALSE(result.available_space);
  ASSERT_FALSE(result.total_space);
}

TEST(TelemetryApiConverters, NetworkStateEnum) {
  EXPECT_EQ(
      cx_telem::NetworkState::kUninitialized,
      Convert(chromeos::network_health::mojom::NetworkState::kUninitialized));
  EXPECT_EQ(cx_telem::NetworkState::kDisabled,
            Convert(chromeos::network_health::mojom::NetworkState::kDisabled));
  EXPECT_EQ(
      cx_telem::NetworkState::kProhibited,
      Convert(chromeos::network_health::mojom::NetworkState::kProhibited));
  EXPECT_EQ(
      cx_telem::NetworkState::kNotConnected,
      Convert(chromeos::network_health::mojom::NetworkState::kNotConnected));
  EXPECT_EQ(
      cx_telem::NetworkState::kConnecting,
      Convert(chromeos::network_health::mojom::NetworkState::kConnecting));
  EXPECT_EQ(cx_telem::NetworkState::kPortal,
            Convert(chromeos::network_health::mojom::NetworkState::kPortal));
  EXPECT_EQ(cx_telem::NetworkState::kConnected,
            Convert(chromeos::network_health::mojom::NetworkState::kConnected));
  EXPECT_EQ(cx_telem::NetworkState::kOnline,
            Convert(chromeos::network_health::mojom::NetworkState::kOnline));
}

TEST(TelemetryApiConverters, NetworkTypeEnum) {
  EXPECT_EQ(cx_telem::NetworkType::kNone,
            Convert(chromeos::network_config::mojom::NetworkType::kAll));
  EXPECT_EQ(cx_telem::NetworkType::kCellular,
            Convert(chromeos::network_config::mojom::NetworkType::kCellular));
  EXPECT_EQ(cx_telem::NetworkType::kEthernet,
            Convert(chromeos::network_config::mojom::NetworkType::kEthernet));
  EXPECT_EQ(cx_telem::NetworkType::kNone,
            Convert(chromeos::network_config::mojom::NetworkType::kMobile));
  EXPECT_EQ(cx_telem::NetworkType::kTether,
            Convert(chromeos::network_config::mojom::NetworkType::kTether));
  EXPECT_EQ(cx_telem::NetworkType::kVpn,
            Convert(chromeos::network_config::mojom::NetworkType::kVPN));
  EXPECT_EQ(cx_telem::NetworkType::kNone,
            Convert(chromeos::network_config::mojom::NetworkType::kWireless));
  EXPECT_EQ(cx_telem::NetworkType::kWifi,
            Convert(chromeos::network_config::mojom::NetworkType::kWiFi));
}

TEST(TelemetryApiConverters, NetworkInfoWithoutPermission) {
  constexpr char kIpv4Address[] = "1.1.1.1";
  const std::vector<std::string> kIpv6Addresses = {
      "FE80:CD00:0000:0CDE:1257:0000:211E:729C",
      "CD00:FE80:0000:1257:0CDE:0000:729C:211E"};
  constexpr uint32_t kSignalStrength = 100;
  constexpr char kMacAddress[] = "00-B0-D0-63-C2-26";

  auto input = chromeos::network_health::mojom::Network::New();
  input->type = chromeos::network_config::mojom::NetworkType::kWiFi;
  input->state = chromeos::network_health::mojom::NetworkState::kOnline;
  input->ipv4_address = kIpv4Address;
  input->ipv6_addresses = kIpv6Addresses;
  input->mac_address = kMacAddress;
  input->signal_strength =
      chromeos::network_health::mojom::UInt32Value::New(kSignalStrength);

  auto result =
      ConvertPtr(std::move(input), /* has_mac_address_permission= */ false);
  EXPECT_EQ(result.type, cx_telem::NetworkType::kWifi);
  EXPECT_EQ(result.state, cx_telem::NetworkState::kOnline);

  ASSERT_TRUE(result.ipv4_address);
  EXPECT_EQ(*result.ipv4_address, kIpv4Address);

  ASSERT_EQ(result.ipv6_addresses.size(), 2LU);
  EXPECT_EQ(result.ipv6_addresses, kIpv6Addresses);

  // mac_address is not converted in ConvertPtr() without permission.
  EXPECT_FALSE(result.mac_address);

  ASSERT_TRUE(result.signal_strength);
  EXPECT_EQ(static_cast<double_t>(*result.signal_strength), kSignalStrength);
}

TEST(TelemetryApiConverters, NetworkInfoWithPermission) {
  constexpr char kMacAddress[] = "00-B0-D0-63-C2-26";

  auto input = chromeos::network_health::mojom::Network::New();
  input->mac_address = kMacAddress;

  auto result =
      ConvertPtr(std::move(input), /* has_mac_address_permission= */ true);

  ASSERT_TRUE(result.mac_address);
  EXPECT_EQ(*result.mac_address, kMacAddress);
}

TEST(TelemetryApiConverters, InternetConnectivityInfoWithoutPermission) {
  constexpr char kIpv4Address[] = "1.1.1.1";
  const std::vector<std::string> kIpv6Addresses = {
      "FE80:CD00:0000:0CDE:1257:0000:211E:729C",
      "CD00:FE80:0000:1257:0CDE:0000:729C:211E"};
  constexpr uint32_t kSignalStrength = 100;
  constexpr char kMacAddress[] = "00-B0-D0-63-C2-26";

  auto network = chromeos::network_health::mojom::Network::New();
  network->type = chromeos::network_config::mojom::NetworkType::kWiFi;
  network->state = chromeos::network_health::mojom::NetworkState::kOnline;
  network->ipv4_address = kIpv4Address;
  network->ipv6_addresses = kIpv6Addresses;
  network->mac_address = kMacAddress;
  network->signal_strength =
      chromeos::network_health::mojom::UInt32Value::New(kSignalStrength);

  auto input = chromeos::network_health::mojom::NetworkHealthState::New();
  input->networks.push_back(std::move(network));

  auto result =
      ConvertPtr(std::move(input), /* has_mac_address_permission= */ false);
  ASSERT_EQ(result.networks.size(), 1UL);

  auto result_network = std::move(result.networks.front());
  EXPECT_EQ(result_network.type, cx_telem::NetworkType::kWifi);
  EXPECT_EQ(result_network.state, cx_telem::NetworkState::kOnline);

  ASSERT_TRUE(result_network.ipv4_address);
  EXPECT_EQ(*result_network.ipv4_address, kIpv4Address);

  ASSERT_EQ(result_network.ipv6_addresses.size(), 2LU);
  EXPECT_EQ(result_network.ipv6_addresses, kIpv6Addresses);

  // mac_address is not converted in ConvertPtr() without permission.
  EXPECT_FALSE(result_network.mac_address);

  ASSERT_TRUE(result_network.signal_strength);
  EXPECT_EQ(static_cast<double_t>(*result_network.signal_strength),
            kSignalStrength);
}

TEST(TelemetryApiConverters, InternetConnectivityInfoWithPermission) {
  constexpr char kIpv4Address[] = "1.1.1.1";
  const std::vector<std::string> kIpv6Addresses = {
      "FE80:CD00:0000:0CDE:1257:0000:211E:729C",
      "CD00:FE80:0000:1257:0CDE:0000:729C:211E"};
  constexpr uint32_t kSignalStrength = 100;
  constexpr char kMacAddress[] = "00-B0-D0-63-C2-26";

  auto network = chromeos::network_health::mojom::Network::New();
  network->type = chromeos::network_config::mojom::NetworkType::kWiFi;
  network->state = chromeos::network_health::mojom::NetworkState::kOnline;
  network->ipv4_address = kIpv4Address;
  network->ipv6_addresses = kIpv6Addresses;
  network->mac_address = kMacAddress;
  network->signal_strength =
      chromeos::network_health::mojom::UInt32Value::New(kSignalStrength);

  // Networks with a type like kAll, kMobile and kWireless should not show
  // up.
  auto invalid_network = chromeos::network_health::mojom::Network::New();
  invalid_network->type = chromeos::network_config::mojom::NetworkType::kAll;
  invalid_network->state =
      chromeos::network_health::mojom::NetworkState::kOnline;
  invalid_network->mac_address = "00:00:5e:00:53:fu";
  invalid_network->ipv4_address = "2.2.2.2";
  invalid_network->ipv6_addresses = {"FE80:0000:CD00:729C:0CDE:1257:0000:211E"};
  invalid_network->signal_strength =
      chromeos::network_health::mojom::UInt32Value::New(100);

  auto input = chromeos::network_health::mojom::NetworkHealthState::New();
  input->networks.push_back(std::move(invalid_network));
  input->networks.push_back(std::move(network));

  auto result =
      ConvertPtr(std::move(input), /* has_mac_address_permission= */ true);
  ASSERT_EQ(result.networks.size(), 1UL);

  auto result_network = std::move(result.networks.front());
  EXPECT_EQ(result_network.type, cx_telem::NetworkType::kWifi);
  EXPECT_EQ(result_network.state, cx_telem::NetworkState::kOnline);

  ASSERT_TRUE(result_network.ipv4_address);
  EXPECT_EQ(*result_network.ipv4_address, kIpv4Address);

  ASSERT_EQ(result_network.ipv6_addresses.size(), 2LU);
  EXPECT_EQ(result_network.ipv6_addresses, kIpv6Addresses);

  ASSERT_TRUE(result_network.mac_address);
  EXPECT_EQ(*result_network.mac_address, kMacAddress);

  ASSERT_TRUE(result_network.signal_strength);
  EXPECT_EQ(static_cast<double_t>(*result_network.signal_strength),
            kSignalStrength);
}

TEST(TelemetryApiConverters, TpmVersion) {
  constexpr uint32_t kFamily = 0x322e3000;
  constexpr uint64_t kSpecLevel = 1000;
  constexpr uint32_t kManufacturer = 42;
  constexpr uint32_t kTpmModel = 101;
  constexpr uint64_t kFirmwareVersion = 1001;
  constexpr char kVendorSpecific[] = "info";

  auto input = crosapi::ProbeTpmVersion::New();
  input->gsc_version = crosapi::ProbeTpmGSCVersion::kCr50;
  input->family = crosapi::UInt32Value::New(kFamily);
  input->spec_level = crosapi::UInt64Value::New(kSpecLevel);
  input->manufacturer = crosapi::UInt32Value::New(kManufacturer);
  input->tpm_model = crosapi::UInt32Value::New(kTpmModel);
  input->firmware_version = crosapi::UInt64Value::New(kFirmwareVersion);
  input->vendor_specific = kVendorSpecific;

  auto result = ConvertPtr(std::move(input));
  EXPECT_EQ(cx_telem::TpmGSCVersion::kCr50, result.gsc_version);
  ASSERT_TRUE(result.family);
  EXPECT_EQ(kFamily, static_cast<uint32_t>(*result.family));
  ASSERT_TRUE(result.spec_level);
  EXPECT_EQ(kSpecLevel, static_cast<uint64_t>(*result.spec_level));
  ASSERT_TRUE(result.manufacturer);
  EXPECT_EQ(kManufacturer, static_cast<uint32_t>(*result.manufacturer));
  ASSERT_TRUE(result.tpm_model);
  EXPECT_EQ(kTpmModel, static_cast<uint32_t>(*result.tpm_model));
  ASSERT_TRUE(result.firmware_version);
  EXPECT_EQ(kFirmwareVersion, static_cast<uint64_t>(*result.firmware_version));
  ASSERT_TRUE(result.vendor_specific);
  EXPECT_EQ(kVendorSpecific, *result.vendor_specific);
}

TEST(TelemetryApiConverters, TpmStatus) {
  constexpr bool kEnabled = true;
  constexpr bool kOwned = false;
  constexpr bool kOwnerPasswortIsPresent = false;

  auto input = crosapi::ProbeTpmStatus::New();
  input->enabled = crosapi::BoolValue::New(kEnabled);
  input->owned = crosapi::BoolValue::New(kOwned);
  input->owner_password_is_present =
      crosapi::BoolValue::New(kOwnerPasswortIsPresent);

  auto result = ConvertPtr(std::move(input));
  ASSERT_TRUE(result.enabled);
  EXPECT_EQ(kEnabled, *result.enabled);
  ASSERT_TRUE(result.owned);
  EXPECT_EQ(kOwned, *result.owned);
  ASSERT_TRUE(result.owner_password_is_present);
  EXPECT_EQ(kOwnerPasswortIsPresent, *result.owner_password_is_present);
}

TEST(TelemetryApiConverters, TpmDictionaryAttack) {
  constexpr uint32_t kCounter = 42;
  constexpr uint32_t kThreshold = 100;
  constexpr bool kLockOutInEffect = true;
  constexpr uint32_t kLockoutSecondsRemaining = 5;

  auto input = crosapi::ProbeTpmDictionaryAttack::New();
  input->counter = crosapi::UInt32Value::New(kCounter);
  input->threshold = crosapi::UInt32Value::New(kThreshold);
  input->lockout_in_effect = crosapi::BoolValue::New(kLockOutInEffect);
  input->lockout_seconds_remaining =
      crosapi::UInt32Value::New(kLockoutSecondsRemaining);

  auto result = ConvertPtr(std::move(input));
  ASSERT_TRUE(result.counter);
  EXPECT_EQ(kCounter, static_cast<uint32_t>(*result.counter));
  ASSERT_TRUE(result.threshold);
  EXPECT_EQ(kThreshold, static_cast<uint32_t>(*result.threshold));
  ASSERT_TRUE(result.lockout_in_effect);
  EXPECT_EQ(kLockOutInEffect, *result.lockout_in_effect);
  ASSERT_TRUE(result.lockout_seconds_remaining);
  EXPECT_EQ(kLockoutSecondsRemaining,
            static_cast<uint32_t>(*result.lockout_seconds_remaining));
}

TEST(TelemetryApiConverters, TpmInfo) {
  // TPM Version fields.
  constexpr uint32_t kFamily = 0x322e3000;
  constexpr uint64_t kSpecLevel = 1000;
  constexpr uint32_t kManufacturer = 42;
  constexpr uint32_t kTpmModel = 101;
  constexpr uint64_t kFirmwareVersion = 1001;
  constexpr char kVendorSpecific[] = "info";

  // TPM Status fields.
  constexpr bool kEnabled = true;
  constexpr bool kOwned = false;
  constexpr bool kOwnerPasswortIsPresent = false;

  // TPM dictionary attack fields.
  constexpr uint32_t kCounter = 42;
  constexpr uint32_t kThreshold = 100;
  constexpr bool kLockOutInEffect = true;
  constexpr uint32_t kLockoutSecondsRemaining = 5;

  auto tpm_version = crosapi::ProbeTpmVersion::New();
  tpm_version->gsc_version = crosapi::ProbeTpmGSCVersion::kCr50;
  tpm_version->family = crosapi::UInt32Value::New(kFamily);
  tpm_version->spec_level = crosapi::UInt64Value::New(kSpecLevel);
  tpm_version->manufacturer = crosapi::UInt32Value::New(kManufacturer);
  tpm_version->tpm_model = crosapi::UInt32Value::New(kTpmModel);
  tpm_version->firmware_version = crosapi::UInt64Value::New(kFirmwareVersion);
  tpm_version->vendor_specific = kVendorSpecific;

  auto tpm_status = crosapi::ProbeTpmStatus::New();
  tpm_status->enabled = crosapi::BoolValue::New(kEnabled);
  tpm_status->owned = crosapi::BoolValue::New(kOwned);
  tpm_status->owner_password_is_present =
      crosapi::BoolValue::New(kOwnerPasswortIsPresent);

  auto dictionary_attack = crosapi::ProbeTpmDictionaryAttack::New();
  dictionary_attack->counter = crosapi::UInt32Value::New(kCounter);
  dictionary_attack->threshold = crosapi::UInt32Value::New(kThreshold);
  dictionary_attack->lockout_in_effect =
      crosapi::BoolValue::New(kLockOutInEffect);
  dictionary_attack->lockout_seconds_remaining =
      crosapi::UInt32Value::New(kLockoutSecondsRemaining);

  auto input = crosapi::ProbeTpmInfo::New();
  input->version = std::move(tpm_version);
  input->status = std::move(tpm_status);
  input->dictionary_attack = std::move(dictionary_attack);

  auto result = ConvertPtr(std::move(input));

  auto version_result = std::move(result.version);
  EXPECT_EQ(cx_telem::TpmGSCVersion::kCr50, version_result.gsc_version);
  ASSERT_TRUE(version_result.family);
  EXPECT_EQ(kFamily, static_cast<uint32_t>(*version_result.family));
  ASSERT_TRUE(version_result.spec_level);
  EXPECT_EQ(kSpecLevel, static_cast<uint64_t>(*version_result.spec_level));
  ASSERT_TRUE(version_result.manufacturer);
  EXPECT_EQ(kManufacturer, static_cast<uint32_t>(*version_result.manufacturer));
  ASSERT_TRUE(version_result.tpm_model);
  EXPECT_EQ(kTpmModel, static_cast<uint32_t>(*version_result.tpm_model));
  ASSERT_TRUE(version_result.firmware_version);
  EXPECT_EQ(kFirmwareVersion,
            static_cast<uint64_t>(*version_result.firmware_version));
  ASSERT_TRUE(version_result.vendor_specific);
  EXPECT_EQ(kVendorSpecific, *version_result.vendor_specific);

  auto status_result = std::move(result.status);
  ASSERT_TRUE(status_result.enabled);
  EXPECT_EQ(kEnabled, *status_result.enabled);
  ASSERT_TRUE(status_result.owned);
  EXPECT_EQ(kOwned, *status_result.owned);
  ASSERT_TRUE(status_result.owner_password_is_present);
  EXPECT_EQ(kOwnerPasswortIsPresent, *status_result.owner_password_is_present);

  auto dictionary_attack_result = std::move(result.dictionary_attack);
  ASSERT_TRUE(dictionary_attack_result.counter);
  EXPECT_EQ(kCounter, static_cast<uint32_t>(*dictionary_attack_result.counter));
  ASSERT_TRUE(dictionary_attack_result.threshold);
  EXPECT_EQ(kThreshold,
            static_cast<uint32_t>(*dictionary_attack_result.threshold));
  ASSERT_TRUE(dictionary_attack_result.lockout_in_effect);
  EXPECT_EQ(kLockOutInEffect, *dictionary_attack_result.lockout_in_effect);
  ASSERT_TRUE(dictionary_attack_result.lockout_seconds_remaining);
  EXPECT_EQ(kLockoutSecondsRemaining,
            static_cast<uint32_t>(
                *dictionary_attack_result.lockout_seconds_remaining));
}

TEST(TelemetryApiConverters, UsbVersion) {
  EXPECT_EQ(Convert(crosapi::ProbeUsbVersion::kUnknown),
            cx_telem::UsbVersion::kUnknown);

  EXPECT_EQ(Convert(crosapi::ProbeUsbVersion::kUsb1),
            cx_telem::UsbVersion::kUsb1);

  EXPECT_EQ(Convert(crosapi::ProbeUsbVersion::kUsb2),
            cx_telem::UsbVersion::kUsb2);

  EXPECT_EQ(Convert(crosapi::ProbeUsbVersion::kUsb3),
            cx_telem::UsbVersion::kUsb3);
}

TEST(TelemetryApiConverters, UsbSpecSpeed) {
  EXPECT_EQ(Convert(crosapi::ProbeUsbSpecSpeed::kUnknown),
            cx_telem::UsbSpecSpeed::kUnknown);

  EXPECT_EQ(Convert(crosapi::ProbeUsbSpecSpeed::k1_5Mbps),
            cx_telem::UsbSpecSpeed::kN1_5mbps);

  EXPECT_EQ(Convert(crosapi::ProbeUsbSpecSpeed::k12Mbps),
            cx_telem::UsbSpecSpeed::kN12Mbps);

  EXPECT_EQ(Convert(crosapi::ProbeUsbSpecSpeed::k480Mbps),
            cx_telem::UsbSpecSpeed::kN480Mbps);

  EXPECT_EQ(Convert(crosapi::ProbeUsbSpecSpeed::k5Gbps),
            cx_telem::UsbSpecSpeed::kN5Gbps);

  EXPECT_EQ(Convert(crosapi::ProbeUsbSpecSpeed::k10Gbps),
            cx_telem::UsbSpecSpeed::kN10Gbps);

  EXPECT_EQ(Convert(crosapi::ProbeUsbSpecSpeed::k20Gbps),
            cx_telem::UsbSpecSpeed::kN20Gbps);
}

TEST(TelemetryApiConverters, FwupdVersionFormat) {
  EXPECT_EQ(Convert(crosapi::ProbeFwupdVersionFormat::kUnknown),
            cx_telem::FwupdVersionFormat::kPlain);

  EXPECT_EQ(Convert(crosapi::ProbeFwupdVersionFormat::kPlain),
            cx_telem::FwupdVersionFormat::kPlain);

  EXPECT_EQ(Convert(crosapi::ProbeFwupdVersionFormat::kNumber),
            cx_telem::FwupdVersionFormat::kNumber);

  EXPECT_EQ(Convert(crosapi::ProbeFwupdVersionFormat::kPair),
            cx_telem::FwupdVersionFormat::kPair);

  EXPECT_EQ(Convert(crosapi::ProbeFwupdVersionFormat::kTriplet),
            cx_telem::FwupdVersionFormat::kTriplet);

  EXPECT_EQ(Convert(crosapi::ProbeFwupdVersionFormat::kBcd),
            cx_telem::FwupdVersionFormat::kBcd);

  EXPECT_EQ(Convert(crosapi::ProbeFwupdVersionFormat::kIntelMe),
            cx_telem::FwupdVersionFormat::kIntelMe);

  EXPECT_EQ(Convert(crosapi::ProbeFwupdVersionFormat::kIntelMe2),
            cx_telem::FwupdVersionFormat::kIntelMe2);

  EXPECT_EQ(Convert(crosapi::ProbeFwupdVersionFormat::kSurfaceLegacy),
            cx_telem::FwupdVersionFormat::kSurfaceLegacy);

  EXPECT_EQ(Convert(crosapi::ProbeFwupdVersionFormat::kSurface),
            cx_telem::FwupdVersionFormat::kSurface);

  EXPECT_EQ(Convert(crosapi::ProbeFwupdVersionFormat::kDellBios),
            cx_telem::FwupdVersionFormat::kDellBios);

  EXPECT_EQ(Convert(crosapi::ProbeFwupdVersionFormat::kHex),
            cx_telem::FwupdVersionFormat::kHex);
}

TEST(TelemetryApiConverters, FwupdFirmwareVersionInfo) {
  constexpr char kVersion[] = "MyVersion";

  auto input = crosapi::ProbeFwupdFirmwareVersionInfo::New(
      kVersion, crosapi::ProbeFwupdVersionFormat::kHex);

  auto result = ConvertPtr(std::move(input));

  EXPECT_EQ(result.version, kVersion);
  EXPECT_EQ(result.version_format, cx_telem::FwupdVersionFormat::kHex);
}

TEST(TelemetryApiConverters, UsbBusInterfaceInfo) {
  constexpr uint8_t kInterfaceNumber = 41;
  constexpr uint8_t kClassId = 42;
  constexpr uint8_t kSubclassId = 43;
  constexpr uint8_t kProtocolId = 44;
  constexpr char kDriver[] = "MyDriver";

  auto input = crosapi::ProbeUsbBusInterfaceInfo::New(
      crosapi::UInt8Value::New(kInterfaceNumber),
      crosapi::UInt8Value::New(kClassId), crosapi::UInt8Value::New(kSubclassId),
      crosapi::UInt8Value::New(kProtocolId), kDriver);

  auto result = ConvertPtr(std::move(input));

  ASSERT_TRUE(result.interface_number);
  EXPECT_EQ(static_cast<uint8_t>(*result.interface_number), kInterfaceNumber);
  ASSERT_TRUE(result.class_id);
  EXPECT_EQ(static_cast<uint8_t>(*result.class_id), kClassId);
  ASSERT_TRUE(result.subclass_id);
  EXPECT_EQ(static_cast<uint8_t>(*result.subclass_id), kSubclassId);
  ASSERT_TRUE(result.protocol_id);
  EXPECT_EQ(static_cast<uint8_t>(*result.protocol_id), kProtocolId);
  ASSERT_TRUE(result.driver);
  EXPECT_EQ(result.driver, kDriver);
}

TEST(TelemetryApiConverters, UsbBusInfo) {
  constexpr uint8_t kInterfaceNumberInterface = 41;
  constexpr uint8_t kClassIdInterface = 42;
  constexpr uint8_t kSubclassIdInterface = 43;
  constexpr uint8_t kProtocolIdInterface = 44;
  constexpr char kDriverInterface[] = "MyDriver";

  std::vector<crosapi::ProbeUsbBusInterfaceInfoPtr> interfaces;
  interfaces.push_back(crosapi::ProbeUsbBusInterfaceInfo::New(
      crosapi::UInt8Value::New(kInterfaceNumberInterface),
      crosapi::UInt8Value::New(kClassIdInterface),
      crosapi::UInt8Value::New(kSubclassIdInterface),
      crosapi::UInt8Value::New(kProtocolIdInterface), kDriverInterface));

  constexpr uint8_t kClassId = 45;
  constexpr uint8_t kSubclassId = 46;
  constexpr uint8_t kProtocolId = 47;
  constexpr uint16_t kVendor = 48;
  constexpr uint16_t kProductId = 49;

  constexpr char kVersion[] = "MyVersion";

  auto fwupd_version = crosapi::ProbeFwupdFirmwareVersionInfo::New(
      kVersion, crosapi::ProbeFwupdVersionFormat::kPair);

  auto input = crosapi::ProbeUsbBusInfo::New();
  input->class_id = crosapi::UInt8Value::New(kClassId);
  input->subclass_id = crosapi::UInt8Value::New(kSubclassId);
  input->protocol_id = crosapi::UInt8Value::New(kProtocolId);
  input->vendor_id = crosapi::UInt16Value::New(kVendor);
  input->product_id = crosapi::UInt16Value::New(kProductId);
  input->interfaces = std::move(interfaces);
  input->fwupd_firmware_version_info = std::move(fwupd_version);
  input->version = crosapi::ProbeUsbVersion::kUsb3;
  input->spec_speed = crosapi::ProbeUsbSpecSpeed::k20Gbps;

  auto result = ConvertPtr(std::move(input));

  ASSERT_TRUE(result.class_id);
  EXPECT_EQ(static_cast<uint8_t>(*result.class_id), kClassId);
  ASSERT_TRUE(result.subclass_id);
  EXPECT_EQ(static_cast<uint8_t>(*result.subclass_id), kSubclassId);
  ASSERT_TRUE(result.protocol_id);
  EXPECT_EQ(static_cast<uint8_t>(*result.protocol_id), kProtocolId);
  ASSERT_TRUE(result.product_id);
  EXPECT_EQ(static_cast<uint16_t>(*result.product_id), kProductId);
  ASSERT_TRUE(result.vendor_id);
  EXPECT_EQ(static_cast<uint16_t>(*result.vendor_id), kVendor);

  ASSERT_EQ(result.interfaces.size(), 1UL);
  ASSERT_TRUE(result.interfaces[0].interface_number);
  EXPECT_EQ(static_cast<uint8_t>(*result.interfaces[0].interface_number),
            kInterfaceNumberInterface);
  ASSERT_TRUE(result.interfaces[0].class_id);
  EXPECT_EQ(static_cast<uint8_t>(*result.interfaces[0].class_id),
            kClassIdInterface);
  ASSERT_TRUE(result.interfaces[0].subclass_id);
  EXPECT_EQ(static_cast<uint8_t>(*result.interfaces[0].subclass_id),
            kSubclassIdInterface);
  ASSERT_TRUE(result.interfaces[0].protocol_id);
  EXPECT_EQ(static_cast<uint8_t>(*result.interfaces[0].protocol_id),
            kProtocolIdInterface);
  ASSERT_TRUE(result.interfaces[0].driver);
  EXPECT_EQ(result.interfaces[0].driver, kDriverInterface);

  ASSERT_TRUE(result.fwupd_firmware_version_info);
  EXPECT_EQ(result.fwupd_firmware_version_info->version, kVersion);
  EXPECT_EQ(result.fwupd_firmware_version_info->version_format,
            cx_telem::FwupdVersionFormat::kPair);

  EXPECT_EQ(result.version, cx_telem::UsbVersion::kUsb3);
  EXPECT_EQ(result.spec_speed, cx_telem::UsbSpecSpeed::kN20Gbps);
}

TEST(TelemetryApiConverters, VpdInfoWithoutPermission) {
  constexpr char kFirstPowerDate[] = "01/01/00";
  constexpr char kModelName[] = "TestModel";
  constexpr char kSkuNumber[] = "TestSKU";
  constexpr char kSerialNumber[] = "TestNumber";

  auto input = crosapi::ProbeCachedVpdInfo::New();
  input->first_power_date = kFirstPowerDate;
  input->model_name = kModelName;
  input->sku_number = kSkuNumber;
  input->serial_number = kSerialNumber;

  auto result =
      ConvertPtr(std::move(input), /* has_serial_number_permission= */ false);

  ASSERT_TRUE(result.activate_date);
  EXPECT_EQ(*result.activate_date, kFirstPowerDate);

  ASSERT_TRUE(result.model_name);
  EXPECT_EQ(*result.model_name, kModelName);

  ASSERT_TRUE(result.sku_number);
  EXPECT_EQ(*result.sku_number, kSkuNumber);

  // serial_number is not converted in ConvertPtr() without permission.
  EXPECT_FALSE(result.serial_number);
}

TEST(TelemetryApiConverters, VpdInfoWithPermission) {
  constexpr char kSerialNumber[] = "TestNumber";

  auto input = crosapi::ProbeCachedVpdInfo::New();
  input->serial_number = kSerialNumber;

  auto result =
      ConvertPtr(std::move(input), /* has_serial_number_permission= */ true);

  ASSERT_TRUE(result.serial_number);
  EXPECT_EQ(*result.serial_number, kSerialNumber);
}

TEST(TelemetryApiConverters, DisplayInputType) {
  EXPECT_EQ(Convert(crosapi::ProbeDisplayInputType::kUnmappedEnumField),
            cx_telem::DisplayInputType::kUnknown);

  EXPECT_EQ(Convert(crosapi::ProbeDisplayInputType::kDigital),
            cx_telem::DisplayInputType::kDigital);

  EXPECT_EQ(Convert(crosapi::ProbeDisplayInputType::kAnalog),
            cx_telem::DisplayInputType::kAnalog);
}

TEST(TelemetryApiConverters, DisplayInfo) {
  // Constants for embedded display
  constexpr bool kPrivacyScreenSupported = true;
  constexpr bool kPrivacyScreenEnabled = false;
  constexpr uint32_t kDisplayWidthEmbedded = 0;
  constexpr uint32_t kDisplayHeightEmbedded = 1;
  constexpr uint32_t kResolutionHorizontalEmbedded = 2;
  constexpr uint32_t kResolutionVerticalEmbedded = 3;
  constexpr double kRefreshRateEmbedded = 4.4;
  constexpr char kManufacturerEmbedded[] = "manufacturer_1";
  constexpr uint16_t kModelIdEmbedded = 5;
  constexpr uint32_t kSerialNumberEmbedded = 6;
  constexpr uint8_t kManufactureWeekEmbedded = 7;
  constexpr uint16_t kManufactureYearEmbedded = 8;
  constexpr char kEdidVersionEmbedded[] = "1.4";
  constexpr crosapi::ProbeDisplayInputType kInputTypeEmbedded =
      crosapi::ProbeDisplayInputType::kAnalog;
  constexpr char kDisplayNameEmbedded[] = "display_1";

  // constants for external display 1
  constexpr uint32_t kDisplayWidthExternal = 10;
  constexpr uint32_t kDisplayHeightExternal = 11;
  constexpr uint32_t kResolutionHorizontalExternal = 12;
  constexpr uint32_t kResolutionVerticalExternal = 13;
  constexpr double kRefreshRateExternal = 14.4;
  constexpr char kManufacturerExternal[] = "manufacturer_2";
  constexpr uint16_t kModelIdExternal = 15;
  constexpr uint32_t kSerialNumberExternal = 16;
  constexpr uint8_t kManufactureWeekExternal = 17;
  constexpr uint16_t kManufactureYearExternal = 18;
  constexpr char kEdidVersionExternal[] = "1.4";
  constexpr crosapi::ProbeDisplayInputType kInputTypeExternal =
      crosapi::ProbeDisplayInputType::kDigital;
  constexpr char kDisplayNameExternal[] = "display_2";

  auto input = crosapi::ProbeDisplayInfo::New();
  {
    auto embedded_display = crosapi::ProbeEmbeddedDisplayInfo::New(
        kPrivacyScreenSupported, kPrivacyScreenEnabled, kDisplayWidthEmbedded,
        kDisplayHeightEmbedded, kResolutionHorizontalEmbedded,
        kResolutionVerticalEmbedded, kRefreshRateEmbedded,
        std::string(kManufacturerEmbedded), kModelIdEmbedded,
        kSerialNumberEmbedded, kManufactureWeekEmbedded,
        kManufactureYearEmbedded, std::string(kEdidVersionEmbedded),
        kInputTypeEmbedded, std::string(kDisplayNameEmbedded));

    auto external_display_1 = crosapi::ProbeExternalDisplayInfo::New(
        kDisplayWidthExternal, kDisplayHeightExternal,
        kResolutionHorizontalExternal, kResolutionVerticalExternal,
        kRefreshRateExternal, std::string(kManufacturerExternal),
        kModelIdExternal, kSerialNumberExternal, kManufactureWeekExternal,
        kManufactureYearExternal, std::string(kEdidVersionExternal),
        kInputTypeExternal, std::string(kDisplayNameExternal));

    auto external_display_empty = crosapi::ProbeExternalDisplayInfo::New(
        std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
        std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
        std::nullopt, crosapi::ProbeDisplayInputType::kUnmappedEnumField,
        std::nullopt);

    std::vector<crosapi::ProbeExternalDisplayInfoPtr> external_displays;
    external_displays.push_back(std::move(external_display_1));
    external_displays.push_back(std::move(external_display_empty));

    input->embedded_display = std::move(embedded_display);
    input->external_displays = std::move(external_displays);
  }

  auto result = ConvertPtr(std::move(input));

  const auto& embedded_display = result.embedded_display;
  const auto& external_displays = result.external_displays;
  EXPECT_EQ(external_displays.size(), static_cast<size_t>(2));

  // Check equality for embedded display
  EXPECT_EQ(embedded_display.privacy_screen_supported, kPrivacyScreenSupported);
  EXPECT_EQ(embedded_display.privacy_screen_enabled, kPrivacyScreenEnabled);
  ASSERT_TRUE(embedded_display.display_width);
  EXPECT_EQ(static_cast<uint32_t>(*embedded_display.display_width),
            kDisplayWidthEmbedded);
  ASSERT_TRUE(embedded_display.display_height);
  EXPECT_EQ(static_cast<uint32_t>(*embedded_display.display_height),
            kDisplayHeightEmbedded);
  ASSERT_TRUE(embedded_display.resolution_horizontal);
  EXPECT_EQ(static_cast<uint32_t>(*embedded_display.resolution_horizontal),
            kResolutionHorizontalEmbedded);
  ASSERT_TRUE(embedded_display.resolution_vertical);
  EXPECT_EQ(static_cast<uint32_t>(*embedded_display.resolution_vertical),
            kResolutionVerticalEmbedded);
  ASSERT_TRUE(embedded_display.refresh_rate);
  EXPECT_EQ(static_cast<double>(*embedded_display.refresh_rate),
            kRefreshRateEmbedded);
  EXPECT_EQ(embedded_display.manufacturer, kManufacturerEmbedded);
  ASSERT_TRUE(embedded_display.model_id);
  EXPECT_EQ(static_cast<uint16_t>(*embedded_display.model_id),
            kModelIdEmbedded);
  // serial_number is not converted in ConvertPtr() for now.
  EXPECT_FALSE(embedded_display.serial_number);
  ASSERT_TRUE(embedded_display.manufacture_week);
  EXPECT_EQ(static_cast<uint8_t>(*embedded_display.manufacture_week),
            kManufactureWeekEmbedded);
  ASSERT_TRUE(embedded_display.manufacture_year);
  EXPECT_EQ(static_cast<uint16_t>(*embedded_display.manufacture_year),
            kManufactureYearEmbedded);
  EXPECT_EQ(embedded_display.edid_version, kEdidVersionEmbedded);
  EXPECT_EQ(embedded_display.input_type, Convert(kInputTypeEmbedded));
  EXPECT_EQ(embedded_display.display_name, kDisplayNameEmbedded);

  // Check equality for external display 1
  ASSERT_TRUE(external_displays[0].display_width);
  EXPECT_EQ(static_cast<uint32_t>(*external_displays[0].display_width),
            kDisplayWidthExternal);
  ASSERT_TRUE(external_displays[0].display_height);
  EXPECT_EQ(static_cast<uint32_t>(*external_displays[0].display_height),
            kDisplayHeightExternal);
  ASSERT_TRUE(external_displays[0].resolution_horizontal);
  EXPECT_EQ(static_cast<uint32_t>(*external_displays[0].resolution_horizontal),
            kResolutionHorizontalExternal);
  ASSERT_TRUE(external_displays[0].resolution_vertical);
  EXPECT_EQ(static_cast<uint32_t>(*external_displays[0].resolution_vertical),
            kResolutionVerticalExternal);
  ASSERT_TRUE(external_displays[0].refresh_rate);
  EXPECT_EQ(static_cast<double>(*external_displays[0].refresh_rate),
            kRefreshRateExternal);
  EXPECT_EQ(external_displays[0].manufacturer, kManufacturerExternal);
  ASSERT_TRUE(external_displays[0].model_id);
  EXPECT_EQ(static_cast<uint16_t>(*external_displays[0].model_id),
            kModelIdExternal);
  // serial_number is not converted in ConvertPtr() for now.
  EXPECT_FALSE(external_displays[0].serial_number);
  ASSERT_TRUE(external_displays[0].manufacture_week);
  EXPECT_EQ(static_cast<uint8_t>(*external_displays[0].manufacture_week),
            kManufactureWeekExternal);
  ASSERT_TRUE(external_displays[0].manufacture_year);
  EXPECT_EQ(static_cast<uint16_t>(*external_displays[0].manufacture_year),
            kManufactureYearExternal);
  EXPECT_EQ(external_displays[0].edid_version, kEdidVersionExternal);
  EXPECT_EQ(external_displays[0].input_type, Convert(kInputTypeExternal));
  EXPECT_EQ(external_displays[0].display_name, kDisplayNameExternal);

  // Check equality for empty external display
  ASSERT_FALSE(external_displays[1].display_width);
  ASSERT_FALSE(external_displays[1].display_height);
  ASSERT_FALSE(external_displays[1].resolution_horizontal);
  ASSERT_FALSE(external_displays[1].resolution_vertical);
  ASSERT_FALSE(external_displays[1].refresh_rate);
  EXPECT_EQ(external_displays[1].manufacturer, std::nullopt);
  ASSERT_FALSE(external_displays[1].model_id);
  ASSERT_FALSE(external_displays[1].serial_number);
  ASSERT_FALSE(external_displays[1].manufacture_week);
  ASSERT_FALSE(external_displays[1].manufacture_year);
  EXPECT_EQ(external_displays[1].edid_version, std::nullopt);
  EXPECT_EQ(external_displays[1].input_type,
            Convert(crosapi::ProbeDisplayInputType::kUnmappedEnumField));
  EXPECT_EQ(external_displays[1].display_name, std::nullopt);
}

TEST(TelemetryApiConverters, ThermalSensorSource) {
  EXPECT_EQ(Convert(crosapi::ProbeThermalSensorSource::kUnmappedEnumField),
            cx_telem::ThermalSensorSource::kUnknown);

  EXPECT_EQ(Convert(crosapi::ProbeThermalSensorSource::kEc),
            cx_telem::ThermalSensorSource::kEc);

  EXPECT_EQ(Convert(crosapi::ProbeThermalSensorSource::kSysFs),
            cx_telem::ThermalSensorSource::kSysFs);
}

TEST(TelemetryApiConverters, ThermalInfo) {
  // Constant values for the first thermal sensor.
  constexpr char kSensorName1[] = "thermal_sensor_1";
  constexpr double kSensorTemp1 = 100;
  constexpr crosapi::ProbeThermalSensorSource kSensorSource1 =
      crosapi::ProbeThermalSensorSource::kEc;

  // Constant values for the first thermal sensor.
  constexpr char kSensorName2[] = "thermal_sensor_2";
  constexpr double kSensorTemp2 = 50;
  constexpr crosapi::ProbeThermalSensorSource kSensorSource2 =
      crosapi::ProbeThermalSensorSource::kSysFs;

  auto input = crosapi::ProbeThermalInfo::New();
  {
    auto thermal_sensor_1 = crosapi::ProbeThermalSensorInfo::New(
        kSensorName1, kSensorTemp1, kSensorSource1);

    auto thermal_sensor_2 = crosapi::ProbeThermalSensorInfo::New(
        kSensorName2, kSensorTemp2, kSensorSource2);

    std::vector<crosapi::ProbeThermalSensorInfoPtr> thermal_sensors;
    thermal_sensors.push_back(std::move(thermal_sensor_1));
    thermal_sensors.push_back(std::move(thermal_sensor_2));

    input->thermal_sensors = std::move(thermal_sensors);
  }

  auto result = ConvertPtr(std::move(input));

  const auto& thermal_sensors = result.thermal_sensors;
  EXPECT_EQ(thermal_sensors.size(), static_cast<size_t>(2));

  // Check equality for thermal sensor 1
  EXPECT_EQ(thermal_sensors[0].name, kSensorName1);
  ASSERT_TRUE(thermal_sensors[0].temperature_celsius);
  EXPECT_EQ(static_cast<double>(*thermal_sensors[0].temperature_celsius),
            kSensorTemp1);
  EXPECT_EQ(thermal_sensors[0].source, Convert(kSensorSource1));

  // Check equality for thermal sensor 2
  EXPECT_EQ(thermal_sensors[1].name, kSensorName2);
  ASSERT_TRUE(thermal_sensors[1].temperature_celsius);
  EXPECT_EQ(static_cast<double>(*thermal_sensors[1].temperature_celsius),
            kSensorTemp2);
  EXPECT_EQ(thermal_sensors[1].source, Convert(kSensorSource2));
}

}  // namespace chromeos::converters::telemetry
