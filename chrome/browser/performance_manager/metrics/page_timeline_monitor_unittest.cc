// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/page_timeline_monitor.h"

#include <map>
#include <memory>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/system/sys_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/metrics/page_timeline_cpu_monitor.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/decorators/tab_page_decorator.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/mojom/lifecycle.mojom-shared.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/resource_attribution/simulated_cpu_measurement_delegate.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/performance_manager/policies/high_efficiency_mode_policy.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace performance_manager::metrics {

namespace {

using PageMeasurementBackgroundState =
    PageTimelineMonitor::PageMeasurementBackgroundState;

}  // namespace

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

    graph()->PassToGraph(
        std::make_unique<performance_manager::TabPageDecorator>());

    // Return 50% CPU used by default.
    cpu_delegate_factory_.SetDefaultCPUUsage(0.5);

    std::unique_ptr<PageTimelineMonitor> monitor =
        std::make_unique<PageTimelineMonitor>();
    monitor_ = monitor.get();
    monitor_->SetTriggerCollectionManuallyForTesting();
    monitor_->SetShouldCollectSliceCallbackForTesting(
        base::BindRepeating([]() { return true; }));
    monitor_->cpu_monitor_.SetCPUMeasurementDelegateFactoryForTesting(
        graph(), cpu_delegate_factory_.GetFactoryCallback());
    graph()->PassToGraph(std::move(monitor));
    ResetHistogramTester();
    ResetUkmRecorder();
  }

  void TearDown() override {
    test_ukm_recorder_.reset();
    GraphTestHarness::TearDown();
  }

  // To allow tests to call its methods and view its state.
  raw_ptr<PageTimelineMonitor> monitor_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;

  // Factory to return CPUMeasurementDelegates. This must be deleted after
  // `monitor_` to ensure that it outlives all delegates it creates.
  resource_attribution::SimulatedCPUMeasurementDelegateFactory
      cpu_delegate_factory_;

 protected:
  ukm::TestUkmRecorder* test_ukm_recorder() { return test_ukm_recorder_.get(); }
  PageTimelineMonitor* monitor() { return monitor_; }

  void TriggerCollectSlice() { monitor_->CollectSlice(); }

  void TriggerCollectPageResourceUsage() {
    base::RunLoop run_loop;
    monitor_->CollectPageResourceUsage(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Expects PerformanceManager.PerformanceInterventions.CPU.`name`.`suffix` to
  // contain exactly 1 sample in the `sample_bucket` bucket.
  template <typename T>
  void ExpectCPUHistogram(
      base::StringPiece name,
      base::StringPiece suffix,
      T sample_bucket,
      const base::Location& location = base::Location::Current()) {
    histogram_tester_->ExpectUniqueSample(
        base::StrCat({"PerformanceManager.PerformanceInterventions.CPU.", name,
                      ".", suffix}),
        sample_bucket, 1, location);
  }

  // Expects PerformanceManager.PerformanceInterventions.CPU.`name` to contain
  // exactly 1 sample in the `sample_bucket` bucket.
  template <typename T>
  void ExpectCPUHistogram(
      base::StringPiece name,
      T sample_bucket,
      const base::Location& location = base::Location::Current()) {
    histogram_tester_->ExpectUniqueSample(
        base::StrCat(
            {"PerformanceManager.PerformanceInterventions.CPU.", name}),
        sample_bucket, 1, location);
  }

  // Expects PerformanceManager.PerformanceInterventions.CPU.`name`.`suffix` to
  // contain no samples at all.
  void ExpectNoCPUHistogram(
      base::StringPiece name,
      base::StringPiece suffix,
      const base::Location& location = base::Location::Current()) {
    histogram_tester_->ExpectTotalCount(
        base::StrCat({"PerformanceManager.PerformanceInterventions.CPU.", name,
                      ".", suffix}),
        0, location);
  }

  // Expects PerformanceManager.PerformanceInterventions.CPU.`name` to contain
  // no samples at all.
  void ExpectNoCPUHistogram(
      base::StringPiece name,
      const base::Location& location = base::Location::Current()) {
    histogram_tester_->ExpectTotalCount(
        base::StrCat(
            {"PerformanceManager.PerformanceInterventions.CPU.", name}),
        0, location);
  }

  void ResetHistogramTester() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void ResetUkmRecorder() {
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  // Triggers a metrics collection and tests whether the BackgroundState logged
  // for each ukm::SourceId matches the given expectation, then clears the
  // collected UKM's for the next slice.
  void TestBackgroundStates(
      std::map<ukm::SourceId, PageMeasurementBackgroundState> expected_states);

 private:
  std::unique_ptr<ukm::TestUkmRecorder> test_ukm_recorder_;
};

void PageTimelineMonitorUnitTest::TestBackgroundStates(
    std::map<ukm::SourceId, PageMeasurementBackgroundState> expected_states) {
  TriggerCollectPageResourceUsage();
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  // Expect 1 entry per page.
  EXPECT_EQ(entries.size(), expected_states.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    test_ukm_recorder()->ExpectEntryMetric(
        entry, "BackgroundState",
        static_cast<int64_t>(expected_states.at(entry->source_id)));
  }
  ResetUkmRecorder();
}

// A test that runs with various values of the kUseResourceAttributionCPUMonitor
// feature flag.
class PageTimelineMonitorWithFeatureTest
    : public PageTimelineMonitorUnitTest,
      public ::testing::WithParamInterface<bool> {
 public:
  PageTimelineMonitorWithFeatureTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kPageTimelineMonitor,
             {{"use_resource_attribution_cpu_monitor",
               GetParam() ? "true" : "false"}}},
            {performance_manager::features::kCPUInterventionEvaluationLogging,
             {{"threshold_chrome_cpu_percent", "50"}}},
        },
        {});
  }

  void SetUp() override {
    if (features::kUseResourceAttributionCPUMonitor.Get()) {
      GetGraphFeatures().EnableResourceAttributionScheduler();
    }
    PageTimelineMonitorUnitTest::SetUp();
  }

  void TearDown() override {
    // Destroy `monitor_` before `scoped_feature_list_` so that the feature flag
    // doesn't change during its destructor.
    graph()->TakeFromGraph(monitor_.ExtractAsDangling());
    PageTimelineMonitorUnitTest::TearDown();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PageTimelineMonitorWithFeatureTest,
                         ::testing::Bool());

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
  EXPECT_EQ(entries.size(), 1UL);

  // Unsliced resource usage metrics should not be collected along with the
  // slice.
  auto entries2 = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  EXPECT_TRUE(entries2.empty());
}

