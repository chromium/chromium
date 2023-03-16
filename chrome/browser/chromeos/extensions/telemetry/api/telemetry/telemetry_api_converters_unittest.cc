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

namespace chromeos {

namespace {

namespace telemetry_api = ::chromeos::api::os_telemetry;
namespace telemetry_service = ::crosapi::mojom;

}  // namespace

namespace converters {

TEST(TelemetryApiConverters, AudioInputNodeInfo) {
  constexpr uint64_t kId = 42;
  constexpr char kName[] = "Internal Mic";
  constexpr char kDeviceName[] = "HDA Intel PCH: CA0132 Analog:0,0";
  constexpr bool kActive = true;
  constexpr uint8_t kNodeGain = 1;

  auto input = telemetry_service::ProbeAudioInputNodeInfo::New();
  input->id = crosapi::mojom::UInt64Value::New(kId);
  input->name = kName;
  input->device_name = kDeviceName;
  input->active = crosapi::mojom::BoolValue::New(kActive);
  input->node_gain = crosapi::mojom::UInt8Value::New(kNodeGain);

  auto result = ConvertPtr<telemetry_api::AudioInputNodeInfo>(std::move(input));

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

  auto input = telemetry_service::ProbeAudioOutputNodeInfo::New();
  input->id = crosapi::mojom::UInt64Value::New(kId);
  input->name = kName;
  input->device_name = kDeviceName;
  input->active = crosapi::mojom::BoolValue::New(kActive);
  input->node_volume = crosapi::mojom::UInt8Value::New(kNodeVolume);

  auto result =
      ConvertPtr<telemetry_api::AudioOutputNodeInfo>(std::move(input));

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

  std::vector<telemetry_service::ProbeAudioInputNodeInfoPtr> input_node_info;
  auto input_node = telemetry_service::ProbeAudioInputNodeInfo::New();
  input_node->id = crosapi::mojom::UInt64Value::New(kIdInput);
  input_node->name = kNameInput;
  input_node->device_name = kDeviceNameInput;
  input_node->active = crosapi::mojom::BoolValue::New(kActiveInput);
  input_node->node_gain = crosapi::mojom::UInt8Value::New(kNodeGainInput);
  input_node_info.push_back(std::move(input_node));

  std::vector<telemetry_service::ProbeAudioOutputNodeInfoPtr> output_node_info;
  auto output_node = telemetry_service::ProbeAudioOutputNodeInfo::New();
  output_node->id = crosapi::mojom::UInt64Value::New(kIdOutput);
  output_node->name = kNameOutput;
  output_node->device_name = kDeviceNameOutput;
  output_node->active = crosapi::mojom::BoolValue::New(kActiveOutput);
  output_node->node_volume = crosapi::mojom::UInt8Value::New(kNodeVolumeOutput);
  output_node_info.push_back(std::move(output_node));

  auto input = telemetry_service::ProbeAudioInfo::New();
  input->output_mute = crosapi::mojom::BoolValue::New(kOutputMute);
  input->input_mute = crosapi::mojom::BoolValue::New(kInputMute);
  input->underruns = crosapi::mojom::UInt32Value::New(kUnderruns);
  input->severe_underruns = crosapi::mojom::UInt32Value::New(kSevereUnderruns);
  input->output_nodes = std::move(output_node_info);
  input->input_nodes = std::move(input_node_info);

  auto result = ConvertPtr<telemetry_api::AudioInfo>(std::move(input));

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
  EXPECT_EQ(telemetry_api::CpuArchitectureEnum::kUnknown,
            Convert(telemetry_service::ProbeCpuArchitectureEnum::kUnknown));
  EXPECT_EQ(telemetry_api::CpuArchitectureEnum::kX8664,
            Convert(telemetry_service::ProbeCpuArchitectureEnum::kX86_64));
  EXPECT_EQ(telemetry_api::CpuArchitectureEnum::kAarch64,
            Convert(telemetry_service::ProbeCpuArchitectureEnum::kAArch64));
  EXPECT_EQ(telemetry_api::CpuArchitectureEnum::kArmv7l,
            Convert(telemetry_service::ProbeCpuArchitectureEnum::kArmv7l));
}

TEST(TelemetryApiConverters, CpuCStateInfo) {
  constexpr char kName[] = "C0";
  constexpr uint64_t kTimeInStateSinceLastBootUs = 123456;

  auto input = telemetry_service::ProbeCpuCStateInfo::New(
      kName, telemetry_service::UInt64Value::New(kTimeInStateSinceLastBootUs));

  auto result = ConvertPtr<telemetry_api::CpuCStateInfo>(std::move(input));
  ASSERT_TRUE(result.name);
  EXPECT_EQ(kName, *result.name);

  ASSERT_TRUE(result.time_in_state_since_last_boot_us);
  EXPECT_EQ(kTimeInStateSinceLastBootUs,
            *result.time_in_state_since_last_boot_us);
}

TEST(TelemetryApiConverters, LogicalCpuInfo) {
  constexpr char kCpuCStateName[] = "C1";
  constexpr uint64_t kCpuCStateTime = (1 << 27) + 50000;

  std::vector<telemetry_service::ProbeCpuCStateInfoPtr> expected_c_states;
  expected_c_states.push_back(telemetry_service::ProbeCpuCStateInfo::New(
      kCpuCStateName, telemetry_service::UInt64Value::New(kCpuCStateTime)));

  constexpr uint32_t kMaxClockSpeedKhz = (1 << 30) + 10000;
  constexpr uint32_t kScalingMaxFrequencyKhz = (1 << 30) + 20000;
  constexpr uint32_t kScalingCurrentFrequencyKhz = (1 << 29) + 30000;
  constexpr uint64_t kIdleTime = (1ULL << 52) + 40000;

  auto input = telemetry_service::ProbeLogicalCpuInfo::New(
      telemetry_service::UInt32Value::New(kMaxClockSpeedKhz),
      telemetry_service::UInt32Value::New(kScalingMaxFrequencyKhz),
      telemetry_service::UInt32Value::New(kScalingCurrentFrequencyKhz),
      telemetry_service::UInt64Value::New(kIdleTime),
      std::move(expected_c_states));

  auto result = ConvertPtr<telemetry_api::LogicalCpuInfo>(std::move(input));
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
}

TEST(TelemetryApiConverters, PhysicalCpuInfo) {
  constexpr char kCpuCStateName[] = "C2";
  constexpr uint64_t kCpuCStateTime = (1 << 27) + 90000;

  std::vector<telemetry_service::ProbeCpuCStateInfoPtr> expected_c_states;
  expected_c_states.push_back(telemetry_service::ProbeCpuCStateInfo::New(
      kCpuCStateName, telemetry_service::UInt64Value::New(kCpuCStateTime)));

  constexpr uint32_t kMaxClockSpeedKhz = (1 << 30) + 80000;
  constexpr uint32_t kScalingMaxFrequencyKhz = (1 << 30) + 70000;
  constexpr uint32_t kScalingCurrentFrequencyKhz = (1 << 29) + 60000;
  constexpr uint64_t kIdleTime = (1ULL << 52) + 50000;

  std::vector<telemetry_service::ProbeLogicalCpuInfoPtr> logical_cpus;
  logical_cpus.push_back(telemetry_service::ProbeLogicalCpuInfo::New(
      telemetry_service::UInt32Value::New(kMaxClockSpeedKhz),
      telemetry_service::UInt32Value::New(kScalingMaxFrequencyKhz),
      telemetry_service::UInt32Value::New(kScalingCurrentFrequencyKhz),
      telemetry_service::UInt64Value::New(kIdleTime),
      std::move(expected_c_states)));

  constexpr char kModelName[] = "i9";

  auto input = telemetry_service::ProbePhysicalCpuInfo::New(
      kModelName, std::move(logical_cpus));

  auto result = ConvertPtr<telemetry_api::PhysicalCpuInfo>(std::move(input));
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
}

TEST(TelemetryApiConverters, BatteryInfo) {
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

  telemetry_service::ProbeBatteryInfoPtr input =
      telemetry_service::ProbeBatteryInfo::New(
          telemetry_service::Int64Value::New(kCycleCount),
          telemetry_service::DoubleValue::New(kVoltageNow), kVendor,
          kSerialNumber, telemetry_service::DoubleValue::New(kChargeFullDesign),
          telemetry_service::DoubleValue::New(kChargeFull),
          telemetry_service::DoubleValue::New(kVoltageMinDesign), kModelName,
          telemetry_service::DoubleValue::New(kChargeNow),
          telemetry_service::DoubleValue::New(kCurrentNow), kTechnology,
          kStatus, kManufacturerDate,
          telemetry_service::UInt64Value::New(kTemperature));

  auto result = ConvertPtr<telemetry_api::BatteryInfo>(std::move(input));
  ASSERT_TRUE(result.cycle_count);
  EXPECT_EQ(kCycleCount, static_cast<int64_t>(*result.cycle_count));

  ASSERT_TRUE(result.voltage_now);
  EXPECT_EQ(kVoltageNow, static_cast<double_t>(*result.voltage_now));

  ASSERT_TRUE(result.vendor);
  EXPECT_EQ(kVendor, *result.vendor);
  // serial_number is not converted in ConvertPtr().
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

TEST(TelemetryApiConverters, NonRemovableBlockDevice) {
  constexpr uint64_t kSize1 = 100000000000;
  constexpr char kName1[] = "TestName1";
  constexpr char kType1[] = "TestType1";

  constexpr uint64_t kSize2 = 200000000000;
  constexpr char kName2[] = "TestName2";
  constexpr char kType2[] = "TestType2";

  auto first_element =
      telemetry_service::ProbeNonRemovableBlockDeviceInfo::New();
  first_element->size = telemetry_service::UInt64Value::New(kSize1);
  first_element->name = kName1;
  first_element->type = kType1;

  auto second_element =
      telemetry_service::ProbeNonRemovableBlockDeviceInfo::New();
  second_element->size = telemetry_service::UInt64Value::New(kSize2);
  second_element->name = kName2;
  second_element->type = kType2;

  std::vector<telemetry_service::ProbeNonRemovableBlockDeviceInfoPtr> input;
  input.push_back(std::move(first_element));
  input.push_back(std::move(second_element));

  auto result = ConvertPtrVector<telemetry_api::NonRemovableBlockDeviceInfo>(
      std::move(input));

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

  auto input = telemetry_service::ProbeOsVersion::New(
      kReleaseMilestone, kBuildNumber, kPatchNumber, kReleaseChannel);

  auto result = ConvertPtr<telemetry_api::OsVersionInfo>(std::move(input));
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

  telemetry_service::ProbeStatefulPartitionInfoPtr input =
      telemetry_service::ProbeStatefulPartitionInfo::New(
          telemetry_service::UInt64Value::New(kAvailableSpace),
          telemetry_service::UInt64Value::New(kTotalSpace));

  auto result =
      ConvertPtr<telemetry_api::StatefulPartitionInfo>(std::move(input));
  ASSERT_TRUE(result.available_space);
  EXPECT_EQ(kAvailableSpace, *result.available_space);

  ASSERT_TRUE(result.total_space);
  EXPECT_EQ(kTotalSpace, *result.total_space);
}

TEST(TelemetryApiConverters, StatefulPartitionInfoNullFields) {
  telemetry_service::ProbeStatefulPartitionInfoPtr input =
      telemetry_service::ProbeStatefulPartitionInfo::New<
          telemetry_service::UInt64ValuePtr, telemetry_service::UInt64ValuePtr>(
          nullptr, nullptr);

  auto result =
      ConvertPtr<telemetry_api::StatefulPartitionInfo>(std::move(input));
  ASSERT_FALSE(result.available_space);
  ASSERT_FALSE(result.total_space);
}

TEST(TelemetryApiConverters, NetworkStateEnum) {
  EXPECT_EQ(
      telemetry_api::NetworkState::kUninitialized,
      Convert(chromeos::network_health::mojom::NetworkState::kUninitialized));
  EXPECT_EQ(telemetry_api::NetworkState::kDisabled,
            Convert(chromeos::network_health::mojom::NetworkState::kDisabled));
  EXPECT_EQ(
      telemetry_api::NetworkState::kProhibited,
      Convert(chromeos::network_health::mojom::NetworkState::kProhibited));
  EXPECT_EQ(
      telemetry_api::NetworkState::kNotConnected,
      Convert(chromeos::network_health::mojom::NetworkState::kNotConnected));
  EXPECT_EQ(
      telemetry_api::NetworkState::kConnecting,
      Convert(chromeos::network_health::mojom::NetworkState::kConnecting));
  EXPECT_EQ(telemetry_api::NetworkState::kPortal,
            Convert(chromeos::network_health::mojom::NetworkState::kPortal));
  EXPECT_EQ(telemetry_api::NetworkState::kConnected,
            Convert(chromeos::network_health::mojom::NetworkState::kConnected));
  EXPECT_EQ(telemetry_api::NetworkState::kOnline,
            Convert(chromeos::network_health::mojom::NetworkState::kOnline));
}

TEST(TelemetryApiConverters, NetworkTypeEnum) {
  EXPECT_EQ(telemetry_api::NetworkType::kNone,
            Convert(chromeos::network_config::mojom::NetworkType::kAll));
  EXPECT_EQ(telemetry_api::NetworkType::kCellular,
            Convert(chromeos::network_config::mojom::NetworkType::kCellular));
  EXPECT_EQ(telemetry_api::NetworkType::kEthernet,
            Convert(chromeos::network_config::mojom::NetworkType::kEthernet));
  EXPECT_EQ(telemetry_api::NetworkType::kNone,
            Convert(chromeos::network_config::mojom::NetworkType::kMobile));
  EXPECT_EQ(telemetry_api::NetworkType::kTether,
            Convert(chromeos::network_config::mojom::NetworkType::kTether));
  EXPECT_EQ(telemetry_api::NetworkType::kVpn,
            Convert(chromeos::network_config::mojom::NetworkType::kVPN));
  EXPECT_EQ(telemetry_api::NetworkType::kNone,
            Convert(chromeos::network_config::mojom::NetworkType::kWireless));
  EXPECT_EQ(telemetry_api::NetworkType::kWifi,
            Convert(chromeos::network_config::mojom::NetworkType::kWiFi));
}

TEST(TelemetryApiConverters, NetworkInfo) {
  constexpr char kIpv4Address[] = "1.1.1.1";
  const std::vector<std::string> kIpv6Addresses = {
      "FE80:CD00:0000:0CDE:1257:0000:211E:729C",
      "CD00:FE80:0000:1257:0CDE:0000:729C:211E"};
  constexpr uint32_t kSignalStrength = 100;

  auto input = chromeos::network_health::mojom::Network::New();
  input->type = chromeos::network_config::mojom::NetworkType::kWiFi;
  input->state = chromeos::network_health::mojom::NetworkState::kOnline;
  input->ipv4_address = kIpv4Address;
  input->ipv6_addresses = kIpv6Addresses;
  input->signal_strength =
      chromeos::network_health::mojom::UInt32Value::New(kSignalStrength);

  auto result = ConvertPtr<telemetry_api::NetworkInfo>(std::move(input));
  EXPECT_EQ(result.type, telemetry_api::NetworkType::kWifi);
  EXPECT_EQ(result.state, telemetry_api::NetworkState::kOnline);

  ASSERT_TRUE(result.ipv4_address);
  EXPECT_EQ(*result.ipv4_address, kIpv4Address);

  ASSERT_EQ(result.ipv6_addresses.size(), 2LU);
  EXPECT_EQ(result.ipv6_addresses, kIpv6Addresses);

  ASSERT_TRUE(result.signal_strength);
  EXPECT_EQ(static_cast<double_t>(*result.signal_strength), kSignalStrength);
}

TEST(TelemetryApiConverters, TpmVersion) {
  constexpr uint32_t kFamily = 0x322e3000;
  constexpr uint64_t kSpecLevel = 1000;
  constexpr uint32_t kManufacturer = 42;
  constexpr uint32_t kTpmModel = 101;
  constexpr uint64_t kFirmwareVersion = 1001;
  constexpr char kVendorSpecific[] = "info";

  auto input = telemetry_service::ProbeTpmVersion::New();
  input->gsc_version = telemetry_service::ProbeTpmGSCVersion::kCr50;
  input->family = telemetry_service::UInt32Value::New(kFamily);
  input->spec_level = telemetry_service::UInt64Value::New(kSpecLevel);
  input->manufacturer = telemetry_service::UInt32Value::New(kManufacturer);
  input->tpm_model = telemetry_service::UInt32Value::New(kTpmModel);
  input->firmware_version =
      telemetry_service::UInt64Value::New(kFirmwareVersion);
  input->vendor_specific = kVendorSpecific;

  auto result = ConvertPtr<telemetry_api::TpmVersion>(std::move(input));
  EXPECT_EQ(telemetry_api::TpmGSCVersion::kCr50, result.gsc_version);
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

  auto input = telemetry_service::ProbeTpmStatus::New();
  input->enabled = telemetry_service::BoolValue::New(kEnabled);
  input->owned = telemetry_service::BoolValue::New(kOwned);
  input->owner_password_is_present =
      telemetry_service::BoolValue::New(kOwnerPasswortIsPresent);

  auto result = ConvertPtr<telemetry_api::TpmStatus>(std::move(input));
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

  auto input = telemetry_service::ProbeTpmDictionaryAttack::New();
  input->counter = telemetry_service::UInt32Value::New(kCounter);
  input->threshold = telemetry_service::UInt32Value::New(kThreshold);
  input->lockout_in_effect =
      telemetry_service::BoolValue::New(kLockOutInEffect);
  input->lockout_seconds_remaining =
      telemetry_service::UInt32Value::New(kLockoutSecondsRemaining);

  auto result =
      ConvertPtr<telemetry_api::TpmDictionaryAttack>(std::move(input));
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

  auto tpm_version = telemetry_service::ProbeTpmVersion::New();
  tpm_version->gsc_version = telemetry_service::ProbeTpmGSCVersion::kCr50;
  tpm_version->family = telemetry_service::UInt32Value::New(kFamily);
  tpm_version->spec_level = telemetry_service::UInt64Value::New(kSpecLevel);
  tpm_version->manufacturer =
      telemetry_service::UInt32Value::New(kManufacturer);
  tpm_version->tpm_model = telemetry_service::UInt32Value::New(kTpmModel);
  tpm_version->firmware_version =
      telemetry_service::UInt64Value::New(kFirmwareVersion);
  tpm_version->vendor_specific = kVendorSpecific;

  auto tpm_status = telemetry_service::ProbeTpmStatus::New();
  tpm_status->enabled = telemetry_service::BoolValue::New(kEnabled);
  tpm_status->owned = telemetry_service::BoolValue::New(kOwned);
  tpm_status->owner_password_is_present =
      telemetry_service::BoolValue::New(kOwnerPasswortIsPresent);

  auto dictionary_attack = telemetry_service::ProbeTpmDictionaryAttack::New();
  dictionary_attack->counter = telemetry_service::UInt32Value::New(kCounter);
  dictionary_attack->threshold =
      telemetry_service::UInt32Value::New(kThreshold);
  dictionary_attack->lockout_in_effect =
      telemetry_service::BoolValue::New(kLockOutInEffect);
  dictionary_attack->lockout_seconds_remaining =
      telemetry_service::UInt32Value::New(kLockoutSecondsRemaining);

  auto input = telemetry_service::ProbeTpmInfo::New();
  input->version = std::move(tpm_version);
  input->status = std::move(tpm_status);
  input->dictionary_attack = std::move(dictionary_attack);

  auto result = ConvertPtr<telemetry_api::TpmInfo>(std::move(input));

  auto version_result = std::move(result.version);
  EXPECT_EQ(telemetry_api::TpmGSCVersion::kCr50, version_result.gsc_version);
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
  EXPECT_EQ(Convert(crosapi::mojom::ProbeUsbVersion::kUnknown),
            telemetry_api::UsbVersion::kUnknown);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeUsbVersion::kUsb1),
            telemetry_api::UsbVersion::kUsb1);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeUsbVersion::kUsb2),
            telemetry_api::UsbVersion::kUsb2);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeUsbVersion::kUsb3),
            telemetry_api::UsbVersion::kUsb3);
}

