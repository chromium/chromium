// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/page_resource_monitor.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
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
#include "chrome/browser/performance_manager/metrics/page_resource_cpu_monitor.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/resource_attribution/cpu_measurement_delegate.h"
#include "components/performance_manager/public/resource_attribution/memory_measurement_delegate.h"
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

using PageMeasurementAlgorithm = PageResourceMonitor::PageMeasurementAlgorithm;
using PageMeasurementBackgroundState =
    PageResourceMonitor::PageMeasurementBackgroundState;

using CPUMeasurementDelegate = resource_attribution::CPUMeasurementDelegate;
using FakeMemoryMeasurementDelegateFactory =
    resource_attribution::FakeMemoryMeasurementDelegateFactory;
using MemoryMeasurementDelegate =
    resource_attribution::MemoryMeasurementDelegate;
using SimulatedCPUMeasurementDelegate =
    resource_attribution::SimulatedCPUMeasurementDelegate;
using SimulatedCPUMeasurementDelegateFactory =
    resource_attribution::SimulatedCPUMeasurementDelegateFactory;

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

struct TestParams {
  // If true, the kValidateResourceAttribution feature will be enabled.
  bool validate_resource_attribution = false;

  // Expect that whenever the PageResourceUsage2 UKM is recorded, it will
  // contain an entry for each algorithm in this list.
  std::vector<PageMeasurementAlgorithm> expected_algorithms;
};

const TestParams kAllTestParams[] = {
    {.validate_resource_attribution = false,
     .expected_algorithms = {PageMeasurementAlgorithm::kEvenSplitAndAggregate}},
    {.validate_resource_attribution = true,
     .expected_algorithms = {PageMeasurementAlgorithm::kEvenSplitAndAggregate,
                             PageMeasurementAlgorithm::kLegacy}},
};

