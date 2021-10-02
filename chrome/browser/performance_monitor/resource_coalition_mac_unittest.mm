// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/resource_coalition_mac.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/performance_monitor/resource_coalition_internal_types_mac.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_monitor {

namespace {

constexpr mach_timebase_info_data_t kIntelTimebase = {1, 1};

// A sample (not definitive) timebase for M1.
constexpr mach_timebase_info_data_t kM1Timebase = {125, 3};

// Initializes to no EI constants and the Intel timebase.
class TestResourceCoalition : public ResourceCoalition {
 public:
  TestResourceCoalition() {
    SetEnergyImpactCoefficientsForTesting(absl::nullopt);
    SetMachTimebaseForTesting(kIntelTimebase);
  }
  // Expose as public for testing.
  using ResourceCoalition::SetCoalitionIDToCurrentProcessIdForTesting;
  using ResourceCoalition::GetDataRateFromFakeDataForTesting;
  using ResourceCoalition::EnergyImpactCoefficients;
  using ResourceCoalition::ReadEnergyImpactCoefficientsFromPath;
  using ResourceCoalition::ComputeEnergyImpactForCoalitionUsage;
  using ResourceCoalition::ReadEnergyImpactOrDefaultForBoardId;
  using ResourceCoalition::MaybeGetBoardIdForThisMachine;
  using ResourceCoalition::SetEnergyImpactCoefficientsForTesting;
  using ResourceCoalition::SetMachTimebaseForTesting;
};
using EnergyImpactCoefficients =
    TestResourceCoalition::EnergyImpactCoefficients;

EnergyImpactCoefficients GetEnergyImpactTestCoefficients() {
  constexpr EnergyImpactCoefficients coefficients{
      .kcpu_wakeups = base::Microseconds(200).InSecondsF(),
      .kqos_default = 1.0,
      .kqos_background = 0.8,
      .kqos_utility = 1.0,
      .kqos_legacy = 1.0,
      .kqos_user_initiated = 1.0,
      .kqos_user_interactive = 1.0,
      .kgpu_time = 2.5,
  };

  return coefficients;
}

constexpr base::TimeDelta kIntervalLength = base::Seconds(2.5);

TEST(ResourceCoalitionTests, Basics) {
  base::HistogramTester histogram_tester;
  TestResourceCoalition coalition;
  // Tests are usually run from a terminal and so they share their coalition ID
  // with it. This will fail if the tests is started with |launchd| or with
  // |open|.
  histogram_tester.ExpectUniqueSample(
      "PerformanceMonitor.ResourceCoalition.Availability",
      4 /* kNotAloneInCoalition */, 1);
  EXPECT_FALSE(coalition.IsAvailable());

  coalition.SetCoalitionIDToCurrentProcessIdForTesting();
  EXPECT_TRUE(coalition.IsAvailable());

  base::TimeTicks begin = base::TimeTicks::Now();
  constexpr base::TimeDelta busy_time = base::Seconds(1);
  double number = 1;
  while (base::TimeTicks::Now() < (begin + busy_time)) {
    for (int i = 0; i < 10000; ++i) {
      number *= base::RandDouble() / std::numeric_limits<double>::max() * 2;
    }
  }

  auto sample = coalition.GetDataRate();
  EXPECT_TRUE(sample.has_value());
  EXPECT_NE(sample->cpu_time_per_second, 0);
}

constexpr double kExpectedCPUUsagePerSecondPercent = 0.7;
constexpr double kExpectedGPUUsagePerSecondPercent = 0.3;
// Note: The following counters must have an integral value once multiplied by
// the interval length in seconds (2.5).
constexpr double kExpectedInterruptWakeUpPerSecond = 0.4;
constexpr double kExpectedPlatformIdleWakeUpPerSecond = 10;
constexpr double kExpectedBytesReadPerSecond = 0.8;
constexpr double kExpectedBytesWrittenPerSecond = 1.6;
constexpr double kExpectedPowerNW = 10000.0;
// This number will be multiplied by the int value associated with a QoS level
// to compute the expected time spent in this QoS level. E.g.
// |QoSLevels::kUtility == 3| so the time spent in the utility QoS state will
// be set to 3 * 0.1 = 30%.
constexpr double kExpectedQoSTimeBucketIdMultiplier = 0.1;

// Scales a time given in ns to mach_time in |timebase|.
uint64_t NsScaleToTimebase(const mach_timebase_info_data_t& timebase,
                           int64_t time_ns) {
  return time_ns * timebase.denom / timebase.numer;
}

// Returns test data with all time quantities scaled to the given time base.
std::unique_ptr<coalition_resource_usage> GetCoalitionResourceUsageTestData(
    const mach_timebase_info_data_t& timebase) {
  std::unique_ptr<coalition_resource_usage> test_data =
      std::make_unique<coalition_resource_usage>();

  // Scales a time given in ns to mach_time in |timebase|.
  auto scale_to_timebase = [&timebase](double time_ns) -> int64_t {
    return NsScaleToTimebase(timebase, time_ns);
  };

  test_data->cpu_time = scale_to_timebase(kExpectedCPUUsagePerSecondPercent *
                                          kIntervalLength.InNanoseconds());
  test_data->interrupt_wakeups =
      kExpectedInterruptWakeUpPerSecond * kIntervalLength.InSecondsF();
  test_data->platform_idle_wakeups =
      kExpectedPlatformIdleWakeUpPerSecond * kIntervalLength.InSecondsF();
  test_data->bytesread =
      kExpectedBytesReadPerSecond * kIntervalLength.InSecondsF();
  test_data->byteswritten =
      kExpectedBytesWrittenPerSecond * kIntervalLength.InSecondsF();
  test_data->gpu_time = scale_to_timebase(kExpectedGPUUsagePerSecondPercent *
                                          kIntervalLength.InNanoseconds());
  test_data->energy = kExpectedPowerNW * kIntervalLength.InSecondsF();
  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    test_data->cpu_time_eqos[i] =
        scale_to_timebase(i * kExpectedQoSTimeBucketIdMultiplier *
                          kIntervalLength.InNanoseconds());
  }
  test_data->cpu_time_eqos_len = COALITION_NUM_THREAD_QOS_TYPES;

