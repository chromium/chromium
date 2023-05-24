// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/user_performance_tuning_notifier.h"

#include <memory>
#include <utility>
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/performance_controls/resource_usage_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_contents_factory.h"
#include "url/gurl.h"

namespace performance_manager::user_tuning {

class UserPerformanceTuningNotifierTest : public GraphTestHarness {
 public:
  class TestReceiver : public UserPerformanceTuningNotifier::Receiver {
   public:
    ~TestReceiver() override = default;
    void NotifyTabCountThresholdReached() override {
      ++tab_count_threshold_reached_count_;
    }

    void NotifyMemoryThresholdReached() override {
      ++memory_percent_threshold_reached_count_;
    }

    void NotifyMemoryMetricsRefreshed() override { ++memory_refreshed_count_; }

    int tab_count_threshold_reached_count_ = 0;
    int memory_percent_threshold_reached_count_ = 0;
    int memory_refreshed_count_ = 0;
  };

  void SetUp() override {
    GraphTestHarness::SetUp();

    graph()->PassToGraph(
        std::make_unique<performance_manager::ProcessMetricsDecorator>());

    auto receiver = std::make_unique<TestReceiver>();
    receiver_ = receiver.get();

    auto notifier = std::make_unique<UserPerformanceTuningNotifier>(
        std::move(receiver), /*memory_theshold_kb=*/10,
        /*tab_count_threshold=*/2);
    graph()->PassToGraph(std::move(notifier));
  }

