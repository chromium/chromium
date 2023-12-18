// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/page_resource_monitor.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/system/sys_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/resource_attribution/measurement_delegates.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::metrics {

namespace {

using PageMeasurementBackgroundState =
    PageResourceMonitor::PageMeasurementBackgroundState;

// Helper class to repeatedly test a HistogramTester for histograms with a
// common naming pattern. The default pattern is
// PerformanceManager.PerformanceInterventions.CPU.*.
//
// WithSuffix() creates another PatternedHistogramTester that tests the same
// pattern with a suffix. The original PatternedHistogramTester and all others
// created from it WithSuffix() share the same HistogramTester. When they all go
// out of scope, it stops recording histograms.
//
// Usage:
//
//   {
//     PatternedHistogramTester h1("start");
//     PatternedHistogramTester h2 = h1.WithSuffix("end");
//     // Do things.
//     h1.ExpectUniqueSample("foo", 2);  // Expects "start.foo::2" has 1 sample.
//     h2.ExpectNone("bar");  // Expects "start.bar.end" has no samples in any
//                            // bucket.
//   }
//   {
//     PatternedHistogramTester h3("start");
//     // Do more things.
//     h3.ExpectUniqueSample("foo", 4);  // Expects "start.foo::4" has 1 sample
//                                       // since `h3` was created. The samples
//                                       // seen by `h1` and `h2` are ignored.
//   }
class PatternedHistogramTester {
 public:
  explicit PatternedHistogramTester(
      base::StringPiece prefix =
          "PerformanceManager.PerformanceInterventions.CPU",
      base::StringPiece suffix = "")
      : prefix_(prefix), suffix_(suffix) {}

  ~PatternedHistogramTester() = default;

  PatternedHistogramTester(const PatternedHistogramTester&) = delete;
  PatternedHistogramTester operator=(const PatternedHistogramTester&) = delete;

  // Expects prefix.`name`.suffix to contain exactly 1 sample in the
  // `sample_bucket` bucket.
  void ExpectUniqueSample(
      base::StringPiece name,
      int sample_bucket,
      const base::Location& location = base::Location::Current()) const {
    histogram_tester_->ExpectUniqueSample(HistogramName(name), sample_bucket, 1,
                                          location);
  }

  // Expects prefix.`name`.suffix to contain no samples at all.
  void ExpectNone(
      base::StringPiece name,
      const base::Location& location = base::Location::Current()) const {
    histogram_tester_->ExpectTotalCount(HistogramName(name), 0, location);
  }

  // Expects either:
  //
  // CpuProbe succeeded in calculating system CPU, so:
  //   prefix.System.suffix contains 1 sample in any bucket and
  //   prefix.NonChrome.suffix contains 1 sample in any bucket
  //
  // Or:
  //
  // CpuProbe got an error calculating system CPU, so:
  //   prefix.SystemCPUError.suffix contains 1 sample of "true"
  void ExpectSystemCPUHistograms(
      const base::Location& location = base::Location::Current()) const {
    if (histogram_tester_->GetBucketCount(HistogramName("SystemCPUError"),
                                          true) == 1) {
      ExpectNone("System", location);
      ExpectNone("NonChrome", location);
    } else {
      ExpectNone("SystemCPUError", location);
      histogram_tester_->ExpectTotalCount(HistogramName("System"), 1, location);
      histogram_tester_->ExpectTotalCount(HistogramName("NonChrome"), 1,
                                          location);
    }
  }

  // Returns a copy of this object that appends `suffix` to histogram names.
  PatternedHistogramTester WithSuffix(base::StringPiece suffix) const {
    return PatternedHistogramTester(prefix_, suffix, histogram_tester_);
  }

 private:
  class RefCountedHistogramTester
      : public base::RefCounted<RefCountedHistogramTester>,
        public base::HistogramTester {
   private:
    friend class base::RefCounted<RefCountedHistogramTester>;
    ~RefCountedHistogramTester() = default;
  };