TEST(TelemetryApiConverters, UsbSpecSpeed) {
  EXPECT_EQ(Convert(crosapi::mojom::ProbeUsbSpecSpeed::kUnknown),
            telemetry_api::UsbSpecSpeed::kUnknown);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeUsbSpecSpeed::k1_5Mbps),
            telemetry_api::UsbSpecSpeed::kN15mbps);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeUsbSpecSpeed::k12Mbps),
            telemetry_api::UsbSpecSpeed::kN12Mbps);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeUsbSpecSpeed::k480Mbps),
            telemetry_api::UsbSpecSpeed::kN480Mbps);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeUsbSpecSpeed::k5Gbps),
            telemetry_api::UsbSpecSpeed::kN5Gbps);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeUsbSpecSpeed::k10Gbps),
            telemetry_api::UsbSpecSpeed::kN10Gbps);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeUsbSpecSpeed::k20Gbps),
            telemetry_api::UsbSpecSpeed::kN20Gbps);
}

TEST(TelemetryApiConverters, FwupdVersionFormat) {
  EXPECT_EQ(Convert(crosapi::mojom::ProbeFwupdVersionFormat::kUnknown),
            telemetry_api::FwupdVersionFormat::kPlain);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeFwupdVersionFormat::kPlain),
            telemetry_api::FwupdVersionFormat::kPlain);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeFwupdVersionFormat::kNumber),
            telemetry_api::FwupdVersionFormat::kNumber);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeFwupdVersionFormat::kPair),
            telemetry_api::FwupdVersionFormat::kPair);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeFwupdVersionFormat::kTriplet),
            telemetry_api::FwupdVersionFormat::kTriplet);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeFwupdVersionFormat::kBcd),
            telemetry_api::FwupdVersionFormat::kBcd);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeFwupdVersionFormat::kIntelMe),
            telemetry_api::FwupdVersionFormat::kIntelMe);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeFwupdVersionFormat::kIntelMe2),
            telemetry_api::FwupdVersionFormat::kIntelMe2);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeFwupdVersionFormat::kSurfaceLegacy),
            telemetry_api::FwupdVersionFormat::kSurfaceLegacy);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeFwupdVersionFormat::kSurface),
            telemetry_api::FwupdVersionFormat::kSurface);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeFwupdVersionFormat::kDellBios),
            telemetry_api::FwupdVersionFormat::kDellBios);

  EXPECT_EQ(Convert(crosapi::mojom::ProbeFwupdVersionFormat::kHex),
            telemetry_api::FwupdVersionFormat::kHex);
}

