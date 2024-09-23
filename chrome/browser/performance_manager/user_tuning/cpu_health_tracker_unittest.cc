// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/cpu_health_tracker.h"

#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "chrome/browser/performance_manager/user_tuning/profile_discard_opt_out_list_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/performance_manager/test_support/run_in_graph.h"
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

const CpuHealthTracker::CpuPercent kUnhealthySystemCpuUsagePercentage{
    performance_manager::features::kCPUUnhealthyPercentageThreshold.Get() + 1};
const CpuHealthTracker::CpuPercent kDegradedSystemCpuUsagePercentage{
    performance_manager::features::kCPUDegradedHealthPercentageThreshold.Get() +
    1};

class StatusWaiter : public PerformanceDetectionManager::StatusObserver {
 public:
  void OnStatusChanged(PerformanceDetectionManager::ResourceType resource_type,
                       PerformanceDetectionManager::HealthLevel health_level,
                       bool is_actionable) override {
    health_level_ = health_level;
    is_actionable_ = is_actionable;
    if (run_loop_) {
      run_loop_->QuitClosure().Run();
    }
  }

  std::optional<PerformanceDetectionManager::HealthLevel> health_level() {
    return health_level_;
  }

  void Wait() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  std::optional<bool> is_actionable() { return is_actionable_; }

 private:
  std::optional<PerformanceDetectionManager::HealthLevel> health_level_;
  std::optional<bool> is_actionable_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class ActionabilityWaiter
    : public PerformanceDetectionManager::ActionableTabsObserver {
 public:
  void OnActionableTabListChanged(
      PerformanceDetectionManager::ResourceType resource_type,
      std::vector<resource_attribution::PageContext> tabs) override {
    actionable_tabs_ = tabs;
    if (run_loop_) {
      run_loop_->QuitClosure().Run();
    }
  }

  std::optional<std::vector<resource_attribution::PageContext>>
  actionable_tabs() {
    return actionable_tabs_;
  }

  void Wait() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  std::optional<std::vector<resource_attribution::PageContext>>
      actionable_tabs_;
  std::unique_ptr<base::RunLoop> run_loop_;
};
}  // namespace

class CpuHealthTrackerTestHelper {
 public:
  void SetUpGraphObjects() {
    performance_manager::RunInGraph([](Graph* graph) {
      ASSERT_TRUE(!CpuHealthTracker::NothingRegistered(graph));
      // Stop the timer to prevent the cpu probe from recording real CPU
      // data which makes the health status non-deterministic when we
      // fast forward time.
      CpuHealthTracker* health_tracker = CpuHealthTracker::GetFromGraph(graph);
      health_tracker->cpu_probe_timer_.Stop();

      auto page_discarding_helper =
          std::make_unique<policies::PageDiscardingHelper>();
      page_discarding_helper->SetMockDiscarderForTesting(
          std::make_unique<testing::MockPageDiscarder>());
      graph->PassToGraph(std::move(page_discarding_helper));
    });
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

  void ProcessQueryResultMap(
      CpuHealthTracker::CpuPercent system_cpu_usage_percentage,
      resource_attribution::QueryResultMap results) {
    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindOnce(
            [](CpuHealthTracker::CpuPercent system_cpu_usage_percentage,
               resource_attribution::QueryResultMap results, Graph* graph) {
              CpuHealthTracker* const health_tracker =
                  CpuHealthTracker::GetFromGraph(graph);
              CHECK(health_tracker);
              health_tracker->ProcessQueryResultMap(system_cpu_usage_percentage,
                                                    results);
            },
            system_cpu_usage_percentage, results));
  }
};