  // Internal constructor used by WithSuffix() to use a shared HistogramTester.
  PatternedHistogramTester(
      base::StringPiece prefix,
      base::StringPiece suffix,
      scoped_refptr<RefCountedHistogramTester> histogram_tester)
      : prefix_(prefix), suffix_(suffix), histogram_tester_(histogram_tester) {}

  std::string HistogramName(base::StringPiece name) const {
    return base::StrCat({
        prefix_,
        prefix_.empty() ? "" : ".",
        name,
        suffix_.empty() ? "" : ".",
        suffix_,
    });
  }

  const std::string prefix_;
  const std::string suffix_;
  scoped_refptr<RefCountedHistogramTester> histogram_tester_ =
      base::MakeRefCounted<RefCountedHistogramTester>();
};

}  // namespace

class PageResourceMonitorUnitTest : public GraphTestHarness {
 public:
  PageResourceMonitorUnitTest() = default;
  ~PageResourceMonitorUnitTest() override = default;
  PageResourceMonitorUnitTest(const PageResourceMonitorUnitTest& other) =
      delete;
  PageResourceMonitorUnitTest& operator=(const PageResourceMonitorUnitTest&) =
      delete;

  void SetUp() override {
    GraphTestHarness::SetUp();

    // Return 50% CPU used by default.
    cpu_delegate_factory_.SetDefaultCPUUsage(0.5);

    std::unique_ptr<PageResourceMonitor> monitor =
        std::make_unique<PageResourceMonitor>(enable_system_cpu_probe_);
    monitor_ = monitor.get();
    monitor_->SetTriggerCollectionManuallyForTesting();
    monitor_->SetCPUMeasurementDelegateFactoryForTesting(
        graph(), &cpu_delegate_factory_);
    graph()->PassToGraph(std::move(monitor));
    ResetUkmRecorder();
  }

  void TearDown() override {
    test_ukm_recorder_.reset();
    GraphTestHarness::TearDown();
  }

  // To allow tests to call its methods and view its state.
  raw_ptr<PageResourceMonitor> monitor_;

  // Factory to return CPUMeasurementDelegates. This must be deleted after
  // `monitor_` to ensure that it outlives all delegates it creates.
  resource_attribution::SimulatedCPUMeasurementDelegateFactory
      cpu_delegate_factory_;

 protected:
  ukm::TestUkmRecorder* test_ukm_recorder() { return test_ukm_recorder_.get(); }
  PageResourceMonitor* monitor() { return monitor_; }

  void TriggerCollectPageResourceUsage() {
    base::RunLoop run_loop;
    monitor_->CollectPageResourceUsage(run_loop.QuitClosure());
    // GraphTestHarness uses ThreadPoolExecutionMode::QUEUED, so RunLoop only
    // pumps the main thread. Manually pump ThreadPool threads for CPUProbe.
    task_env().FastForwardBy(base::TimeDelta());
    run_loop.Run();
  }

  // Let an arbitrary amount of time pass so there's some CPU usage to measure.
  // Page CPU can use the mock clock, but CPUProbe needs real time to pass.
  void LetTimePass() {
    task_env().FastForwardBy(base::Minutes(1));
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }

  void ResetUkmRecorder() {
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  // Triggers a metrics collection and tests whether the BackgroundState logged
  // for each ukm::SourceId matches the given expectation, then clears the
  // collected UKM's for the next slice.
  void TestBackgroundStates(
      std::map<ukm::SourceId, PageMeasurementBackgroundState> expected_states);

  // Subclasses can override this before calling
  // PageResourceMonitorUnitTest::SetUp() to simulate an environment where
  // CPUProbe::Create() returns nullptr.
  bool enable_system_cpu_probe_ = true;

 private:
  std::unique_ptr<ukm::TestUkmRecorder> test_ukm_recorder_;
};

void PageResourceMonitorUnitTest::TestBackgroundStates(
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
class PageResourceMonitorWithFeatureTest
    : public PageResourceMonitorUnitTest,
      public ::testing::WithParamInterface<bool> {
 public:
  PageResourceMonitorWithFeatureTest() {
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
    PageResourceMonitorUnitTest::SetUp();
  }

  void TearDown() override {
    // Destroy `monitor_` before `scoped_feature_list_` so that the feature flag
    // doesn't change during its destructor.
    graph()->TakeFromGraph(monitor_.ExtractAsDangling());
    PageResourceMonitorUnitTest::TearDown();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PageResourceMonitorWithFeatureTest,
                         ::testing::Bool());

// A test of CPU intervention logging when the system CPUProbe is not available.
class PageResourceMonitorNoCPUProbeTest : public PageResourceMonitorUnitTest {
 public:
  PageResourceMonitorNoCPUProbeTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {performance_manager::features::kCPUInterventionEvaluationLogging,
             {{"threshold_chrome_cpu_percent", "50"}}},
        },
        {});
    enable_system_cpu_probe_ = false;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PageResourceMonitorUnitTest, TestPageResourceUsage) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);

  TriggerCollectPageResourceUsage();

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  EXPECT_EQ(entries.size(), 1UL);
}