  return test_data;
}

TEST(ResourceCoalitionTests, GetDataRate_NoEnergyImpact_Intel) {
  TestResourceCoalition coalition;
  coalition.SetCoalitionIDToCurrentProcessIdForTesting();

  EXPECT_TRUE(coalition.IsAvailable());

  // Keep the initial data zero initialized.
  std::unique_ptr<coalition_resource_usage> t0_data =
      std::make_unique<coalition_resource_usage>();
  std::unique_ptr<coalition_resource_usage> t1_data =
      GetCoalitionResourceUsageTestData(kIntelTimebase);

  auto data_rate = coalition.GetDataRateFromFakeDataForTesting(
      std::move(t0_data), std::move(t1_data), kIntervalLength);
  ASSERT_TRUE(data_rate);
  EXPECT_EQ(kExpectedCPUUsagePerSecondPercent, data_rate->cpu_time_per_second);
  EXPECT_EQ(kExpectedInterruptWakeUpPerSecond,
            data_rate->interrupt_wakeups_per_second);
  EXPECT_EQ(kExpectedPlatformIdleWakeUpPerSecond,
            data_rate->platform_idle_wakeups_per_second);
  EXPECT_EQ(kExpectedBytesReadPerSecond, data_rate->bytesread_per_second);
  EXPECT_EQ(kExpectedBytesWrittenPerSecond, data_rate->byteswritten_per_second);
  EXPECT_EQ(kExpectedGPUUsagePerSecondPercent, data_rate->gpu_time_per_second);
  EXPECT_EQ(0, data_rate->energy_impact_per_second);
  EXPECT_EQ(kExpectedPowerNW, data_rate->power_nw);

  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    EXPECT_DOUBLE_EQ(i * kExpectedQoSTimeBucketIdMultiplier,
                     data_rate->qos_time_per_second[i]);
  }
}