class CpuHealthTrackerTest : public ChromeRenderViewHostTestHarness,
                             public CpuHealthTrackerTestHelper {
 public:
  CpuHealthTrackerTest()
      : ChromeRenderViewHostTestHarness(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    pm_harness_.SetUp();
    SetContents(CreateTestWebContents());

    performance_manager::RunInGraph(
        [status_change_cb = base::BindPostTask(
             content::GetUIThreadTaskRunner({}),
             status_change_future_.GetRepeatingCallback())](Graph* graph) {
          std::unique_ptr<CpuHealthTracker> cpu_health_tracker =
              std::make_unique<CpuHealthTracker>(std::move(status_change_cb),
                                                 base::DoNothing());

          graph->PassToGraph(std::move(cpu_health_tracker));
        });
    SetUpGraphObjects();
  }

  void TearDown() override {
    DeleteContents();
    pm_harness_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  resource_attribution::PageContext CreatePageContext(
      content::WebContents* web_contents) {
    return resource_attribution::PageContext::FromWebContents(web_contents)
        .value();
  }

  CpuHealthTracker::HealthLevel GetFutureHealthLevel() {
    return std::get<1>(status_change_future_.Take());
  }

 private:
  performance_manager::PerformanceManagerTestHarnessHelper pm_harness_;
  base::test::TestFuture<CpuHealthTracker::ResourceType,
                         CpuHealthTracker::HealthLevel,
                         bool>
      status_change_future_;
};

TEST_F(CpuHealthTrackerTest, RecordCpuAndUpdateHealthStatus) {
  std::unique_ptr<CpuHealthTracker> health_tracker =
      std::make_unique<CpuHealthTracker>(base::DoNothing(), base::DoNothing());

  EXPECT_EQ(health_tracker->GetCurrentHealthLevel(),
            CpuHealthTracker::HealthLevel::kHealthy);

  // Simulate continuously receiving system cpu
  // Health status should remain as healthy since we didn't
  // exceed the number of times for the health to change
  for (int i = 0; i < kNumHealthStatusForChange - 1; i++) {
    health_tracker->RecordAndUpdateHealthStatus(
        kUnhealthySystemCpuUsagePercentage);
    EXPECT_EQ(health_tracker->GetCurrentHealthLevel(),
              CpuHealthTracker::HealthLevel::kHealthy);
  }

  // Status changes after exceeding threshold to be continuously being
  // unhealthy
  health_tracker->RecordAndUpdateHealthStatus(
      kUnhealthySystemCpuUsagePercentage);
  EXPECT_EQ(health_tracker->GetCurrentHealthLevel(),
            CpuHealthTracker::HealthLevel::kUnhealthy);

  // simulate medium but doesn't meet continuous requirement
  health_tracker->RecordAndUpdateHealthStatus(
      kDegradedSystemCpuUsagePercentage);
  EXPECT_EQ(health_tracker->GetCurrentHealthLevel(),
            CpuHealthTracker::HealthLevel::kDegraded);

  // Status should stay as medium even when receiving unhealthy cpu usage
  // since the manager received a medium health status recently and the window
  // is no longer consistently unhealthy
  for (int i = 0; i < kNumHealthStatusForChange - 1; i++) {
    health_tracker->RecordAndUpdateHealthStatus(
        kUnhealthySystemCpuUsagePercentage);
    EXPECT_EQ(health_tracker->GetCurrentHealthLevel(),
              CpuHealthTracker::HealthLevel::kDegraded);
  }

  // Health status should change since we have been consistently unhealthy for a
  // while now
  health_tracker->RecordAndUpdateHealthStatus(
      kUnhealthySystemCpuUsagePercentage);
  EXPECT_EQ(health_tracker->GetCurrentHealthLevel(),
            CpuHealthTracker::HealthLevel::kUnhealthy);

  // Health status stays as medium when oscillating between medium and unhealthy
  health_tracker->RecordAndUpdateHealthStatus(
      kDegradedSystemCpuUsagePercentage);
  EXPECT_EQ(health_tracker->GetCurrentHealthLevel(),
            CpuHealthTracker::HealthLevel::kDegraded);

  health_tracker->RecordAndUpdateHealthStatus(
      kUnhealthySystemCpuUsagePercentage);
  EXPECT_EQ(health_tracker->GetCurrentHealthLevel(),
            CpuHealthTracker::HealthLevel::kDegraded);

  health_tracker->RecordAndUpdateHealthStatus(
      kDegradedSystemCpuUsagePercentage);
  EXPECT_EQ(health_tracker->GetCurrentHealthLevel(),
            CpuHealthTracker::HealthLevel::kDegraded);
}

TEST_F(CpuHealthTrackerTest, CpuStatusUpdates) {
  resource_attribution::QueryResultMap result_map;
  resource_attribution::PageContext page_context =
      CreatePageContext(CreateTestWebContents().get());

  // Exceed threshold for degraded CPU usage for status change
  for (int i = 0; i < kNumHealthStatusForChange; i++) {
    task_environment()->FastForwardBy(base::Seconds(60));
    result_map[page_context] = {
        .cpu_time_result = CreateFakeCpuResult(
            base::Seconds(20 * base::SysInfo::NumberOfProcessors()))};
    ProcessQueryResultMap(kDegradedSystemCpuUsagePercentage, result_map);
  }

  // Verify that the health status changed to degraded and the status change
  // callback was called
  EXPECT_EQ(CpuHealthTracker::HealthLevel::kDegraded, GetFutureHealthLevel());

  // Consistently receive unhealthy CPU usage for status change
  for (int i = 0; i < kNumHealthStatusForChange; i++) {
    task_environment()->FastForwardBy(base::Seconds(1));
    result_map[page_context] = {
        .cpu_time_result = CreateFakeCpuResult(
            base::Seconds(1 * base::SysInfo::NumberOfProcessors()))};
    ProcessQueryResultMap(kUnhealthySystemCpuUsagePercentage, result_map);
  }

  // Verify that the status callback is called when status changed to unhealthy
  EXPECT_EQ(CpuHealthTracker::HealthLevel::kUnhealthy, GetFutureHealthLevel());
}

TEST_F(CpuHealthTrackerTest, HealthyCpuUsageFromProbe) {
  resource_attribution::QueryResultMap result_map;
  resource_attribution::PageContext page_context =
      CreatePageContext(CreateTestWebContents().get());

  // Consistently receive medium CPU usage for status change
  for (int i = 0; i < kNumHealthStatusForChange; i++) {
    task_environment()->FastForwardBy(base::Seconds(60));
    result_map[page_context] = {
        .cpu_time_result = CreateFakeCpuResult(
            base::Seconds(20 * base::SysInfo::NumberOfProcessors()))};

    ProcessQueryResultMap(kDegradedSystemCpuUsagePercentage, result_map);
  }

  EXPECT_EQ(CpuHealthTracker::HealthLevel::kDegraded, GetFutureHealthLevel());

  performance_manager::RunInGraph([](Graph* graph) {
    CpuHealthTracker* const health_tracker =
        CpuHealthTracker::GetFromGraph(graph);
    CHECK(health_tracker);
    for (int i = 0; i < kNumHealthStatusForChange; i++) {
      health_tracker->ProcessCpuProbeResult(system_cpu::CpuSample{0});
    }
  });

  EXPECT_EQ(CpuHealthTracker::HealthLevel::kHealthy, GetFutureHealthLevel());
}

class CpuHealthTrackerBrowserTest : public BrowserWithTestWindowTest,
                                    public CpuHealthTrackerTestHelper {
 public:
  CpuHealthTrackerBrowserTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~CpuHealthTrackerBrowserTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    pm_harness_.SetUp();
    manager_.reset(new PerformanceDetectionManager());
    SetUpGraphObjects();
    performance_manager::RunInGraph(
        [context_id = browser()->profile()->UniqueId()](Graph* graph) {
          policies::PageDiscardingHelper* const discard_helper =
              policies::PageDiscardingHelper::GetFromGraph(graph);
          CHECK(discard_helper);
          discard_helper->SetNoDiscardPatternsForProfile(context_id, {});
        });

    helper_ = std::make_unique<ProfileDiscardOptOutListHelper>();
    helper_->OnProfileAdded(browser()->profile());
    AddTab(browser(), GURL("http://a.com"));
  }

  void TearDown() override {
    helper_->OnProfileWillBeRemoved(browser()->profile());
    pm_harness_.TearDown();
    BrowserWithTestWindowTest::TearDown();
  }

  PerformanceDetectionManager* manager() {
    return PerformanceDetectionManager::GetInstance();
  }

  // Adds a tab at index 0 that is in the background. The current active tab
  // will be the tab at the highest index.
  resource_attribution::PageContext AddBackgroundTab(std::string url,
                                                     Browser* browser) {
    AddTab(browser, GURL(url));
    TabStripModel* const tab_strip_model = browser->tab_strip_model();
    const int num_tabs = tab_strip_model->count();
    CHECK_GT(num_tabs, 0);
    // Activate tab at doesn't hide the newly added tab so we manually hide the
    // tab to make it eligible for discarding
    tab_strip_model->ActivateTabAt(num_tabs - 1);
    content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(0);
    web_contents->WasHidden();
    return resource_attribution::PageContext::FromWebContents(web_contents)
        .value();
  }

  // Have the CpuHealthTracker process a QueryResultMap that contains
  // 0 CPU for all current tabs to establish a baseline for subsequent CPU
  // result processing.
  void StartFirstCpuInterval() {
    TabStripModel* const tab_strip = browser()->tab_strip_model();
    task_environment()->FastForwardBy(base::Seconds(60));
    resource_attribution::QueryResultMap result_map;
    for (int i = 0; i < tab_strip->count(); i++) {
      content::WebContents* const web_contents = tab_strip->GetWebContentsAt(i);
      resource_attribution::PageContext context =
          resource_attribution::PageContext::FromWebContents(web_contents)
              .value();
      result_map[context] = {.cpu_time_result =
                                 CreateFakeCpuResult(base::Seconds(0))};
    }

    ProcessQueryResultMap(CpuHealthTracker::CpuPercent(0), result_map);
  }

  // Simulate that the CpuHealthTracker has consistently receive CPU
  // measurements so that it is in the `health_level` state.
  void SetHealthLevel(PerformanceDetectionManager::HealthLevel health_level) {
    CpuHealthTracker::CpuPercent cpu_usage{0};
    switch (health_level) {
      case PerformanceDetectionManager::HealthLevel::kUnhealthy:
        cpu_usage = kUnhealthySystemCpuUsagePercentage;
        break;
      case PerformanceDetectionManager::HealthLevel::kDegraded:
        cpu_usage = kDegradedSystemCpuUsagePercentage;
        break;
      case PerformanceDetectionManager::HealthLevel::kHealthy:
        cpu_usage = CpuHealthTracker::CpuPercent(0);
        break;
    }

    for (int i = 0; i < kNumHealthStatusForChange; i++) {
      ProcessQueryResultMap(cpu_usage, {});
    }
  }

 private:
  std::unique_ptr<PerformanceDetectionManager> manager_;
  PerformanceManagerTestHarnessHelper pm_harness_;
  std::unique_ptr<ProfileDiscardOptOutListHelper> helper_;
};