TEST_F(PageResourceMonitorUnitTest, TestPageResourceUsageNavigation) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id =
      ukm::AssignNewSourceId();  // ukm::NoURLSourceId();
  ukm::SourceId mock_source_id_2 = ukm::AssignNewSourceId();

  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetType(performance_manager::PageType::kTab);

  TriggerCollectPageResourceUsage();

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  EXPECT_EQ(entries.size(), 1UL);

  mock_graph.page->SetUkmSourceId(mock_source_id_2);

  TriggerCollectPageResourceUsage();

  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  EXPECT_EQ(entries.size(), 2UL);

  std::vector<ukm::SourceId> ids;
  for (const ukm::mojom::UkmEntry* entry : entries) {
    ids.push_back(entry->source_id);
  }

  EXPECT_NE(ids[0], ids[1]);
}

TEST_F(PageResourceMonitorUnitTest, TestOnlyRecordTabs) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetUkmSourceId(mock_source_id);

  TriggerCollectPageResourceUsage();

  auto entries2 = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  EXPECT_EQ(entries2.size(), 0UL);
}

TEST_P(PageResourceMonitorWithFeatureTest, TestResourceUsage) {
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

TEST_F(PageResourceMonitorUnitTest, TestResourceUsageBackgroundState) {
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
TEST_P(PageResourceMonitorWithFeatureTest, TestCPUInterventionMetrics) {
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());

  // Foreground page.
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetIsVisible(true);

  // Background page.
  mock_graph.other_page->SetType(performance_manager::PageType::kTab);
  mock_graph.other_page->SetIsVisible(false);

  // Set CPU usage to near-0, so only the .Baseline metrics should be logged.
  // 0 is used as an error value by base::ProcessMetrics so it will cause no
  // results at all for the process to be returned.
  cpu_delegate_factory_.GetDelegate(mock_graph.process.get()).SetCPUUsage(0.01);
  cpu_delegate_factory_.GetDelegate(mock_graph.other_process.get())
      .SetCPUUsage(0.01);

  {
    PatternedHistogramTester histograms;

    LetTimePass();
    TriggerCollectPageResourceUsage();

    auto baseline = histograms.WithSuffix("Baseline");
    baseline.ExpectUniqueSample("AverageBackgroundCPU", 0);
    baseline.ExpectUniqueSample("TotalBackgroundCPU", 0);
    baseline.ExpectUniqueSample("TotalBackgroundTabCount", 1);
    baseline.ExpectUniqueSample("AverageForegroundCPU", 0);
    baseline.ExpectUniqueSample("TotalForegroundCPU", 0);
    baseline.ExpectUniqueSample("TotalForegroundTabCount", 1);
    baseline.ExpectSystemCPUHistograms();

    auto immediate = histograms.WithSuffix("Immediate");
    immediate.ExpectNone("AverageBackgroundCPU");
    immediate.ExpectNone("TotalBackgroundCPU");
    immediate.ExpectNone("TotalBackgroundTabCount");
    immediate.ExpectNone("AverageForegroundCPU");
    immediate.ExpectNone("TotalForegroundCPU");
    immediate.ExpectNone("TotalForegroundTabCount");
    immediate.ExpectNone("BackgroundTabsToGetUnderCPUThreshold");
    immediate.ExpectNone("TopNBackgroundCPU.1");
    immediate.ExpectNone("TopNBackgroundCPU.2");
    immediate.ExpectNone("System");
    immediate.ExpectNone("NonChrome");
    immediate.ExpectNone("SystemCPUError");

    auto delayed = histograms.WithSuffix("Delayed");
    delayed.ExpectNone("AverageBackgroundCPU");
    delayed.ExpectNone("TotalBackgroundCPU");
    delayed.ExpectNone("TotalBackgroundTabCount");
    delayed.ExpectNone("AverageForegroundCPU");
    delayed.ExpectNone("TotalForegroundCPU");
    delayed.ExpectNone("TotalForegroundTabCount");
    delayed.ExpectNone("BackgroundTabsToGetUnderCPUThreshold");
    delayed.ExpectNone("TopNBackgroundCPU.1");
    delayed.ExpectNone("TopNBackgroundCPU.2");
    delayed.ExpectNone("System");
    delayed.ExpectNone("NonChrome");
    delayed.ExpectNone("SystemCPUError");

    histograms.ExpectNone("DurationOverThreshold");
  }

  // The intervention metrics measure total CPU, not percentage of each core, so
  // set the measurement delegates to return half of the total available CPU
  // (100% per processor).
  cpu_delegate_factory_.GetDelegate(mock_graph.process.get())
      .SetCPUUsage(base::SysInfo::NumberOfProcessors() / 2.0);
  cpu_delegate_factory_.GetDelegate(mock_graph.other_process.get())
      .SetCPUUsage(base::SysInfo::NumberOfProcessors() / 2.0);

  {
    PatternedHistogramTester histograms;

    LetTimePass();
    TriggerCollectPageResourceUsage();

    // `page` is in the foreground, and gets 50% of the `process` CPU (25% of
    // total CPU). `other_page` is in the background, and gets 50% of the
    // `process` CPU + all of the `other_process` CPU (75% of total CPU).
    auto baseline = histograms.WithSuffix("Baseline");
    baseline.ExpectUniqueSample("AverageBackgroundCPU", 75);
    baseline.ExpectUniqueSample("TotalBackgroundCPU", 75);
    baseline.ExpectUniqueSample("TotalBackgroundTabCount", 1);
    baseline.ExpectUniqueSample("AverageForegroundCPU", 25);
    baseline.ExpectUniqueSample("TotalForegroundCPU", 25);
    baseline.ExpectUniqueSample("TotalForegroundTabCount", 1);
    baseline.ExpectSystemCPUHistograms();

    auto immediate = histograms.WithSuffix("Immediate");
    immediate.ExpectUniqueSample("AverageBackgroundCPU", 75);
    immediate.ExpectUniqueSample("TotalBackgroundCPU", 75);
    immediate.ExpectUniqueSample("TotalBackgroundTabCount", 1);
    immediate.ExpectUniqueSample("AverageForegroundCPU", 25);
    immediate.ExpectUniqueSample("TotalForegroundCPU", 25);
    immediate.ExpectUniqueSample("TotalForegroundTabCount", 1);
    immediate.ExpectUniqueSample("BackgroundTabsToGetUnderCPUThreshold", 1);
    immediate.ExpectUniqueSample("TopNBackgroundCPU.1", 75);
    immediate.ExpectUniqueSample("TopNBackgroundCPU.2", 75);
    immediate.ExpectSystemCPUHistograms();

    auto delayed = histograms.WithSuffix("Delayed");
    delayed.ExpectNone("AverageBackgroundCPU");
    delayed.ExpectNone("TotalBackgroundCPU");
    delayed.ExpectNone("TotalBackgroundTabCount");
    delayed.ExpectNone("AverageForegroundCPU");
    delayed.ExpectNone("TotalForegroundCPU");
    delayed.ExpectNone("TotalForegroundTabCount");
    delayed.ExpectNone("BackgroundTabsToGetUnderCPUThreshold");
    delayed.ExpectNone("TopNBackgroundCPU.1");
    delayed.ExpectNone("TopNBackgroundCPU.2");
    delayed.ExpectNone("System");
    delayed.ExpectNone("NonChrome");
    delayed.ExpectNone("SystemCPUError");

    histograms.ExpectNone("DurationOverThreshold");
  }

  {
    PatternedHistogramTester histograms;

    // Fast forward for Delayed UMA to be logged.
    LetTimePass();

    auto baseline = histograms.WithSuffix("Baseline");
    baseline.ExpectNone("AverageBackgroundCPU");
    baseline.ExpectNone("TotalBackgroundCPU");
    baseline.ExpectNone("TotalBackgroundTabCount");
    baseline.ExpectNone("AverageForegroundCPU");
    baseline.ExpectNone("TotalForegroundCPU");
    baseline.ExpectNone("TotalForegroundTabCount");
    baseline.ExpectNone("System");
    baseline.ExpectNone("NonChrome");
    baseline.ExpectNone("SystemCPUError");

    auto immediate = histograms.WithSuffix("Immediate");
    immediate.ExpectNone("AverageBackgroundCPU");
    immediate.ExpectNone("TotalBackgroundCPU");
    immediate.ExpectNone("TotalBackgroundTabCount");
    immediate.ExpectNone("AverageForegroundCPU");
    immediate.ExpectNone("TotalForegroundCPU");
    immediate.ExpectNone("TotalForegroundTabCount");
    immediate.ExpectNone("BackgroundTabsToGetUnderCPUThreshold");
    immediate.ExpectNone("TopNBackgroundCPU.1");
    immediate.ExpectNone("TopNBackgroundCPU.2");
    immediate.ExpectNone("System");
    immediate.ExpectNone("NonChrome");
    immediate.ExpectNone("SystemCPUError");

    auto delayed = histograms.WithSuffix("Delayed");
    if (GetParam()) {
      delayed.ExpectUniqueSample("AverageBackgroundCPU", 75);
      delayed.ExpectUniqueSample("TotalBackgroundCPU", 75);
      delayed.ExpectUniqueSample("TotalBackgroundTabCount", 1);
      delayed.ExpectUniqueSample("AverageForegroundCPU", 25);
      delayed.ExpectUniqueSample("TotalForegroundCPU", 25);
      delayed.ExpectUniqueSample("TotalForegroundTabCount", 1);
      delayed.ExpectUniqueSample("BackgroundTabsToGetUnderCPUThreshold", 1);
      delayed.ExpectUniqueSample("TopNBackgroundCPU.1", 75);
      delayed.ExpectUniqueSample("TopNBackgroundCPU.2", 75);
      delayed.ExpectSystemCPUHistograms();
    } else {
      delayed.ExpectNone("AverageBackgroundCPU");
      delayed.ExpectNone("TotalBackgroundCPU");
      delayed.ExpectNone("TotalBackgroundTabCount");
      delayed.ExpectNone("AverageForegroundCPU");
      delayed.ExpectNone("TotalForegroundCPU");
      delayed.ExpectNone("TotalForegroundTabCount");
      delayed.ExpectNone("BackgroundTabsToGetUnderCPUThreshold");
      delayed.ExpectNone("TopNBackgroundCPU.1");
      delayed.ExpectNone("TopNBackgroundCPU.2");
      delayed.ExpectNone("System");
      delayed.ExpectNone("NonChrome");
      delayed.ExpectNone("SystemCPUError");
    }
    histograms.ExpectNone("DurationOverThreshold");
  }

  if (!GetParam()) {
    // The legacy CPU monitor only measures the CPU during
    // TriggerCollectPageResourceUsage(), and returns the average CPU since
    // the last call. Measure now so the next test doesn't include the last
    // minute of CPU in the average.
    TriggerCollectPageResourceUsage();
  }

  // Lower CPU measurement so the duration is logged.
  cpu_delegate_factory_.GetDelegate(mock_graph.process.get())
      .SetCPUUsage(base::SysInfo::NumberOfProcessors() / 8.0);
  cpu_delegate_factory_.GetDelegate(mock_graph.other_process.get())
      .SetCPUUsage(base::SysInfo::NumberOfProcessors() / 8.0);

  {
    PatternedHistogramTester histograms;

    LetTimePass();
    TriggerCollectPageResourceUsage();

    histograms.ExpectUniqueSample("DurationOverThreshold",
                                  base::Minutes(2).InMilliseconds());

    // `page` is in the foreground, and gets 50% of the `process` CPU (6.25%
    // of total CPU). `other_page` is in the background, and gets 50% of the
    // `process` CPU + all of the `other_process` CPU (18.75% of total CPU).
    auto baseline = histograms.WithSuffix("Baseline");
    baseline.ExpectUniqueSample("AverageBackgroundCPU", 18);
    baseline.ExpectUniqueSample("TotalBackgroundCPU", 18);
    baseline.ExpectUniqueSample("TotalBackgroundTabCount", 1);
    baseline.ExpectUniqueSample("AverageForegroundCPU", 6);
    baseline.ExpectUniqueSample("TotalForegroundCPU", 6);
    baseline.ExpectUniqueSample("TotalForegroundTabCount", 1);
    baseline.ExpectSystemCPUHistograms();

    auto immediate = histograms.WithSuffix("Immediate");
    immediate.ExpectNone("AverageBackgroundCPU");
    immediate.ExpectNone("TotalBackgroundCPU");
    immediate.ExpectNone("TotalBackgroundTabCount");
    immediate.ExpectNone("AverageForegroundCPU");
    immediate.ExpectNone("TotalForegroundCPU");
    immediate.ExpectNone("TotalForegroundTabCount");
    immediate.ExpectNone("BackgroundTabsToGetUnderCPUThreshold");
    immediate.ExpectNone("TopNBackgroundCPU.1");
    immediate.ExpectNone("TopNBackgroundCPU.2");
    immediate.ExpectNone("System");
    immediate.ExpectNone("NonChrome");
    immediate.ExpectNone("SystemCPUError");

    auto delayed = histograms.WithSuffix("Delayed");
    delayed.ExpectNone("AverageBackgroundCPU");
    delayed.ExpectNone("TotalBackgroundCPU");
    delayed.ExpectNone("TotalBackgroundTabCount");
    delayed.ExpectNone("AverageForegroundCPU");
    delayed.ExpectNone("TotalForegroundCPU");
    delayed.ExpectNone("TotalForegroundTabCount");
    delayed.ExpectNone("BackgroundTabsToGetUnderCPUThreshold");
    delayed.ExpectNone("TopNBackgroundCPU.1");
    delayed.ExpectNone("TopNBackgroundCPU.2");
    delayed.ExpectNone("System");
    delayed.ExpectNone("NonChrome");
    delayed.ExpectNone("SystemCPUError");
  }
}

TEST_P(PageResourceMonitorWithFeatureTest,
       CPUInterventionMetricsNoForegroundTabs) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  cpu_delegate_factory_.GetDelegate(mock_graph.process.get())
      .SetCPUUsage(base::SysInfo::NumberOfProcessors());

  // Put the only tab in the background.
  mock_graph.page->SetIsVisible(false);

  PatternedHistogramTester histograms;
  LetTimePass();
  TriggerCollectPageResourceUsage();

  auto baseline = histograms.WithSuffix("Baseline");
  baseline.ExpectUniqueSample("AverageBackgroundCPU", 100);
  baseline.ExpectUniqueSample("TotalBackgroundCPU", 100);
  baseline.ExpectUniqueSample("TotalBackgroundTabCount", 1);
  // AverageForegroundCPU would divide by 0.
  baseline.ExpectNone("AverageForegroundCPU");
  baseline.ExpectUniqueSample("TotalForegroundCPU", 0);
  baseline.ExpectUniqueSample("TotalForegroundTabCount", 0);

  auto immediate = histograms.WithSuffix("Immediate");
  immediate.ExpectUniqueSample("AverageBackgroundCPU", 100);
  immediate.ExpectUniqueSample("TotalBackgroundCPU", 100);
  immediate.ExpectUniqueSample("TotalBackgroundTabCount", 1);
  // AverageForegroundCPU would divide by 0.
  immediate.ExpectNone("AverageForegroundCPU");
  immediate.ExpectUniqueSample("TotalForegroundCPU", 0);
  immediate.ExpectUniqueSample("TotalForegroundTabCount", 0);
  immediate.ExpectUniqueSample("BackgroundTabsToGetUnderCPUThreshold", 1);
  immediate.ExpectUniqueSample("TopNBackgroundCPU.1", 100);
  immediate.ExpectUniqueSample("TopNBackgroundCPU.2", 100);
}

