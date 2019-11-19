// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/memory_pressure_monitor_win.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/performance_monitor/system_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory {
namespace {

using SamplingFrequency = performance_monitor::SystemMonitor::SamplingFrequency;

// Mock MetricEvaluatorsHelper that will be used to send some test system metric
// values to SystemMonitor.
class LenientMockMetricEvaluatorsHelper
    : public performance_monitor::MetricEvaluatorsHelper {
 public:
  ~LenientMockMetricEvaluatorsHelper() override {}
  MOCK_METHOD0(GetFreePhysicalMemoryMb, base::Optional<int>());
  MOCK_METHOD0(GetDiskIdleTimePercent, base::Optional<float>());
  MOCK_METHOD0(GetChromeTotalResidentSetEstimateMb, base::Optional<int>());
};
using MockMetricHelper =
    ::testing::StrictMock<LenientMockMetricEvaluatorsHelper>;

}  // namespace

class MemoryPressureMonitorWinTest : public testing::Test {
 public:
  MemoryPressureMonitorWinTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        tick_clock_(task_environment_.GetMockTickClock()) {}
  ~MemoryPressureMonitorWinTest() override = default;

  void SetUp() override {
    EXPECT_EQ(nullptr, performance_monitor::SystemMonitor::Get());

    mock_helper_ = new MockMetricHelper();

    system_monitor_ = performance_monitor::SystemMonitor::CreateForTesting(
        base::WrapUnique(mock_helper_));

    pressure_monitor_.reset(new MemoryPressureMonitorWin());
  }

  void TearDown() override {
    mock_helper_ = nullptr;
    system_monitor_.reset(nullptr);
  }

 protected:
  void CheckMonitorRefreshFrequencies(
      SamplingFrequency expected_free_mem_freq,
      SamplingFrequency expected_disk_idle_time_freq) const {
    EXPECT_EQ(
        expected_free_mem_freq,
        pressure_monitor_->refresh_frequencies_.free_phys_memory_mb_frequency);
    EXPECT_EQ(expected_disk_idle_time_freq,
              pressure_monitor_->refresh_frequencies_
                  .disk_idle_time_percent_frequency);
  }

  base::test::TaskEnvironment task_environment_;
  const base::TickClock* tick_clock_;
  std::unique_ptr<performance_monitor::SystemMonitor> system_monitor_;

  // The mock metric helper, owned by |system_monitor_|.
  MockMetricHelper* mock_helper_;

  // This needs to be created after |system_monitor_|.
  std::unique_ptr<MemoryPressureMonitorWin> pressure_monitor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryPressureMonitorWinTest);
};