TEST(ResourceCoalitionTests, GetDataRate_NoEnergyImpact_M1) {
  TestResourceCoalition coalition;
  coalition.SetCoalitionIDToCurrentProcessIdForTesting();
  coalition.SetMachTimebaseForTesting(kM1Timebase);

  EXPECT_TRUE(coalition.IsAvailable());

  // Keep the initial data zero initialized.
  std::unique_ptr<coalition_resource_usage> t0_data =
      std::make_unique<coalition_resource_usage>();
  std::unique_ptr<coalition_resource_usage> t1_data =
      GetCoalitionResourceUsageTestData(kM1Timebase);

  auto data_rate = coalition.GetDataRateFromFakeDataForTesting(
      std::move(t0_data), std::move(t1_data), kIntervalLength);
  ASSERT_TRUE(data_rate);
  EXPECT_DOUBLE_EQ(kExpectedCPUUsagePerSecondPercent,
                   data_rate->cpu_time_per_second);
  EXPECT_DOUBLE_EQ(kExpectedInterruptWakeUpPerSecond,
                   data_rate->interrupt_wakeups_per_second);
  EXPECT_DOUBLE_EQ(kExpectedPlatformIdleWakeUpPerSecond,
                   data_rate->platform_idle_wakeups_per_second);
  EXPECT_DOUBLE_EQ(kExpectedBytesReadPerSecond,
                   data_rate->bytesread_per_second);
  EXPECT_DOUBLE_EQ(kExpectedBytesWrittenPerSecond,
                   data_rate->byteswritten_per_second);
  EXPECT_DOUBLE_EQ(kExpectedGPUUsagePerSecondPercent,
                   data_rate->gpu_time_per_second);
  EXPECT_DOUBLE_EQ(0, data_rate->energy_impact_per_second);
  EXPECT_DOUBLE_EQ(kExpectedPowerNW, data_rate->power_nw);

  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    EXPECT_DOUBLE_EQ(i * kExpectedQoSTimeBucketIdMultiplier,
                     data_rate->qos_time_per_second[i]);
  }
}

TEST(ResourceCoalitionTests, GetDataRate_WithEnergyImpact_Intel) {
  TestResourceCoalition coalition;
  coalition.SetCoalitionIDToCurrentProcessIdForTesting();
  coalition.SetEnergyImpactCoefficientsForTesting(
      GetEnergyImpactTestCoefficients());

  std::unique_ptr<coalition_resource_usage> t0_data =
      std::make_unique<coalition_resource_usage>();
  std::unique_ptr<coalition_resource_usage> t1_data =
      GetCoalitionResourceUsageTestData(kIntelTimebase);

  auto ei_data_rate = coalition.GetDataRateFromFakeDataForTesting(
      std::move(t0_data), std::move(t1_data), kIntervalLength);
  ASSERT_TRUE(ei_data_rate);
  EXPECT_EQ(kExpectedCPUUsagePerSecondPercent,
            ei_data_rate->cpu_time_per_second);
  EXPECT_EQ(kExpectedInterruptWakeUpPerSecond,
            ei_data_rate->interrupt_wakeups_per_second);
  EXPECT_EQ(kExpectedPlatformIdleWakeUpPerSecond,
            ei_data_rate->platform_idle_wakeups_per_second);
  EXPECT_EQ(kExpectedBytesReadPerSecond, ei_data_rate->bytesread_per_second);
  EXPECT_EQ(kExpectedBytesWrittenPerSecond,
            ei_data_rate->byteswritten_per_second);
  EXPECT_EQ(kExpectedGPUUsagePerSecondPercent,
            ei_data_rate->gpu_time_per_second);
  EXPECT_EQ(271.2, ei_data_rate->energy_impact_per_second);
  EXPECT_FLOAT_EQ(kExpectedPowerNW, ei_data_rate->power_nw);

  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    EXPECT_DOUBLE_EQ(i * kExpectedQoSTimeBucketIdMultiplier,
                     ei_data_rate->qos_time_per_second[i]);
  }
}

