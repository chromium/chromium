// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"

#include <optional>
#include <vector>

#include "base/location.h"
#include "base/system/sys_info.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "components/performance_manager/public/resource_attribution/process_context.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "components/system_cpu/cpu_sample.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::user_tuning {

namespace {
class MockPerformanceDetectionManagerStatusObserver
    : public PerformanceDetectionManager::StatusObserver {
 public:
  MOCK_METHOD(void,
              OnStatusChanged,
              (PerformanceDetectionManager::ResourceType,
               PerformanceDetectionManager::HealthLevel,
               bool),
              (override));
};

class PerformanceDetectionManagerStatusObserver
    : public PerformanceDetectionManager::StatusObserver {
 public:
  void OnStatusChanged(PerformanceDetectionManager::ResourceType resource_type,
                       PerformanceDetectionManager::HealthLevel health_level,
                       bool is_actionable) override {
    resource_type_ = resource_type;
    health_level_ = health_level;
    is_actionable_ = is_actionable;
  }

  std::optional<PerformanceDetectionManager::ResourceType> resource_type() {
    return resource_type_;
  }

  std::optional<PerformanceDetectionManager::HealthLevel> health_level() {
    return health_level_;
  }

  std::optional<bool> is_actionable() { return is_actionable_; }

 private:
  std::optional<PerformanceDetectionManager::ResourceType> resource_type_;
  std::optional<PerformanceDetectionManager::HealthLevel> health_level_;
  std::optional<bool> is_actionable_;
};

class MockPerformanceDetectionManagerActionableTabsObserver
    : public PerformanceDetectionManager::ActionableTabsObserver {
 public:
  MOCK_METHOD(void,
              OnActionableTabListChanged,
              (PerformanceDetectionManager::ResourceType,
               std::vector<resource_attribution::PageContext>),
              (override));
};

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

class PerformanceDetectionManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  PerformanceDetectionManagerTest()
      : ChromeRenderViewHostTestHarness(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    pm_harness_.SetUp();
    SetContents(CreateTestWebContents());
  }

