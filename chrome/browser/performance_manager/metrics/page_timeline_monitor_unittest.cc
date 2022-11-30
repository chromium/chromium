// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/page_timeline_monitor.h"
#include <memory>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/performance_manager/public/mojom/lifecycle.mojom-shared.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::metrics {

class PageTimelineMonitorUnitTest : public GraphTestHarness {
 public:
  PageTimelineMonitorUnitTest() = default;
  ~PageTimelineMonitorUnitTest() override = default;
  PageTimelineMonitorUnitTest(const PageTimelineMonitorUnitTest& other) =
      delete;
  PageTimelineMonitorUnitTest& operator=(const PageTimelineMonitorUnitTest&) =
      delete;

  void SetUp() override {
    GraphTestHarness::SetUp();
    std::unique_ptr<PageTimelineMonitor> monitor =
        std::make_unique<PageTimelineMonitor>(
            base::BindRepeating([]() { return true; }));
    monitor_ = monitor.get();
    graph()->PassToGraph(std::move(monitor));
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void TearDown() override {
    test_ukm_recorder_.reset();
    GraphTestHarness::TearDown();
  }

  // To allow tests to call its methods and view its state.
  raw_ptr<PageTimelineMonitor> monitor_;

 protected:
  ukm::TestUkmRecorder* test_ukm_recorder() { return test_ukm_recorder_.get(); }
  PageTimelineMonitor* monitor() { return monitor_; }

  void TriggerCollectSlice() { monitor_->CollectSlice(); }

 private:
  std::unique_ptr<ukm::TestUkmRecorder> test_ukm_recorder_;
};

TEST_F(PageTimelineMonitorUnitTest, TestPageTimeline) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kRunning);

  TriggerCollectSlice();

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_GE(entries.size(), static_cast<const unsigned long>(1));
}

TEST_F(PageTimelineMonitorUnitTest, TestPageTimelineNavigation) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id =
      ukm::AssignNewSourceId();  // ukm::NoURLSourceId();
  ukm::SourceId mock_source_id_2 = ukm::AssignNewSourceId();

  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kRunning);

  TriggerCollectSlice();
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_TRUE(entries.size() == 1);

  mock_graph.page->SetUkmSourceId(mock_source_id_2);

  TriggerCollectSlice();
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_TRUE(entries.size() == 2);

  std::vector<ukm::SourceId> ids;
  for (const ukm::mojom::UkmEntry* entry : entries) {
    ids.push_back(entry->source_id);
  }

  EXPECT_NE(ids[0], ids[1]);
}

TEST_F(PageTimelineMonitorUnitTest, TestOnlyRecordTabs) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kRunning);

  TriggerCollectSlice();

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_TRUE(entries.size() == 0);
}

TEST_F(PageTimelineMonitorUnitTest, TestUpdateFaviconInBackground) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetIsVisible(false);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kRunning);

  monitor()->OnIsVisibleChanged(mock_graph.page.get());
  monitor()->OnPageLifecycleStateChanged(mock_graph.page.get());
  monitor()->OnFaviconUpdated(mock_graph.page.get());

  EXPECT_TRUE(monitor()
                  ->page_node_info_map_[mock_graph.page.get()]
                  ->updated_title_or_favicon_in_background);
}

TEST_F(PageTimelineMonitorUnitTest, TestUpdateTitleInBackground) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetIsVisible(false);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kRunning);

  monitor()->OnIsVisibleChanged(mock_graph.page.get());
  monitor()->OnPageLifecycleStateChanged(mock_graph.page.get());
  monitor()->OnTitleUpdated(mock_graph.page.get());

  EXPECT_TRUE(monitor()
                  ->page_node_info_map_[mock_graph.page.get()]
                  ->updated_title_or_favicon_in_background);
}

TEST_F(PageTimelineMonitorUnitTest, TestUpdateLifecycleState) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kFrozen);
  mock_graph.page->SetIsVisible(false);

  EXPECT_EQ(
      monitor()->page_node_info_map_[mock_graph.page.get()]->current_lifecycle,
      mojom::LifecycleState::kFrozen);
}

}  // namespace performance_manager::metrics