TEST_F(PageTimelineMonitorUnitTest, TestPageResourceUsage) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kRunning);

  TriggerCollectPageResourceUsage();

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  EXPECT_EQ(entries.size(), 1UL);

  // Sliced resource usage metrics should not be collected along with
  // PageResourceUsage.
  auto entries2 = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_TRUE(entries2.empty());
}

TEST_F(PageTimelineMonitorUnitTest,
       TestPageTimelineDoesntRecordIfShouldCollectSliceReturnsFalse) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kRunning);

  monitor()->SetShouldCollectSliceCallbackForTesting(
      base::BindRepeating([]() { return false; }));
  TriggerCollectSlice();

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 0UL);
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
  TriggerCollectPageResourceUsage();

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 1UL);
  auto entries2 = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  EXPECT_EQ(entries2.size(), 1UL);

  mock_graph.page->SetUkmSourceId(mock_source_id_2);

  TriggerCollectSlice();
  TriggerCollectPageResourceUsage();

  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 2UL);
  entries2 = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  EXPECT_EQ(entries2.size(), 2UL);

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
  TriggerCollectPageResourceUsage();

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 0UL);
  auto entries2 = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  EXPECT_EQ(entries2.size(), 0UL);
}

TEST_F(PageTimelineMonitorUnitTest, TestUpdateTitleOrFaviconInBackground) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetIsVisible(false);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kRunning);

  // Collect one slice before updating, one after.
  TriggerCollectSlice();

  PageLiveStateDecorator::Data* data =
      PageLiveStateDecorator::Data::GetOrCreateForPageNode(
          mock_graph.page.get());
  data->SetUpdatedTitleOrFaviconInBackgroundForTesting(true);

  TriggerCollectSlice();
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 2UL);
  test_ukm_recorder()->ExpectEntryMetric(
      entries[0], "ChangedFaviconOrTitleInBackground", false);
  test_ukm_recorder()->ExpectEntryMetric(
      entries[1], "ChangedFaviconOrTitleInBackground", true);
}

