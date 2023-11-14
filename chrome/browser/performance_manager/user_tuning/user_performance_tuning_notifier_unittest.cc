// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/user_performance_tuning_notifier.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/public/decorators/process_metrics_decorator.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

    void NotifyMemoryMetricsRefreshed(
        ProxyAndPmfKbVector proxies_and_pmf) override {
      pages_pmf_kb_.clear();

      base::ranges::transform(proxies_and_pmf,
                              std::back_inserter(pages_pmf_kb_),
                              &std::pair<WebContentsProxy, uint64_t>::second);
      ++memory_refreshed_count_;
    }

    int tab_count_threshold_reached_count_ = 0;
    int memory_percent_threshold_reached_count_ = 0;
    int memory_refreshed_count_ = 0;
    std::vector<uint64_t> pages_pmf_kb_;
  };

  class TestProcessMetricsDecorator : public ProcessMetricsDecorator {
   public:
    void RequestProcessesMemoryMetrics(
        bool immediate_request,
        ProcessMemoryDumpCallback callback) override {
      if (immediate_request) {
        ++request_immediate_metrics_count_;
      }
      ProcessMetricsDecorator::RequestProcessesMemoryMetrics(
          immediate_request, std::move(callback));
    }

    int request_immediate_metrics_count_ = 0;
  };

  void SetUp() override {
    GraphTestHarness::SetUp();

    auto decorator = std::make_unique<TestProcessMetricsDecorator>();
    decorator_ = decorator.get();
    graph()->PassToGraph(std::move(decorator));

    auto receiver = std::make_unique<TestReceiver>();
    receiver_ = receiver.get();

    auto notifier = std::make_unique<UserPerformanceTuningNotifier>(
        std::move(receiver), /*memory_theshold_kb=*/10,
        /*tab_count_threshold=*/2);
    graph()->PassToGraph(std::move(notifier));
  }

  raw_ptr<TestProcessMetricsDecorator> decorator_;
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
  auto process1 = CreateNode<ProcessNodeImpl>();
  auto page1 = CreateNode<PageNodeImpl>();
  auto frame1 = CreateFrameNodeAutoId(process1.get(), page1.get());
  frame1->SetPrivateFootprintKbEstimate(10);

  auto process2 = CreateNode<ProcessNodeImpl>();
  auto page2 = CreateNode<PageNodeImpl>();
  auto frame2 = CreateFrameNodeAutoId(process2.get(), page2.get());
  frame2->SetPrivateFootprintKbEstimate(20);

  SystemNodeImpl::FromNode(graph()->GetSystemNode())
      ->OnProcessMemoryMetricsAvailable();
  EXPECT_EQ(1, receiver_->memory_refreshed_count_);

  std::vector<uint64_t> expected_pmf_kb{
      frame1->GetPrivateFootprintKbEstimate(),
      frame2->GetPrivateFootprintKbEstimate()};
  EXPECT_EQ(std::size(expected_pmf_kb), receiver_->pages_pmf_kb_.size());
  EXPECT_THAT(expected_pmf_kb,
              testing::UnorderedElementsAreArray(receiver_->pages_pmf_kb_));

  // When memory metrics are available again, the notifier should be
  // triggered again
  SystemNodeImpl::FromNode(graph()->GetSystemNode())
      ->OnProcessMemoryMetricsAvailable();
  EXPECT_EQ(2, receiver_->memory_refreshed_count_);
}

TEST_F(UserPerformanceTuningNotifierTest,
       TestRequestImmediateMetricsTriggered) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kMemoryUsageInHovercards,
      {{"memory_update_trigger", "navigation"}});

  // Memory Metrics are available
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  page->SetType(PageType::kTab);
  auto frame = CreateFrameNodeAutoId(process.get(), page.get());
  frame->SetPrivateFootprintKbEstimate(30);

  // No memory refresh should occur while loading.
  page->SetLoadingState(PageNode::LoadingState::kLoading);
  EXPECT_EQ(0, decorator_->request_immediate_metrics_count_);

  // Memory refresh should occur after MainFrameDocumentCommitted.
  page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_EQ(1, decorator_->request_immediate_metrics_count_);
}

}  // namespace performance_manager::user_tuning