  raw_ptr<TestReceiver> receiver_;
};

TEST_F(UserPerformanceTuningNotifierTest, TestTabsThresholdTriggered) {
  EXPECT_EQ(0, receiver_->tab_count_threshold_reached_count_);

  // The threshold is 2, so having one tab doesn't trigger it.
  auto page1 = CreateNode<PageNodeImpl>();
  page1->SetType(PageType::kTab);
  EXPECT_EQ(0, receiver_->tab_count_threshold_reached_count_);

  {
    // Reaching the threshold triggers the event exactly once.
    auto page2 = CreateNode<PageNodeImpl>();
    page2->SetType(PageType::kTab);
    EXPECT_EQ(1, receiver_->tab_count_threshold_reached_count_);

    // Adding more tabs past the threshold won't trigger the event again.
    auto page3 = CreateNode<PageNodeImpl>();
    page3->SetType(PageType::kTab);
    EXPECT_EQ(1, receiver_->tab_count_threshold_reached_count_);
  }

  // Here `page2` and `page3` have been removed, so the tab count is back under
  // the threshold. Adding another tab will trigger it again.
  auto page4 = CreateNode<PageNodeImpl>();
  page4->SetType(PageType::kTab);
  EXPECT_EQ(2, receiver_->tab_count_threshold_reached_count_);
}

TEST_F(UserPerformanceTuningNotifierTest, TestOnlyTabsCount) {
  // Pages are created as type unknown. Only tabs count towards the threshold.
  auto page1 = CreateNode<PageNodeImpl>();
  auto page2 = CreateNode<PageNodeImpl>();
  auto page3 = CreateNode<PageNodeImpl>();
  EXPECT_EQ(0, receiver_->tab_count_threshold_reached_count_);
}

TEST_F(UserPerformanceTuningNotifierTest, TestMemoryThresholdTriggered) {
  auto process1 = CreateNode<ProcessNodeImpl>();
  process1->set_resident_set_kb(8);
  SystemNodeImpl::FromNode(graph()->GetSystemNode())
      ->OnProcessMemoryMetricsAvailable();
  EXPECT_EQ(0, receiver_->memory_percent_threshold_reached_count_);

  auto process2 = CreateNode<ProcessNodeImpl>();
  process2->set_resident_set_kb(5);
  SystemNodeImpl::FromNode(graph()->GetSystemNode())
      ->OnProcessMemoryMetricsAvailable();
  EXPECT_EQ(1, receiver_->memory_percent_threshold_reached_count_);

  // Staying above the threshold doesn't re-trigger it.
  auto process3 = CreateNode<ProcessNodeImpl>();
  process3->set_resident_set_kb(5);
  SystemNodeImpl::FromNode(graph()->GetSystemNode())
      ->OnProcessMemoryMetricsAvailable();
  EXPECT_EQ(1, receiver_->memory_percent_threshold_reached_count_);
}

TEST_F(UserPerformanceTuningNotifierTest, TestMemoryAvailableTriggered) {
  // Memory Metrics are available
  SystemNodeImpl::FromNode(graph()->GetSystemNode())
      ->OnProcessMemoryMetricsAvailable();
  EXPECT_EQ(1, receiver_->memory_refreshed_count_);

  // When memory metrics are available again, the notifier should be
  // triggered again
  SystemNodeImpl::FromNode(graph()->GetSystemNode())
      ->OnProcessMemoryMetricsAvailable();
  EXPECT_EQ(2, receiver_->memory_refreshed_count_);
}

class UserPerformanceTuningNotifierWithWebContentsTest
    : public ChromeRenderViewHostTestHarness {
 public:
  class TestReceiverWithCallback
      : public UserPerformanceTuningNotifier::Receiver {
   public:
    ~TestReceiverWithCallback() override = default;

    void NotifyTabCountThresholdReached() override {}

    void NotifyMemoryThresholdReached() override {}

    void NotifyMemoryMetricsRefreshed() override {
      if (metrics_refreshed_callback_) {
        content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
            ->PostTask(FROM_HERE, std::move(metrics_refreshed_callback_));
      }
    }

    void SetMemoryMetricsRefreshedCallback(
        base::OnceClosure metrics_refreshed_callback) {
      metrics_refreshed_callback_ = std::move(metrics_refreshed_callback);
    }

    base::OnceClosure metrics_refreshed_callback_;
  };

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    pm_helper_.SetUp();

    base::RunLoop run_loop;
    auto receiver = std::make_unique<TestReceiverWithCallback>();
    receiver_ = receiver.get();
    PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindLambdaForTesting([&](Graph* graph) {
          graph->PassToGraph(std::make_unique<ProcessMetricsDecorator>());
          auto notifier = std::make_unique<UserPerformanceTuningNotifier>(
              std::move(receiver), /*memory_theshold_kb=*/10,
              /*tab_count_threshold=*/2);
          graph->PassToGraph(std::move(notifier));
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  void TearDown() override {
    pm_helper_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  PerformanceManagerTestHarnessHelper pm_helper_;
  raw_ptr<TestReceiverWithCallback> receiver_;
};

TEST_F(UserPerformanceTuningNotifierWithWebContentsTest,
       TestWritingMemoryUsageToTabHelper) {
  // Enable memory usage in hovercards
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      performance_manager::features::kMemoryUsageInHovercards);

  // Set the memory metrics refreshed callback ahead of navigation.
  base::RunLoop run_loop;
  receiver_->SetMemoryMetricsRefreshedCallback(run_loop.QuitClosure());

  // Trigger a navigation so frames are set up.
  SetContents(CreateTestWebContents());
  ResourceUsageTabHelper::CreateForWebContents(web_contents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.example.com/"));

  // Set active memory usage on the frame and process memory metrics.
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([&](Graph* graph) {
        for (const FrameNode* node : graph->GetAllFrameNodes()) {
          FrameNodeImpl::FromNode(node)->SetPrivateFootprintKbEstimate(11);
        }
        SystemNodeImpl::FromNode(graph->GetSystemNode())
            ->OnProcessMemoryMetricsAvailable();
      }));
  run_loop.Run();

  // Memory usage should be written to the tab helper.
  auto* tab_helper = ResourceUsageTabHelper::FromWebContents(web_contents());
  EXPECT_EQ(tab_helper->GetMemoryUsageInBytes(), 11u * 1024);
}

}  // namespace performance_manager::user_tuning