TEST_F(PageTimelineMonitorUnitTest, TestUpdateLifecycleState) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kFrozen);
  mock_graph.page->SetIsVisible(false);

  EXPECT_EQ(monitor()
                ->page_node_info_map_[TabPageDecorator::FromPageNode(
                    mock_graph.page.get())]
                ->current_lifecycle,
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

  performance_manager::policies::HighEfficiencyModePolicy* policy =
      performance_manager::policies::HighEfficiencyModePolicy::GetInstance();
  policy->SetTimeBeforeDiscard(base::Hours(2));
  policy->OnHighEfficiencyModeChanged(true);

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

TEST_F(PageTimelineMonitorUnitTest, TestMemory) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetIsVisible(true);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kRunning);
  mock_graph.frame->SetResidentSetKbEstimate(123);
  mock_graph.frame->SetPrivateFootprintKbEstimate(456);

  TriggerCollectSlice();
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageTimelineState::kEntryName);
  EXPECT_EQ(entries.size(), 1UL);
  test_ukm_recorder()->ExpectEntryMetric(entries[0], "ResidentSetSize", 123);
  test_ukm_recorder()->ExpectEntryMetric(entries[0], "PrivateFootprint", 456);
}

TEST_F(PageTimelineMonitorUnitTest, TestUpdatePageNodeBeforeTypeChange) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetIsVisible(false);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetLifecycleStateForTesting(mojom::LifecycleState::kFrozen);
  mock_graph.page->SetType(performance_manager::PageType::kTab);

  TabPageDecorator::TabHandle* tab_handle =
      TabPageDecorator::FromPageNode(mock_graph.page.get());

  EXPECT_EQ(monitor()->page_node_info_map_[tab_handle]->current_lifecycle,
            mojom::LifecycleState::kFrozen);
  EXPECT_EQ(monitor()->page_node_info_map_[tab_handle]->currently_visible,
            false);

  // making sure no DCHECKs are hit
  TriggerCollectSlice();
}

TEST_P(PageTimelineMonitorWithFeatureTest, TestResourceUsage) {
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());
  const ukm::SourceId mock_source_id = ukm::AssignNewSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.frame->SetResidentSetKbEstimate(123);

  const ukm::SourceId mock_source_id2 = ukm::AssignNewSourceId();
  mock_graph.other_page->SetType(performance_manager::PageType::kTab);
  mock_graph.other_page->SetUkmSourceId(mock_source_id2);
  mock_graph.other_frame->SetResidentSetKbEstimate(456);
  mock_graph.other_frame->SetPrivateFootprintKbEstimate(789);
  mock_graph.child_frame->SetPrivateFootprintKbEstimate(1000);

  // Let an arbitrary amount of time pass so there's some CPU usage to measure.
  task_env().FastForwardBy(base::Minutes(1));

  TriggerCollectPageResourceUsage();

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  // Expect 1 entry per page.
  EXPECT_EQ(entries.size(), 2UL);

  const auto kExpectedResidentSetSize =
      base::MakeFixedFlatMap<ukm::SourceId, int64_t>({
          {mock_source_id, 123},
          {mock_source_id2, 456},
      });
  const auto kExpectedPrivateFootprint =
      base::MakeFixedFlatMap<ukm::SourceId, int64_t>({
          {mock_source_id, 0},
          // `other_page` is the sum of `other_frame` and `child_frame`
          {mock_source_id2, 1789},
      });
  // The SimulatedCPUMeasurementDelegate returns 50% of the CPU is used.
  // `process` contains `frame` and `other_frame` -> each gets 25%
  // `other_process` contains `child_frame` -> 50%
  const auto kExpectedCPUUsage =
      base::MakeFixedFlatMap<ukm::SourceId, int64_t>({
          // `page` contains `frame`
          {mock_source_id, 2500},
          // `other_page` gets the sum of `other_frame` and `child_frame`
          {mock_source_id2, 7500},
      });
  const auto kExpectedAllCPUUsage = 2500 + 7500;
  for (const ukm::mojom::UkmEntry* entry : entries) {
    test_ukm_recorder()->ExpectEntryMetric(
        entry, "ResidentSetSizeEstimate",
        kExpectedResidentSetSize.at(entry->source_id));
    test_ukm_recorder()->ExpectEntryMetric(
        entry, "PrivateFootprintEstimate",
        kExpectedPrivateFootprint.at(entry->source_id));
    test_ukm_recorder()->ExpectEntryMetric(
        entry, "RecentCPUUsage", kExpectedCPUUsage.at(entry->source_id));
    test_ukm_recorder()->ExpectEntryMetric(entry, "TotalRecentCPUUsageAllPages",
                                           kExpectedAllCPUUsage);
  }
}