class PageResourceMonitorUnitTest
    : public GraphTestHarness,
      public ::testing::WithParamInterface<TestParams> {
 public:
  PageResourceMonitorUnitTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features{
        {features::kCPUInterventionEvaluationLogging,
         {{"threshold_chrome_cpu_percent", "50"}}},
    };
    std::vector<base::test::FeatureRef> disabled_features;
    if (GetParam().validate_resource_attribution) {
      enabled_features.push_back(
          {features::kResourceAttributionValidation, {}});
    } else {
      disabled_features.push_back(features::kResourceAttributionValidation);
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  ~PageResourceMonitorUnitTest() override = default;

  PageResourceMonitorUnitTest(const PageResourceMonitorUnitTest&) = delete;
  PageResourceMonitorUnitTest& operator=(const PageResourceMonitorUnitTest&) =
      delete;

  void SetUp() override {
    GetGraphFeatures().EnableResourceAttributionScheduler();
    GraphTestHarness::SetUp();

    // Return 50% CPU used by default.
    cpu_delegate_factory_.SetDefaultCPUUsage(0.5);
    CPUMeasurementDelegate::SetDelegateFactoryForTesting(
        graph(), &cpu_delegate_factory_);
    MemoryMeasurementDelegate::SetDelegateFactoryForTesting(
        graph(), &memory_delegate_factory_);

    std::unique_ptr<PageResourceMonitor> monitor =
        std::make_unique<PageResourceMonitor>(enable_system_cpu_probe_);
    monitor_ = monitor.get();
    graph()->PassToGraph(std::move(monitor));
    ResetUkmRecorder();
    last_collection_time_ = base::TimeTicks::Now();

    if (GetParam().validate_resource_attribution) {
      // Also set legacy test data.
      legacy_cpu_delegate_factory_.SetDefaultCPUUsage(0.5);
      monitor_->GetCPUMonitorForTesting()
          ->SetCPUMeasurementDelegateFactoryForTesting(
              graph(), &legacy_cpu_delegate_factory_);
    }
  }

  void TearDown() override {
    test_ukm_recorder_.reset();
    GraphTestHarness::TearDown();
  }

 protected:
  ukm::TestUkmRecorder* test_ukm_recorder() { return test_ukm_recorder_.get(); }
  PageResourceMonitor* monitor() { return monitor_; }

  SimulatedCPUMeasurementDelegate& GetCPUDelegate(const ProcessNode* node) {
    return cpu_delegate_factory_.GetDelegate(node);
  }

  FakeMemoryMeasurementDelegateFactory& GetMemoryDelegate() {
    return memory_delegate_factory_;
  }

  // Advances the clock to trigger the PageResourceUsage UKM.
  void TriggerCollectPageResourceUsage();

  // Advances the mock clock slightly to give enough time to make asynchronous
  // measurements after TriggerCollectPageResourceUsage(). If
  // `include_system_cpu` is true, also waits for some real time to let the
  // system CpuProbe collect data.
  void WaitForMetrics(bool include_system_cpu = false);

  // Advances the clock long enough to record
  // PerformanceManager.PerformanceInterventions.CPU.*.Delayed after logging CPU
  // intervention metrics.
  void WaitForDelayedCPUInterventionMetrics();

  void ResetUkmRecorder() {
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  // Waits for metrics collection and tests whether the BackgroundState logged
  // for each ukm::SourceId matches the given expectation, then clears the
  // collected UKM's for the next slice.
  void WaitForMetricsAndTestBackgroundStates(
      std::map<ukm::SourceId, PageMeasurementBackgroundState> expected_states);

  // Subclasses can override this before calling
  // PageResourceMonitorUnitTest::SetUp() to simulate an environment where
  // CPUProbe::Create() returns nullptr.
  bool enable_system_cpu_probe_ = true;

 private:
  // Advances the mock clock to `target_time`.
  void WaitUntil(base::TimeTicks target_time) {
    ASSERT_GT(target_time, base::TimeTicks::Now());
    task_env().FastForwardBy(target_time - base::TimeTicks::Now());
  }

  raw_ptr<PageResourceMonitor> monitor_;

  std::unique_ptr<ukm::TestUkmRecorder> test_ukm_recorder_;

  // The last time TriggerCollectPageResourceUsage() was called.
  base::TimeTicks last_collection_time_;

  // Factories to return fake measurement delegates. These must be deleted after
  // `monitor_` to ensure that they outlive all delegates they create.
  SimulatedCPUMeasurementDelegateFactory cpu_delegate_factory_;
  SimulatedCPUMeasurementDelegateFactory legacy_cpu_delegate_factory_;
  FakeMemoryMeasurementDelegateFactory memory_delegate_factory_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

void PageResourceMonitorUnitTest::TriggerCollectPageResourceUsage() {
  WaitUntil(last_collection_time_ + monitor_->GetCollectionDelayForTesting());
  last_collection_time_ = base::TimeTicks::Now();
}

void PageResourceMonitorUnitTest::WaitForMetrics(bool include_system_cpu) {
  if (include_system_cpu) {
    // Page CPU can use the mock clock, but CPUProbe needs real time to pass.
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }
  // GraphTestHarness uses ThreadPoolExecutionMode::QUEUED, so RunLoop only
  // pumps the main thread. FastForwardBy also pumps ThreadPool threads. With
  // the mock clock, any timeout will pump the threads enough to post the tasks
  // that gather and record metrics.
  task_env().FastForwardBy(TestTimeouts::tiny_timeout());
}

void PageResourceMonitorUnitTest::WaitForDelayedCPUInterventionMetrics() {
  // Trigger the delayed metrics timer.
  WaitUntil(last_collection_time_ +
            monitor_->GetDelayedMetricsTimeoutForTesting());

  // Now that the timer has triggered, wait for the measurements to finish.
  WaitForMetrics(/*include_system_cpu=*/true);
}

void PageResourceMonitorUnitTest::WaitForMetricsAndTestBackgroundStates(
    std::map<ukm::SourceId, PageMeasurementBackgroundState> expected_states) {
  WaitForMetrics();
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  // Expect 1 entry per page per algorithm.
  EXPECT_EQ(entries.size(),
            expected_states.size() * GetParam().expected_algorithms.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    test_ukm_recorder()->ExpectEntryMetric(
        entry, "BackgroundState",
        static_cast<int64_t>(expected_states.at(entry->source_id)));
  }
  ResetUkmRecorder();
}

INSTANTIATE_TEST_SUITE_P(All,
                         PageResourceMonitorUnitTest,
                         ::testing::ValuesIn(kAllTestParams));

TEST_P(PageResourceMonitorUnitTest, TestPageResourceUsage) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);

  TriggerCollectPageResourceUsage();
  WaitForMetrics();

  // Expect 1 entry per algorithm.
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  EXPECT_EQ(entries.size(), GetParam().expected_algorithms.size());
}

TEST_P(PageResourceMonitorUnitTest, TestPageResourceUsageNavigation) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id =
      ukm::AssignNewSourceId();  // ukm::NoURLSourceId();
  ukm::SourceId mock_source_id_2 = ukm::AssignNewSourceId();

  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetType(performance_manager::PageType::kTab);

  // Each SourceId should record an entry for each algorithm.
  std::vector<std::pair<ukm::SourceId, PageMeasurementAlgorithm>>
      expected_ids_and_algorithms;
  for (const PageMeasurementAlgorithm algorithm :
       GetParam().expected_algorithms) {
    expected_ids_and_algorithms.emplace_back(mock_source_id, algorithm);
    expected_ids_and_algorithms.emplace_back(mock_source_id_2, algorithm);
  }

  TriggerCollectPageResourceUsage();
  WaitForMetrics();

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  EXPECT_EQ(entries.size(), GetParam().expected_algorithms.size());

  mock_graph.page->SetUkmSourceId(mock_source_id_2);

  TriggerCollectPageResourceUsage();
  WaitForMetrics();

  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  EXPECT_EQ(entries.size(), 2 * GetParam().expected_algorithms.size());

  std::vector<std::pair<ukm::SourceId, PageMeasurementAlgorithm>>
      ids_and_algorithms;
  for (const ukm::mojom::UkmEntry* entry : entries) {
    const int64_t* algorithm =
        test_ukm_recorder()->GetEntryMetric(entry, "MeasurementAlgorithm");
    ASSERT_NE(algorithm, nullptr);
    ids_and_algorithms.emplace_back(
        entry->source_id, static_cast<PageMeasurementAlgorithm>(*algorithm));
  }

  EXPECT_THAT(ids_and_algorithms, ::testing::UnorderedElementsAreArray(
                                      expected_ids_and_algorithms));
}

