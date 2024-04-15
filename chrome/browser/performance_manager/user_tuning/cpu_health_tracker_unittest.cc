// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/cpu_health_tracker.h"

#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/system/sys_info.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "components/system_cpu/cpu_sample.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::user_tuning {

namespace {

// Number of times to see a health status consecutively for the health status to
// change
const int kNumHealthStatusForChange =
    performance_manager::features::kCPUTimeOverThreshold.Get() /
    performance_manager::features::kCPUSampleFrequency.Get();

const int kUnhealthySystemCpuUsagePercentage =
    performance_manager::features::kCPUUnhealthyPercentageThreshold.Get() + 1;
const int kDegradedSystemCpuUsagePercentage =
    performance_manager::features::kCPUDegradedHealthPercentageThreshold.Get() +
    1;
}  // namespace

class CpuHealthTrackerTest : public ChromeRenderViewHostTestHarness {
 public:
  CpuHealthTrackerTest()
      : ChromeRenderViewHostTestHarness(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    pm_harness_.SetUp();
    SetContents(CreateTestWebContents());
    cpu_health_tracker_ = std::make_unique<CpuHealthTracker>(
        status_change_future_.GetRepeatingCallback(), base::DoNothing());
  }

  void TearDown() override {
    // Reset the health tracker and have the task environment
    // run until all tasks posted from the destructor are complete to make sure
    // that any objects owned by `SequenceBound` have been destroyed to avoid
    // tripping memory leak detection.
    cpu_health_tracker_.reset();
    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE, task_environment()->QuitClosure());
    task_environment()->RunUntilQuit();
    DeleteContents();
    pm_harness_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  resource_attribution::CPUTimeResult CreateFakeCpuResult(
      base::TimeDelta cumulative_cpu) {
    resource_attribution::ResultMetadata metadata(
        base::TimeTicks::Now(),
        resource_attribution::MeasurementAlgorithm::kDirectMeasurement);
    return {.metadata = metadata,
            .start_time = base::TimeTicks::Now(),
            .cumulative_cpu = cumulative_cpu};
  }

  CpuHealthTracker* cpu_health_tracker() { return cpu_health_tracker_.get(); }

  CpuHealthTracker::HealthLevel GetFutureHealthLevel() {
    return std::get<1>(status_change_future_.Take());
  }

 private:
  performance_manager::PerformanceManagerTestHarnessHelper pm_harness_;
  std::unique_ptr<CpuHealthTracker> cpu_health_tracker_;
  base::test::TestFuture<CpuHealthTracker::ResourceType,
                         CpuHealthTracker::HealthLevel,
                         bool>
      status_change_future_;
};

TEST_F(CpuHealthTrackerTest, RecordCpuAndUpdateHealthStatus) {
  CpuHealthTracker* const health_tracker = cpu_health_tracker();

  EXPECT_EQ(health_tracker->GetHealthLevelForTesting(),
            CpuHealthTracker::HealthLevel::kHealthy);

  // Simulate continuously receiving system cpu
  // Health status should remain as healthy since we didn't
  // exceed the number of times for the health to change
  for (int i = 0; i < kNumHealthStatusForChange - 1; i++) {
    health_tracker->RecordAndUpdateHealthStatus(
        kUnhealthySystemCpuUsagePercentage);
    EXPECT_EQ(health_tracker->GetHealthLevelForTesting(),
              CpuHealthTracker::HealthLevel::kHealthy);
  }

  // Status changes after exceeding threshold to be continuously being
  // unhealthy
  health_tracker->RecordAndUpdateHealthStatus(
      kUnhealthySystemCpuUsagePercentage);
  EXPECT_EQ(health_tracker->GetHealthLevelForTesting(),
            CpuHealthTracker::HealthLevel::kUnhealthy);

  // simulate medium but doesn't meet continuous requirement
  health_tracker->RecordAndUpdateHealthStatus(
      kDegradedSystemCpuUsagePercentage);
  EXPECT_EQ(health_tracker->GetHealthLevelForTesting(),
            CpuHealthTracker::HealthLevel::kDegraded);

  // Status should stay as medium even when receiving unhealthy cpu usage
  // since the manager received a medium health status recently and the window
  // is no longer consistently unhealthy
  for (int i = 0; i < kNumHealthStatusForChange - 1; i++) {
    health_tracker->RecordAndUpdateHealthStatus(
        kUnhealthySystemCpuUsagePercentage);
    EXPECT_EQ(health_tracker->GetHealthLevelForTesting(),
              CpuHealthTracker::HealthLevel::kDegraded);
  }

  // Health status should change since we have been consistently unhealthy for a
  // while now
  health_tracker->RecordAndUpdateHealthStatus(
      kUnhealthySystemCpuUsagePercentage);
  EXPECT_EQ(health_tracker->GetHealthLevelForTesting(),
            CpuHealthTracker::HealthLevel::kUnhealthy);

  // Health status stays as medium when oscillating between medium and unhealthy
  health_tracker->RecordAndUpdateHealthStatus(
      kDegradedSystemCpuUsagePercentage);
  EXPECT_EQ(health_tracker->GetHealthLevelForTesting(),
            CpuHealthTracker::HealthLevel::kDegraded);

  health_tracker->RecordAndUpdateHealthStatus(
      kUnhealthySystemCpuUsagePercentage);
  EXPECT_EQ(health_tracker->GetHealthLevelForTesting(),
            CpuHealthTracker::HealthLevel::kDegraded);

  health_tracker->RecordAndUpdateHealthStatus(
      kDegradedSystemCpuUsagePercentage);
  EXPECT_EQ(health_tracker->GetHealthLevelForTesting(),
            CpuHealthTracker::HealthLevel::kDegraded);
}

