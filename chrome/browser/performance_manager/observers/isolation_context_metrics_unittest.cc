// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/isolation_context_metrics.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"

namespace performance_manager {

class TestIsolationContextMetrics : public IsolationContextMetrics {
 public:
  TestIsolationContextMetrics() = default;
  ~TestIsolationContextMetrics() override = default;

  void OnReportingTimerFired() override {
    IsolationContextMetrics::OnReportingTimerFired();
    if (on_reporting_timer_fired_closure_)
      on_reporting_timer_fired_closure_.Run();
  }

  // Expose some things for testing.
  using IsolationContextMetrics::GetProcessDataState;
  using IsolationContextMetrics::kFramesPerRendererByTimeHistogram;
  using IsolationContextMetrics::kProcessDataByProcessHistogramName;
  using IsolationContextMetrics::kProcessDataByTimeHistogramName;
  using IsolationContextMetrics::kReportingInterval;
  using IsolationContextMetrics::kSiteInstancesPerRendererByTimeHistogram;
  using IsolationContextMetrics::ProcessData;
  using IsolationContextMetrics::ProcessDataState;

  // This closure will be invoked when OnReportingTimerFired. Allows the timer
  // to be exercised under tests.
  base::RepeatingClosure on_reporting_timer_fired_closure_;
};

class IsolationContextMetricsTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  IsolationContextMetricsTest()
      : GraphTestHarness(
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~IsolationContextMetricsTest() override = default;

  // Bring some types into the namespace for convenience.
  using ProcessData = TestIsolationContextMetrics::ProcessData;
  using ProcessDataState = TestIsolationContextMetrics::ProcessDataState;

  // Browsing instance IDs.
  static constexpr int32_t kBID1 = 1;
  static constexpr int32_t kBID2 = 2;
  static constexpr int32_t kBID3 = 3;

  // Site instance IDs.
  static constexpr int32_t kSID1 = 1;
  static constexpr int32_t kSID2 = 2;
  static constexpr int32_t kSID3 = 3;

  void SetUp() override {
    Super::SetUp();
    metrics_ = new TestIsolationContextMetrics();

    // Sets a valid starting time.
    AdvanceClock(base::TimeDelta::FromSeconds(1));
    graph()->PassToGraph(base::WrapUnique(metrics_));
  }

  // A frame node constructor that lets us specify the browsing instance ID and
  // site instance ID, but defaults everything else.
  TestNodeWrapper<FrameNodeImpl> CreateFrameNode(
      ProcessNodeImpl* process_node,
      PageNodeImpl* page_node,
      int32_t browsing_instance_id,
      int32_t site_instance_id,
      FrameNodeImpl* parent_frame_node = nullptr) {
    return CreateNode<FrameNodeImpl>(
        process_node, page_node, parent_frame_node, 0 /* frame_tree_node_id */,
        ++next_render_frame_id_, blink::LocalFrameToken(), browsing_instance_id,
        site_instance_id);
  }

  // Advance time until the timer fires.
  void FastForwardUntilTimerFires() {
    base::RunLoop run_loop;
    metrics_->on_reporting_timer_fired_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    metrics_->on_reporting_timer_fired_closure_ = base::RepeatingClosure();
  }

  base::HistogramTester histogram_tester_;
  TestIsolationContextMetrics* metrics_;
  int next_render_frame_id_ = 0;
};

// static
constexpr int32_t IsolationContextMetricsTest::kBID1;
constexpr int32_t IsolationContextMetricsTest::kBID2;
constexpr int32_t IsolationContextMetricsTest::kBID3;
constexpr int32_t IsolationContextMetricsTest::kSID1;
constexpr int32_t IsolationContextMetricsTest::kSID2;
constexpr int32_t IsolationContextMetricsTest::kSID3;

TEST_F(IsolationContextMetricsTest, GetProcessDataState) {
  TestIsolationContextMetrics::ProcessData data;
  EXPECT_TRUE(data.site_instance_frame_count.empty());
  EXPECT_EQ(0, data.multi_frame_site_instance_count);
  EXPECT_FALSE(data.has_hosted_multiple_frames_with_same_site_instance);
  EXPECT_EQ(task_env().NowTicks(), data.last_reported);

  EXPECT_EQ(ProcessDataState::kUndefined,
            TestIsolationContextMetrics::GetProcessDataState(&data));

  // Make up a site instance with one frame.
  data.site_instance_frame_count[kSID1] = 1;
  EXPECT_EQ(1u, data.site_instance_frame_count.size());
  EXPECT_EQ(ProcessDataState::kOnlyOneFrameExists,
            TestIsolationContextMetrics::GetProcessDataState(&data));

  // Make up another site instance with one frame.
  data.site_instance_frame_count[kSID2] = 1;
  EXPECT_EQ(2u, data.site_instance_frame_count.size());
  EXPECT_EQ(ProcessDataState::kAllFramesHaveDistinctSiteInstances,
            TestIsolationContextMetrics::GetProcessDataState(&data));

  // Make one site instance have multiple frames.
  data.site_instance_frame_count[kSID1] = 2;
  data.multi_frame_site_instance_count = 1;
  data.has_hosted_multiple_frames_with_same_site_instance = true;
  EXPECT_EQ(2u, data.site_instance_frame_count.size());
  EXPECT_EQ(ProcessDataState::kSomeFramesHaveSameSiteInstance,
            TestIsolationContextMetrics::GetProcessDataState(&data));

  // Make the second site instance have multiple frames.
  data.site_instance_frame_count[kSID2] = 2;
  data.multi_frame_site_instance_count = 2;
  EXPECT_EQ(2u, data.site_instance_frame_count.size());
  EXPECT_EQ(ProcessDataState::kSomeFramesHaveSameSiteInstance,
            TestIsolationContextMetrics::GetProcessDataState(&data));

  // Reduce the first site instance to 1 frame.
  data.site_instance_frame_count[kSID1] = 1;
  data.multi_frame_site_instance_count = 1;
  EXPECT_EQ(2u, data.site_instance_frame_count.size());
  EXPECT_EQ(ProcessDataState::kSomeFramesHaveSameSiteInstance,
            TestIsolationContextMetrics::GetProcessDataState(&data));

  // And reduce the second site instance to 1 frame.
  data.site_instance_frame_count[kSID2] = 1;
  data.multi_frame_site_instance_count = 0;
  EXPECT_EQ(2u, data.site_instance_frame_count.size());
  EXPECT_EQ(ProcessDataState::kAllFramesHaveDistinctSiteInstances,
            TestIsolationContextMetrics::GetProcessDataState(&data));

  // Erase the first site instance.
  data.site_instance_frame_count.erase(kSID1);
  EXPECT_EQ(1u, data.site_instance_frame_count.size());
  EXPECT_EQ(ProcessDataState::kOnlyOneFrameExists,
            TestIsolationContextMetrics::GetProcessDataState(&data));
}

TEST_F(IsolationContextMetricsTest, ProcessDataReporting) {
  metrics_->StartTimer();

  // Create a process that never hosts any frames. It should never contribute
  // at all to the metrics.
  auto empty_process = CreateNode<ProcessNodeImpl>();

  // Create a process that hosts 1 frame from 1 page.
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame1 = CreateFrameNode(process.get(), page.get(), kBID1, kSID1);

  // Expect the ProcessData to exist and be correctly filled out.
  auto* data1 = ProcessData::GetOrCreate(process.get());
  EXPECT_EQ(1u, data1->site_instance_frame_count.size());
  EXPECT_EQ(1, data1->frame_count);
  EXPECT_EQ(0, data1->multi_frame_site_instance_count);
  EXPECT_FALSE(data1->has_hosted_multiple_frames);
  EXPECT_FALSE(data1->has_hosted_multiple_frames_with_same_site_instance);
  EXPECT_EQ(task_env().NowTicks(), data1->last_reported);
  EXPECT_EQ(ProcessDataState::kOnlyOneFrameExists,
            TestIsolationContextMetrics::GetProcessDataState(data1));

  // Expect no metrics to have been emitted.
  histogram_tester_.ExpectTotalCount(metrics_->kProcessDataByTimeHistogramName,
                                     0);
  histogram_tester_.ExpectTotalCount(
      metrics_->kProcessDataByProcessHistogramName, 0);
  histogram_tester_.ExpectTotalCount(
      metrics_->kFramesPerRendererByTimeHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      metrics_->kSiteInstancesPerRendererByTimeHistogram, 0);

  FastForwardUntilTimerFires();

  // Expect "by time" metrics to have been emitted.
  EXPECT_EQ(task_env().NowTicks(), data1->last_reported);
  histogram_tester_.ExpectUniqueSample(
      metrics_->kProcessDataByTimeHistogramName,
      ProcessDataState::kOnlyOneFrameExists,
      metrics_->kReportingInterval.InSeconds());
  histogram_tester_.ExpectTotalCount(
      metrics_->kProcessDataByProcessHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      metrics_->kFramesPerRendererByTimeHistogram, 1,
      metrics_->kReportingInterval.InSeconds());
  histogram_tester_.ExpectUniqueSample(
      metrics_->kSiteInstancesPerRendererByTimeHistogram, 1,
      metrics_->kReportingInterval.InSeconds());

  {
    // Advance time and add another frame to a new site instance, as a child
    // of |frame1|.
    AdvanceClock(base::TimeDelta::FromSeconds(1));
    auto frame2 =
        CreateFrameNode(process.get(), page.get(), kBID1, kSID2, frame1.get());
    EXPECT_EQ(2u, data1->site_instance_frame_count.size());
    EXPECT_EQ(2, data1->frame_count);
    EXPECT_EQ(0, data1->multi_frame_site_instance_count);
    EXPECT_TRUE(data1->has_hosted_multiple_frames);
    EXPECT_FALSE(data1->has_hosted_multiple_frames_with_same_site_instance);
    EXPECT_EQ(ProcessDataState::kAllFramesHaveDistinctSiteInstances,
              TestIsolationContextMetrics::GetProcessDataState(data1));

    // Expect metrics to have been reported on the state change.
    EXPECT_EQ(task_env().NowTicks(), data1->last_reported);
    histogram_tester_.ExpectUniqueSample(
        metrics_->kProcessDataByTimeHistogramName,
        ProcessDataState::kOnlyOneFrameExists,
        metrics_->kReportingInterval.InSeconds() + 1);
    histogram_tester_.ExpectTotalCount(
        metrics_->kProcessDataByProcessHistogramName, 0);
    histogram_tester_.ExpectBucketCount(
        metrics_->kFramesPerRendererByTimeHistogram, 1,
        metrics_->kReportingInterval.InSeconds());
    histogram_tester_.ExpectBucketCount(
        metrics_->kFramesPerRendererByTimeHistogram, 2, 1);
    histogram_tester_.ExpectBucketCount(
        metrics_->kSiteInstancesPerRendererByTimeHistogram, 1,
        metrics_->kReportingInterval.InSeconds());
    histogram_tester_.ExpectBucketCount(
        metrics_->kSiteInstancesPerRendererByTimeHistogram, 2, 1);

    // Advance time.
    AdvanceClock(base::TimeDelta::FromSeconds(1));
  }

  // The second frame will be destroyed as it goes out of scope. Expect another
  // flush of metrics.
  EXPECT_EQ(task_env().NowTicks(), data1->last_reported);
  histogram_tester_.ExpectTotalCount(
      metrics_->kProcessDataByTimeHistogramName,
      metrics_->kReportingInterval.InSeconds() + 2);
  histogram_tester_.ExpectBucketCount(
      metrics_->kProcessDataByTimeHistogramName,
      ProcessDataState::kAllFramesHaveDistinctSiteInstances, 1);
  histogram_tester_.ExpectBucketCount(
      metrics_->kProcessDataByTimeHistogramName,
      ProcessDataState::kOnlyOneFrameExists,
      metrics_->kReportingInterval.InSeconds() + 1);
  histogram_tester_.ExpectTotalCount(
      metrics_->kProcessDataByProcessHistogramName, 0);
  histogram_tester_.ExpectBucketCount(
      metrics_->kFramesPerRendererByTimeHistogram, 1,
      metrics_->kReportingInterval.InSeconds() + 1);
  histogram_tester_.ExpectBucketCount(
      metrics_->kFramesPerRendererByTimeHistogram, 2, 1);
  histogram_tester_.ExpectBucketCount(
      metrics_->kSiteInstancesPerRendererByTimeHistogram, 1,
      metrics_->kReportingInterval.InSeconds() + 1);
  histogram_tester_.ExpectBucketCount(
      metrics_->kSiteInstancesPerRendererByTimeHistogram, 2, 1);

  {
    // Advance time and add another frame to the same site instance, as a child
    // of |frame1|.
    AdvanceClock(base::TimeDelta::FromSeconds(1));
    auto frame2 =
        CreateFrameNode(process.get(), page.get(), kBID1, kSID1, frame1.get());
    EXPECT_EQ(1u, data1->site_instance_frame_count.size());
    EXPECT_EQ(2, data1->frame_count);
    EXPECT_EQ(1, data1->multi_frame_site_instance_count);
    EXPECT_TRUE(data1->has_hosted_multiple_frames);
    EXPECT_TRUE(data1->has_hosted_multiple_frames_with_same_site_instance);
    EXPECT_EQ(ProcessDataState::kSomeFramesHaveSameSiteInstance,
              TestIsolationContextMetrics::GetProcessDataState(data1));

    // Expect metrics to have been reported on the state change.
    EXPECT_EQ(task_env().NowTicks(), data1->last_reported);
    histogram_tester_.ExpectTotalCount(
        metrics_->kProcessDataByTimeHistogramName,
        metrics_->kReportingInterval.InSeconds() + 3);
    histogram_tester_.ExpectBucketCount(
        metrics_->kProcessDataByTimeHistogramName,
        ProcessDataState::kAllFramesHaveDistinctSiteInstances, 1);
    histogram_tester_.ExpectBucketCount(
        metrics_->kProcessDataByTimeHistogramName,
        ProcessDataState::kOnlyOneFrameExists,
        metrics_->kReportingInterval.InSeconds() + 2);
    histogram_tester_.ExpectTotalCount(
        metrics_->kProcessDataByProcessHistogramName, 0);
    histogram_tester_.ExpectBucketCount(
        metrics_->kFramesPerRendererByTimeHistogram, 1,
        metrics_->kReportingInterval.InSeconds() + 1);
    histogram_tester_.ExpectBucketCount(
        metrics_->kFramesPerRendererByTimeHistogram, 2, 2);
    histogram_tester_.ExpectBucketCount(
        metrics_->kSiteInstancesPerRendererByTimeHistogram, 1,
        metrics_->kReportingInterval.InSeconds() + 2);
    histogram_tester_.ExpectBucketCount(
        metrics_->kSiteInstancesPerRendererByTimeHistogram, 2, 1);

    // Advance time.
    AdvanceClock(base::TimeDelta::FromSeconds(1));
  }

  // The second frame will be destroyed as it goes out of scope. Expect another
  // flush of metrics.
  EXPECT_EQ(task_env().NowTicks(), data1->last_reported);
  histogram_tester_.ExpectTotalCount(
      metrics_->kProcessDataByTimeHistogramName,
      metrics_->kReportingInterval.InSeconds() + 4);
  histogram_tester_.ExpectBucketCount(
      metrics_->kProcessDataByTimeHistogramName,
      ProcessDataState::kAllFramesHaveDistinctSiteInstances, 1);
  histogram_tester_.ExpectBucketCount(
      metrics_->kProcessDataByTimeHistogramName,
      ProcessDataState::kSomeFramesHaveSameSiteInstance, 1);
  histogram_tester_.ExpectBucketCount(
      metrics_->kProcessDataByTimeHistogramName,
      ProcessDataState::kOnlyOneFrameExists,
      metrics_->kReportingInterval.InSeconds() + 2);
  histogram_tester_.ExpectTotalCount(
      metrics_->kProcessDataByProcessHistogramName, 0);
  histogram_tester_.ExpectBucketCount(
      metrics_->kFramesPerRendererByTimeHistogram, 1,
      metrics_->kReportingInterval.InSeconds() + 2);
  histogram_tester_.ExpectBucketCount(
      metrics_->kFramesPerRendererByTimeHistogram, 2, 2);
  histogram_tester_.ExpectBucketCount(
      metrics_->kSiteInstancesPerRendererByTimeHistogram, 1,
      metrics_->kReportingInterval.InSeconds() + 3);
  histogram_tester_.ExpectBucketCount(
      metrics_->kSiteInstancesPerRendererByTimeHistogram, 2, 1);

  // Destroy the other frame and the page. No metrics should be flushed.
  {
    base::HistogramTester tester;
    frame1.reset();
    page.reset();
    tester.ExpectTotalCount(metrics_->kProcessDataByTimeHistogramName, 0);
    tester.ExpectTotalCount(metrics_->kProcessDataByProcessHistogramName, 0);
  }

  // Finally, destroy the process. This should flush metrics to the
  // "by process" histogram.
  {
    base::HistogramTester tester;
    process.reset();
    tester.ExpectTotalCount(metrics_->kProcessDataByTimeHistogramName, 0);
    tester.ExpectUniqueSample(metrics_->kProcessDataByProcessHistogramName,
                              ProcessDataState::kSomeFramesHaveSameSiteInstance,
                              1);
  }

  // Ensure that the empty process never ended up having ProcessData created
  // for it.
  EXPECT_FALSE(ProcessData::Get(empty_process.get()));

  // Destroy the empty process and expect no new metrics.
  {
    base::HistogramTester tester;
    empty_process.reset();
    tester.ExpectTotalCount(metrics_->kProcessDataByTimeHistogramName, 0);
    tester.ExpectTotalCount(metrics_->kProcessDataByProcessHistogramName, 0);
  }
}

}  // namespace performance_manager
