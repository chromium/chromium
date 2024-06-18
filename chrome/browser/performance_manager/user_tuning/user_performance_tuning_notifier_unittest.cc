// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/user_performance_tuning_notifier.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

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

    int tab_count_threshold_reached_count_ = 0;
    int memory_percent_threshold_reached_count_ = 0;
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
}  // namespace performance_manager::user_tuning