TEST(ResourceCoalitionTests, GetDataRate_WithEnergyImpact_M1) {
  TestResourceCoalition coalition;
  coalition.SetCoalitionIDToCurrentProcessIdForTesting();
  coalition.SetEnergyImpactCoefficientsForTesting(
      GetEnergyImpactTestCoefficients());
  coalition.SetMachTimebaseForTesting(kM1Timebase);

  std::unique_ptr<coalition_resource_usage> t0_data =
      std::make_unique<coalition_resource_usage>();
  std::unique_ptr<coalition_resource_usage> t1_data =
      GetCoalitionResourceUsageTestData(kM1Timebase);

  auto ei_data_rate = coalition.GetDataRateFromFakeDataForTesting(
      std::move(t0_data), std::move(t1_data), kIntervalLength);
  ASSERT_TRUE(ei_data_rate);
  EXPECT_EQ(kExpectedCPUUsagePerSecondPercent,
            ei_data_rate->cpu_time_per_second);
  EXPECT_EQ(kExpectedInterruptWakeUpPerSecond,
            ei_data_rate->interrupt_wakeups_per_second);
  EXPECT_EQ(kExpectedPlatformIdleWakeUpPerSecond,
            ei_data_rate->platform_idle_wakeups_per_second);
  EXPECT_EQ(kExpectedBytesReadPerSecond, ei_data_rate->bytesread_per_second);
  EXPECT_EQ(kExpectedBytesWrittenPerSecond,
            ei_data_rate->byteswritten_per_second);
  EXPECT_EQ(kExpectedGPUUsagePerSecondPercent,
            ei_data_rate->gpu_time_per_second);
  EXPECT_EQ(271.2, ei_data_rate->energy_impact_per_second);
  EXPECT_FLOAT_EQ(kExpectedPowerNW, ei_data_rate->power_nw);

  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    EXPECT_DOUBLE_EQ(i * kExpectedQoSTimeBucketIdMultiplier,
                     ei_data_rate->qos_time_per_second[i]);
  }
}

bool DataOverflowInvalidatesDiffImpl(
    TestResourceCoalition& coalition,
    std::unique_ptr<coalition_resource_usage> t0,
    std::unique_ptr<coalition_resource_usage> t1,
    uint64_t* field_to_overflow) {
  // Initialize all fields to a non zero value.
  ::memset(t0.get(), 1000, sizeof(coalition_resource_usage));
  ::memset(t1.get(), 1000, sizeof(coalition_resource_usage));
  *field_to_overflow = 0;

  t1->cpu_time_eqos_len = COALITION_NUM_THREAD_QOS_TYPES;
  return !coalition
              .GetDataRateFromFakeDataForTesting(std::move(t0), std::move(t1),
                                                 kIntervalLength)
              .has_value();
}

bool DataOverflowInvalidatesDiff(
    TestResourceCoalition& coalition,
    uint64_t coalition_resource_usage::*member_ptr) {
  std::unique_ptr<coalition_resource_usage> t0_data =
      std::make_unique<coalition_resource_usage>();
  std::unique_ptr<coalition_resource_usage> t1_data =
      std::make_unique<coalition_resource_usage>();
  auto* ptr = &(t1_data.get()->*member_ptr);
  return DataOverflowInvalidatesDiffImpl(coalition, std::move(t0_data),
                                         std::move(t1_data), ptr);
}

bool DataOverflowInvalidatesDiff(
    TestResourceCoalition& coalition,
    uint64_t (
        coalition_resource_usage::*member_ptr)[COALITION_NUM_THREAD_QOS_TYPES],
    int index_to_check) {
  std::unique_ptr<coalition_resource_usage> t0_data =
      std::make_unique<coalition_resource_usage>();
  std::unique_ptr<coalition_resource_usage> t1_data =
      std::make_unique<coalition_resource_usage>();
  auto* ptr = &(t1_data.get()->*member_ptr)[index_to_check];
  return DataOverflowInvalidatesDiffImpl(coalition, std::move(t0_data),
                                         std::move(t1_data), ptr);
}