TEST_P(PageResourceMonitorUnitTest, TestOnlyRecordTabs) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetUkmSourceId(mock_source_id);

  TriggerCollectPageResourceUsage();
  WaitForMetrics();

  auto entries2 = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  EXPECT_EQ(entries2.size(), 0UL);
}

TEST_P(PageResourceMonitorUnitTest, TestResourceUsage) {
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());

  // Register fake memory results. Make sure they're divisible by 2 for easier
  // matching when divided between frames.
  MemoryMeasurementDelegate::MemorySummaryMap& memory_summaries =
      GetMemoryDelegate().memory_summaries();
  memory_summaries[mock_graph.process->GetResourceContext()] = {
      .resident_set_size_kb = 1230};
  memory_summaries[mock_graph.other_process->GetResourceContext()] = {
      .resident_set_size_kb = 4560, .private_footprint_kb = 7890};
  if (GetParam().validate_resource_attribution) {
    // Also set legacy test data.
    mock_graph.frame->SetResidentSetKbEstimate(1230 / 2);
    mock_graph.other_frame->SetResidentSetKbEstimate(1230 / 2);
    mock_graph.child_frame->SetResidentSetKbEstimate(4560);
    mock_graph.other_frame->SetPrivateFootprintKbEstimate(7890);
  }

  const ukm::SourceId mock_source_id = ukm::AssignNewSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);

  const ukm::SourceId mock_source_id2 = ukm::AssignNewSourceId();
  mock_graph.other_page->SetType(performance_manager::PageType::kTab);
  mock_graph.other_page->SetUkmSourceId(mock_source_id2);

  TriggerCollectPageResourceUsage();
  WaitForMetrics();

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  // Expect 1 entry per page per algorithm.
  EXPECT_EQ(entries.size(), GetParam().expected_algorithms.size() * 2);

  // `page` gets its memory from `frame`, which is 1/2 the memory from
  // `process`.
  // `other_page` gets its memory from `other_frame` (1/2 of `process`) +
  // `child_frame` (all of `other_process`).
  // See the diagram in
  // components/performance_manager/test_support/mock_graphs.h.
  const base::flat_map<ukm::SourceId, int64_t> expected_resident_set_size{
      {mock_source_id, 1230 / 2},
      {mock_source_id2, 1230 / 2 + 4560},
  };
  const base::flat_map<ukm::SourceId, int64_t> expected_private_footprint{
      {mock_source_id, 0},
      {mock_source_id2, 7890},
  };
  // The SimulatedCPUMeasurementDelegate returns 50% of the CPU is used.
  // `process` contains `frame` and `other_frame` -> each gets 25%
  // `other_process` contains `child_frame` -> 50%
  const base::flat_map<ukm::SourceId, int64_t> expected_cpu_usage{
      // `page` contains `frame`
      {mock_source_id, 2500},
      // `other_page` gets the sum of `other_frame` and `child_frame`
      {mock_source_id2, 7500},
  };
  const auto kExpectedAllCPUUsage = 2500 + 7500;

  // Each SourceId should record an entry for each algorithm.
  std::vector<std::pair<ukm::SourceId, PageMeasurementAlgorithm>>
      expected_ids_and_algorithms;
  for (const PageMeasurementAlgorithm algorithm :
       GetParam().expected_algorithms) {
    expected_ids_and_algorithms.emplace_back(mock_source_id, algorithm);
    expected_ids_and_algorithms.emplace_back(mock_source_id2, algorithm);
  }

  std::vector<std::pair<ukm::SourceId, PageMeasurementAlgorithm>>
      ids_and_algorithms;
  for (const ukm::mojom::UkmEntry* entry : entries) {
    test_ukm_recorder()->ExpectEntryMetric(
        entry, "ResidentSetSizeEstimate",
        expected_resident_set_size.at(entry->source_id));
    test_ukm_recorder()->ExpectEntryMetric(
        entry, "PrivateFootprintEstimate",
        expected_private_footprint.at(entry->source_id));
    test_ukm_recorder()->ExpectEntryMetric(
        entry, "RecentCPUUsage", expected_cpu_usage.at(entry->source_id));
    test_ukm_recorder()->ExpectEntryMetric(entry, "TotalRecentCPUUsageAllPages",
                                           kExpectedAllCPUUsage);
    const int64_t* algorithm =
        test_ukm_recorder()->GetEntryMetric(entry, "MeasurementAlgorithm");
    ASSERT_NE(algorithm, nullptr);
    ids_and_algorithms.emplace_back(
        entry->source_id, static_cast<PageMeasurementAlgorithm>(*algorithm));
  }
  EXPECT_THAT(ids_and_algorithms, ::testing::UnorderedElementsAreArray(
                                      expected_ids_and_algorithms));
}