TEST_F(CpuHealthTrackerBrowserTest, HealthStatusUpdates) {
  resource_attribution::PageContext first_page_context =
      AddBackgroundTab("http://b.com", browser());
  StartFirstCpuInterval();

  StatusWaiter observer;
  manager()->AddStatusObserver(
      {PerformanceDetectionManager::ResourceType::kCpu}, &observer);

  EXPECT_EQ(observer.health_level().value(),
            PerformanceDetectionManager::HealthLevel::kHealthy);
  EXPECT_FALSE(observer.is_actionable().value());

  // Simulate constantly receiving a tab is using high cpu
  for (int i = 0; i < kNumHealthStatusForChange; i++) {
    task_environment()->FastForwardBy(base::Seconds(60));
    resource_attribution::QueryResultMap result_map;
    result_map[first_page_context] = {
        .cpu_time_result = CreateFakeCpuResult(
            base::Seconds(kUnhealthySystemCpuUsagePercentage.value() *
                          base::SysInfo::NumberOfProcessors()))};
    ProcessQueryResultMap(kUnhealthySystemCpuUsagePercentage, result_map);
  }
  observer.Wait();

  EXPECT_EQ(observer.health_level().value(),
            PerformanceDetectionManager::HealthLevel::kUnhealthy);
  EXPECT_TRUE(observer.is_actionable().value());
  manager()->RemoveStatusObserver(&observer);
}