TEST(ResourceCoalitionTests, Overflows) {
  TestResourceCoalition coalition;
  coalition.SetCoalitionIDToCurrentProcessIdForTesting();

  EXPECT_TRUE(coalition.IsAvailable());

  // Keep the initial data zero initialized.
  std::unique_ptr<coalition_resource_usage> t0_data =
      std::make_unique<coalition_resource_usage>();

  {
    std::unique_ptr<coalition_resource_usage> t1_data =
        std::make_unique<coalition_resource_usage>();
    t1_data->cpu_time_eqos_len = COALITION_NUM_THREAD_QOS_TYPES;
    auto data_rate = coalition.GetDataRateFromFakeDataForTesting(
        std::move(t0_data), std::move(t1_data), kIntervalLength);
    // It's valid for the data from the 2 samples to be equal to one another.
    EXPECT_TRUE(data_rate);
  }

  // If one of these tests fails then it means that overflows on a newly tracked
  // coalition field aren't tracked properly in GetCoalitionDataDiff.
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::energy_billed_to_me));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::tasks_started));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::tasks_exited));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::time_nonempty));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(coalition,
                                          &coalition_resource_usage::cpu_time));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::interrupt_wakeups));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::platform_idle_wakeups));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::bytesread));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::byteswritten));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(coalition,
                                          &coalition_resource_usage::gpu_time));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::cpu_time_billed_to_me));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::cpu_time_billed_to_others));
  EXPECT_TRUE(DataOverflowInvalidatesDiff(coalition,
                                          &coalition_resource_usage::energy));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::logical_immediate_writes));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::logical_deferred_writes));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::logical_invalidated_writes));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::logical_metadata_writes));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition,
      &coalition_resource_usage::logical_immediate_writes_to_external));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition,
      &coalition_resource_usage::logical_deferred_writes_to_external));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition,
      &coalition_resource_usage::logical_invalidated_writes_to_external));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition,
      &coalition_resource_usage::logical_metadata_writes_to_external));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::energy_billed_to_me));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::energy_billed_to_others));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::cpu_ptime));
  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    EXPECT_TRUE(DataOverflowInvalidatesDiff(
        coalition, &coalition_resource_usage::cpu_time_eqos, i));
  }
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::cpu_instructions));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::cpu_cycles));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::fs_metadata_writes));
  EXPECT_FALSE(DataOverflowInvalidatesDiff(
      coalition, &coalition_resource_usage::pm_writes));
}

base::FilePath GetTestDataPath() {
  base::FilePath test_path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_path));
  return test_path.Append(FILE_PATH_LITERAL("performance_monitor"));
}

TEST(ResourceCoalitionTests, ReadEnergyImpactCoefficientsFromPath) {
  base::FilePath test_path = GetTestDataPath();

  // Validate that attempting to read from a non-existent file fails.
  auto coefficients =
      TestResourceCoalition::ReadEnergyImpactCoefficientsFromPath(
          test_path.Append(FILE_PATH_LITERAL("does-not-exist.plist")));
  EXPECT_FALSE(coefficients.has_value());

  // Validate that a well-formed file returns the expected coefficients.
  coefficients = TestResourceCoalition::ReadEnergyImpactCoefficientsFromPath(
      test_path.Append(FILE_PATH_LITERAL("test.plist")));
  ASSERT_TRUE(coefficients.has_value());

  const EnergyImpactCoefficients& value = coefficients.value();
  EXPECT_FLOAT_EQ(value.kcpu_time, 1.23);
  EXPECT_FLOAT_EQ(value.kdiskio_bytesread, 7.89);
  EXPECT_FLOAT_EQ(value.kdiskio_byteswritten, 1.2345);
  EXPECT_FLOAT_EQ(value.kgpu_time, 6.789);
  EXPECT_FLOAT_EQ(value.knetwork_recv_bytes, 12.3);
  EXPECT_FLOAT_EQ(value.knetwork_recv_packets, 45.6);
  EXPECT_FLOAT_EQ(value.knetwork_sent_bytes, 67.8);
  EXPECT_FLOAT_EQ(value.knetwork_sent_packets, 89);
  EXPECT_FLOAT_EQ(value.kqos_background, 8.9);
  EXPECT_FLOAT_EQ(value.kqos_default, 6.78);
  EXPECT_FLOAT_EQ(value.kqos_legacy, 5.678);
  EXPECT_FLOAT_EQ(value.kqos_user_initiated, 9.012);
  EXPECT_FLOAT_EQ(value.kqos_user_interactive, 3.456);
  EXPECT_FLOAT_EQ(value.kqos_utility, 1.234);
  EXPECT_FLOAT_EQ(value.kcpu_wakeups, 3.45);
}

coalition_resource_usage MakeResourceUsageWithQOS(int qos_level,
                                                  base::TimeDelta cpu_time) {
  coalition_resource_usage result{};
  result.cpu_time_eqos_len = COALITION_NUM_THREAD_QOS_TYPES;
  result.cpu_time_eqos[qos_level] = cpu_time.InNanoseconds();
  return result;
}

