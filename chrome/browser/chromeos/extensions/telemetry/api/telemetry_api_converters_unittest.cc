// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry_api_converters.h"
#include "chrome/common/chromeos/extensions/api/telemetry.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
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

}  // namespace converters
}  // namespace chromeos