TEST_F(CpuHealthTrackerTest, CpuStatusUpdates) {
  CpuHealthTracker* const health_tracker = cpu_health_tracker();

  // Stop the timer to prevent the cpu probe from recording real CPU data
  // which makes the health status non-deterministic when we fast forward time.
  health_tracker->cpu_probe_timer_.Stop();

  resource_attribution::QueryResultMap result_map;
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  std::optional<resource_attribution::PageContext> page_context =
      resource_attribution::PageContext::FromWebContents(web_contents.get());
  ASSERT_TRUE(page_context.has_value());

  // Exceed threshold for degraded CPU usage for status change
  for (int i = 0; i < kNumHealthStatusForChange; i++) {
    task_environment()->FastForwardBy(base::Seconds(60));
    result_map[page_context.value()] = {
        .cpu_time_result = CreateFakeCpuResult(
            base::Seconds(20 * base::SysInfo::NumberOfProcessors()))};
    health_tracker->ProcessQueryResultMap(kDegradedSystemCpuUsagePercentage,
                                          result_map);
  }

  // Verify that the health status changed to degraded and the status change
  // callback was called
  EXPECT_EQ(CpuHealthTracker::HealthLevel::kDegraded, GetFutureHealthLevel());

  // Consistently receive unhealthy CPU usage for status change
  for (int i = 0; i < kNumHealthStatusForChange; i++) {
    task_environment()->FastForwardBy(base::Seconds(1));
    result_map[page_context.value()] = {
        .cpu_time_result = CreateFakeCpuResult(
            base::Seconds(1 * base::SysInfo::NumberOfProcessors()))};
    health_tracker->ProcessQueryResultMap(kUnhealthySystemCpuUsagePercentage,
                                          result_map);
  }

  // Verify that the status callback is called when status changed to unhealthy
  EXPECT_EQ(CpuHealthTracker::HealthLevel::kUnhealthy, GetFutureHealthLevel());
}

TEST_F(CpuHealthTrackerTest, HealthyCpuUsageFromProbe) {
  CpuHealthTracker* const health_tracker = cpu_health_tracker();

  // Stop the timer to prevent the cpu probe from recording real CPU data
  // which makes the health status non-deterministic when we fast forward time.
  health_tracker->cpu_probe_timer_.Stop();

  EXPECT_EQ(health_tracker->GetHealthLevelForTesting(),
            CpuHealthTracker::HealthLevel::kHealthy);

  resource_attribution::QueryResultMap result_map;
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  std::optional<resource_attribution::PageContext> page_context =
      resource_attribution::PageContext::FromWebContents(web_contents.get());
  ASSERT_TRUE(page_context.has_value());

  // Consistently receive medium CPU usage for status change
  for (int i = 0; i < kNumHealthStatusForChange; i++) {
    task_environment()->FastForwardBy(base::Seconds(60));
    result_map[page_context.value()] = {
        .cpu_time_result = CreateFakeCpuResult(
            base::Seconds(20 * base::SysInfo::NumberOfProcessors()))};
    health_tracker->ProcessQueryResultMap(kDegradedSystemCpuUsagePercentage,
                                          result_map);
  }

  EXPECT_EQ(CpuHealthTracker::HealthLevel::kDegraded, GetFutureHealthLevel());

  // Consistently receive healthy cpu usage from the CPU Probe
  for (int i = 0; i < kNumHealthStatusForChange; i++) {
    system_cpu::CpuSample sample{0};
    health_tracker->ProcessCpuProbeResult(sample);
  }

  EXPECT_EQ(CpuHealthTracker::HealthLevel::kHealthy, GetFutureHealthLevel());
}

TEST_F(CpuHealthTrackerTest, GetPagesMeetMinimumCpuUsage) {
  CpuHealthTracker* const health_tracker = cpu_health_tracker();
  std::map<resource_attribution::ResourceContext, double> page_contexts_cpu;

  const int minimum_percent_cpu_usage =
      performance_manager::features::kMinimumActionableTabCPUPercentage.Get();
  const double minimum_decimal_cpu_usage = minimum_percent_cpu_usage / 100.0;

  // Generate a map of page contexts and decimal CPU usage where half the page
  // contexts are below the minimum cpu usage for a tab to be actionable, and
  // half above it
  for (int i = 0; i < 10; i++) {
    std::unique_ptr<content::WebContents> web_contents =
        CreateTestWebContents();
    std::optional<resource_attribution::PageContext> page_context =
        resource_attribution::PageContext::FromWebContents(web_contents.get());
    ASSERT_TRUE(page_context.has_value());
    const double cpu_usage = (i % 2 == 0) ? minimum_decimal_cpu_usage - 0.01
                                          : minimum_decimal_cpu_usage;
    page_contexts_cpu[page_context.value()] =
        cpu_usage * base::SysInfo::NumberOfProcessors();
  }

  CpuHealthTracker::PageResourceMeasurements filtered_measurements =
      health_tracker->GetPagesMeetMinimumCpuUsage(page_contexts_cpu);
  EXPECT_EQ(filtered_measurements.size(), (page_contexts_cpu.size() / 2));

  for (auto& [context, cpu_percentage] : filtered_measurements) {
    EXPECT_EQ(cpu_percentage, minimum_percent_cpu_usage);
  }
}
}  // namespace performance_manager::user_tuning