TEST_F(CpuHealthTrackerBrowserTest, PagesMeetMinimumCpuUsage) {
  base::flat_map<resource_attribution::PageContext,
                 CpuHealthTracker::CpuPercent>
      page_contexts_cpu;

  // Populate a map of page contexts with CPU usage below the minimum required
  // to be considered as actionable.
  for (int i = 0; i < 3; i++) {
    resource_attribution::PageContext page_context =
        AddBackgroundTab("http://b.com", browser());
    page_contexts_cpu.insert(
        {page_context,
         CpuHealthTracker::CpuPercent(
             performance_manager::features::kMinimumActionableTabCPUPercentage
                 .Get() -
             1)});
  }

  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::flat_map<resource_attribution::PageContext,
                            CpuHealthTracker::CpuPercent> page_contexts_cpu,
             Graph* graph) {
            CpuHealthTracker::GetFromGraph(graph)->GetFilteredActionableTabs(
                page_contexts_cpu,
                CpuHealthTracker::CpuPercent(
                    performance_manager::features::
                        kCPUDegradedHealthPercentageThreshold.Get()),
                base::BindOnce(
                    [](CpuHealthTracker::ActionableTabsResult result) {
                      // The actionable tab list should be empty because each
                      // page's CPU usage is below the minimum needed to  be
                      // considered as actionable.
                      EXPECT_TRUE(result.empty());
                    }));
          },
          std::move(page_contexts_cpu)));
}