TEST_P(PageResourceMonitorWithFeatureTest,
       CPUInterventionMetricsNoBackgroundTabs) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  cpu_delegate_factory_.GetDelegate(mock_graph.process.get())
      .SetCPUUsage(base::SysInfo::NumberOfProcessors());

  // Put the only tab in the foreground.
  mock_graph.page->SetIsVisible(true);

  PatternedHistogramTester histograms;
  LetTimePass();
  TriggerCollectPageResourceUsage();

  auto baseline = histograms.WithSuffix("Baseline");
  // AverageBackgroundCPU would divide by 0.
  baseline.ExpectNone("AverageBackgroundCPU");
  baseline.ExpectUniqueSample("TotalBackgroundCPU", 0);
  baseline.ExpectUniqueSample("TotalBackgroundTabCount", 0);
  baseline.ExpectUniqueSample("AverageForegroundCPU", 100);
  baseline.ExpectUniqueSample("TotalForegroundCPU", 100);
  baseline.ExpectUniqueSample("TotalForegroundTabCount", 1);

  auto immediate = histograms.WithSuffix("Immediate");
  // AverageBackgroundCPU would divide by 0.
  immediate.ExpectNone("AverageBackgroundCPU");
  immediate.ExpectUniqueSample("TotalBackgroundCPU", 0);
  immediate.ExpectUniqueSample("TotalBackgroundTabCount", 0);
  immediate.ExpectUniqueSample("AverageForegroundCPU", 100);
  immediate.ExpectUniqueSample("TotalForegroundCPU", 100);
  immediate.ExpectUniqueSample("TotalForegroundTabCount", 1);
  // BackgroundTabsToGetUnderCPUThreshold is basically infinite (goes in the
  // overflow bucket.)
  immediate.ExpectUniqueSample("BackgroundTabsToGetUnderCPUThreshold", 9999);
  immediate.ExpectUniqueSample("TopNBackgroundCPU.1", 0);
  immediate.ExpectUniqueSample("TopNBackgroundCPU.2", 0);
}