TEST_P(PageResourceMonitorUnitTest, TestResourceUsageBackgroundState) {
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());
  const ukm::SourceId mock_source_id = ukm::AssignNewSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);

  const ukm::SourceId mock_source_id2 = ukm::AssignNewSourceId();
  mock_graph.other_page->SetType(performance_manager::PageType::kTab);
  mock_graph.other_page->SetUkmSourceId(mock_source_id2);

  // Start with page 1 in foreground. Pages remain in the same state for all
  // of the first measurement period.
  mock_graph.page->SetIsVisible(true);
  mock_graph.other_page->SetIsVisible(false);
  TriggerCollectPageResourceUsage();

  // Pages become audible for all of next measurement period. Change the state
  // before waiting for the metrics to finish logging so the change happens at
  // the beginning of the period.
  mock_graph.page->SetIsAudible(true);
  mock_graph.other_page->SetIsAudible(true);

  // Test the metrics from the first measurement period.
  WaitForMetricsAndTestBackgroundStates(
      {{mock_source_id, PageMeasurementBackgroundState::kForeground},
       {mock_source_id2, PageMeasurementBackgroundState::kBackground}});

  // Finish the second measurement period.
  TriggerCollectPageResourceUsage();
  WaitForMetricsAndTestBackgroundStates(
      {{mock_source_id, PageMeasurementBackgroundState::kForeground},
       {mock_source_id2,
        PageMeasurementBackgroundState::kAudibleInBackground}});

  // Partway through next measurement period:
  // - Page 1 moves to background (still audible).
  // - Page 2 stops playing audio.
  task_env().FastForwardBy(TestTimeouts::action_timeout());
  mock_graph.page->SetIsVisible(false);
  mock_graph.other_page->SetIsAudible(false);
  TriggerCollectPageResourceUsage();
  WaitForMetricsAndTestBackgroundStates(
      {{mock_source_id,
        PageMeasurementBackgroundState::kMixedForegroundBackground},
       {mock_source_id2,
        PageMeasurementBackgroundState::kBackgroundMixedAudible}});

  // Partway through next measurement period, page 2 moves to foreground (still
  // inaudible).
  task_env().FastForwardBy(TestTimeouts::action_timeout());
  mock_graph.other_page->SetIsVisible(true);
  TriggerCollectPageResourceUsage();
  WaitForMetricsAndTestBackgroundStates(
      {{mock_source_id, PageMeasurementBackgroundState::kAudibleInBackground},
       {mock_source_id2,
        PageMeasurementBackgroundState::kMixedForegroundBackground}});
}