// The PerformanceDetectionManager should properly notify observers
// when a tab is actionable.
TEST_F(CpuHealthTrackerBrowserTest, UpdateActionableTabs) {
  resource_attribution::PageContext first_page_context =
      AddBackgroundTab("http://b.com", browser());
  StartFirstCpuInterval();
  SetHealthLevel(PerformanceDetectionManager::HealthLevel::kDegraded);

  ActionabilityWaiter observer;
  manager()->AddActionableTabsObserver(
      {PerformanceDetectionManager::ResourceType::kCpu}, &observer);

  task_environment()->FastForwardBy(base::Seconds(60));
  resource_attribution::QueryResultMap result_map;
  result_map[first_page_context] = {
      .cpu_time_result = CreateFakeCpuResult(
          base::Seconds(20 * base::SysInfo::NumberOfProcessors()))};

  // Wait for the the initial actionability when adding the observer
  ProcessQueryResultMap(kDegradedSystemCpuUsagePercentage, result_map);
  observer.Wait();

  std::vector<resource_attribution::PageContext> actionable_tabs =
      observer.actionable_tabs().value();

  EXPECT_EQ(actionable_tabs.size(), 1u);
  EXPECT_EQ(actionable_tabs.front(), first_page_context);
  manager()->RemoveActionableTabsObserver(&observer);
}

// When multiple tabs are eligible to improve CPU health, the tab with the
// higher CPU usage is sent to observers
TEST_F(CpuHealthTrackerBrowserTest, HigherCPUTabIsActionable) {
  resource_attribution::PageContext first_page_context =
      AddBackgroundTab("http://b.com", browser());
  resource_attribution::PageContext second_page_context =
      AddBackgroundTab("http://c.com", browser());
  StartFirstCpuInterval();
  SetHealthLevel(PerformanceDetectionManager::HealthLevel::kDegraded);

  task_environment()->FastForwardBy(base::Seconds(60));
  resource_attribution::QueryResultMap result_map;
  result_map[first_page_context] = {
      .cpu_time_result = CreateFakeCpuResult(
          base::Seconds(20 * base::SysInfo::NumberOfProcessors()))};
  result_map[second_page_context] = {
      .cpu_time_result = CreateFakeCpuResult(
          base::Seconds(60 * base::SysInfo::NumberOfProcessors()))};

  ActionabilityWaiter observer;
  manager()->AddActionableTabsObserver(
      {PerformanceDetectionManager::ResourceType::kCpu}, &observer);
  ProcessQueryResultMap(kDegradedSystemCpuUsagePercentage, result_map);
  observer.Wait();

  std::vector<resource_attribution::PageContext> actionable_tabs =
      observer.actionable_tabs().value();

  EXPECT_EQ(actionable_tabs.size(), 1u);
  EXPECT_EQ(actionable_tabs.front(), second_page_context);
  manager()->RemoveActionableTabsObserver(&observer);
}