TEST(ResourceCoalitionTests, ComputeEnergyImpactForCoalitionUsage_Individual) {
  TestResourceCoalition coalition;
  EXPECT_EQ(0.0, coalition.ComputeEnergyImpactForCoalitionUsage(
                     EnergyImpactCoefficients(), coalition_resource_usage()));

  // Test the coefficients and sample factors individually.
  EXPECT_DOUBLE_EQ(
      2.66, coalition.ComputeEnergyImpactForCoalitionUsage(
                EnergyImpactCoefficients{
                    .kcpu_wakeups = base::Microseconds(200).InSecondsF()},
                coalition_resource_usage{.platform_idle_wakeups = 133}));

  // Test 100 ms of CPU, which should come out to 8% of a CPU second with a
  // background QOS discount of rate of 0.8.
  EXPECT_DOUBLE_EQ(8.0, coalition.ComputeEnergyImpactForCoalitionUsage(
                            EnergyImpactCoefficients{.kqos_background = 0.8},
                            MakeResourceUsageWithQOS(THREAD_QOS_BACKGROUND,
                                                     base::Milliseconds(100))));
  EXPECT_DOUBLE_EQ(5.0, coalition.ComputeEnergyImpactForCoalitionUsage(
                            EnergyImpactCoefficients{.kqos_default = 1.0},
                            MakeResourceUsageWithQOS(THREAD_QOS_DEFAULT,
                                                     base::Milliseconds(50))));
  EXPECT_DOUBLE_EQ(10.0, coalition.ComputeEnergyImpactForCoalitionUsage(
                             EnergyImpactCoefficients{.kqos_utility = 1.0},
                             MakeResourceUsageWithQOS(
                                 THREAD_QOS_UTILITY, base::Milliseconds(100))));
  EXPECT_DOUBLE_EQ(1.0, coalition.ComputeEnergyImpactForCoalitionUsage(
                            EnergyImpactCoefficients{.kqos_legacy = 1.0},
                            MakeResourceUsageWithQOS(THREAD_QOS_LEGACY,
                                                     base::Milliseconds(10))));
  EXPECT_DOUBLE_EQ(1.0,
                   coalition.ComputeEnergyImpactForCoalitionUsage(
                       EnergyImpactCoefficients{.kqos_user_initiated = 1.0},
                       MakeResourceUsageWithQOS(THREAD_QOS_USER_INITIATED,
                                                base::Milliseconds(10))));
  EXPECT_DOUBLE_EQ(1.0,
                   coalition.ComputeEnergyImpactForCoalitionUsage(
                       EnergyImpactCoefficients{.kqos_user_interactive = 1.0},
                       MakeResourceUsageWithQOS(THREAD_QOS_USER_INTERACTIVE,
                                                base::Milliseconds(10))));

  EXPECT_DOUBLE_EQ(1.0,
                   coalition.ComputeEnergyImpactForCoalitionUsage(
                       EnergyImpactCoefficients{.kgpu_time = 2.5},
                       coalition_resource_usage{
                           .gpu_time = base::Milliseconds(4).InNanoseconds()}));
}

TEST(ResourceCoalitionTests, ComputeEnergyImpactForCoalitionUsage_Combined) {
  // test that the coefficients and samples add up as expected.
  EnergyImpactCoefficients coefficients{
      .kcpu_wakeups = base::Microseconds(200).InSecondsF(),
      .kqos_default = 1.0,
      .kqos_background = 0.8,
      .kqos_utility = 1.0,
      .kqos_legacy = 1.0,
      .kqos_user_initiated = 1.0,
      .kqos_user_interactive = 1.0,
      .kgpu_time = 2.5,
  };
  coalition_resource_usage sample{
      .platform_idle_wakeups = 133,
      .gpu_time =
          NsScaleToTimebase(kM1Timebase, base::Milliseconds(4).InNanoseconds()),
      .cpu_time_eqos_len = COALITION_NUM_THREAD_QOS_TYPES,
  };
  sample.cpu_time_eqos[THREAD_QOS_BACKGROUND] =
      NsScaleToTimebase(kM1Timebase, base::Milliseconds(100).InNanoseconds());
  sample.cpu_time_eqos[THREAD_QOS_DEFAULT] =
      NsScaleToTimebase(kM1Timebase, base::Milliseconds(50).InNanoseconds());
  sample.cpu_time_eqos[THREAD_QOS_UTILITY] =
      NsScaleToTimebase(kM1Timebase, base::Milliseconds(100).InNanoseconds());
  sample.cpu_time_eqos[THREAD_QOS_LEGACY] =
      NsScaleToTimebase(kM1Timebase, base::Milliseconds(10).InNanoseconds());
  sample.cpu_time_eqos[THREAD_QOS_USER_INITIATED] =
      NsScaleToTimebase(kM1Timebase, base::Milliseconds(10).InNanoseconds());
  sample.cpu_time_eqos[THREAD_QOS_USER_INTERACTIVE] =
      NsScaleToTimebase(kM1Timebase, base::Milliseconds(10).InNanoseconds());

  TestResourceCoalition coalition;
  coalition.SetMachTimebaseForTesting(kM1Timebase);
  EXPECT_DOUBLE_EQ(29.66, coalition.ComputeEnergyImpactForCoalitionUsage(
                              coefficients, sample));
}

