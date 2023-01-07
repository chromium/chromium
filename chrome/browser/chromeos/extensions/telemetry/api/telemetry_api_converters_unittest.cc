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

#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry_api_converters.h"
#include "chrome/common/chromeos/extensions/api/telemetry.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

namespace telemetry_api = ::chromeos::api::os_telemetry;
namespace telemetry_service = ::crosapi::mojom;

}  // namespace

namespace converters {

TEST(TelemetryApiConverters, CpuArchitectureEnum) {
  EXPECT_EQ(telemetry_api::CpuArchitectureEnum::CPU_ARCHITECTURE_ENUM_UNKNOWN,
            Convert(telemetry_service::ProbeCpuArchitectureEnum::kUnknown));
  EXPECT_EQ(telemetry_api::CpuArchitectureEnum::CPU_ARCHITECTURE_ENUM_X86_64,
            Convert(telemetry_service::ProbeCpuArchitectureEnum::kX86_64));
  EXPECT_EQ(telemetry_api::CpuArchitectureEnum::CPU_ARCHITECTURE_ENUM_AARCH64,
            Convert(telemetry_service::ProbeCpuArchitectureEnum::kAArch64));
  EXPECT_EQ(telemetry_api::CpuArchitectureEnum::CPU_ARCHITECTURE_ENUM_ARMV7L,
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
      telemetry_api::NetworkState::NETWORK_STATE_UNINITIALIZED,
      Convert(chromeos::network_health::mojom::NetworkState::kUninitialized));
  EXPECT_EQ(telemetry_api::NetworkState::NETWORK_STATE_DISABLED,
            Convert(chromeos::network_health::mojom::NetworkState::kDisabled));
  EXPECT_EQ(
      telemetry_api::NetworkState::NETWORK_STATE_PROHIBITED,
      Convert(chromeos::network_health::mojom::NetworkState::kProhibited));
  EXPECT_EQ(
      telemetry_api::NetworkState::NETWORK_STATE_NOT_CONNECTED,
      Convert(chromeos::network_health::mojom::NetworkState::kNotConnected));
  EXPECT_EQ(
      telemetry_api::NetworkState::NETWORK_STATE_CONNECTING,
      Convert(chromeos::network_health::mojom::NetworkState::kConnecting));
  EXPECT_EQ(telemetry_api::NetworkState::NETWORK_STATE_PORTAL,
            Convert(chromeos::network_health::mojom::NetworkState::kPortal));
  EXPECT_EQ(telemetry_api::NetworkState::NETWORK_STATE_CONNECTED,
            Convert(chromeos::network_health::mojom::NetworkState::kConnected));
  EXPECT_EQ(telemetry_api::NetworkState::NETWORK_STATE_ONLINE,
            Convert(chromeos::network_health::mojom::NetworkState::kOnline));
}

TEST(TelemetryApiConverters, NetworkTypeEnum) {
  EXPECT_EQ(telemetry_api::NetworkType::NETWORK_TYPE_NONE,
            Convert(chromeos::network_config::mojom::NetworkType::kAll));
  EXPECT_EQ(telemetry_api::NetworkType::NETWORK_TYPE_CELLULAR,
            Convert(chromeos::network_config::mojom::NetworkType::kCellular));
  EXPECT_EQ(telemetry_api::NetworkType::NETWORK_TYPE_ETHERNET,
            Convert(chromeos::network_config::mojom::NetworkType::kEthernet));
  EXPECT_EQ(telemetry_api::NetworkType::NETWORK_TYPE_NONE,
            Convert(chromeos::network_config::mojom::NetworkType::kMobile));
  EXPECT_EQ(telemetry_api::NetworkType::NETWORK_TYPE_TETHER,
            Convert(chromeos::network_config::mojom::NetworkType::kTether));
  EXPECT_EQ(telemetry_api::NetworkType::NETWORK_TYPE_VPN,
            Convert(chromeos::network_config::mojom::NetworkType::kVPN));
  EXPECT_EQ(telemetry_api::NetworkType::NETWORK_TYPE_NONE,
            Convert(chromeos::network_config::mojom::NetworkType::kWireless));
  EXPECT_EQ(telemetry_api::NetworkType::NETWORK_TYPE_WIFI,
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
  EXPECT_EQ(result.type, telemetry_api::NetworkType::NETWORK_TYPE_WIFI);
  EXPECT_EQ(result.state, telemetry_api::NetworkState::NETWORK_STATE_ONLINE);

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
  EXPECT_EQ(telemetry_api::TpmGSCVersion::TPM_GSC_VERSION_CR50,
            result.gsc_version);
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
  EXPECT_EQ(telemetry_api::TpmGSCVersion::TPM_GSC_VERSION_CR50,
            version_result.gsc_version);
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

}  // namespace converters
}  // namespace chromeos