TEST_F(PageResourceMonitorNoCPUProbeTest,
       CPUInterventionMetricsWithoutSystemCPU) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetIsVisible(false);

  cpu_delegate_factory_.GetDelegate(mock_graph.process.get())
      .SetCPUUsage(base::SysInfo::NumberOfProcessors());

  PatternedHistogramTester histograms;
  LetTimePass();
  // Let enough time pass for Delayed histograms to be logged too.
  LetTimePass();
  TriggerCollectPageResourceUsage();

  // Ensure each type of metrics were collected.
  auto baseline = histograms.WithSuffix("Baseline");
  baseline.ExpectUniqueSample("TotalBackgroundTabCount", 1);
  auto immediate = histograms.WithSuffix("Immediate");
  immediate.ExpectUniqueSample("TotalBackgroundTabCount", 1);
  auto delayed = histograms.WithSuffix("Immediate");
  delayed.ExpectUniqueSample("TotalBackgroundTabCount", 1);

  // System CPU should be safely skipped when CPU probe is not available.
  baseline.ExpectNone("System");
  baseline.ExpectNone("NonChrome");
  baseline.ExpectNone("SystemCPUError");
  immediate.ExpectNone("System");
  immediate.ExpectNone("NonChrome");
  immediate.ExpectNone("SystemCPUError");
  delayed.ExpectNone("System");
  delayed.ExpectNone("NonChrome");
  delayed.ExpectNone("SystemCPUError");
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace performance_manager::metrics