// Verify that the PerformanceDetectionManager notifies observers when the
// actionable tab list changes from having actionable tabs to no tabs are
// actionable.
TEST_F(CpuHealthTrackerBrowserTest, NotifyWhenNoTabsAreActionable) {
  resource_attribution::PageContext first_page_context =
      AddBackgroundTab("http://b.com", browser());
  StartFirstCpuInterval();
  SetHealthLevel(PerformanceDetectionManager::HealthLevel::kUnhealthy);

  // Simulate constantly receiving a tab is using high cpu
  ActionabilityWaiter observer;
  manager()->AddActionableTabsObserver(
      {PerformanceDetectionManager::ResourceType::kCpu}, &observer);

  task_environment()->FastForwardBy(base::Seconds(60));
  resource_attribution::QueryResultMap result_map;
  result_map[first_page_context] = {
      .cpu_time_result = CreateFakeCpuResult(
          base::Seconds(kUnhealthySystemCpuUsagePercentage.value() *
                        base::SysInfo::NumberOfProcessors()))};

  // Verify that the observer receive previously recorded actionable tab data
  ProcessQueryResultMap(kUnhealthySystemCpuUsagePercentage, result_map);
  observer.Wait();
  std::vector<resource_attribution::PageContext> actionable_tabs =
      observer.actionable_tabs().value();
  EXPECT_EQ(actionable_tabs.size(), 1u);
  EXPECT_EQ(actionable_tabs.front(), first_page_context);

  task_environment()->FastForwardBy(base::Seconds(60));
  result_map[first_page_context] = {
      .cpu_time_result = CreateFakeCpuResult(base::Seconds(
          features::kMinimumActionableTabCPUPercentage.Get() - 1))};

  // Verify that there is no actionable tabs because the first tab's CPU usage
  // is below the minimum needed to be considered as actionable
  ProcessQueryResultMap(kDegradedSystemCpuUsagePercentage, result_map);
  observer.Wait();
  actionable_tabs = observer.actionable_tabs().value();
  EXPECT_TRUE(actionable_tabs.empty());
  manager()->RemoveActionableTabsObserver(&observer);
}

TEST_F(CpuHealthTrackerBrowserTest, NeedMultipleTabsToBeActionable) {
  resource_attribution::PageContext first_page_context =
      AddBackgroundTab("http://b.com", browser());
  resource_attribution::PageContext second_page_context =
      AddBackgroundTab("http://c.com", browser());
  StartFirstCpuInterval();
  SetHealthLevel(PerformanceDetectionManager::HealthLevel::kUnhealthy);

  task_environment()->FastForwardBy(base::Seconds(60));
  resource_attribution::QueryResultMap result_map;
  const int cpu_time =
      features::kMinimumActionableTabCPUPercentage.Get() / 100.0 * 60;
  result_map[first_page_context] = {
      .cpu_time_result = CreateFakeCpuResult(
          base::Seconds((cpu_time + 1) * base::SysInfo::NumberOfProcessors()))};
  result_map[second_page_context] = {
      .cpu_time_result = CreateFakeCpuResult(
          base::Seconds((cpu_time + 2) * base::SysInfo::NumberOfProcessors()))};

  ActionabilityWaiter observer;
  manager()->AddActionableTabsObserver(
      {PerformanceDetectionManager::ResourceType::kCpu}, &observer);
  ProcessQueryResultMap(
      CpuHealthTracker::CpuPercent(
          features::kCPUUnhealthyPercentageThreshold.Get() +
          (2 * features::kMinimumActionableTabCPUPercentage.Get())),
      result_map);
  observer.Wait();

  std::vector<resource_attribution::PageContext> actionable_tabs =
      observer.actionable_tabs().value();

  EXPECT_EQ(actionable_tabs.size(), 2u);
  EXPECT_EQ(actionable_tabs.front(), second_page_context);
  EXPECT_EQ(actionable_tabs.back(), first_page_context);
  manager()->RemoveActionableTabsObserver(&observer);
}