  void TearDown() override {
    // Reset the performance detection manager and have the task environment
    // run until idle to make sure that any objects owned by `SequenceBound`
    // have been destroyed to avoid tripping memory leak detection.
    manager_.reset();
    PerformanceManager::CallOnGraph(FROM_HERE,
                                    task_environment()->QuitClosure());
    task_environment()->RunUntilQuit();
    DeleteContents();
    pm_harness_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CreateManager() { manager_.reset(new PerformanceDetectionManager()); }

  PerformanceDetectionManager* manager() {
    return PerformanceDetectionManager::GetInstance();
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

 private:
  PerformanceManagerTestHarnessHelper pm_harness_;
  std::unique_ptr<PerformanceDetectionManager> manager_;
};

TEST_F(PerformanceDetectionManagerTest, ReturnsInstance) {
  CreateManager();
  EXPECT_NE(manager(), nullptr);
}

TEST_F(PerformanceDetectionManagerTest, HasInstance) {
  EXPECT_FALSE(PerformanceDetectionManager::HasInstance());
  CreateManager();
  EXPECT_TRUE(PerformanceDetectionManager::HasInstance());
}

TEST_F(PerformanceDetectionManagerTest, StatusObserverCalledOnObserve) {
  CreateManager();

  MockPerformanceDetectionManagerStatusObserver observer;
  EXPECT_CALL(observer, OnStatusChanged).Times(1);
  PerformanceDetectionManager::ResourceTypeSet resources;
  resources.Put(PerformanceDetectionManager::ResourceType::kMemory);
  manager()->AddStatusObserver(resources, &observer);
  manager()->RemoveStatusObserver(&observer);
}

TEST_F(PerformanceDetectionManagerTest, ActionableTabsObserverCalledOnObserve) {
  CreateManager();

  MockPerformanceDetectionManagerActionableTabsObserver observer;
  EXPECT_CALL(observer, OnActionableTabListChanged).Times(1);
  PerformanceDetectionManager::ResourceTypeSet resources;
  resources.Put(PerformanceDetectionManager::ResourceType::kMemory);
  manager()->AddActionableTabsObserver(resources, &observer);
}

TEST_F(PerformanceDetectionManagerTest, UpdatedStatusSentToObservers) {
  CreateManager();

  PerformanceDetectionManagerStatusObserver observer;
  PerformanceDetectionManager::ResourceTypeSet resources;
  resources.Put(PerformanceDetectionManager::ResourceType::kCpu);
  manager()->AddStatusObserver(resources, &observer);

  EXPECT_TRUE(observer.resource_type().has_value());
  EXPECT_TRUE(observer.health_level().has_value());
  EXPECT_TRUE(observer.is_actionable().has_value());

  EXPECT_EQ(observer.resource_type().value(),
            PerformanceDetectionManager::ResourceType::kCpu);
  EXPECT_EQ(observer.health_level().value(),
            PerformanceDetectionManager::HealthLevel::kHealthy);
  EXPECT_FALSE(observer.is_actionable().value());
  manager()->RemoveStatusObserver(&observer);
}

TEST_F(PerformanceDetectionManagerTest, RecordCpuAndUpdateHealthStatus) {
  CreateManager();

  EXPECT_EQ(manager()->GetHealthLevelForTesting(
                PerformanceDetectionManager::ResourceType::kCpu),
            PerformanceDetectionManager::HealthLevel::kHealthy);

  // Simulate continuously receiving system cpu
  // Health status should remain as healthy since we didn't
  // exceed the number of times for the health to change
  for (int i = 0; i < kNumHealthStatusForChange - 1; i++) {
    manager()->RecordAndUpdateCpuHealthStatus(
        kUnhealthySystemCpuUsagePercentage);
    EXPECT_EQ(manager()->GetHealthLevelForTesting(
                  PerformanceDetectionManager::ResourceType::kCpu),
              PerformanceDetectionManager::HealthLevel::kHealthy);
  }

  // Status changes after exceeding threshold to be continuously being
  // unhealthy
  manager()->RecordAndUpdateCpuHealthStatus(kUnhealthySystemCpuUsagePercentage);
  EXPECT_EQ(manager()->GetHealthLevelForTesting(
                PerformanceDetectionManager::ResourceType::kCpu),
            PerformanceDetectionManager::HealthLevel::kUnhealthy);

  // simulate medium but doesn't meet continuous requirement
  manager()->RecordAndUpdateCpuHealthStatus(kDegradedSystemCpuUsagePercentage);
  EXPECT_EQ(manager()->GetHealthLevelForTesting(
                PerformanceDetectionManager::ResourceType::kCpu),
            PerformanceDetectionManager::HealthLevel::kDegraded);

  // Status should stay as medium even when receiving unhealthy cpu usage
  // since the manager received a medium health status recently and the window
  // is no longer consistently unhealthy
  for (int i = 0; i < kNumHealthStatusForChange - 1; i++) {
    manager()->RecordAndUpdateCpuHealthStatus(
        kUnhealthySystemCpuUsagePercentage);
    EXPECT_EQ(manager()->GetHealthLevelForTesting(
                  PerformanceDetectionManager::ResourceType::kCpu),
              PerformanceDetectionManager::HealthLevel::kDegraded);
  }

  // Health status should change since we have been consistently unhealthy for a
  // while now
  manager()->RecordAndUpdateCpuHealthStatus(kUnhealthySystemCpuUsagePercentage);
  EXPECT_EQ(manager()->GetHealthLevelForTesting(
                PerformanceDetectionManager::ResourceType::kCpu),
            PerformanceDetectionManager::HealthLevel::kUnhealthy);

  // Health status stays as medium when oscillating between medium and unhealthy
  manager()->RecordAndUpdateCpuHealthStatus(kDegradedSystemCpuUsagePercentage);
  EXPECT_EQ(manager()->GetHealthLevelForTesting(
                PerformanceDetectionManager::ResourceType::kCpu),
            PerformanceDetectionManager::HealthLevel::kDegraded);

  manager()->RecordAndUpdateCpuHealthStatus(kUnhealthySystemCpuUsagePercentage);
  EXPECT_EQ(manager()->GetHealthLevelForTesting(
                PerformanceDetectionManager::ResourceType::kCpu),
            PerformanceDetectionManager::HealthLevel::kDegraded);

  manager()->RecordAndUpdateCpuHealthStatus(kDegradedSystemCpuUsagePercentage);
  EXPECT_EQ(manager()->GetHealthLevelForTesting(
                PerformanceDetectionManager::ResourceType::kCpu),
            PerformanceDetectionManager::HealthLevel::kDegraded);
}

TEST_F(PerformanceDetectionManagerTest, CpuStatusUpdates) {
  CreateManager();
  // Stop the timer to prevent the cpu probe from recording real CPU data
  // which makes the health status non-deterministic when we fast forward time.
  manager()->cpu_probe_timer_.Stop();

  resource_attribution::QueryResultMap result_map;
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  std::optional<resource_attribution::PageContext> page_context =
      resource_attribution::PageContext::FromWebContents(web_contents.get());
  ASSERT_TRUE(page_context.has_value());

  // Exceed threshold for medium CPU usage for status change
  for (int i = 0; i < kNumHealthStatusForChange; i++) {
    task_environment()->FastForwardBy(base::Seconds(60));
    result_map[page_context.value()] = {
        .cpu_time_result = CreateFakeCpuResult(
            base::Seconds(20 * base::SysInfo::NumberOfProcessors()))};
    manager()->ProcessQueryResultMap(kDegradedSystemCpuUsagePercentage,
                                     result_map);
  }

  PerformanceDetectionManagerStatusObserver observer;
  PerformanceDetectionManager::ResourceTypeSet resources;
  resources.Put(PerformanceDetectionManager::ResourceType::kCpu);
  manager()->AddStatusObserver(resources, &observer);

  // Verify that even though an observer is registered after the status changes,
  // the observer is still notified of the most recent status
  EXPECT_TRUE(observer.resource_type().has_value());
  EXPECT_TRUE(observer.health_level().has_value());
  EXPECT_TRUE(observer.is_actionable().has_value());
  EXPECT_EQ(observer.resource_type().value(),
            PerformanceDetectionManager::ResourceType::kCpu);
  EXPECT_EQ(observer.health_level().value(),
            PerformanceDetectionManager::HealthLevel::kDegraded);

  // Consistently receive unhealthy CPU usage for status change
  for (int i = 0; i < kNumHealthStatusForChange; i++) {
    task_environment()->FastForwardBy(base::Seconds(1));
    result_map[page_context.value()] = {
        .cpu_time_result = CreateFakeCpuResult(
            base::Seconds(1 * base::SysInfo::NumberOfProcessors()))};
    manager()->ProcessQueryResultMap(kUnhealthySystemCpuUsagePercentage,
                                     result_map);
  }

  // Verify that observers are notified of the status change
  EXPECT_EQ(observer.resource_type().value(),
            PerformanceDetectionManager::ResourceType::kCpu);
  EXPECT_EQ(observer.health_level().value(),
            PerformanceDetectionManager::HealthLevel::kUnhealthy);
  manager()->RemoveStatusObserver(&observer);
}

TEST_F(PerformanceDetectionManagerTest, HealthyCpuUsageFromProbe) {
  CreateManager();
  // Stop the timer to prevent the cpu probe from recording real CPU data
  // which makes the health status non-deterministic when we fast forward time.
  manager()->cpu_probe_timer_.Stop();

  PerformanceDetectionManagerStatusObserver observer;
  PerformanceDetectionManager::ResourceTypeSet resources;
  resources.Put(PerformanceDetectionManager::ResourceType::kCpu);
  manager()->AddStatusObserver(resources, &observer);

  EXPECT_EQ(observer.health_level().value(),
            PerformanceDetectionManager::HealthLevel::kHealthy);

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
    manager()->ProcessQueryResultMap(kDegradedSystemCpuUsagePercentage,
                                     result_map);
  }

  EXPECT_EQ(observer.health_level().value(),
            PerformanceDetectionManager::HealthLevel::kDegraded);

  // Consistently receive healthy cpu usage from the CPU Probe
  for (int i = 0; i < kNumHealthStatusForChange; i++) {
    system_cpu::CpuSample sample{0};
    manager()->ProcessCpuProbeResult(sample);
  }

  EXPECT_EQ(observer.health_level().value(),
            PerformanceDetectionManager::HealthLevel::kHealthy);
  manager()->RemoveStatusObserver(&observer);
}
}  // namespace performance_manager::user_tuning
