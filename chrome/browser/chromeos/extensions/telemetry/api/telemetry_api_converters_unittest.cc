// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/webui/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry_api_converters.h"
#include "chrome/common/chromeos/extensions/api/telemetry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

namespace telemetry_api = ::chromeos::api::os_telemetry;
namespace telemetry_service = ::ash::health::mojom;

}  // namespace

namespace converters {

TEST(TelemetryApiConverters, CpuArchitectureEnum) {
  EXPECT_EQ(telemetry_api::CpuArchitectureEnum::CPU_ARCHITECTURE_ENUM_UNKNOWN,
            Convert(telemetry_service::CpuArchitectureEnum::kUnknown));
  EXPECT_EQ(telemetry_api::CpuArchitectureEnum::CPU_ARCHITECTURE_ENUM_X86_64,
            Convert(telemetry_service::CpuArchitectureEnum::kX86_64));
  EXPECT_EQ(telemetry_api::CpuArchitectureEnum::CPU_ARCHITECTURE_ENUM_AARCH64,
            Convert(telemetry_service::CpuArchitectureEnum::kAArch64));
  EXPECT_EQ(telemetry_api::CpuArchitectureEnum::CPU_ARCHITECTURE_ENUM_ARMV7L,
            Convert(telemetry_service::CpuArchitectureEnum::kArmv7l));
}

TEST(TelemetryApiConverters, CpuCStateInfo) {
  constexpr char kName[] = "C0";
  constexpr uint64_t kTimeInStateSinceLastBootUs = 123456;

  auto input = telemetry_service::CpuCStateInfo::New(
      kName, telemetry_service::UInt64Value::New(kTimeInStateSinceLastBootUs));

  auto result = ConvertPtr<telemetry_api::CpuCStateInfo>(std::move(input));
  EXPECT_EQ(kName, *result.name);
  EXPECT_EQ(kTimeInStateSinceLastBootUs,
            *result.time_in_state_since_last_boot_us);
}

TEST(TelemetryApiConverters, LogicalCpuInfo) {
  constexpr char kCpuCStateName[] = "C1";
  constexpr uint64_t kCpuCStateTime = (1 << 27) + 50000;

  std::vector<telemetry_service::CpuCStateInfoPtr> expected_c_states;
  expected_c_states.push_back(telemetry_service::CpuCStateInfo::New(
      kCpuCStateName, telemetry_service::UInt64Value::New(kCpuCStateTime)));

  constexpr uint32_t kMaxClockSpeedKhz = (1 << 30) + 10000;
  constexpr uint32_t kScalingMaxFrequencyKhz = (1 << 30) + 20000;
  constexpr uint32_t kScalingCurrentFrequencyKhz = (1 << 29) + 30000;
  constexpr uint64_t kIdleTime = (1ULL << 52) + 40000;

  auto input = telemetry_service::LogicalCpuInfo::New(
      telemetry_service::UInt32Value::New(kMaxClockSpeedKhz),
      telemetry_service::UInt32Value::New(kScalingMaxFrequencyKhz),
      telemetry_service::UInt32Value::New(kScalingCurrentFrequencyKhz),
      telemetry_service::UInt64Value::New(kIdleTime),
      std::move(expected_c_states));

  auto result = ConvertPtr<telemetry_api::LogicalCpuInfo>(std::move(input));
  EXPECT_EQ(kMaxClockSpeedKhz,
            static_cast<uint32_t>(*result.max_clock_speed_khz));
  EXPECT_EQ(kScalingMaxFrequencyKhz,
            static_cast<uint32_t>(*result.scaling_max_frequency_khz));
  EXPECT_EQ(kScalingCurrentFrequencyKhz,
            static_cast<uint32_t>(*result.scaling_current_frequency_khz));
  EXPECT_EQ(kIdleTime, *result.idle_time_ms);
  EXPECT_EQ(1u, result.c_states.size());
  EXPECT_EQ(kCpuCStateName, *result.c_states[0].name);
  EXPECT_EQ(kCpuCStateTime,
            *result.c_states[0].time_in_state_since_last_boot_us);
}

TEST(TelemetryApiConverters, PhysicalCpuInfo) {
  constexpr char kCpuCStateName[] = "C2";
  constexpr uint64_t kCpuCStateTime = (1 << 27) + 90000;

  std::vector<telemetry_service::CpuCStateInfoPtr> expected_c_states;
  expected_c_states.push_back(telemetry_service::CpuCStateInfo::New(
      kCpuCStateName, telemetry_service::UInt64Value::New(kCpuCStateTime)));

  constexpr uint32_t kMaxClockSpeedKhz = (1 << 30) + 80000;
  constexpr uint32_t kScalingMaxFrequencyKhz = (1 << 30) + 70000;
  constexpr uint32_t kScalingCurrentFrequencyKhz = (1 << 29) + 60000;
  constexpr uint64_t kIdleTime = (1ULL << 52) + 50000;

  std::vector<telemetry_service::LogicalCpuInfoPtr> logical_cpus;
  logical_cpus.push_back(telemetry_service::LogicalCpuInfo::New(
      telemetry_service::UInt32Value::New(kMaxClockSpeedKhz),
      telemetry_service::UInt32Value::New(kScalingMaxFrequencyKhz),
      telemetry_service::UInt32Value::New(kScalingCurrentFrequencyKhz),
      telemetry_service::UInt64Value::New(kIdleTime),
      std::move(expected_c_states)));

  constexpr char kModelName[] = "i9";

  auto input = telemetry_service::PhysicalCpuInfo::New(kModelName,
                                                       std::move(logical_cpus));

  auto result = ConvertPtr<telemetry_api::PhysicalCpuInfo>(std::move(input));
  EXPECT_EQ(kModelName, *result.model_name);
  EXPECT_EQ(1u, result.logical_cpus.size());
  EXPECT_EQ(kMaxClockSpeedKhz,
            static_cast<uint32_t>(*result.logical_cpus[0].max_clock_speed_khz));
  EXPECT_EQ(
      kScalingMaxFrequencyKhz,
      static_cast<uint32_t>(*result.logical_cpus[0].scaling_max_frequency_khz));
  EXPECT_EQ(kScalingCurrentFrequencyKhz,
            static_cast<uint32_t>(
                *result.logical_cpus[0].scaling_current_frequency_khz));
  EXPECT_EQ(kIdleTime, *result.logical_cpus[0].idle_time_ms);
  EXPECT_EQ(1u, result.logical_cpus[0].c_states.size());
  EXPECT_EQ(kCpuCStateName, *result.logical_cpus[0].c_states[0].name);
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

  telemetry_service::BatteryInfoPtr input = telemetry_service::BatteryInfo::New(
      telemetry_service::Int64Value::New(kCycleCount),
      telemetry_service::DoubleValue::New(kVoltageNow), kVendor, kSerialNumber,
      telemetry_service::DoubleValue::New(kChargeFullDesign),
      telemetry_service::DoubleValue::New(kChargeFull),
      telemetry_service::DoubleValue::New(kVoltageMinDesign), kModelName,
      telemetry_service::DoubleValue::New(kChargeNow),
      telemetry_service::DoubleValue::New(kCurrentNow), kTechnology, kStatus,
      kManufacturerDate, telemetry_service::UInt64Value::New(kTemperature));

  auto result = ConvertPtr<telemetry_api::BatteryInfo>(std::move(input));
  EXPECT_EQ(kCycleCount, static_cast<int64_t>(*result.cycle_count));
  EXPECT_EQ(kVoltageNow, static_cast<double_t>(*result.voltage_now));
  EXPECT_EQ(kVendor, *result.vendor);
  // serial_number is not converted in ConvertPtr().
  EXPECT_TRUE(result.serial_number == nullptr);
  EXPECT_EQ(kChargeFullDesign,
            static_cast<double_t>(*result.charge_full_design));
  EXPECT_EQ(kChargeFull, static_cast<double_t>(*result.charge_full));
  EXPECT_EQ(kVoltageMinDesign,
            static_cast<double_t>(*result.voltage_min_design));
  EXPECT_EQ(kModelName, *result.model_name);
  EXPECT_EQ(kChargeNow, static_cast<double_t>(*result.charge_now));
  EXPECT_EQ(kCurrentNow, static_cast<double_t>(*result.current_now));
  EXPECT_EQ(kTechnology, *result.technology);
  EXPECT_EQ(kStatus, *result.status);
  EXPECT_EQ(kManufacturerDate, *result.manufacture_date);
  EXPECT_EQ(kTemperature, static_cast<uint64_t>(*result.temperature));
}

}  // namespace converters
}  // namespace chromeos