// Tabs on the discard exceptions list should not be actionable
TEST_F(CpuHealthTrackerBrowserTest, ActionableTabsRespectExceptionsList) {
  resource_attribution::PageContext first_page_context =
      AddBackgroundTab("http://b.com", browser());
  resource_attribution::PageContext second_page_context =
      AddBackgroundTab("http://c.com", browser());
  StartFirstCpuInterval();
  SetHealthLevel(PerformanceDetectionManager::HealthLevel::kUnhealthy);

  performance_manager::user_tuning::prefs::AddSiteToTabDiscardExceptionsList(
      browser()->profile()->GetPrefs(), "c.com");

  task_environment()->FastForwardBy(base::Seconds(60));
  resource_attribution::QueryResultMap result_map;
  result_map[first_page_context] = {
      .cpu_time_result = CreateFakeCpuResult(
          base::Seconds(20 * base::SysInfo::NumberOfProcessors()))};
  result_map[second_page_context] = {
      .cpu_time_result = CreateFakeCpuResult(
          base::Seconds(30 * base::SysInfo::NumberOfProcessors()))};

  ActionabilityWaiter observer;
  manager()->AddActionableTabsObserver(
      {PerformanceDetectionManager::ResourceType::kCpu}, &observer);
  ProcessQueryResultMap(kUnhealthySystemCpuUsagePercentage, result_map);
  observer.Wait();

  std::vector<resource_attribution::PageContext> actionable_tabs =
      observer.actionable_tabs().value();

  // Even though the second page is hidden and using more CPU, it should not be
  // included in the actionable tab list since it is already on the discard
  // exception list
  EXPECT_EQ(actionable_tabs.size(), 1u);
  EXPECT_EQ(actionable_tabs.front(), first_page_context);
  manager()->RemoveActionableTabsObserver(&observer);
}

TEST_F(CpuHealthTrackerBrowserTest, ActionableTabsIgnoreIncognitoTabs) {
  Profile* const default_profile = profile();
  Profile* const incognito_profile =
      default_profile->GetPrimaryOTRProfile(true);
  auto browser_window = CreateBrowserWindow();
  auto incognito_browser = CreateBrowser(
      incognito_profile, Browser::TYPE_NORMAL, false, browser_window.get());
  AddTab(incognito_browser.get(), GURL("http://a.com"));

  // This is usually called when the profile is created. Fake it here since it
  // doesn't happen in tests.
  RunInGraph([&](Graph* graph) {
    policies::PageDiscardingHelper::GetFromGraph(graph)
        ->SetNoDiscardPatternsForProfile(incognito_profile->UniqueId(), {});
  });

  resource_attribution::PageContext default_page_context =
      AddBackgroundTab("http://b.com", browser());
  resource_attribution::PageContext incognito_page_context =
      AddBackgroundTab("http://c.com", incognito_browser.get());
  StartFirstCpuInterval();
  SetHealthLevel(PerformanceDetectionManager::HealthLevel::kUnhealthy);

  task_environment()->FastForwardBy(base::Seconds(60));
  resource_attribution::QueryResultMap result_map;
  result_map[default_page_context] = {
      .cpu_time_result = CreateFakeCpuResult(
          base::Seconds(20 * base::SysInfo::NumberOfProcessors()))};
  result_map[incognito_page_context] = {
      .cpu_time_result = CreateFakeCpuResult(
          base::Seconds(30 * base::SysInfo::NumberOfProcessors()))};

  ActionabilityWaiter observer;
  manager()->AddActionableTabsObserver(
      {PerformanceDetectionManager::ResourceType::kCpu}, &observer);
  ProcessQueryResultMap(kUnhealthySystemCpuUsagePercentage, result_map);
  observer.Wait();

  std::vector<resource_attribution::PageContext> actionable_tabs =
      observer.actionable_tabs().value();

  // Even though the incognito page is hidden and using more CPU, it should not
  // be included in the actionable tab list because it is an incognito tab.
  EXPECT_EQ(actionable_tabs.size(), 1u);
  EXPECT_EQ(actionable_tabs.front(), default_page_context);
  manager()->RemoveActionableTabsObserver(&observer);

  incognito_browser->tab_strip_model()->CloseAllTabs();
  incognito_browser.reset();
}

}  // namespace performance_manager::user_tuning