TEST_F(PageTimelineMonitorUnitTest, TestResourceUsageBackgroundState) {
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());
  const ukm::SourceId mock_source_id = ukm::AssignNewSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);

  const ukm::SourceId mock_source_id2 = ukm::AssignNewSourceId();
  mock_graph.other_page->SetType(performance_manager::PageType::kTab);
  mock_graph.other_page->SetUkmSourceId(mock_source_id2);

  // Start with page 1 in foreground.
  mock_graph.page->SetIsVisible(true);
  mock_graph.other_page->SetIsVisible(false);
  task_env().FastForwardBy(base::Minutes(1));
  TestBackgroundStates(
      {{mock_source_id, PageMeasurementBackgroundState::kForeground},
       {mock_source_id2, PageMeasurementBackgroundState::kBackground}});

  // Pages become audible for all of next measurement period.
  mock_graph.page->SetIsAudible(true);
  mock_graph.other_page->SetIsAudible(true);
  task_env().FastForwardBy(base::Minutes(1));
  TestBackgroundStates(
      {{mock_source_id, PageMeasurementBackgroundState::kForeground},
       {mock_source_id2,
        PageMeasurementBackgroundState::kAudibleInBackground}});

  // Partway through next measurement period:
  // - Page 1 moves to background (still audible).
  // - Page 2 stops playing audio.
  task_env().FastForwardBy(base::Minutes(1));
  mock_graph.page->SetIsVisible(false);
  mock_graph.other_page->SetIsAudible(false);
  TestBackgroundStates(
      {{mock_source_id,
        PageMeasurementBackgroundState::kMixedForegroundBackground},
       {mock_source_id2,
        PageMeasurementBackgroundState::kBackgroundMixedAudible}});

  // Partway through next measurement period, page 2 moves to foreground (still
  // inaudible).
  task_env().FastForwardBy(base::Minutes(1));
  mock_graph.other_page->SetIsVisible(true);
  TestBackgroundStates(
      {{mock_source_id, PageMeasurementBackgroundState::kAudibleInBackground},
       {mock_source_id2,
        PageMeasurementBackgroundState::kMixedForegroundBackground}});
}

