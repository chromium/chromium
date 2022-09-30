// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/probe_service_converters.h"

#include <cstdint>
#include <vector>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::converters {

using ::testing::ElementsAre;

// Note: in some tests we intentionally use New() with no arguments for
// cros_healthd::mojom types, because there can be some fields that we don't
// test yet.
// Also, we intentionally use New() with arguments for crosapi::mojom
// types to let the compiler detect untested data members.

// Tests that |ConvertProbePtr| function returns nullptr if input is nullptr.
// ConvertProbePtr is a template, so we can test this function with any valid
// type.
TEST(ProbeServiceConverters, ConvertProbePtrTakesNullPtr) {
  EXPECT_TRUE(ConvertProbePtr(cros_healthd::mojom::ProbeErrorPtr()).is_null());
}

TEST(ProbeServiceConverters, ConvertCategoryVector) {
  const std::vector<crosapi::mojom::ProbeCategoryEnum> kInput{
      crosapi::mojom::ProbeCategoryEnum::kUnknown,
      crosapi::mojom::ProbeCategoryEnum::kBattery,
      crosapi::mojom::ProbeCategoryEnum::kNonRemovableBlockDevices,
      crosapi::mojom::ProbeCategoryEnum::kCachedVpdData,
      crosapi::mojom::ProbeCategoryEnum::kCpu,
      crosapi::mojom::ProbeCategoryEnum::kTimezone,
      crosapi::mojom::ProbeCategoryEnum::kMemory,
      crosapi::mojom::ProbeCategoryEnum::kBacklight,
      crosapi::mojom::ProbeCategoryEnum::kFan,
      crosapi::mojom::ProbeCategoryEnum::kStatefulPartition,
      crosapi::mojom::ProbeCategoryEnum::kBluetooth,
      crosapi::mojom::ProbeCategoryEnum::kSystem,
      crosapi::mojom::ProbeCategoryEnum::kNetwork,
      crosapi::mojom::ProbeCategoryEnum::kTpm};
  EXPECT_THAT(
      ConvertCategoryVector(kInput),
      ElementsAre(
          cros_healthd::mojom::ProbeCategoryEnum::kUnknown,
          cros_healthd::mojom::ProbeCategoryEnum::kBattery,
          cros_healthd::mojom::ProbeCategoryEnum::kNonRemovableBlockDevices,
          cros_healthd::mojom::ProbeCategoryEnum::kSystem,
          cros_healthd::mojom::ProbeCategoryEnum::kCpu,
          cros_healthd::mojom::ProbeCategoryEnum::kTimezone,
          cros_healthd::mojom::ProbeCategoryEnum::kMemory,
          cros_healthd::mojom::ProbeCategoryEnum::kBacklight,
          cros_healthd::mojom::ProbeCategoryEnum::kFan,
          cros_healthd::mojom::ProbeCategoryEnum::kStatefulPartition,
          cros_healthd::mojom::ProbeCategoryEnum::kBluetooth,
          cros_healthd::mojom::ProbeCategoryEnum::kSystem,
          cros_healthd::mojom::ProbeCategoryEnum::kNetwork,
          cros_healthd::mojom::ProbeCategoryEnum::kTpm));
}

TEST(ProbeServiceConverters, ErrorType) {
  EXPECT_EQ(Convert(cros_healthd::mojom::ErrorType::kUnknown),
            crosapi::mojom::ProbeErrorType::kUnknown);

  EXPECT_EQ(Convert(cros_healthd::mojom::ErrorType::kFileReadError),
            crosapi::mojom::ProbeErrorType::kFileReadError);

  EXPECT_EQ(Convert(cros_healthd::mojom::ErrorType::kParseError),
            crosapi::mojom::ProbeErrorType::kParseError);

  EXPECT_EQ(Convert(cros_healthd::mojom::ErrorType::kSystemUtilityError),
            crosapi::mojom::ProbeErrorType::kSystemUtilityError);

  EXPECT_EQ(Convert(cros_healthd::mojom::ErrorType::kServiceUnavailable),
            crosapi::mojom::ProbeErrorType::kServiceUnavailable);
}

TEST(ProbeServiceConverters, ProbeErrorPtr) {
  constexpr char kMsg[] = "file not found";
  EXPECT_EQ(ConvertProbePtr(cros_healthd::mojom::ProbeError::New(
                cros_healthd::mojom::ErrorType::kFileReadError, kMsg)),
            crosapi::mojom::ProbeError::New(
                crosapi::mojom::ProbeErrorType::kFileReadError, kMsg));
}

TEST(ProbeServiceConverters, BoolValue) {
  EXPECT_EQ(Convert(false), crosapi::mojom::BoolValue::New(false));
  EXPECT_EQ(Convert(true), crosapi::mojom::BoolValue::New(true));
}

TEST(ProbeServiceConverters, DoubleValue) {
  constexpr double kValue = 100500111111.500100;
  EXPECT_EQ(Convert(kValue), crosapi::mojom::DoubleValue::New(kValue));
}

TEST(ProbeServiceConverters, Int64Value) {
  constexpr int64_t kValue = -(1LL << 62) + 1000;
  EXPECT_EQ(Convert(kValue), crosapi::mojom::Int64Value::New(kValue));
}

TEST(ProbeServiceConverters, UInt64Value) {
  constexpr uint64_t kValue = (1ULL << 63) + 1000000000;
  EXPECT_EQ(Convert(kValue), crosapi::mojom::UInt64Value::New(kValue));
}

TEST(ProbeServiceConverters, UInt64ValuePtr) {
  constexpr uint64_t kValue = (1ULL << 63) + 3000000000;
  EXPECT_EQ(ConvertProbePtr(cros_healthd::mojom::NullableUint64::New(kValue)),
            crosapi::mojom::UInt64Value::New(kValue));
}