TEST(TelemetryApiConverters, FwupdFirmwareVersionInfo) {
  constexpr char kVersion[] = "MyVersion";

  auto input = crosapi::mojom::ProbeFwupdFirmwareVersionInfo::New(
      kVersion, crosapi::mojom::ProbeFwupdVersionFormat::kHex);

  auto result =
      ConvertPtr<telemetry_api::FwupdFirmwareVersionInfo>(std::move(input));

  EXPECT_EQ(result.version, kVersion);
  EXPECT_EQ(result.version_format, telemetry_api::FwupdVersionFormat::kHex);
}

TEST(TelemetryApiConverters, UsbBusInterfaceInfo) {
  constexpr uint8_t kInterfaceNumber = 41;
  constexpr uint8_t kClassId = 42;
  constexpr uint8_t kSubclassId = 43;
  constexpr uint8_t kProtocolId = 44;
  constexpr char kDriver[] = "MyDriver";

  auto input = crosapi::mojom::ProbeUsbBusInterfaceInfo::New(
      crosapi::mojom::UInt8Value::New(kInterfaceNumber),
      crosapi::mojom::UInt8Value::New(kClassId),
      crosapi::mojom::UInt8Value::New(kSubclassId),
      crosapi::mojom::UInt8Value::New(kProtocolId), kDriver);

  auto result =
      ConvertPtr<telemetry_api::UsbBusInterfaceInfo>(std::move(input));

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

  std::vector<crosapi::mojom::ProbeUsbBusInterfaceInfoPtr> interfaces;
  interfaces.push_back(crosapi::mojom::ProbeUsbBusInterfaceInfo::New(
      crosapi::mojom::UInt8Value::New(kInterfaceNumberInterface),
      crosapi::mojom::UInt8Value::New(kClassIdInterface),
      crosapi::mojom::UInt8Value::New(kSubclassIdInterface),
      crosapi::mojom::UInt8Value::New(kProtocolIdInterface), kDriverInterface));

  constexpr uint8_t kClassId = 45;
  constexpr uint8_t kSubclassId = 46;
  constexpr uint8_t kProtocolId = 47;
  constexpr uint16_t kVendor = 48;
  constexpr uint16_t kProductId = 49;

  constexpr char kVersion[] = "MyVersion";

  auto fwupd_version = crosapi::mojom::ProbeFwupdFirmwareVersionInfo::New(
      kVersion, crosapi::mojom::ProbeFwupdVersionFormat::kPair);

  auto input = crosapi::mojom::ProbeUsbBusInfo::New();
  input->class_id = crosapi::mojom::UInt8Value::New(kClassId);
  input->subclass_id = crosapi::mojom::UInt8Value::New(kSubclassId);
  input->protocol_id = crosapi::mojom::UInt8Value::New(kProtocolId);
  input->vendor_id = crosapi::mojom::UInt16Value::New(kVendor);
  input->product_id = crosapi::mojom::UInt16Value::New(kProductId);
  input->interfaces = std::move(interfaces);
  input->fwupd_firmware_version_info = std::move(fwupd_version);
  input->version = crosapi::mojom::ProbeUsbVersion::kUsb3;
  input->spec_speed = crosapi::mojom::ProbeUsbSpecSpeed::k20Gbps;

  auto result = ConvertPtr<telemetry_api::UsbBusInfo>(std::move(input));

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
            telemetry_api::FwupdVersionFormat::kPair);

  EXPECT_EQ(result.version, telemetry_api::UsbVersion::kUsb3);
  EXPECT_EQ(result.spec_speed, telemetry_api::UsbSpecSpeed::kN20Gbps);
}

}  // namespace converters
}  // namespace chromeos