#if !BUILDFLAG(IS_ANDROID)
TEST_P(PageResourceMonitorUnitTest, TestCPUInterventionMetrics) {
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
  GetCPUDelegate(mock_graph.process.get()).SetCPUUsage(0.01);
  GetCPUDelegate(mock_graph.other_process.get()).SetCPUUsage(0.01);

  {
    PatternedHistogramTester histograms;
    TriggerCollectPageResourceUsage();

    // At the end of the measurement interval, before waiting for metrics to be
    // logged, increase the CPU usage so that the new level applies to the whole
    // next interval.
    //
    // The intervention metrics measure total CPU, not percentage of each core,
    // so set the measurement delegates to return half of the total available
    // CPU (100% per processor).
    GetCPUDelegate(mock_graph.process.get())
        .SetCPUUsage(base::SysInfo::NumberOfProcessors() / 2.0);
    GetCPUDelegate(mock_graph.other_process.get())
        .SetCPUUsage(base::SysInfo::NumberOfProcessors() / 2.0);

    WaitForMetrics(/*include_system_cpu=*/true);

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

  // Finish the next measurement interval with the higher CPU usage.
  {
    PatternedHistogramTester histograms;
    TriggerCollectPageResourceUsage();
    WaitForMetrics(/*include_system_cpu=*/true);

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

  // Fast forward for Delayed UMA to be logged.
  {
    PatternedHistogramTester histograms;
    WaitForDelayedCPUInterventionMetrics();

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

    histograms.ExpectNone("DurationOverThreshold");
  }

  // Finish this measurement interval, then lower the CPU measurement so the
  // average CPU usage over the next interval is under the threshold.
  TriggerCollectPageResourceUsage();
  GetCPUDelegate(mock_graph.process.get())
      .SetCPUUsage(base::SysInfo::NumberOfProcessors() / 8.0);
  GetCPUDelegate(mock_graph.other_process.get())
      .SetCPUUsage(base::SysInfo::NumberOfProcessors() / 8.0);

  {
    PatternedHistogramTester histograms;
    TriggerCollectPageResourceUsage();
    WaitForMetrics(/*include_system_cpu=*/true);

    // CPU was over the threshold for one interval, and went under it during the
    // second interval. The drop was detected at the end of the interval.
    histograms.ExpectUniqueSample(
        "DurationOverThreshold",
        (2 * monitor()->GetCollectionDelayForTesting()).InMilliseconds());

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

TEST_P(PageResourceMonitorUnitTest, CPUInterventionMetricsNoForegroundTabs) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  GetCPUDelegate(mock_graph.process.get())
      .SetCPUUsage(base::SysInfo::NumberOfProcessors());

  // Put the only tab in the background.
  mock_graph.page->SetIsVisible(false);

  PatternedHistogramTester histograms;
  TriggerCollectPageResourceUsage();
  WaitForMetrics();

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

TEST_P(PageResourceMonitorUnitTest, CPUInterventionMetricsNoBackgroundTabs) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  GetCPUDelegate(mock_graph.process.get())
      .SetCPUUsage(base::SysInfo::NumberOfProcessors());

  // Put the only tab in the foreground.
  mock_graph.page->SetIsVisible(true);

  PatternedHistogramTester histograms;
  TriggerCollectPageResourceUsage();
  WaitForMetrics();

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

// A test of CPU intervention logging when the system CPUProbe is not available.
class PageResourceMonitorNoCPUProbeTest : public PageResourceMonitorUnitTest {
 public:
  PageResourceMonitorNoCPUProbeTest() { enable_system_cpu_probe_ = false; }
};

INSTANTIATE_TEST_SUITE_P(All,
                         PageResourceMonitorNoCPUProbeTest,
                         ::testing::ValuesIn(kAllTestParams));

TEST_P(PageResourceMonitorNoCPUProbeTest,
       CPUInterventionMetricsWithoutSystemCPU) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetIsVisible(false);

  GetCPUDelegate(mock_graph.process.get())
      .SetCPUUsage(base::SysInfo::NumberOfProcessors());

  PatternedHistogramTester histograms;
  TriggerCollectPageResourceUsage();
  // Let enough time pass for Delayed histograms to be logged too.
  WaitForDelayedCPUInterventionMetrics();

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

}  // namespace

}  // namespace performance_manager::metrics