TEST(ResourceCoalitionTests, ComputeEnergyImpactForCoalitionUsage_Unused) {
  // Test that the unused coefficients and sample factors contribute nothing.
  EnergyImpactCoefficients coefficients{
      .kdiskio_bytesread = 1000,
      .kdiskio_byteswritten = 1000,
      .knetwork_recv_bytes = 1000,
      .knetwork_recv_packets = 1000,
      .knetwork_sent_bytes = 1000,
      .knetwork_sent_packets = 1000,
  };
  coalition_resource_usage sample{
      .tasks_started = 1000,
      .tasks_exited = 1000,
      .time_nonempty = 1000,
      .cpu_time = 1000,
      .interrupt_wakeups = 1000,
      .bytesread = 1000,
      .byteswritten = 1000,
      .cpu_time_billed_to_me = 1000,
      .cpu_time_billed_to_others = 1000,
      .energy = 1000,
      .logical_immediate_writes = 1000,
      .logical_deferred_writes = 1000,
      .logical_invalidated_writes = 1000,
      .logical_metadata_writes = 1000,
      .logical_immediate_writes_to_external = 1000,
      .logical_deferred_writes_to_external = 1000,
      .logical_invalidated_writes_to_external = 1000,
      .logical_metadata_writes_to_external = 1000,
      .energy_billed_to_me = 1000,
      .energy_billed_to_others = 1000,
      .cpu_ptime = 1000,
      .cpu_instructions = 1000,
      .cpu_cycles = 1000,
      .fs_metadata_writes = 1000,
      .pm_writes = 1000,
  };

  TestResourceCoalition coalition;
  EXPECT_EQ(
      0, coalition.ComputeEnergyImpactForCoalitionUsage(coefficients, sample));
}

TEST(ResourceCoalitionTests, ReadEnergyImpactOrDefaultForBoardId_Exists) {
  // This board-id should exist.
  auto coefficients =
      TestResourceCoalition::ReadEnergyImpactOrDefaultForBoardId(
          GetTestDataPath(), "Mac-7BA5B2DFE22DDD8C");
  ASSERT_TRUE(coefficients.has_value());

  // Validate that the default coefficients haven't been loaded.
  EXPECT_FLOAT_EQ(3.4, coefficients.value().kgpu_time);
  EXPECT_FLOAT_EQ(0.39, coefficients.value().kqos_background);
}

TEST(ResourceCoalitionTests, ReadEnergyImpactOrDefaultForBoardId_Default) {
  // This board-id should not exist.
  auto coefficients =
      TestResourceCoalition::ReadEnergyImpactOrDefaultForBoardId(
          GetTestDataPath(), "Mac-031B6874CF7F642A");
  ASSERT_TRUE(coefficients.has_value());

  // Validate that the default coefficients were loaded.
  EXPECT_FLOAT_EQ(0, coefficients.value().kgpu_time);
  EXPECT_FLOAT_EQ(0.8, coefficients.value().kqos_background);
}

TEST(ResourceCoalitionTests, ReadEnergyImpactOrDefaultForBoardId_NoFiles) {
  // This directory shouldn't exist, so nothing should be loaded.
  EXPECT_FALSE(
      TestResourceCoalition::ReadEnergyImpactOrDefaultForBoardId(
          GetTestDataPath().Append("nonexistent"), "Mac-7BA5B2DFE22DDD8C")
          .has_value());
}

TEST(ResourceCoalitionTests, MaybeGetBoardIdForThisMachine) {
  // This can't really be tested except that the contract holds one way
  // or the other.
  auto board_id = TestResourceCoalition::MaybeGetBoardIdForThisMachine();
  if (board_id.has_value()) {
    EXPECT_FALSE(board_id.value().empty());
  }
}

}  // namespace

}  // namespace performance_monitor
