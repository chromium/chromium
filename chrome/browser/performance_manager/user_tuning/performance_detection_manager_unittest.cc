// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
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

class PerformanceDetectionManagerActionableTabObserver
    : public PerformanceDetectionManager::ActionableTabsObserver {
 public:
  void OnActionableTabListChanged(
      PerformanceDetectionManager::ResourceType resource_type,
      std::vector<resource_attribution::PageContext> tabs) override {
    resource_type_ = resource_type;
    actionable_tabs_ = tabs;
  }

  std::optional<PerformanceDetectionManager::ResourceType> resource_type() {
    return resource_type_;
  }

  std::optional<std::vector<resource_attribution::PageContext>>
  actionable_tabs() {
    return actionable_tabs_;
  }

 private:
  std::optional<PerformanceDetectionManager::ResourceType> resource_type_;
  std::optional<std::vector<resource_attribution::PageContext>>
      actionable_tabs_;
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
    performance_manager::RunInGraph([](Graph* graph) {
      auto page_discarding_helper =
          std::make_unique<policies::PageDiscardingHelper>();
      page_discarding_helper->SetMockDiscarderForTesting(
          std::make_unique<testing::MockPageDiscarder>());
      graph->PassToGraph(std::move(page_discarding_helper));
    });
  }

  void TearDown() override {
    DeleteContents();
    pm_harness_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CreateManager() { manager_.reset(new PerformanceDetectionManager()); }

  PerformanceDetectionManager* manager() {
    return PerformanceDetectionManager::GetInstance();
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

TEST_F(PerformanceDetectionManagerTest, UpdatedActionableTabsSentToObservers) {
  CreateManager();

  PerformanceDetectionManagerActionableTabObserver observer;
  PerformanceDetectionManager::ResourceTypeSet resources;
  resources.Put(PerformanceDetectionManager::ResourceType::kCpu);
  manager()->AddActionableTabsObserver(resources, &observer);

  EXPECT_TRUE(observer.resource_type().has_value());
  EXPECT_TRUE(observer.actionable_tabs().has_value());

  EXPECT_EQ(observer.resource_type().value(),
            PerformanceDetectionManager::ResourceType::kCpu);
  EXPECT_TRUE(observer.actionable_tabs().value().empty());
  manager()->RemoveActionableTabsObserver(&observer);
}

TEST_F(PerformanceDetectionManagerTest, DiscardMetricsRecorded) {
  CreateManager();
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  manager()->DiscardTabs(
      {}, base::BindOnce([](base::RepeatingClosure quit_closure,
                            bool did_discard) { quit_closure.Run(); },
                         run_loop.QuitClosure()));
  run_loop.Run();
  const std::string health_status_prefix =
      "PerformanceControls.Intervention.BackgroundTab.Cpu."
      "HealthStatusAfterDiscard.";
  const std::string one_minute_metric =
      base::StrCat({health_status_prefix, "1Min"});
  const std::string two_minutes_metric =
      base::StrCat({health_status_prefix, "2Min"});
  const std::string four_minutes_metric =
      base::StrCat({health_status_prefix, "4Min"});

  // Immediately after discard, we shouldn't record any health measurements
  histogram_tester.ExpectBucketCount(
      one_minute_metric, PerformanceDetectionManager::HealthLevel::kHealthy, 0);
  histogram_tester.ExpectBucketCount(
      two_minutes_metric, PerformanceDetectionManager::HealthLevel::kHealthy,
      0);
  histogram_tester.ExpectBucketCount(
      four_minutes_metric, PerformanceDetectionManager::HealthLevel::kHealthy,
      0);

  // One minute have elapsed since discard
  task_environment()->FastForwardBy(base::Minutes(1));
  histogram_tester.ExpectBucketCount(
      one_minute_metric, PerformanceDetectionManager::HealthLevel::kHealthy, 1);
  histogram_tester.ExpectBucketCount(
      two_minutes_metric, PerformanceDetectionManager::HealthLevel::kHealthy,
      0);
  histogram_tester.ExpectBucketCount(
      four_minutes_metric, PerformanceDetectionManager::HealthLevel::kHealthy,
      0);

  // Two minutes have elapsed since discard
  task_environment()->FastForwardBy(base::Minutes(1));
  histogram_tester.ExpectBucketCount(
      one_minute_metric, PerformanceDetectionManager::HealthLevel::kHealthy, 1);
  histogram_tester.ExpectBucketCount(
      two_minutes_metric, PerformanceDetectionManager::HealthLevel::kHealthy,
      1);
  histogram_tester.ExpectBucketCount(
      four_minutes_metric, PerformanceDetectionManager::HealthLevel::kHealthy,
      0);

  // Four minutes have elapsed since discard
  task_environment()->FastForwardBy(base::Minutes(2));
  histogram_tester.ExpectBucketCount(
      one_minute_metric, PerformanceDetectionManager::HealthLevel::kHealthy, 1);
  histogram_tester.ExpectBucketCount(
      two_minutes_metric, PerformanceDetectionManager::HealthLevel::kHealthy,
      1);
  histogram_tester.ExpectBucketCount(
      four_minutes_metric, PerformanceDetectionManager::HealthLevel::kHealthy,
      1);
}
}  // namespace performance_manager::user_tuning