#if !BUILDFLAG(IS_ANDROID)
TEST_P(PageTimelineMonitorWithFeatureTest, TestCPUInterventionMetrics) {
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());
  const ukm::SourceId mock_source_id = ukm::AssignNewSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetIsVisible(true);

  const ukm::SourceId mock_source_id2 = ukm::AssignNewSourceId();
  mock_graph.other_page->SetType(performance_manager::PageType::kTab);
  mock_graph.other_page->SetUkmSourceId(mock_source_id2);

  // Set CPU usage to 0, so only the .Baseline metrics should be logged.
  cpu_delegate_factory_.GetDelegate(mock_graph.process.get()).SetCPUUsage(0.0);
  cpu_delegate_factory_.GetDelegate(mock_graph.other_process.get())
      .SetCPUUsage(0.0);

  // Let an arbitrary amount of time pass so there's some CPU usage to measure.
  task_env().FastForwardBy(base::Minutes(1));
  TriggerCollectPageResourceUsage();

  ExpectCPUHistogram("AverageBackgroundCPU", "Baseline", 0);
  ExpectCPUHistogram("TotalBackgroundCPU", "Baseline", 0);
  ExpectCPUHistogram("TotalBackgroundTabCount", "Baseline", 1);
  ExpectCPUHistogram("AverageForegroundCPU", "Baseline", 0);
  ExpectCPUHistogram("TotalForegroundCPU", "Baseline", 0);
  ExpectCPUHistogram("TotalForegroundTabCount", "Baseline", 1);

  ExpectNoCPUHistogram("AverageBackgroundCPU", "Immediate");
  ExpectNoCPUHistogram("TotalBackgroundCPU", "Immediate");
  ExpectNoCPUHistogram("TotalBackgroundTabCount", "Immediate");
  ExpectNoCPUHistogram("AverageForegroundCPU", "Immediate");
  ExpectNoCPUHistogram("TotalForegroundCPU", "Immediate");
  ExpectNoCPUHistogram("TotalForegroundTabCount", "Immediate");
  ExpectNoCPUHistogram("BackgroundTabsToGetUnderCPUThreshold", "Immediate");
  ExpectNoCPUHistogram("TopNBackgroundCPU.1", "Immediate");
  ExpectNoCPUHistogram("TopNBackgroundCPU.2", "Immediate");

  ExpectNoCPUHistogram("AverageBackgroundCPU", "Delayed");
  ExpectNoCPUHistogram("TotalBackgroundCPU", "Delayed");
  ExpectNoCPUHistogram("TotalBackgroundTabCount", "Delayed");
  ExpectNoCPUHistogram("AverageForegroundCPU", "Delayed");
  ExpectNoCPUHistogram("TotalForegroundCPU", "Delayed");
  ExpectNoCPUHistogram("TotalForegroundTabCount", "Delayed");
  ExpectNoCPUHistogram("BackgroundTabsToGetUnderCPUThreshold", "Delayed");
  ExpectNoCPUHistogram("TopNBackgroundCPU.1", "Delayed");
  ExpectNoCPUHistogram("TopNBackgroundCPU.2", "Delayed");

  ExpectNoCPUHistogram("DurationOverThreshold");

  ResetHistogramTester();

  // The intervention metrics measure total CPU, not percentage of each core, so
  // set the measurement delegates to return half of the total available CPU
  // (100% per processor).
  cpu_delegate_factory_.GetDelegate(mock_graph.process.get())
      .SetCPUUsage(base::SysInfo::NumberOfProcessors() / 2.0);
  cpu_delegate_factory_.GetDelegate(mock_graph.other_process.get())
      .SetCPUUsage(base::SysInfo::NumberOfProcessors() / 2.0);

  // Let an arbitrary amount of time pass so there's some CPU usage to measure.
  task_env().FastForwardBy(base::Minutes(1));
  TriggerCollectPageResourceUsage();

  // `page` is in the foreground, and gets 50% of the `process` CPU (25% of
  // total CPU). `other_page` is in the background, and gets 50% of the
  // `process` CPU + all of the `other_process` CPU (75% of total CPU).

  ExpectCPUHistogram("AverageBackgroundCPU", "Baseline", 75);
  ExpectCPUHistogram("TotalBackgroundCPU", "Baseline", 75);
  ExpectCPUHistogram("TotalBackgroundTabCount", "Baseline", 1);
  ExpectCPUHistogram("AverageForegroundCPU", "Baseline", 25);
  ExpectCPUHistogram("TotalForegroundCPU", "Baseline", 25);
  ExpectCPUHistogram("TotalForegroundTabCount", "Baseline", 1);

  ExpectCPUHistogram("AverageBackgroundCPU", "Immediate", 75);
  ExpectCPUHistogram("TotalBackgroundCPU", "Immediate", 75);
  ExpectCPUHistogram("TotalBackgroundTabCount", "Immediate", 1);
  ExpectCPUHistogram("AverageForegroundCPU", "Immediate", 25);
  ExpectCPUHistogram("TotalForegroundCPU", "Immediate", 25);
  ExpectCPUHistogram("TotalForegroundTabCount", "Immediate", 1);
  ExpectCPUHistogram("BackgroundTabsToGetUnderCPUThreshold", "Immediate", 1);
  ExpectCPUHistogram("TopNBackgroundCPU.1", "Immediate", 75);
  ExpectCPUHistogram("TopNBackgroundCPU.2", "Immediate", 75);

  ExpectNoCPUHistogram("AverageBackgroundCPU", "Delayed");
  ExpectNoCPUHistogram("TotalBackgroundCPU", "Delayed");
  ExpectNoCPUHistogram("TotalBackgroundTabCount", "Delayed");
  ExpectNoCPUHistogram("AverageForegroundCPU", "Delayed");
  ExpectNoCPUHistogram("TotalForegroundCPU", "Delayed");
  ExpectNoCPUHistogram("TotalForegroundTabCount", "Delayed");
  ExpectNoCPUHistogram("BackgroundTabsToGetUnderCPUThreshold", "Delayed");
  ExpectNoCPUHistogram("TopNBackgroundCPU.1", "Delayed");
  ExpectNoCPUHistogram("TopNBackgroundCPU.2", "Delayed");

  ExpectNoCPUHistogram("DurationOverThreshold");

  ResetHistogramTester();

  // Fast forward for Delayed UMA to be logged.
  task_env().FastForwardBy(base::Minutes(1));

  ExpectNoCPUHistogram("AverageBackgroundCPU", "Baseline");
  ExpectNoCPUHistogram("TotalBackgroundCPU", "Baseline");
  ExpectNoCPUHistogram("TotalBackgroundTabCount", "Baseline");
  ExpectNoCPUHistogram("AverageForegroundCPU", "Baseline");
  ExpectNoCPUHistogram("TotalForegroundCPU", "Baseline");
  ExpectNoCPUHistogram("TotalForegroundTabCount", "Baseline");

  ExpectNoCPUHistogram("AverageBackgroundCPU", "Immediate");
  ExpectNoCPUHistogram("TotalBackgroundCPU", "Immediate");
  ExpectNoCPUHistogram("TotalBackgroundTabCount", "Immediate");
  ExpectNoCPUHistogram("AverageForegroundCPU", "Immediate");
  ExpectNoCPUHistogram("TotalForegroundCPU", "Immediate");
  ExpectNoCPUHistogram("TotalForegroundTabCount", "Immediate");
  ExpectNoCPUHistogram("BackgroundTabsToGetUnderCPUThreshold", "Immediate");
  ExpectNoCPUHistogram("TopNBackgroundCPU.1", "Immediate");
  ExpectNoCPUHistogram("TopNBackgroundCPU.2", "Immediate");

  ExpectNoCPUHistogram("DurationOverThreshold");

  if (GetParam()) {
    ExpectCPUHistogram("AverageBackgroundCPU", "Delayed", 75);
    ExpectCPUHistogram("TotalBackgroundCPU", "Delayed", 75);
    ExpectCPUHistogram("TotalBackgroundTabCount", "Delayed", 1);
    ExpectCPUHistogram("AverageForegroundCPU", "Delayed", 25);
    ExpectCPUHistogram("TotalForegroundCPU", "Delayed", 25);
    ExpectCPUHistogram("TotalForegroundTabCount", "Delayed", 1);
    ExpectCPUHistogram("BackgroundTabsToGetUnderCPUThreshold", "Delayed", 1);
    ExpectCPUHistogram("TopNBackgroundCPU.1", "Delayed", 75);
    ExpectCPUHistogram("TopNBackgroundCPU.2", "Delayed", 75);
  } else {
    ExpectNoCPUHistogram("AverageBackgroundCPU", "Delayed");
    ExpectNoCPUHistogram("TotalBackgroundCPU", "Delayed");
    ExpectNoCPUHistogram("TotalBackgroundTabCount", "Delayed");
    ExpectNoCPUHistogram("AverageForegroundCPU", "Delayed");
    ExpectNoCPUHistogram("TotalForegroundCPU", "Delayed");
    ExpectNoCPUHistogram("TotalForegroundTabCount", "Delayed");
    ExpectNoCPUHistogram("BackgroundTabsToGetUnderCPUThreshold", "Delayed");
    ExpectNoCPUHistogram("TopNBackgroundCPU.1", "Delayed");
    ExpectNoCPUHistogram("TopNBackgroundCPU.2", "Delayed");

    // The legacy CPU monitor only measures the CPU during
    // TriggerCollectPageResourceUsage(), and returns the average CPU since the
    // last call. Measure now so the next test doesn't include the last minute
    // of CPU in the average.
    TriggerCollectPageResourceUsage();
  }
  ResetHistogramTester();

  // Lower CPU measurement so the duration is logged.
  cpu_delegate_factory_.GetDelegate(mock_graph.process.get())
      .SetCPUUsage(base::SysInfo::NumberOfProcessors() / 8.0);
  cpu_delegate_factory_.GetDelegate(mock_graph.other_process.get())
      .SetCPUUsage(base::SysInfo::NumberOfProcessors() / 8.0);
  task_env().FastForwardBy(base::Minutes(1));
  TriggerCollectPageResourceUsage();

  ExpectCPUHistogram("DurationOverThreshold",
                     base::Minutes(2).InMilliseconds());

  // `page` is in the foreground, and gets 50% of the `process` CPU (6.25%
  // of total CPU). `other_page` is in the background, and gets 50% of the
  // `process` CPU + all of the `other_process` CPU (18.75% of total CPU).

  ExpectCPUHistogram("AverageBackgroundCPU", "Baseline", 18);
  ExpectCPUHistogram("TotalBackgroundCPU", "Baseline", 18);
  ExpectCPUHistogram("TotalBackgroundTabCount", "Baseline", 1);
  ExpectCPUHistogram("AverageForegroundCPU", "Baseline", 6);
  ExpectCPUHistogram("TotalForegroundCPU", "Baseline", 6);
  ExpectCPUHistogram("TotalForegroundTabCount", "Baseline", 1);

  ExpectNoCPUHistogram("AverageBackgroundCPU", "Immediate");
  ExpectNoCPUHistogram("TotalBackgroundCPU", "Immediate");
  ExpectNoCPUHistogram("TotalBackgroundTabCount", "Immediate");
  ExpectNoCPUHistogram("AverageForegroundCPU", "Immediate");
  ExpectNoCPUHistogram("TotalForegroundCPU", "Immediate");
  ExpectNoCPUHistogram("TotalForegroundTabCount", "Immediate");
  ExpectNoCPUHistogram("BackgroundTabsToGetUnderCPUThreshold", "Immediate");
  ExpectNoCPUHistogram("TopNBackgroundCPU.1", "Immediate");
  ExpectNoCPUHistogram("TopNBackgroundCPU.2", "Immediate");

  ExpectNoCPUHistogram("AverageBackgroundCPU", "Delayed");
  ExpectNoCPUHistogram("TotalBackgroundCPU", "Delayed");
  ExpectNoCPUHistogram("TotalBackgroundTabCount", "Delayed");
  ExpectNoCPUHistogram("AverageForegroundCPU", "Delayed");
  ExpectNoCPUHistogram("TotalForegroundCPU", "Delayed");
  ExpectNoCPUHistogram("TotalForegroundTabCount", "Delayed");
  ExpectNoCPUHistogram("BackgroundTabsToGetUnderCPUThreshold", "Delayed");
  ExpectNoCPUHistogram("TopNBackgroundCPU.1", "Delayed");
  ExpectNoCPUHistogram("TopNBackgroundCPU.2", "Delayed");
}
#endif

}  // namespace performance_manager::metrics