TEST(ProbeServiceConverters, BatteryInfoPtr) {
  constexpr int64_t kCycleCount = (1LL << 62) + 45;
  constexpr double kVoltageNow = 1000000000000.2;
  constexpr char kVendor[] = "Google";
  constexpr char kSerialNumber[] = "ABCDEF123456";
  constexpr double kChargeFullDesign = 10000000000.3;
  constexpr double kChargeFull = 99999999999999.0;
  constexpr double kVoltageMinDesign = 41111111111111.1;
  constexpr char kModelName[] = "Google Battery";
  constexpr double kChargeNow = 200000000000000.1;
  constexpr double kCurrentNow = 1555555555555.2;
  constexpr char kTechnology[] = "FastCharge";
  constexpr char kStatus[] = "Charging";
  constexpr char kManufactureDate[] = "2018-10-01";
  constexpr uint64_t kTemperature = (1ULL << 63) + 9000;

  auto input = cros_healthd::mojom::BatteryInfo::New();
  {
    input->cycle_count = kCycleCount;
    input->voltage_now = kVoltageNow;
    input->vendor = kVendor;
    input->serial_number = kSerialNumber;
    input->charge_full_design = kChargeFullDesign;
    input->charge_full = kChargeFull;
    input->voltage_min_design = kVoltageMinDesign;
    input->model_name = kModelName;
    input->charge_now = kChargeNow;
    input->current_now = kCurrentNow;
    input->technology = kTechnology;
    input->status = kStatus;
    input->manufacture_date = kManufactureDate;
    input->temperature = cros_healthd::mojom::NullableUint64::New(kTemperature);
  }

  EXPECT_EQ(
      ConvertProbePtr(std::move(input)),
      crosapi::mojom::ProbeBatteryInfo::New(
          crosapi::mojom::Int64Value::New(kCycleCount),
          crosapi::mojom::DoubleValue::New(kVoltageNow), kVendor, kSerialNumber,
          crosapi::mojom::DoubleValue::New(kChargeFullDesign),
          crosapi::mojom::DoubleValue::New(kChargeFull),
          crosapi::mojom::DoubleValue::New(kVoltageMinDesign), kModelName,
          crosapi::mojom::DoubleValue::New(kChargeNow),
          crosapi::mojom::DoubleValue::New(kCurrentNow), kTechnology, kStatus,
          kManufactureDate, crosapi::mojom::UInt64Value::New(kTemperature)));
}

TEST(ProbeServiceConverters, BatteryResultPtrInfo) {
  const auto output = ConvertProbePtr(
      cros_healthd::mojom::BatteryResult::NewBatteryInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_battery_info());
}

TEST(ProbeServiceConverters, BatteryResultPtrError) {
  const auto output =
      ConvertProbePtr(cros_healthd::mojom::BatteryResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConverters, NonRemovableBlockDeviceInfoPtr) {
  constexpr char kPath[] = "/dev/device1";
  constexpr uint64_t kSize = (1ULL << 63) + 111;
  constexpr char kType[] = "NVMe";
  constexpr uint8_t kManufacturerId = 200;
  constexpr char kName[] = "goog";
  constexpr uint32_t kSerial = 4287654321;
  constexpr char kSerialString[] = "4287654321";
  constexpr uint64_t kBytesReadSinceLastBoot = (1ULL << 62) + 222;
  constexpr uint64_t kBytesWrittenSinceLastBoot = (1ULL << 61) + 333;
  constexpr uint64_t kReadTimeSecondsSinceLastBoot = (1ULL << 60) + 444;
  constexpr uint64_t kWriteTimeSecondsSinceLastBoot = (1ULL << 59) + 555;
  constexpr uint64_t kIoTimeSecondsSinceLastBoot = (1ULL << 58) + 666;
  constexpr uint64_t kDiscardTimeSecondsSinceLastBoot = (1ULL << 57) + 777;

  auto input = cros_healthd::mojom::NonRemovableBlockDeviceInfo::New();
  {
    input->path = kPath;
    input->size = kSize;
    input->type = kType;
    input->manufacturer_id = kManufacturerId;
    input->name = kName;
    input->serial = kSerial;
    input->bytes_read_since_last_boot = kBytesReadSinceLastBoot;
    input->bytes_written_since_last_boot = kBytesWrittenSinceLastBoot;
    input->read_time_seconds_since_last_boot = kReadTimeSecondsSinceLastBoot;
    input->write_time_seconds_since_last_boot = kWriteTimeSecondsSinceLastBoot;
    input->io_time_seconds_since_last_boot = kIoTimeSecondsSinceLastBoot;
    input->discard_time_seconds_since_last_boot =
        cros_healthd::mojom::NullableUint64::New(
            kDiscardTimeSecondsSinceLastBoot);
  }

  EXPECT_EQ(
      ConvertProbePtr(std::move(input)),
      crosapi::mojom::ProbeNonRemovableBlockDeviceInfo::New(
          kPath, crosapi::mojom::UInt64Value::New(kSize), kType,
          crosapi::mojom::UInt32Value::New(kManufacturerId), kName,
          kSerialString,
          crosapi::mojom::UInt64Value::New(kBytesReadSinceLastBoot),
          crosapi::mojom::UInt64Value::New(kBytesWrittenSinceLastBoot),
          crosapi::mojom::UInt64Value::New(kReadTimeSecondsSinceLastBoot),
          crosapi::mojom::UInt64Value::New(kWriteTimeSecondsSinceLastBoot),
          crosapi::mojom::UInt64Value::New(kIoTimeSecondsSinceLastBoot),
          crosapi::mojom::UInt64Value::New(kDiscardTimeSecondsSinceLastBoot)));
}

TEST(ProbeServiceConverters, NonRemovableBlockDeviceResultPtrInfo) {
  constexpr char kPath1[] = "Path1";
  constexpr char kPath2[] = "Path2";

  std::vector<cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr> infos;
  {
    auto info1 = cros_healthd::mojom::NonRemovableBlockDeviceInfo::New();
    info1->path = kPath1;

    auto info2 = cros_healthd::mojom::NonRemovableBlockDeviceInfo::New();
    info2->path = kPath2;

    infos.push_back(std::move(info1));
    infos.push_back(std::move(info2));
  }

  const auto output = ConvertProbePtr(
      cros_healthd::mojom::NonRemovableBlockDeviceResult::NewBlockDeviceInfo(
          std::move(infos)));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_block_device_info());
  ASSERT_EQ(output->get_block_device_info().size(), 2ULL);
  EXPECT_EQ(output->get_block_device_info()[0]->path, kPath1);
  EXPECT_EQ(output->get_block_device_info()[1]->path, kPath2);
}