// Test the transition from MEMORY_PRESSURE_LEVEL_NONE to
// MEMORY_PRESSURE_LEVEL_CRITICAL and vice versa.
TEST_F(MemoryPressureMonitorWinTest, MemoryPressureChanges) {
  const auto& free_mem_window_config =
      pressure_monitor_->free_memory_obs_window_for_testing()
          .config_for_testing();

  CheckMonitorRefreshFrequencies(SamplingFrequency::kDefaultFrequency,
                                 SamplingFrequency::kNoSampling);

  // Wait for the memory to be under the early limit.
  EXPECT_FALSE(pressure_monitor_->free_memory_obs_window_for_testing()
                   .MemoryIsUnderEarlyLimit());
  while (!pressure_monitor_->free_memory_obs_window_for_testing()
              .MemoryIsUnderEarlyLimit()) {
    // Make the helper return an amount of free physical memory slightly under
    // the early limit.
    EXPECT_CALL(*mock_helper_, GetFreePhysicalMemoryMb())
        .Times(1)
        .WillOnce(::testing::Return(
            free_mem_window_config.low_memory_early_limit_mb - 1));

    // Fast forward to the next sample notification.
    EXPECT_TRUE(system_monitor_->refresh_timer_for_testing().IsRunning());
    task_environment_.FastForwardBy(
        system_monitor_->refresh_timer_for_testing().GetCurrentDelay());
    task_environment_.RunUntilIdle();
    ::testing::Mock::VerifyAndClear(mock_helper_);
  }

  EXPECT_EQ(base::MemoryPressureListener::MemoryPressureLevel::
                MEMORY_PRESSURE_LEVEL_NONE,
            pressure_monitor_->memory_pressure_level_for_testing());

  CheckMonitorRefreshFrequencies(SamplingFrequency::kDefaultFrequency,
                                 SamplingFrequency::kDefaultFrequency);

  // Make the helper return an amount of free physical memory slightly under
  // the critical limit and a disk idle time of 100%.
  EXPECT_CALL(*mock_helper_, GetFreePhysicalMemoryMb())
      .Times(::testing::AtLeast(1))
      .WillRepeatedly(::testing::Return(
          free_mem_window_config.low_memory_critical_limit_mb - 1));
  EXPECT_CALL(*mock_helper_, GetDiskIdleTimePercent())
      .Times(::testing::AtLeast(1))
      .WillRepeatedly(::testing::Return(1.0));

  EXPECT_FALSE(pressure_monitor_->free_memory_obs_window_for_testing()
                   .MemoryIsUnderCriticalLimit());

  while (!pressure_monitor_->free_memory_obs_window_for_testing()
              .MemoryIsUnderCriticalLimit()) {
    // Fast forward to the next sample notification.
    EXPECT_TRUE(system_monitor_->refresh_timer_for_testing().IsRunning());
    task_environment_.FastForwardBy(
        system_monitor_->refresh_timer_for_testing().GetCurrentDelay());
    task_environment_.RunUntilIdle();
  }

  // The disk idle time is at 100%, there's no memory pressure.
  EXPECT_EQ(base::MemoryPressureListener::MemoryPressureLevel::
                MEMORY_PRESSURE_LEVEL_NONE,
            pressure_monitor_->memory_pressure_level_for_testing());

  // The helper will now return an idle time of 0% and an amount of free memory
  // that's under the critical threshold, this should lead to memory pressure.
  EXPECT_CALL(*mock_helper_, GetDiskIdleTimePercent())
      .Times(::testing::AtLeast(1))
      .WillRepeatedly(::testing::Return(0.0));

  while (!pressure_monitor_->disk_idle_time_obs_window_for_testing()
              .DiskIdleTimeIsLow()) {
    // Fast forward to the next sample notification.
    EXPECT_TRUE(system_monitor_->refresh_timer_for_testing().IsRunning());
    task_environment_.FastForwardBy(
        system_monitor_->refresh_timer_for_testing().GetCurrentDelay());
    task_environment_.RunUntilIdle();
  }

  EXPECT_EQ(base::MemoryPressureListener::MemoryPressureLevel::
                MEMORY_PRESSURE_LEVEL_CRITICAL,
            pressure_monitor_->memory_pressure_level_for_testing());

  // Make the helper return an amount of free physical memory greater than the
  // early limit, the memory pressure should go back to the
  // MEMORY_PRESSURE_LEVEL_NONE level and the monitor should stop observing the
  // disk idle time.
  EXPECT_CALL(*mock_helper_, GetFreePhysicalMemoryMb())
      .Times(::testing::AtLeast(1))
      .WillRepeatedly(::testing::Return(
          free_mem_window_config.low_memory_early_limit_mb + 1));

  while (pressure_monitor_->free_memory_obs_window_for_testing()
             .MemoryIsUnderEarlyLimit()) {
    // Fast forward to the next sample notification.
    EXPECT_TRUE(system_monitor_->refresh_timer_for_testing().IsRunning());
    task_environment_.FastForwardBy(
        system_monitor_->refresh_timer_for_testing().GetCurrentDelay());
    task_environment_.RunUntilIdle();
  }

  CheckMonitorRefreshFrequencies(SamplingFrequency::kDefaultFrequency,
                                 SamplingFrequency::kNoSampling);

  EXPECT_EQ(base::MemoryPressureListener::MemoryPressureLevel::
                MEMORY_PRESSURE_LEVEL_NONE,
            pressure_monitor_->memory_pressure_level_for_testing());

  ::testing::Mock::VerifyAndClear(mock_helper_);
}

}  // namespace memory
