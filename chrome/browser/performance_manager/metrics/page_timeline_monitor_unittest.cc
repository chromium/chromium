// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/page_timeline_monitor.h"
#include <memory>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/mojom/lifecycle.mojom-shared.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/performance_manager/policies/high_efficiency_mode_policy.h"
#endif  // !BUILDFLAG(IS_ANDROID)

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

#if !BUILDFLAG(IS_ANDROID)
TEST_F(PageTimelineMonitorUnitTest, TestHighEfficiencyMode) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kRunning);

  // Collecting without an installed HEM policy reports it as disabled.
  TriggerCollectSlice();
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 1UL);
  test_ukm_recorder()->ExpectEntryMetric(entries[0], "HighEfficiencyMode", 0);

  graph()->PassToGraph(
      std::make_unique<
          performance_manager::policies::HighEfficiencyModePolicy>());

  TriggerCollectSlice();
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 2UL);
  test_ukm_recorder()->ExpectEntryMetric(entries[1], "HighEfficiencyMode", 0);

  performance_manager::policies::HighEfficiencyModePolicy::GetInstance()
      ->OnHighEfficiencyModeChanged(true);

  TriggerCollectSlice();
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 3UL);

  test_ukm_recorder()->ExpectEntryMetric(entries[2], "HighEfficiencyMode", 1);
}

TEST_F(PageTimelineMonitorUnitTest, TestBatterySaverMode) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kRunning);

  TriggerCollectSlice();
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 1UL);
  test_ukm_recorder()->ExpectEntryMetric(entries[0], "BatterySaverMode", 0);

  monitor()->SetBatterySaverEnabled(true);

  TriggerCollectSlice();
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 2UL);

  test_ukm_recorder()->ExpectEntryMetric(entries[1], "BatterySaverMode", 1);
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(PageTimelineMonitorUnitTest, TestHasNotificationsPermission) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kRunning);

  PageLiveStateDecorator::Data* data =
      PageLiveStateDecorator::Data::GetOrCreateForPageNode(
          mock_graph.page.get());
  data->SetContentSettingsForTesting(
      {{ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW}});

  TriggerCollectSlice();
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 1UL);
  test_ukm_recorder()->ExpectEntryMetric(entries[0],
                                         "HasNotificationPermission", 1);

  data->SetContentSettingsForTesting(
      {{ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_BLOCK}});

  TriggerCollectSlice();
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 2UL);

  test_ukm_recorder()->ExpectEntryMetric(entries[1],
                                         "HasNotificationPermission", 0);
}

TEST_F(PageTimelineMonitorUnitTest, TestCapturingMedia) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kRunning);

  PageLiveStateDecorator::Data* data =
      PageLiveStateDecorator::Data::GetOrCreateForPageNode(
          mock_graph.page.get());
  data->SetIsCapturingVideoForTesting(false);

  TriggerCollectSlice();
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 1UL);
  test_ukm_recorder()->ExpectEntryMetric(entries[0], "IsCapturingMedia", 0);

  data->SetIsCapturingVideoForTesting(true);
  TriggerCollectSlice();
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 2UL);
  test_ukm_recorder()->ExpectEntryMetric(entries[1], "IsCapturingMedia", 1);
}

TEST_F(PageTimelineMonitorUnitTest, TestConnectedToDevice) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kRunning);

  PageLiveStateDecorator::Data* data =
      PageLiveStateDecorator::Data::GetOrCreateForPageNode(
          mock_graph.page.get());
  data->SetIsConnectedToUSBDeviceForTesting(false);

  TriggerCollectSlice();
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 1UL);
  test_ukm_recorder()->ExpectEntryMetric(entries[0], "IsConnectedToDevice", 0);

  data->SetIsConnectedToUSBDeviceForTesting(true);
  TriggerCollectSlice();
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 2UL);
  test_ukm_recorder()->ExpectEntryMetric(entries[1], "IsConnectedToDevice", 1);
}

TEST_F(PageTimelineMonitorUnitTest, TestAudible) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kRunning);

  mock_graph.page->SetIsAudible(false);
  TriggerCollectSlice();
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 1UL);
  test_ukm_recorder()->ExpectEntryMetric(entries[0], "IsPlayingAudio", 0);

  mock_graph.page->SetIsAudible(true);
  TriggerCollectSlice();
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 2UL);
  test_ukm_recorder()->ExpectEntryMetric(entries[1], "IsPlayingAudio", 1);
}

TEST_F(PageTimelineMonitorUnitTest, TestIsActiveTab) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kRunning);

  PageLiveStateDecorator::Data* data =
      PageLiveStateDecorator::Data::GetOrCreateForPageNode(
          mock_graph.page.get());
  data->SetIsActiveTabForTesting(false);

  TriggerCollectSlice();
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 1UL);
  test_ukm_recorder()->ExpectEntryMetric(entries[0], "IsActiveTab", 0);

  data->SetIsActiveTabForTesting(true);
  TriggerCollectSlice();
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 2UL);
  test_ukm_recorder()->ExpectEntryMetric(entries[1], "IsActiveTab", 1);
}

TEST_F(PageTimelineMonitorUnitTest, TestResidentSetSize) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kRunning);
  mock_graph.frame->SetResidentSetKbEstimate(123);

  TriggerCollectSlice();
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 1UL);
  test_ukm_recorder()->ExpectEntryMetric(entries[0], "ResidentSetSize", 123);
}

TEST_F(PageTimelineMonitorUnitTest, TestUpdatePageNodeBeforeTypeChange) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetIsVisible(false);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kFrozen);
  mock_graph.page->SetType(performance_manager::PageType::kTab);

  EXPECT_EQ(
      monitor()->page_node_info_map_[mock_graph.page.get()]->current_lifecycle,
      mojom::LifecycleState::kFrozen);
  EXPECT_EQ(
      monitor()->page_node_info_map_[mock_graph.page.get()]->currently_visible,
      false);

  // making sure no DCHECKs are hit
  TriggerCollectSlice();
}

}  // namespace performance_manager::metrics