TEST(ProbeServiceConverters, NonRemovableBlockDeviceResultPtrError) {
  const crosapi::mojom::ProbeNonRemovableBlockDeviceResultPtr output =
      ConvertProbePtr(
          cros_healthd::mojom::NonRemovableBlockDeviceResult::NewError(
              nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConverters, CachedVpdInfoPtr) {
  constexpr char kFirstPowerDate[] = "2021-43";
  constexpr char kSkuNumber[] = "sku-1";
  constexpr char kSerialNumber[] = "5CD9132880";
  constexpr char kModelName[] = "XX ModelName 007 XY";

  auto input = cros_healthd::mojom::VpdInfo::New();
  input->activate_date = kFirstPowerDate;
  input->sku_number = kSkuNumber;
  input->serial_number = kSerialNumber;
  input->model_name = kModelName;

  EXPECT_EQ(ConvertProbePtr(std::move(input)),
            crosapi::mojom::ProbeCachedVpdInfo::New(kFirstPowerDate, kSkuNumber,
                                                    kSerialNumber, kModelName));
}

TEST(ProbeServiceConverters, CpuCStateInfoPtr) {
  constexpr char kName[] = "C0";
  constexpr uint64_t kTimeInStateSinceLastBootUs = 123456;

  auto input = cros_healthd::mojom::CpuCStateInfo::New();
  {
    input->name = kName;
    input->time_in_state_since_last_boot_us = kTimeInStateSinceLastBootUs;
  }

  EXPECT_EQ(ConvertProbePtr(std::move(input)),
            crosapi::mojom::ProbeCpuCStateInfo::New(
                kName,
                crosapi::mojom::UInt64Value::New(kTimeInStateSinceLastBootUs)));
}

TEST(ProbeServiceConverters, LogicalCpuInfoPtr) {
  constexpr uint32_t kMaxClockSpeedKhz = (1 << 31) + 10000;
  constexpr uint32_t kScalingMaxFrequencyKhz = (1 << 30) + 20000;
  constexpr uint32_t kScalingCurrentFrequencyKhz = (1 << 29) + 30000;

  // Idle time cannot be tested with ConvertPtr, because it requires
  // USER_HZ system constant to convert idle_time_user_hz to milliseconds.
  constexpr uint32_t kIdleTime = 0;

  constexpr char kCpuCStateName[] = "C1";
  constexpr uint64_t kCpuCStateTime = (1 << 27) + 50000;

  auto input = cros_healthd::mojom::LogicalCpuInfo::New();
  {
    auto c_state = cros_healthd::mojom::CpuCStateInfo::New();
    c_state->name = kCpuCStateName;
    c_state->time_in_state_since_last_boot_us = kCpuCStateTime;

    input->max_clock_speed_khz = kMaxClockSpeedKhz;
    input->scaling_max_frequency_khz = kScalingMaxFrequencyKhz;
    input->scaling_current_frequency_khz = kScalingCurrentFrequencyKhz;
    input->idle_time_user_hz = kIdleTime;
    input->c_states.push_back(std::move(c_state));
  }

  std::vector<crosapi::mojom::ProbeCpuCStateInfoPtr> expected_c_states;
  expected_c_states.push_back(crosapi::mojom::ProbeCpuCStateInfo::New(
      kCpuCStateName, crosapi::mojom::UInt64Value::New(kCpuCStateTime)));

  EXPECT_EQ(ConvertProbePtr(std::move(input)),
            crosapi::mojom::ProbeLogicalCpuInfo::New(
                crosapi::mojom::UInt32Value::New(kMaxClockSpeedKhz),
                crosapi::mojom::UInt32Value::New(kScalingMaxFrequencyKhz),
                crosapi::mojom::UInt32Value::New(kScalingCurrentFrequencyKhz),
                crosapi::mojom::UInt64Value::New(kIdleTime),
                std::move(expected_c_states)));
}

TEST(ProbeServiceConverters, LogicalCpuInfoPtrNonZeroIdleTime) {
  constexpr uint64_t kUserHz = 100;
  constexpr uint32_t kIdleTimeUserHz = 4291234295;
  constexpr uint64_t kIdleTimeMs = 42912342950;

  auto input = cros_healthd::mojom::LogicalCpuInfo::New();
  input->idle_time_user_hz = kIdleTimeUserHz;

  const auto output = unchecked::UncheckedConvertPtr(std::move(input), kUserHz);
  ASSERT_TRUE(output);
  EXPECT_EQ(output->idle_time_ms,
            crosapi::mojom::UInt64Value::New(kIdleTimeMs));
}

TEST(ProbeServiceConverters, PhysicalCpuInfoPtr) {
  constexpr char kModelName[] = "i9";

  constexpr uint32_t kMaxClockSpeedKhz = (1 << 31) + 11111;
  constexpr uint32_t kScalingMaxFrequencyKhz = (1 << 30) + 22222;
  constexpr uint32_t kScalingCurrentFrequencyKhz = (1 << 29) + 33333;

  // Idle time cannot be tested with ConvertPtr, because it requires
  // USER_HZ system constant to convert idle_time_user_hz to milliseconds.
  constexpr uint32_t kIdleTime = 0;

  auto input = cros_healthd::mojom::PhysicalCpuInfo::New();
  {
    auto logical_info = cros_healthd::mojom::LogicalCpuInfo::New();
    logical_info->max_clock_speed_khz = kMaxClockSpeedKhz;
    logical_info->scaling_max_frequency_khz = kScalingMaxFrequencyKhz;
    logical_info->scaling_current_frequency_khz = kScalingCurrentFrequencyKhz;
    logical_info->idle_time_user_hz = kIdleTime;

    input->model_name = kModelName;
    input->logical_cpus.push_back(std::move(logical_info));
  }

  std::vector<crosapi::mojom::ProbeLogicalCpuInfoPtr> expected_infos;
  expected_infos.push_back(crosapi::mojom::ProbeLogicalCpuInfo::New(
      crosapi::mojom::UInt32Value::New(kMaxClockSpeedKhz),
      crosapi::mojom::UInt32Value::New(kScalingMaxFrequencyKhz),
      crosapi::mojom::UInt32Value::New(kScalingCurrentFrequencyKhz),
      crosapi::mojom::UInt64Value::New(kIdleTime),
      std::vector<crosapi::mojom::ProbeCpuCStateInfoPtr>{}));

  EXPECT_EQ(ConvertProbePtr(std::move(input)),
            crosapi::mojom::ProbePhysicalCpuInfo::New(
                kModelName, std::move(expected_infos)));
}

TEST(ProbeServiceConverters, CpuArchitectureEnum) {
  EXPECT_EQ(Convert(cros_healthd::mojom::CpuArchitectureEnum::kUnknown),
            crosapi::mojom::ProbeCpuArchitectureEnum::kUnknown);
  EXPECT_EQ(Convert(cros_healthd::mojom::CpuArchitectureEnum::kX86_64),
            crosapi::mojom::ProbeCpuArchitectureEnum::kX86_64);
  EXPECT_EQ(Convert(cros_healthd::mojom::CpuArchitectureEnum::kAArch64),
            crosapi::mojom::ProbeCpuArchitectureEnum::kAArch64);
  EXPECT_EQ(Convert(cros_healthd::mojom::CpuArchitectureEnum::kArmv7l),
            crosapi::mojom::ProbeCpuArchitectureEnum::kArmv7l);
}

TEST(ProbeServiceConverters, CpuInfoPtr) {
  constexpr uint32_t kNumTotalThreads = (1 << 31) + 111;
  constexpr char kModelName[] = "i9";

  auto input = cros_healthd::mojom::CpuInfo::New();
  {
    auto physical_info = cros_healthd::mojom::PhysicalCpuInfo::New();
    physical_info->model_name = kModelName;

    input->num_total_threads = kNumTotalThreads;
    input->architecture = cros_healthd::mojom::CpuArchitectureEnum::kArmv7l;
    input->physical_cpus.push_back(std::move(physical_info));
  }

  std::vector<crosapi::mojom::ProbePhysicalCpuInfoPtr> expected_infos;
  expected_infos.push_back(crosapi::mojom::ProbePhysicalCpuInfo::New(
      kModelName, std::vector<crosapi::mojom::ProbeLogicalCpuInfoPtr>{}));

  EXPECT_EQ(ConvertProbePtr(std::move(input)),
            crosapi::mojom::ProbeCpuInfo::New(
                crosapi::mojom::UInt32Value::New(kNumTotalThreads),
                crosapi::mojom::ProbeCpuArchitectureEnum::kArmv7l,
                std::move(expected_infos)));
}

TEST(ProbeServiceConverters, CpuResultPtrInfo) {
  const auto output =
      ConvertProbePtr(cros_healthd::mojom::CpuResult::NewCpuInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_cpu_info());
}

TEST(ProbeServiceConverters, CpuResultPtrError) {
  const auto output =
      ConvertProbePtr(cros_healthd::mojom::CpuResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConverters, TimezoneInfoPtr) {
  constexpr char kPosix[] = "TZ=CST6CDT,M3.2.0/2:00:00,M11.1.0/2:00:00";
  constexpr char kRegion[] = "Europe/Berlin";

  auto input = cros_healthd::mojom::TimezoneInfo::New();
  input->posix = kPosix;
  input->region = kRegion;

  EXPECT_EQ(ConvertProbePtr(std::move(input)),
            crosapi::mojom::ProbeTimezoneInfo::New(kPosix, kRegion));
}

TEST(ProbeServiceConverters, TimezoneResultPtrInfo) {
  const auto output = ConvertProbePtr(
      cros_healthd::mojom::TimezoneResult::NewTimezoneInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_timezone_info());
}

TEST(ProbeServiceConverters, TimezoneResultPtrError) {
  const auto output =
      ConvertProbePtr(cros_healthd::mojom::TimezoneResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConverters, MemoryInfoPtr) {
  constexpr uint32_t kTotalMemoryKib = (1 << 31) + 100;
  constexpr uint32_t kFreeMemoryKib = (1 << 30) + 200;
  constexpr uint32_t kAvailableMemoryKib = (1 << 29) + 300;
  constexpr uint32_t kPageFaultsSinceLastBoot = (1 << 28) + 400;

  auto input = cros_healthd::mojom::MemoryInfo::New();
  input->total_memory_kib = kTotalMemoryKib;
  input->free_memory_kib = kFreeMemoryKib;
  input->available_memory_kib = kAvailableMemoryKib;
  input->page_faults_since_last_boot = kPageFaultsSinceLastBoot;

  EXPECT_EQ(ConvertProbePtr(std::move(input)),
            crosapi::mojom::ProbeMemoryInfo::New(
                crosapi::mojom::UInt32Value::New(kTotalMemoryKib),
                crosapi::mojom::UInt32Value::New(kFreeMemoryKib),
                crosapi::mojom::UInt32Value::New(kAvailableMemoryKib),
                crosapi::mojom::UInt64Value::New(kPageFaultsSinceLastBoot)));
}

TEST(ProbeServiceConverters, MemoryResultPtrInfo) {
  const crosapi::mojom::ProbeMemoryResultPtr output = ConvertProbePtr(
      cros_healthd::mojom::MemoryResult::NewMemoryInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_memory_info());
}

TEST(ProbeServiceConverters, MemoryResultPtrError) {
  const crosapi::mojom::ProbeMemoryResultPtr output =
      ConvertProbePtr(cros_healthd::mojom::MemoryResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConverters, BacklightInfoPtr) {
  constexpr char kPath[] = "/sys/backlight";
  constexpr uint32_t kMaxBrightness = (1 << 31) + 31;
  constexpr uint32_t kBrightness = (1 << 30) + 30;

  auto input = cros_healthd::mojom::BacklightInfo::New();
  input->path = kPath;
  input->max_brightness = kMaxBrightness;
  input->brightness = kBrightness;

  EXPECT_EQ(ConvertProbePtr(std::move(input)),
            crosapi::mojom::ProbeBacklightInfo::New(
                kPath, crosapi::mojom::UInt32Value::New(kMaxBrightness),
                crosapi::mojom::UInt32Value::New(kBrightness)));
}

TEST(ProbeServiceConverters, BacklightResultPtrInfo) {
  constexpr char kPath[] = "/sys/backlight";

  cros_healthd::mojom::BacklightResultPtr input;
  {
    auto backlight_info = cros_healthd::mojom::BacklightInfo::New();
    backlight_info->path = kPath;

    std::vector<cros_healthd::mojom::BacklightInfoPtr> backlight_infos;
    backlight_infos.push_back(std::move(backlight_info));

    input = cros_healthd::mojom::BacklightResult::NewBacklightInfo(
        std::move(backlight_infos));
  }

  const auto output = ConvertProbePtr(std::move(input));
  ASSERT_TRUE(output);
  ASSERT_TRUE(output->is_backlight_info());

  const auto& backlight_info_output = output->get_backlight_info();
  ASSERT_EQ(backlight_info_output.size(), 1ULL);
  ASSERT_TRUE(backlight_info_output[0]);
  EXPECT_EQ(backlight_info_output[0]->path, kPath);
}

TEST(ProbeServiceConverters, BacklightResultPtrError) {
  const auto output =
      ConvertProbePtr(cros_healthd::mojom::BacklightResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConverters, FanInfoPtr) {
  constexpr uint32_t kSpeedRpm = (1 << 31) + 777;

  auto input = cros_healthd::mojom::FanInfo::New();
  input->speed_rpm = kSpeedRpm;

  const auto output = ConvertProbePtr(std::move(input));
  ASSERT_TRUE(output);
  EXPECT_EQ(output->speed_rpm, crosapi::mojom::UInt32Value::New(kSpeedRpm));
}

TEST(ProbeServiceConverters, FanResultPtrInfo) {
  constexpr uint32_t kSpeedRpm = (1 << 31) + 10;

  cros_healthd::mojom::FanResultPtr input;
  {
    auto fan_info = cros_healthd::mojom::FanInfo::New();
    fan_info->speed_rpm = kSpeedRpm;

    std::vector<cros_healthd::mojom::FanInfoPtr> fan_infos;
    fan_infos.push_back(std::move(fan_info));

    input = cros_healthd::mojom::FanResult::NewFanInfo(std::move(fan_infos));
  }

  std::vector<crosapi::mojom::ProbeFanInfoPtr> expected_fans;
  expected_fans.push_back(crosapi::mojom::ProbeFanInfo::New(
      crosapi::mojom::UInt32Value::New(kSpeedRpm)));

  EXPECT_EQ(
      ConvertProbePtr(std::move(input)),
      crosapi::mojom::ProbeFanResult::NewFanInfo(std::move(expected_fans)));
}

TEST(ProbeServiceConverters, FanResultPtrError) {
  const auto output =
      ConvertProbePtr(cros_healthd::mojom::FanResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConverters, StatefulPartitionInfoPtr) {
  constexpr uint64_t k100MiB = 100 * 1024 * 1024;
  constexpr uint64_t kTotalSpace = 9000000 * k100MiB + 17;
  constexpr uint64_t kRoundedAvailableSpace = 800000 * k100MiB;
  constexpr uint64_t kAvailableSpace = kRoundedAvailableSpace + k100MiB - 2000;

  auto input = cros_healthd::mojom::StatefulPartitionInfo::New();
  input->available_space = kAvailableSpace;
  input->total_space = kTotalSpace;

  EXPECT_EQ(ConvertProbePtr(std::move(input)),
            crosapi::mojom::ProbeStatefulPartitionInfo::New(
                crosapi::mojom::UInt64Value::New(kRoundedAvailableSpace),
                crosapi::mojom::UInt64Value::New(kTotalSpace)));
}

TEST(ProbeServiceConverters, ProbeTpmGSCVersion) {
  EXPECT_EQ(Convert(cros_healthd::mojom::TpmGSCVersion::kNotGSC),
            crosapi::mojom::ProbeTpmGSCVersion::kNotGSC);
  EXPECT_EQ(Convert(cros_healthd::mojom::TpmGSCVersion::kCr50),
            crosapi::mojom::ProbeTpmGSCVersion::kCr50);
  EXPECT_EQ(Convert(cros_healthd::mojom::TpmGSCVersion::kTi50),
            crosapi::mojom::ProbeTpmGSCVersion::kTi50);
}

TEST(ProbeServiceConverters, ProbeTpmVersionPtr) {
  constexpr uint32_t kFamily = 0x322e3000;
  constexpr uint64_t kSpecLevel = 1000;
  constexpr uint32_t kManufacturer = 42;
  constexpr uint32_t kTpmModel = 101;
  constexpr uint64_t kFirmwareVersion = 1001;
  constexpr char kVendorSpecific[] = "info";

  auto input = cros_healthd::mojom::TpmVersion::New();
  input->gsc_version = cros_healthd::mojom::TpmGSCVersion::kCr50;
  input->family = kFamily;
  input->spec_level = kSpecLevel;
  input->manufacturer = kManufacturer;
  input->tpm_model = kTpmModel;
  input->firmware_version = kFirmwareVersion;
  input->vendor_specific = kVendorSpecific;

  auto result = ConvertProbePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_EQ(crosapi::mojom::ProbeTpmGSCVersion::kCr50, result->gsc_version);
  EXPECT_EQ(crosapi::mojom::UInt32Value::New(kFamily), result->family);
  EXPECT_EQ(crosapi::mojom::UInt64Value::New(kSpecLevel), result->spec_level);
  EXPECT_EQ(crosapi::mojom::UInt32Value::New(kManufacturer),
            result->manufacturer);
  EXPECT_EQ(crosapi::mojom::UInt32Value::New(kTpmModel), result->tpm_model);
  EXPECT_EQ(crosapi::mojom::UInt64Value::New(kFirmwareVersion),
            result->firmware_version);

  ASSERT_TRUE(result->vendor_specific.has_value());
  EXPECT_EQ(kVendorSpecific, result->vendor_specific.value());
}

TEST(ProbeServiceConverters, ProbeTpmStatusPtr) {
  constexpr bool kEnabled = true;
  constexpr bool kOwned = false;
  constexpr bool kOwnerPasswortIsPresent = false;

  auto input = cros_healthd::mojom::TpmStatus::New();
  input->enabled = kEnabled;
  input->owned = kOwned;
  input->owner_password_is_present = kOwnerPasswortIsPresent;

  auto result = ConvertProbePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_EQ(crosapi::mojom::BoolValue::New(kEnabled), result->enabled);
  EXPECT_EQ(crosapi::mojom::BoolValue::New(kOwned), result->owned);
  EXPECT_EQ(crosapi::mojom::BoolValue::New(kOwnerPasswortIsPresent),
            result->owner_password_is_present);
}

TEST(ProbeServiceConverters, ProbeTpmDictionaryAttackPtr) {
  constexpr uint32_t kCounter = 42;
  constexpr uint32_t kThreshold = 100;
  constexpr bool kLockOutInEffect = true;
  constexpr uint32_t kLockoutSecondsRemaining = 5;

  auto input = cros_healthd::mojom::TpmDictionaryAttack::New();
  input->counter = kCounter;
  input->threshold = kThreshold;
  input->lockout_in_effect = kLockOutInEffect;
  input->lockout_seconds_remaining = kLockoutSecondsRemaining;

  auto result = ConvertProbePtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_EQ(crosapi::mojom::UInt32Value::New(kCounter), result->counter);
  EXPECT_EQ(crosapi::mojom::UInt32Value::New(kThreshold), result->threshold);
  EXPECT_EQ(crosapi::mojom::BoolValue::New(kLockOutInEffect),
            result->lockout_in_effect);
  EXPECT_EQ(crosapi::mojom::UInt32Value::New(kLockoutSecondsRemaining),
            result->lockout_seconds_remaining);
}

TEST(ProbeServiceConverters, ProbeTpmInfoPtr) {
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

  auto tpm_version = cros_healthd::mojom::TpmVersion::New();
  tpm_version->gsc_version = cros_healthd::mojom::TpmGSCVersion::kCr50;
  tpm_version->family = kFamily;
  tpm_version->spec_level = kSpecLevel;
  tpm_version->manufacturer = kManufacturer;
  tpm_version->tpm_model = kTpmModel;
  tpm_version->firmware_version = kFirmwareVersion;
  tpm_version->vendor_specific = kVendorSpecific;

  auto tpm_status = cros_healthd::mojom::TpmStatus::New();
  tpm_status->enabled = kEnabled;
  tpm_status->owned = kOwned;
  tpm_status->owner_password_is_present = kOwnerPasswortIsPresent;

  auto dictonary_attack = cros_healthd::mojom::TpmDictionaryAttack::New();
  dictonary_attack->counter = kCounter;
  dictonary_attack->threshold = kThreshold;
  dictonary_attack->lockout_in_effect = kLockOutInEffect;
  dictonary_attack->lockout_seconds_remaining = kLockoutSecondsRemaining;

  auto input = cros_healthd::mojom::TpmInfo::New();
  input->version = std::move(tpm_version);
  input->status = std::move(tpm_status);
  input->dictionary_attack = std::move(dictonary_attack);

  auto result = ConvertProbePtr(std::move(input));

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->version);

  auto tpm_version_result = std::move(result->version);
  EXPECT_EQ(crosapi::mojom::ProbeTpmGSCVersion::kCr50,
            tpm_version_result->gsc_version);
  EXPECT_EQ(crosapi::mojom::UInt32Value::New(kFamily),
            tpm_version_result->family);
  EXPECT_EQ(crosapi::mojom::UInt64Value::New(kSpecLevel),
            tpm_version_result->spec_level);
  EXPECT_EQ(crosapi::mojom::UInt32Value::New(kManufacturer),
            tpm_version_result->manufacturer);
  EXPECT_EQ(crosapi::mojom::UInt32Value::New(kTpmModel),
            tpm_version_result->tpm_model);
  EXPECT_EQ(crosapi::mojom::UInt64Value::New(kFirmwareVersion),
            tpm_version_result->firmware_version);
  ASSERT_TRUE(tpm_version_result->vendor_specific.has_value());
  EXPECT_EQ(kVendorSpecific, tpm_version_result->vendor_specific.value());

  ASSERT_TRUE(result->status);
  auto tpm_status_result = std::move(result->status);
  EXPECT_EQ(crosapi::mojom::BoolValue::New(kEnabled),
            tpm_status_result->enabled);
  EXPECT_EQ(crosapi::mojom::BoolValue::New(kOwned), tpm_status_result->owned);
  EXPECT_EQ(crosapi::mojom::BoolValue::New(kOwnerPasswortIsPresent),
            tpm_status_result->owner_password_is_present);

  ASSERT_TRUE(result->dictionary_attack);
  auto dictonary_attack_result = std::move(result->dictionary_attack);
  EXPECT_EQ(crosapi::mojom::UInt32Value::New(kCounter),
            dictonary_attack_result->counter);
  EXPECT_EQ(crosapi::mojom::UInt32Value::New(kThreshold),
            dictonary_attack_result->threshold);
  EXPECT_EQ(crosapi::mojom::BoolValue::New(kLockOutInEffect),
            dictonary_attack_result->lockout_in_effect);
  EXPECT_EQ(crosapi::mojom::UInt32Value::New(kLockoutSecondsRemaining),
            dictonary_attack_result->lockout_seconds_remaining);
}

TEST(ProbeServiceConverters, ProbeTpmResultPtrInfo) {
  const auto output =
      ConvertProbePtr(cros_healthd::mojom::TpmResult::NewTpmInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_tpm_info());
}

TEST(ProbeServiceConverters, ProbeTpmResultPtrError) {
  const auto output =
      ConvertProbePtr(cros_healthd::mojom::CpuResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConverters, StatefulPartitionResultPtrInfo) {
  const auto output = ConvertProbePtr(
      cros_healthd::mojom::StatefulPartitionResult::NewPartitionInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_partition_info());
}

TEST(ProbeServiceConverters, StatefulPartitionResultPtrError) {
  const auto output = ConvertProbePtr(
      cros_healthd::mojom::StatefulPartitionResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConverters, BluetoothAdapterInfoPtr) {
  constexpr char kName[] = "hci0";
  constexpr char kAddress[] = "ab:cd:ef:12:34:56";
  constexpr bool kPowered = true;
  constexpr uint32_t kNumConnectedDevices = (1 << 30) + 1;

  auto input = cros_healthd::mojom::BluetoothAdapterInfo::New();
  {
    input->name = kName;
    input->address = kAddress;
    input->powered = kPowered;
    input->num_connected_devices = kNumConnectedDevices;
  }

  EXPECT_EQ(ConvertProbePtr(std::move(input)),
            crosapi::mojom::ProbeBluetoothAdapterInfo::New(
                kName, kAddress, crosapi::mojom::BoolValue::New(kPowered),
                crosapi::mojom::UInt32Value::New(kNumConnectedDevices)));
}

TEST(ProbeServiceConverters, BluetoothResultPtrInfo) {
  constexpr char kName[] = "hci0";

  cros_healthd::mojom::BluetoothResultPtr input;
  {
    auto info = cros_healthd::mojom::BluetoothAdapterInfo::New();
    info->name = kName;

    std::vector<cros_healthd::mojom::BluetoothAdapterInfoPtr> infos;
    infos.push_back(std::move(info));

    input = cros_healthd::mojom::BluetoothResult::NewBluetoothAdapterInfo(
        std::move(infos));
  }

  const auto output = ConvertProbePtr(std::move(input));
  ASSERT_TRUE(output);
  ASSERT_TRUE(output->is_bluetooth_adapter_info());

  const auto& bluetooth_adapter_info_output =
      output->get_bluetooth_adapter_info();
  ASSERT_EQ(bluetooth_adapter_info_output.size(), 1ULL);
  ASSERT_TRUE(bluetooth_adapter_info_output[0]);
  EXPECT_EQ(bluetooth_adapter_info_output[0]->name, kName);
}

TEST(ProbeServiceConverters, BluetoothResultPtrError) {
  const auto output =
      ConvertProbePtr(cros_healthd::mojom::BluetoothResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConverters, SystemInfoPtr) {
  constexpr char kOemName[] = "OEM-NAME";
  constexpr char kReleaseMilestone[] = "87";
  constexpr char kBuildNumber[] = "13544";
  constexpr char kPatchNumber[] = "59.0";
  constexpr char kReleaseChannel[] = "stable-channel";

  auto os_version = cros_healthd::mojom::OsVersion::New(
      kReleaseMilestone, kBuildNumber, kPatchNumber, kReleaseChannel);
  auto input = cros_healthd::mojom::OsInfo::New();
  input->oem_name = kOemName;
  input->os_version = std::move(os_version);

  EXPECT_EQ(
      ConvertProbePtr(std::move(input)),
      crosapi::mojom::ProbeSystemInfo::New(crosapi::mojom::ProbeOsInfo::New(
          kOemName,
          crosapi::mojom::ProbeOsVersion::New(kReleaseMilestone, kBuildNumber,
                                              kPatchNumber, kReleaseChannel))));
}

TEST(ProbeServiceConverters, OsVersionPtr) {
  constexpr char kReleaseMilestone[] = "87";
  constexpr char kBuildNumber[] = "13544";
  constexpr char kPatchNumber[] = "59.0";
  constexpr char kReleaseChannel[] = "stable-channel";

  auto input = cros_healthd::mojom::OsVersion::New(
      kReleaseMilestone, kBuildNumber, kPatchNumber, kReleaseChannel);

  EXPECT_EQ(ConvertProbePtr(std::move(input)),
            crosapi::mojom::ProbeOsVersion::New(kReleaseMilestone, kBuildNumber,
                                                kPatchNumber, kReleaseChannel));
}

TEST(ProbeServiceConverters, NetworkResultPtrInfo) {
  constexpr uint32_t kSignalStrength = 100;

  cros_healthd::mojom::NetworkResultPtr input;
  {
    auto network = chromeos::network_health::mojom::Network::New();
    network->signal_strength =
        chromeos::network_health::mojom::UInt32Value::New(kSignalStrength);

    std::vector<chromeos::network_health::mojom::NetworkPtr> networks;
    networks.push_back(std::move(network));

    auto info = chromeos::network_health::mojom::NetworkHealthState::New();
    info->networks = std::move(networks);

    input =
        cros_healthd::mojom::NetworkResult::NewNetworkHealth(std::move(info));
  }

  const auto output = ConvertProbePtr(std::move(input));
  ASSERT_TRUE(output);
  ASSERT_TRUE(output->is_network_health());

  const auto& network_health_output = output->get_network_health();
  ASSERT_EQ(network_health_output->networks.size(), 1ULL);
  ASSERT_TRUE(network_health_output->networks[0]);
  EXPECT_EQ(network_health_output->networks[0]->signal_strength,
            chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));
}

TEST(ProbeServiceConverters, NetworkResultPtrError) {
  const auto output =
      ConvertProbePtr(cros_healthd::mojom::NetworkResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConverters, PairCachedVpdInfoPtrSystemInfoPtr) {
  constexpr char kOemName[] = "OEM-NAME";
  constexpr char kReleaseMilestone[] = "87";
  constexpr char kBuildNumber[] = "13544";
  constexpr char kPatchNumber[] = "59.0";
  constexpr char kReleaseChannel[] = "stable-channel";
  constexpr char kFirstPowerDate[] = "2021-43";
  constexpr char kSkuNumber[] = "sku-1";
  constexpr char kSerialNumber[] = "5CD9132880";
  constexpr char kModelName[] = "XX ModelName 007 XY";

  auto input = cros_healthd::mojom::SystemInfo::New();
  {
    auto vpd_info = cros_healthd::mojom::VpdInfo::New();
    vpd_info->activate_date = kFirstPowerDate;
    vpd_info->sku_number = kSkuNumber;
    vpd_info->serial_number = kSerialNumber;
    vpd_info->model_name = kModelName;

    auto os_info = cros_healthd::mojom::OsInfo::New();
    os_info->oem_name = kOemName;
    os_info->os_version = cros_healthd::mojom::OsVersion::New(
        kReleaseMilestone, kBuildNumber, kPatchNumber, kReleaseChannel);

    input->os_info = std::move(os_info);
    input->vpd_info = std::move(vpd_info);
  }
  EXPECT_EQ(
      ConvertProbePairPtr(std::move(input)),
      std::make_pair(
          crosapi::mojom::ProbeCachedVpdInfo::New(kFirstPowerDate, kSkuNumber,
                                                  kSerialNumber, kModelName),
          crosapi::mojom::ProbeSystemInfo::New(crosapi::mojom::ProbeOsInfo::New(
              kOemName, crosapi::mojom::ProbeOsVersion::New(
                            kReleaseMilestone, kBuildNumber, kPatchNumber,
                            kReleaseChannel)))));
}

TEST(ProbeServiceConverters, PairCachedVpdResultPtrSystemResultPtrInfo) {
  constexpr char kOemName[] = "OEM-NAME";
  constexpr char kReleaseMilestone[] = "87";
  constexpr char kBuildNumber[] = "13544";
  constexpr char kPatchNumber[] = "59.0";
  constexpr char kReleaseChannel[] = "stable-channel";
  constexpr char kFirstPowerDate[] = "2021-43";
  constexpr char kSkuNumber[] = "sku-1";
  constexpr char kSerialNumber[] = "5CD9132880";
  constexpr char kModelName[] = "XX ModelName 007 XY";

  cros_healthd::mojom::SystemResultPtr input;
  {
    auto vpd_info = cros_healthd::mojom::VpdInfo::New();
    vpd_info->activate_date = kFirstPowerDate;
    vpd_info->sku_number = kSkuNumber;
    vpd_info->serial_number = kSerialNumber;
    vpd_info->model_name = kModelName;

    auto os_info = cros_healthd::mojom::OsInfo::New();
    os_info->oem_name = kOemName;
    os_info->os_version = cros_healthd::mojom::OsVersion::New(
        kReleaseMilestone, kBuildNumber, kPatchNumber, kReleaseChannel);

    auto system_info = cros_healthd::mojom::SystemInfo::New();
    system_info->os_info = std::move(os_info);
    system_info->vpd_info = std::move(vpd_info);

    input = cros_healthd::mojom::SystemResult::NewSystemInfo(
        std::move(system_info));
  }
  const auto output = ConvertProbePairPtr(std::move(input));
  ASSERT_TRUE(output.first);
  ASSERT_TRUE(output.first->is_vpd_info());
  ASSERT_TRUE(output.second);
  ASSERT_TRUE(output.second->is_system_info());
}

TEST(ProbeServiceConverters, PairCachedVpdResultPtrSystemResultPtrError) {
  const auto output =
      ConvertProbePairPtr(cros_healthd::mojom::SystemResult::NewError(nullptr));
  ASSERT_TRUE(output.first);
  ASSERT_TRUE(output.first->is_error());
  ASSERT_TRUE(output.second);
  ASSERT_TRUE(output.second->is_error());
}

TEST(ProbeServiceConverters, TelemetryInfoPtrWithNotNullFields) {
  auto input = cros_healthd::mojom::TelemetryInfo::New();
  {
    input->battery_result = cros_healthd::mojom::BatteryResult::NewBatteryInfo(
        cros_healthd::mojom::BatteryInfo::New());
    input->block_device_result =
        cros_healthd::mojom::NonRemovableBlockDeviceResult::NewBlockDeviceInfo(
            {});
    input->cpu_result = cros_healthd::mojom::CpuResult::NewCpuInfo(
        cros_healthd::mojom::CpuInfo::New());
    input->timezone_result =
        cros_healthd::mojom::TimezoneResult::NewTimezoneInfo(
            cros_healthd::mojom::TimezoneInfo::New());
    input->memory_result = cros_healthd::mojom::MemoryResult::NewMemoryInfo(
        cros_healthd::mojom::MemoryInfo::New());
    input->backlight_result =
        cros_healthd::mojom::BacklightResult::NewBacklightInfo({});
    input->fan_result = cros_healthd::mojom::FanResult::NewFanInfo({});
    input->stateful_partition_result =
        cros_healthd::mojom::StatefulPartitionResult::NewPartitionInfo(
            cros_healthd::mojom::StatefulPartitionInfo::New());
    input->bluetooth_result =
        cros_healthd::mojom::BluetoothResult::NewBluetoothAdapterInfo({});
    input->system_result = cros_healthd::mojom::SystemResult::NewSystemInfo(
        cros_healthd::mojom::SystemInfo::New(
            cros_healthd::mojom::OsInfo::New(),
            cros_healthd::mojom::VpdInfo::New(),
            cros_healthd::mojom::DmiInfo::New()));
    input->network_result =
        cros_healthd::mojom::NetworkResult::NewNetworkHealth(
            chromeos::network_health::mojom::NetworkHealthState::New());
    input->tpm_result = cros_healthd::mojom::TpmResult::NewTpmInfo(
        cros_healthd::mojom::TpmInfo::New());
  }

  EXPECT_EQ(
      ConvertProbePtr(std::move(input)),
      crosapi::mojom::ProbeTelemetryInfo::New(
          crosapi::mojom::ProbeBatteryResult::NewBatteryInfo(
              crosapi::mojom::ProbeBatteryInfo::New(
                  crosapi::mojom::Int64Value::New(0),
                  crosapi::mojom::DoubleValue::New(0.), "", "",
                  crosapi::mojom::DoubleValue::New(0.),
                  crosapi::mojom::DoubleValue::New(0.),
                  crosapi::mojom::DoubleValue::New(0.), "",
                  crosapi::mojom::DoubleValue::New(0.),
                  crosapi::mojom::DoubleValue::New(0.), "", "", absl::nullopt,
                  nullptr)),
          crosapi::mojom::ProbeNonRemovableBlockDeviceResult::
              NewBlockDeviceInfo({}),
          crosapi::mojom::ProbeCachedVpdResult::NewVpdInfo(
              crosapi::mojom::ProbeCachedVpdInfo::New()),
          crosapi::mojom::ProbeCpuResult::NewCpuInfo(
              crosapi::mojom::ProbeCpuInfo::New(
                  crosapi::mojom::UInt32Value::New(0),
                  crosapi::mojom::ProbeCpuArchitectureEnum::kUnknown,
                  std::vector<crosapi::mojom::ProbePhysicalCpuInfoPtr>())),
          crosapi::mojom::ProbeTimezoneResult::NewTimezoneInfo(
              crosapi::mojom::ProbeTimezoneInfo::New("", "")),
          crosapi::mojom::ProbeMemoryResult::NewMemoryInfo(
              crosapi::mojom::ProbeMemoryInfo::New(
                  crosapi::mojom::UInt32Value::New(0),
                  crosapi::mojom::UInt32Value::New(0),
                  crosapi::mojom::UInt32Value::New(0),
                  crosapi::mojom::UInt64Value::New(0))),
          crosapi::mojom::ProbeBacklightResult::NewBacklightInfo({}),
          crosapi::mojom::ProbeFanResult::NewFanInfo({}),
          crosapi::mojom::ProbeStatefulPartitionResult::NewPartitionInfo(
              crosapi::mojom::ProbeStatefulPartitionInfo::New(
                  crosapi::mojom::UInt64Value::New(0),
                  crosapi::mojom::UInt64Value::New(0))),
          crosapi::mojom::ProbeBluetoothResult::NewBluetoothAdapterInfo({}),
          crosapi::mojom::ProbeSystemResult::NewSystemInfo(
              crosapi::mojom::ProbeSystemInfo::New(
                  crosapi::mojom::ProbeOsInfo::New())),
          crosapi::mojom::ProbeNetworkResult::NewNetworkHealth(
              chromeos::network_health::mojom::NetworkHealthState::New()),
          crosapi::mojom::ProbeTpmResult::NewTpmInfo(
              crosapi::mojom::ProbeTpmInfo::New())));
}

TEST(ProbeServiceConverters, TelemetryInfoPtrWithNullFields) {
  EXPECT_EQ(ConvertProbePtr(cros_healthd::mojom::TelemetryInfo::New()),
            crosapi::mojom::ProbeTelemetryInfo::New(
                crosapi::mojom::ProbeBatteryResultPtr(nullptr),
                crosapi::mojom::ProbeNonRemovableBlockDeviceResultPtr(nullptr),
                crosapi::mojom::ProbeCachedVpdResultPtr(nullptr),
                crosapi::mojom::ProbeCpuResultPtr(nullptr),
                crosapi::mojom::ProbeTimezoneResultPtr(nullptr),
                crosapi::mojom::ProbeMemoryResultPtr(nullptr),
                crosapi::mojom::ProbeBacklightResultPtr(nullptr),
                crosapi::mojom::ProbeFanResultPtr(nullptr),
                crosapi::mojom::ProbeStatefulPartitionResultPtr(nullptr),
                crosapi::mojom::ProbeBluetoothResultPtr(nullptr),
                crosapi::mojom::ProbeSystemResultPtr(nullptr),
                crosapi::mojom::ProbeNetworkResultPtr(nullptr),
                crosapi::mojom::ProbeTpmResultPtr(nullptr)));
}

}  // namespace ash::converters
