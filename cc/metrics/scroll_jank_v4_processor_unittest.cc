// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_processor.h"

#include <string>
#include <utility>
#include <variant>

#include "base/strings/to_string.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_trace_processor.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "cc/metrics/event_metrics.h"
#include "cc/test/event_metrics_test_creator.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

using DispatchBeginFrameArgs = ScrollEventMetrics::DispatchBeginFrameArgs;
using TraceId = EventMetrics::TraceId;
using QueryResult = base::test::TestTraceProcessor::QueryResult;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

constexpr base::TimeDelta kVsyncInterval = base::Milliseconds(16);

constexpr base::TimeTicks MillisSinceEpoch(int64_t millis) {
  return base::TimeTicks() + base::Milliseconds(millis);
}

// Variants of the `features::kHandleNonDamagingInputsInScrollJankV4Metric`
// feature and its configuration.
enum class TestVariant {
  // Legacy behavior where `ScrollJankV4Processor` will ignore non-damaging
  // inputs (similarly to the scroll jank v1 metric).
  //
  // Disables `features::kHandleNonDamagingInputsInScrollJankV4Metric`.
  kLegacyBehavior,

  // New behavior where `ScrollJankV4Processor` will reconstruct a timeline of
  // non-damaging and damaging frames for the purposes of evaluating scroll
  // jank. `ScrollJankV4HistogramEmitter` will emit fixed window UMA
  // histograms after each window of 64 damaging frames.
  //
  // Enables `features::kHandleNonDamagingInputsInScrollJankV4Metric` with
  // `feature::kCountNonDamagingFramesTowardsHistogramFrameCount` set to false.
  kNewBehaviorCountDamagingFramesOnly,

  // New behavior where `ScrollJankV4Processor` will reconstruct a timeline of
  // non-damaging and damaging frames for the purposes of evaluating scroll
  // jank. `ScrollJankV4HistogramEmitter` will emit fixed window UMA
  // histograms after each window of 64 frames (both damaging and non-damaging).
  //
  // Enables `features::kHandleNonDamagingInputsInScrollJankV4Metric` with
  // `feature::kCountNonDamagingFramesTowardsHistogramFrameCount` set to true.
  kNewBehaviorCountAllFrames
};

struct ScrollJankV4ProcessorTestCase {
  TestVariant variant;
  std::string test_name;
};

constexpr const char kTraceQuery[] =
    R"(
    SELECT
      EXTRACT_ARG(
          arg_set_id,
          'scroll_jank_v4.updates.real.first_event_latency_id'
        ) AS event_latency_id,
      EXTRACT_ARG(arg_set_id, 'scroll_jank_v4.is_janky')
        AS is_janky,
      (
        -- Concatenate the `missed_vsyncs_per_jank_reason` repeated field into a
        -- single string. For example, the following value:
        --
        --   { jank_reason: REASON_A, missed_vsyncs: 1 }
        --   { jank_reason: REASON_B, missed_vsyncs: 2 }
        --   { jank_reason: REASON_C, missed_vsyncs: 3 }
        --
        -- is converted to 'REASON_A(1),REASON_B(2),REASON_C(3)'.
        SELECT
          GROUP_CONCAT(
            FORMAT('%s(%d)', jank_reason, missed_vsyncs),
            ','
            ORDER BY jank_reason ASC
          )
        FROM
          (
            SELECT
              args.string_value AS jank_reason,
              SUBSTRING(args.key, 1, LENGTH(args.key) - LENGTH('.jank_reason'))
                AS key_prefix
            FROM args
            WHERE
              args.arg_set_id = slice.arg_set_id
              AND args.flat_key =
                'scroll_jank_v4.missed_vsyncs_per_jank_reason.jank_reason'
          )
        JOIN
          (
            SELECT
              args.int_value AS missed_vsyncs,
              SUBSTRING(
                args.key,
                1,
                LENGTH(args.key) - LENGTH('.missed_vsyncs')
              ) AS key_prefix
            FROM args
            WHERE
              args.arg_set_id = slice.arg_set_id
              AND args.flat_key =
                'scroll_jank_v4.missed_vsyncs_per_jank_reason.missed_vsyncs'
          )
          -- Join the corresponding `jank_reason` and `missed_vsyncs` via
          -- their shared 'scroll_jank_v4.missed_vsyncs_per_jank_reason[INDEX]'
          -- key prefix.
          USING(key_prefix)
      ) AS jank_reasons
    FROM slice
    WHERE name = 'ScrollJankV4'
    ORDER BY event_latency_id ASC;
    )";

class ExpectedTraceResults {
 public:
  ~ExpectedTraceResults() {
    if (!expected_results_.empty()) {
      ADD_FAILURE()
          << "Non-empty expected_results_. Did you forget to call Take()?";
    }
  }

  void ExpectIsNotJanky(int trace_id) {
    expected_results_.push_back({base::ToString(trace_id), "0", "[NULL]"});
  }

  void ExpectIsJanky(int trace_id, const char* jank_reasons) {
    expected_results_.push_back({base::ToString(trace_id), "1", jank_reasons});
  }

  QueryResult Take() && {
    QueryResult results;
    std::swap(results, expected_results_);
    return results;
  }

 private:
  QueryResult expected_results_ = {
      {"event_latency_id", "is_janky", "jank_reasons"}};
};

}  // namespace

// Unit test of `ScrollJankV4Processor` parameterized by the
// `features::kHandleNonDamagingInputsInScrollJankV4Metric` feature and its
// configuration. Each of the test cases represents a possible scenario of
// input→frame delivery. The test cases document how the handling of
// non-damaging inputs and frames differs based on the above feature.
class ScrollJankV4ProcessorTest
    : public testing::TestWithParam<ScrollJankV4ProcessorTestCase> {
 public:
  void SetUp() override {
    switch (GetParam().variant) {
      case TestVariant::kLegacyBehavior:
        scoped_feature_list_.InitAndDisableFeature(
            features::kHandleNonDamagingInputsInScrollJankV4Metric);
        break;
      case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        scoped_feature_list_.InitAndEnableFeatureWithParameters(
            features::kHandleNonDamagingInputsInScrollJankV4Metric,
            {{features::kCountNonDamagingFramesTowardsHistogramFrameCount.name,
              base::ToString(false)}});
        break;
      case TestVariant::kNewBehaviorCountAllFrames:
        scoped_feature_list_.InitAndEnableFeatureWithParameters(
            features::kHandleNonDamagingInputsInScrollJankV4Metric,
            {{features::kCountNonDamagingFramesTowardsHistogramFrameCount.name,
              base::ToString(true)}});
        break;
    }
  }

 protected:
  void AdvanceByVsyncs(int vsyncs) {
    base::TimeDelta offset = vsyncs * kVsyncInterval;
    next_input_generation_ts_ += offset;
    next_begin_frame_ts_ += offset;
    next_presentation_ts_ += offset;
  }

  viz::BeginFrameArgs CreateNextBeginFrameArgs() {
    return viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, /* source_id= */ 1,
        next_begin_frame_sequence_id_++,
        /* frame_time= */ next_begin_frame_ts_,
        /* deadline= */ next_begin_frame_ts_ + kVsyncInterval / 3,
        kVsyncInterval, viz::BeginFrameArgs::BeginFrameArgsType::NORMAL);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::TimeTicks next_input_generation_ts_ = MillisSinceEpoch(4);
  base::TimeTicks next_begin_frame_ts_ = MillisSinceEpoch(16);
  base::TimeTicks next_presentation_ts_ = MillisSinceEpoch(32);
  int next_begin_frame_sequence_id_ = 1;
  EventMetricsTestCreator metrics_creator_;
  ScrollJankV4Processor processor_;
  base::test::TracingEnvironment tracing_environment_;
  base::test::TestTraceProcessor trace_processor_;

  QueryResult QueryTraceProcessor(const char* query) {
    auto result = trace_processor_.RunQuery(query);
    if (!result.has_value()) {
      ADD_FAILURE() << result.error();
      return {};
    }
    return result.value();
  }
};

/*
Tests that, regardless of `TestVariant`, the scroll jank v4 metric doesn't mark
frame production with consistent input delivery where each frame contains a
damaging scroll update as janky.
*/
TEST_P(ScrollJankV4ProcessorTest, ConsistentDamagingFrameProduction) {
  trace_processor_.StartTrace("input");
  ExpectedTraceResults expected_results;

  // Start a scroll and present frames 1-64.
  {
    base::HistogramTester histogram_tester;

    // Start with a regular scroll with two inputs per frame.
    {
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List first_metrics;
      first_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(10),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      first_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ + kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(11),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          first_metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(10);
    }

    for (int i = 2; i <= 50; i++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(i * 10),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ + kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(i * 10 + 1),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(i * 10);
    }

    // Switch to a fling with one input per frame.
    for (int i = 51; i <= 64; i++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 2.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(i * 10),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(i * 10);
    }

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // Present frame 65 (end of first fixed window).
  {
    base::HistogramTester histogram_tester;

    AdvanceByVsyncs(1);
    viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
    EventMetrics::List last_metrics_in_fixed_window;
    last_metrics_in_fixed_window.push_back(
        metrics_creator_.CreateInertialGestureScrollUpdate(
            {.timestamp = next_input_generation_ts_,
             .delta = 2.0f,
             .caused_frame_update = true,
             .did_scroll = true,
             .trace_id = TraceId(650),
             .dispatch_args = DispatchBeginFrameArgs::From(args)}));
    processor_.ProcessEventsMetricsForPresentedFrame(
        last_metrics_in_fixed_window, next_presentation_ts_, args);
    expected_results.ExpectIsNotJanky(650);

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // Present 35 more frames.
  {
    base::HistogramTester histogram_tester;

    for (int i = 66; i <= 100; i++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 2.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(i * 10),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(i * 10);
    }

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // Finally, end the scroll.
  {
    base::HistogramTester histogram_tester;

    AdvanceByVsyncs(1);
    viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
    EventMetrics::List end_metrics;
    end_metrics.push_back(metrics_creator_.CreateInertialGestureScrollEnd(
        {.timestamp = next_input_generation_ts_,
         .caused_frame_update = false,
         .dispatch_args = DispatchBeginFrameArgs::From(args)}));
    processor_.ProcessEventsMetricsForPresentedFrame(
        end_metrics, next_presentation_ts_, args);

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0, 1);
  }

  absl::Status status = trace_processor_.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_THAT(QueryTraceProcessor(kTraceQuery),
              ElementsAreArray(std::move(expected_results).Take()));
}

/*
Tests that, regardless of `TestVariant`, the scroll jank v4 metric marks
"hiccups" in frame production with inconsistent damaging input delivery where
each frame contains a damaging scroll update as janky.
*/
TEST_P(ScrollJankV4ProcessorTest, InconsistentDamagingFrameProduction) {
  trace_processor_.StartTrace("input");
  ExpectedTraceResults expected_results;

  // Start a scroll and present frames 1-64.
  {
    base::HistogramTester histogram_tester;

    // Start with a regular scroll with two inputs per frame.
    {
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List first_metrics;
      first_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(10),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      first_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ + kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(11),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          first_metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(10);
    }

    for (int i = 2; i <= 10; i++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(i * 10),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ + kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(i * 10 + 1),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(i * 10);
    }

    {
      // The processor should mark frame 11 as janky:
      // 1. It violates the running consistency rule because the first input
      //    should have been presented 1 VSync earlier (based on Chrome's past
      //    performance).
      // 2. It violates the fast scroll continuity rule because there's more
      //    than 1 VSync between two consecutive presented frames containing
      //    inputs.
      AdvanceByVsyncs(3);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ - kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(110),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(111),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ + kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(112),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsJanky(
          110,
          "MISSED_VSYNC_DUE_TO_DECELERATING_INPUT_FRAME_DELIVERY(1),"
          "MISSED_VSYNC_DURING_FAST_SCROLL(2)");
    }

    for (int i = 12; i <= 50; i++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(i * 10),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ + kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(i * 10 + 1),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(i * 10);
    }

    // Switch to a fling with one input per frame.
    {
      // The processor should mark frame 51 as janky. It violates the fling
      // continuity rule because Chrome missed 5 VSyncs at the transition from a
      // fast regular scroll to a fast fling as janky.
      AdvanceByVsyncs(6);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 2.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(510),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsJanky(510, "MISSED_VSYNC_AT_START_OF_FLING(5)");
    }

    for (int i = 52; i <= 64; i++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 2.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(i * 10),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(i * 10);
    }

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // Present frame 65 (end of first fixed window).
  {
    base::HistogramTester histogram_tester;

    AdvanceByVsyncs(1);
    viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
    EventMetrics::List last_metrics_in_fixed_window;
    last_metrics_in_fixed_window.push_back(
        metrics_creator_.CreateInertialGestureScrollUpdate(
            {.timestamp = next_input_generation_ts_,
             .delta = 2.0f,
             .caused_frame_update = true,
             .did_scroll = true,
             .trace_id = TraceId(650),
             .dispatch_args = DispatchBeginFrameArgs::From(args)}));
    processor_.ProcessEventsMetricsForPresentedFrame(
        last_metrics_in_fixed_window, next_presentation_ts_, args);
    expected_results.ExpectIsNotJanky(650);

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 2 * 100 / 64,
        1);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // Present 35 more frames.
  {
    base::HistogramTester histogram_tester;

    for (int i = 66; i <= 80; i++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 2.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(i * 10),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(i * 10);
    }

    // The processor should mark frame 81 as janky. It violates the fling
    // continuity rule because Chrome missed 9 VSyncs in the middle of a fast
    // fling.
    {
      AdvanceByVsyncs(10);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 2.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(810),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsJanky(810, "MISSED_VSYNC_DURING_FLING(9)");
    }

    for (int i = 82; i <= 100; i++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 2.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(10 * i),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(10 * i);
    }

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // Finally, end the scroll.
  {
    base::HistogramTester histogram_tester;

    AdvanceByVsyncs(1);
    viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
    EventMetrics::List end_metrics;
    end_metrics.push_back(metrics_creator_.CreateInertialGestureScrollEnd(
        {.timestamp = next_input_generation_ts_,
         .caused_frame_update = false,
         .dispatch_args = DispatchBeginFrameArgs::From(args)}));
    processor_.ProcessEventsMetricsForPresentedFrame(
        end_metrics, next_presentation_ts_, args);

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 3 * 100 / 100,
        1);
  }

  absl::Status status = trace_processor_.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_THAT(QueryTraceProcessor(kTraceQuery),
              ElementsAreArray(std::move(expected_results).Take()));
}

/*
Tests the behavior of the scroll jank v4 metric on consistent input delivery
with both damaging and non-damaging frames.

Both `TestVariant::kNewBehaviorCountDamagingFramesOnly` and
`TestVariant::kNewBehaviorCountAllFrames` should correctly mark all frames as
non-janky.

`TestVariant::kLegacyBehavior` yields several false positives because it ignores
non-damaging inputs.
*/
TEST_P(ScrollJankV4ProcessorTest, ConsistentMixedFrameProduction) {
  trace_processor_.StartTrace("input");
  ExpectedTraceResults expected_results;

  // Start with a regular scroll with two inputs per frame.
  {
    base::HistogramTester histogram_tester;

    {
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List first_metrics;
      first_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(10),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      first_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ + kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(11),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          first_metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(10);
    }
    // Frames presented: 1 damaging, 1 total.

    // Interleave damaging and non-damaging frames.
    for (int damaging_frame = 2; damaging_frame <= 31; damaging_frame++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs non_damaging_args = CreateNextBeginFrameArgs();
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      // Two inputs for a non-damaging frame.
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ - kVsyncInterval,
           .delta = 5.0f,
           .caused_frame_update = false,
           .did_scroll = false,
           .trace_id = TraceId(damaging_frame * 10),
           .dispatch_args = DispatchBeginFrameArgs::From(non_damaging_args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ - kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = false,
           .did_scroll = false,
           .trace_id = TraceId(damaging_frame * 10 + 1),
           .dispatch_args = DispatchBeginFrameArgs::From(non_damaging_args)}));
      // Two inputs for a presented damaging frame.
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(damaging_frame * 10 + 2),
           .dispatch_args = DispatchBeginFrameArgs::From(damaging_args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ + kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(damaging_frame * 10 + 3),
           .dispatch_args = DispatchBeginFrameArgs::From(damaging_args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, damaging_args);
      switch (GetParam().variant) {
        case TestVariant::kLegacyBehavior:
          // The legacy behavior ignores `metrics[0]` (because it's
          // non-damaging) and marks `metrics[2]` as janky due to the fast
          // scroll rule (because the metric sees a missed VSync between two
          // consecutive damaging frames).
          expected_results.ExpectIsJanky(damaging_frame * 10 + 2,
                                         "MISSED_VSYNC_DURING_FAST_SCROLL(1)");
          break;
        case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        case TestVariant::kNewBehaviorCountAllFrames:
          // The new behavior identifies one non-damaging frame (starting with
          // `metrics[0]`) and one damaging frame (starting with `metrics[2]`).
          // It doesn't observe any missed VSyncs, so it doesn't mark any frame
          // as janky.
          expected_results.ExpectIsNotJanky(damaging_frame * 10);
          expected_results.ExpectIsNotJanky(damaging_frame * 10 + 2);
      }
    }
    // Frames presented: 31 damaging, 61 total.

    for (int damaging_frame = 32; damaging_frame <= 34; damaging_frame++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(damaging_frame * 10),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ + kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(damaging_frame * 10 + 1),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(damaging_frame * 10);
    }
    // Frames presented: 34 damaging, 64 total.

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  {
    base::HistogramTester histogram_tester;

    {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(350),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ + kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(351),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(350);
    }
    // Frames presented: 35 damaging, 65 total.

    switch (GetParam().variant) {
      case TestVariant::kLegacyBehavior:
      case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        // Non-damaging frames don't count towards the histogram frame count, so
        // the processor shouldn't emit any histograms yet because it has only
        // seen 35 damaging frames so far.
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
        break;
      case TestVariant::kNewBehaviorCountAllFrames:
        // Non-damaging frames count towards the histogram frame count, so
        // the processor should emit fixed window histograms now because it has
        // seen 65 frames in total.
        histogram_tester.ExpectUniqueSample(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
    }
  }

  // Switch to a fling with one input per frame.
  {
    base::HistogramTester histogram_tester;

    // Interleave non-damaging and damaging frames, but this time the
    // non-damaging frames are presented.
    for (int damaging_frame = 36; damaging_frame <= 64; damaging_frame++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs non_damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List non_damaging_metrics;
      non_damaging_metrics.push_back(
          metrics_creator_.CreateInertialGestureScrollUpdate(
              {.timestamp = next_input_generation_ts_,
               .delta = 2.0f,
               .caused_frame_update = false,
               .did_scroll = false,
               .trace_id = TraceId(damaging_frame * 10),
               .dispatch_args =
                   DispatchBeginFrameArgs::From(non_damaging_args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          non_damaging_metrics, next_presentation_ts_, non_damaging_args);
      switch (GetParam().variant) {
        case TestVariant::kLegacyBehavior:
          break;
        case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        case TestVariant::kNewBehaviorCountAllFrames:
          expected_results.ExpectIsNotJanky(damaging_frame * 10);
      }

      AdvanceByVsyncs(1);
      viz::BeginFrameArgs damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List damaging_metrics;
      damaging_metrics.push_back(
          metrics_creator_.CreateInertialGestureScrollUpdate(
              {.timestamp = next_input_generation_ts_,
               .delta = 2.0f,
               .caused_frame_update = true,
               .did_scroll = true,
               .trace_id = TraceId(damaging_frame * 10 + 1),
               .dispatch_args = DispatchBeginFrameArgs::From(damaging_args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          damaging_metrics, next_presentation_ts_, damaging_args);
      switch (GetParam().variant) {
        case TestVariant::kLegacyBehavior:
          expected_results.ExpectIsJanky(
              damaging_frame * 10 + 1, damaging_frame == 36
                                           ? "MISSED_VSYNC_AT_START_OF_FLING(1)"
                                           : "MISSED_VSYNC_DURING_FLING(1)");
          break;
        case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        case TestVariant::kNewBehaviorCountAllFrames:
          expected_results.ExpectIsNotJanky(damaging_frame * 10 + 1);
      }
    }
    // Frames presented: 64 damaging, 123 total.

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  {
    base::HistogramTester histogram_tester;

    AdvanceByVsyncs(1);
    viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
    EventMetrics::List metrics;
    metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
        {.timestamp = next_input_generation_ts_,
         .delta = 2.0f,
         .caused_frame_update = true,
         .did_scroll = true,
         .trace_id = TraceId(650),
         .dispatch_args = DispatchBeginFrameArgs::From(args)}));
    processor_.ProcessEventsMetricsForPresentedFrame(
        metrics, next_presentation_ts_, args);
    expected_results.ExpectIsNotJanky(650);
    // Frames presented: 65 damaging, 124 total.

    switch (GetParam().variant) {
      case TestVariant::kLegacyBehavior:
        // The processor has finally seen 65 damaging frames, so it should emit
        // fixed window histograms.
        histogram_tester.ExpectUniqueSample(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
            59 * 100 / 64 /* Frames 2-31 & 36-64 */, 1);
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
        break;
      case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        // The processor has finally seen 65 damaging frames, so it should emit
        // fixed window histograms.
        histogram_tester.ExpectUniqueSample(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
        break;
      case TestVariant::kNewBehaviorCountAllFrames:
        // The processor has seen 124 frames in total, which is not at the
        // window boundary, so it shouldn't emit any histograms.
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
    }
  }

  // Present 4 more damaging frames.
  {
    base::HistogramTester histogram_tester;

    for (int damaging_frame = 66; damaging_frame <= 69; damaging_frame++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 2.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(damaging_frame * 10),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(damaging_frame * 10);
    }
    // Frames presented: 69 damaging, 128 total.

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // Finally, end the scroll.
  {
    base::HistogramTester histogram_tester;

    AdvanceByVsyncs(1);
    viz::BeginFrameArgs end_args = CreateNextBeginFrameArgs();
    EventMetrics::List end_metrics;
    end_metrics.push_back(metrics_creator_.CreateInertialGestureScrollEnd(
        {.timestamp = next_input_generation_ts_,
         .caused_frame_update = false,
         .dispatch_args = DispatchBeginFrameArgs::From(end_args)}));
    processor_.ProcessEventsMetricsForPresentedFrame(
        end_metrics, next_presentation_ts_, end_args);

    switch (GetParam().variant) {
      case TestVariant::kLegacyBehavior:
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
        histogram_tester.ExpectUniqueSample(
            "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
            59 * 100 / 69 /* Frames 2-31 & 36-64 */, 1);
        break;
      case TestVariant::kNewBehaviorCountDamagingFramesOnly:
      case TestVariant::kNewBehaviorCountAllFrames:
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
        histogram_tester.ExpectUniqueSample(
            "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0, 1);
    }
  }

  absl::Status status = trace_processor_.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_THAT(QueryTraceProcessor(kTraceQuery),
              ElementsAreArray(std::move(expected_results).Take()));
}

/*
Tests the behavior of the scroll jank v4 metric on inconsistent input delivery
with both damaging and non-damaging frames.

Both `TestVariant::kNewBehaviorCountDamagingFramesOnly` and
`TestVariant::kNewBehaviorCountAllFrames` should correctly mark frames which
missed one or more VSyncs as janky.

`TestVariant::kLegacyBehavior` yields several false positives because it ignores
non-damaging inputs.
*/
TEST_P(ScrollJankV4ProcessorTest, InconsistentMixedFrameProduction) {
  trace_processor_.StartTrace("input");
  ExpectedTraceResults expected_results;

  // Start with a regular scroll with two inputs per frame.
  {
    base::HistogramTester histogram_tester;

    {
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List first_metrics;
      first_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(10),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      first_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ + kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(11),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          first_metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(10);
    }
    // Frames presented: 1 damaging, 1 total.

    // Interleave damaging and non-damaging frames.
    for (int damaging_frame = 2; damaging_frame <= 10; damaging_frame++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs non_damaging_args = CreateNextBeginFrameArgs();
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      // Two inputs for a non-damaging frame.
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ - kVsyncInterval,
           .delta = 5.0f,
           .caused_frame_update = false,
           .did_scroll = false,
           .trace_id = TraceId(damaging_frame * 10),
           .dispatch_args = DispatchBeginFrameArgs::From(non_damaging_args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ - kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = false,
           .did_scroll = false,
           .trace_id = TraceId(damaging_frame * 10 + 1),
           .dispatch_args = DispatchBeginFrameArgs::From(non_damaging_args)}));
      // Two inputs for a presented damaging frame.
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(damaging_frame * 10 + 2),
           .dispatch_args = DispatchBeginFrameArgs::From(damaging_args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ + kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(damaging_frame * 10 + 3),
           .dispatch_args = DispatchBeginFrameArgs::From(damaging_args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, damaging_args);
      switch (GetParam().variant) {
        case TestVariant::kLegacyBehavior:
          // The legacy behavior ignores `metrics[0]` (because it's
          // non-damaging) and marks `metrics[2]` as janky due to the fast
          // scroll rule (because the metric sees a missed VSync between two
          // consecutive damaging frames).
          expected_results.ExpectIsJanky(damaging_frame * 10 + 2,
                                         "MISSED_VSYNC_DURING_FAST_SCROLL(1)");
          break;
        case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        case TestVariant::kNewBehaviorCountAllFrames:
          // The new behavior identifies one non-damaging frame (starting with
          // `metrics[0]`) and one damaging frame (starting with `metrics[2]`).
          // It doesn't observe any missed VSyncs, so it doesn't mark any frame
          // as janky.
          expected_results.ExpectIsNotJanky(damaging_frame * 10);
          expected_results.ExpectIsNotJanky(damaging_frame * 10 + 2);
      }
    }
    // Frames presented: 10 damaging, 19 total.

    // Frame 11 is janky because:
    // 1. It violates the running consistency rule because the first input
    //    should have been presented 1 VSync earlier (based on Chrome's past
    //    performance).
    // 2. It violates the fast scroll continuity rule because there's more
    //    than 1 VSync between two consecutive presented frames containing
    //    inputs.
    {
      AdvanceByVsyncs(3);
      viz::BeginFrameArgs non_damaging_args = CreateNextBeginFrameArgs();
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      // Three inputs for a non-damaging frame.
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ - 1.5 * kVsyncInterval,
           .delta = 5.0f,
           .caused_frame_update = false,
           .did_scroll = false,
           .trace_id = TraceId(110),
           .dispatch_args = DispatchBeginFrameArgs::From(non_damaging_args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ - kVsyncInterval,
           .delta = 5.0f,
           .caused_frame_update = false,
           .did_scroll = false,
           .trace_id = TraceId(111),
           .dispatch_args = DispatchBeginFrameArgs::From(non_damaging_args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ - kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = false,
           .did_scroll = false,
           .trace_id = TraceId(112),
           .dispatch_args = DispatchBeginFrameArgs::From(non_damaging_args)}));
      // Two inputs for a presented damaging frame.
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(113),
           .dispatch_args = DispatchBeginFrameArgs::From(damaging_args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ + kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(114),
           .dispatch_args = DispatchBeginFrameArgs::From(damaging_args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, damaging_args);
      switch (GetParam().variant) {
        case TestVariant::kLegacyBehavior:
          // The legacy behavior ignores `metrics[0]` (because it's
          // non-damaging) and marks `metrics[3]` as janky due to the fast
          // scroll rule (because the metric sees 3 missed VSync between two
          // consecutive damaging frames). Note that the legacy behavior
          // completely misses that there was also a violation of the running
          // consistency rule (see the `TestVariant::kNewBehavior.*` case
          // below).
          expected_results.ExpectIsJanky(113,
                                         "MISSED_VSYNC_DURING_FAST_SCROLL(3)");
          break;
        case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        case TestVariant::kNewBehaviorCountAllFrames:
          // The new behavior marks the non-damaging frame (starting with
          // `metrics[0]`) as janky because:
          // 1. `metrics[0]` should have been included in a begin frame one
          //    VSync earlier.
          // 2. There were 2 VSyncs missed (with no inputs) before the
          //    non-damaging frame during a fast scroll.
          expected_results.ExpectIsJanky(
              110,
              "MISSED_VSYNC_DUE_TO_DECELERATING_INPUT_FRAME_DELIVERY(1),"
              "MISSED_VSYNC_DURING_FAST_SCROLL(2)");
          expected_results.ExpectIsNotJanky(113);
      }
    }
    // Frames presented: 11 damaging, 21 total.

    for (int damaging_frame = 12; damaging_frame <= 31; damaging_frame++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs non_damaging_args = CreateNextBeginFrameArgs();
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      // Two inputs for a non-damaging frame.
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ - kVsyncInterval,
           .delta = 5.0f,
           .caused_frame_update = false,
           .did_scroll = false,
           .trace_id = TraceId(damaging_frame * 10),
           .dispatch_args = DispatchBeginFrameArgs::From(non_damaging_args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ - kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = false,
           .did_scroll = false,
           .trace_id = TraceId(damaging_frame * 10 + 1),
           .dispatch_args = DispatchBeginFrameArgs::From(non_damaging_args)}));
      // Two inputs for a presented damaging frame.
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(damaging_frame * 10 + 2),
           .dispatch_args = DispatchBeginFrameArgs::From(damaging_args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ + kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(damaging_frame * 10 + 3),
           .dispatch_args = DispatchBeginFrameArgs::From(damaging_args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, damaging_args);
      switch (GetParam().variant) {
        case TestVariant::kLegacyBehavior:
          // The legacy behavior ignores `metrics[0]` (because it's
          // non-damaging) and marks `metrics[2]` as janky due to the fast
          // scroll rule (because the metric sees a missed VSync between two
          // consecutive damaging frames).
          expected_results.ExpectIsJanky(damaging_frame * 10 + 2,
                                         "MISSED_VSYNC_DURING_FAST_SCROLL(1)");
          break;
        case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        case TestVariant::kNewBehaviorCountAllFrames:
          // The new behavior identifies one non-damaging frame (starting with
          // `metrics[0]`) and one damaging frame (starting with `metrics[2]`).
          // It doesn't observe any missed VSyncs, so it doesn't mark any frame
          // as janky.
          expected_results.ExpectIsNotJanky(damaging_frame * 10);
          expected_results.ExpectIsNotJanky(damaging_frame * 10 + 2);
      }
    }
    // Frames presented: 31 damaging, 61 total.

    for (int damaging_frame = 32; damaging_frame <= 34; damaging_frame++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(damaging_frame * 10),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ + kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(damaging_frame * 10 + 1),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(damaging_frame * 10);
    }
    // Frames presented: 34 damaging, 64 total.

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  {
    base::HistogramTester histogram_tester;

    {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(350),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_ + kVsyncInterval / 2,
           .delta = 5.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(351),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(350);
    }
    // Frames presented: 35 damaging, 65 total.

    switch (GetParam().variant) {
      case TestVariant::kLegacyBehavior:
      case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        // Non-damaging frames don't count towards the histogram frame count, so
        // the processor shouldn't emit any histograms yet because it has only
        // seen 35 damaging frames so far.
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
        break;
      case TestVariant::kNewBehaviorCountAllFrames:
        // Non-damaging frames count towards the histogram frame count, so
        // the processor should emit fixed window histograms now because it has
        // seen 65 frames in total.
        histogram_tester.ExpectUniqueSample(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
            1 * 100 / 64 /* Frame 11 */, 1);
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
    }
  }

  // Switch to a fling with one input per frame.
  {
    base::HistogramTester histogram_tester;

    // Interleave non-damaging and damaging frames, but this time the
    // non-damaging frames are presented.
    {
      // Frame 36 is janky. It violates the fling continuity rule because
      // Chrome missed 5 VSyncs at the transition from a fast regular scroll to
      // a fast fling as janky.
      AdvanceByVsyncs(6);
      viz::BeginFrameArgs non_damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List non_damaging_metrics;
      non_damaging_metrics.push_back(
          metrics_creator_.CreateInertialGestureScrollUpdate(
              {.timestamp = next_input_generation_ts_,
               .delta = 2.0f,
               .caused_frame_update = false,
               .did_scroll = false,
               .trace_id = TraceId(360),
               .dispatch_args =
                   DispatchBeginFrameArgs::From(non_damaging_args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          non_damaging_metrics, next_presentation_ts_, non_damaging_args);
      switch (GetParam().variant) {
        case TestVariant::kLegacyBehavior:
          break;
        case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        case TestVariant::kNewBehaviorCountAllFrames:
          // The new behavior marks the non-damaging frame as janky.
          expected_results.ExpectIsJanky(360,
                                         "MISSED_VSYNC_AT_START_OF_FLING(5)");
      }

      AdvanceByVsyncs(1);
      viz::BeginFrameArgs damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List damaging_metrics;
      damaging_metrics.push_back(
          metrics_creator_.CreateInertialGestureScrollUpdate(
              {.timestamp = next_input_generation_ts_,
               .delta = 2.0f,
               .caused_frame_update = true,
               .did_scroll = true,
               .trace_id = TraceId(361),
               .dispatch_args = DispatchBeginFrameArgs::From(damaging_args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          damaging_metrics, next_presentation_ts_, damaging_args);
      switch (GetParam().variant) {
        case TestVariant::kLegacyBehavior:
          // Whereas the legacy behavior marks the subsequent damaging frame as
          // janky (with one more VSync than it should).
          expected_results.ExpectIsJanky(361,
                                         "MISSED_VSYNC_AT_START_OF_FLING(6)");
          break;
        case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        case TestVariant::kNewBehaviorCountAllFrames:
          expected_results.ExpectIsNotJanky(361);
      }
    }
    // Frames presented: 36 damaging, 67 total.

    // Continue interleaving non-damaging and damaging frames.
    for (int damaging_frame = 37; damaging_frame <= 64; damaging_frame++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs non_damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List non_damaging_metrics;
      non_damaging_metrics.push_back(
          metrics_creator_.CreateInertialGestureScrollUpdate(
              {.timestamp = next_input_generation_ts_,
               .delta = 2.0f,
               .caused_frame_update = false,
               .did_scroll = false,
               .trace_id = TraceId(damaging_frame * 10),
               .dispatch_args =
                   DispatchBeginFrameArgs::From(non_damaging_args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          non_damaging_metrics, next_presentation_ts_, non_damaging_args);
      switch (GetParam().variant) {
        case TestVariant::kLegacyBehavior:
          break;
        case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        case TestVariant::kNewBehaviorCountAllFrames:
          expected_results.ExpectIsNotJanky(damaging_frame * 10);
      }

      AdvanceByVsyncs(1);
      viz::BeginFrameArgs damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List damaging_metrics;
      damaging_metrics.push_back(
          metrics_creator_.CreateInertialGestureScrollUpdate(
              {.timestamp = next_input_generation_ts_,
               .delta = 2.0f,
               .caused_frame_update = true,
               .did_scroll = true,
               .trace_id = TraceId(damaging_frame * 10 + 1),
               .dispatch_args = DispatchBeginFrameArgs::From(damaging_args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          damaging_metrics, next_presentation_ts_, damaging_args);
      switch (GetParam().variant) {
        case TestVariant::kLegacyBehavior:
          expected_results.ExpectIsJanky(damaging_frame * 10 + 1,
                                         "MISSED_VSYNC_DURING_FLING(1)");
          break;
        case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        case TestVariant::kNewBehaviorCountAllFrames:
          expected_results.ExpectIsNotJanky(damaging_frame * 10 + 1);
      }
    }
    // Frames presented: 64 damaging, 123 total.

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  {
    base::HistogramTester histogram_tester;

    AdvanceByVsyncs(1);
    viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
    EventMetrics::List last_metrics_in_fixed_window;
    last_metrics_in_fixed_window.push_back(
        metrics_creator_.CreateInertialGestureScrollUpdate(
            {.timestamp = next_input_generation_ts_,
             .delta = 2.0f,
             .caused_frame_update = true,
             .did_scroll = true,
             .trace_id = TraceId(650),
             .dispatch_args = DispatchBeginFrameArgs::From(args)}));
    processor_.ProcessEventsMetricsForPresentedFrame(
        last_metrics_in_fixed_window, next_presentation_ts_, args);
    expected_results.ExpectIsNotJanky(650);
    // Frames presented: 65 damaging, 124 total.

    switch (GetParam().variant) {
      case TestVariant::kLegacyBehavior:
        // The processor has finally seen 65 damaging frames, so it should emit
        // fixed window histograms.
        histogram_tester.ExpectUniqueSample(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
            59 * 100 / 64 /* Frames 2-31 & 36-64 */, 1);
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
        break;
      case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        // The processor has finally seen 65 damaging frames, so it should emit
        // fixed window histograms.
        histogram_tester.ExpectUniqueSample(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
            2 * 100 / 64 /* Frames 11 & 36 */, 1);
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
        break;
      case TestVariant::kNewBehaviorCountAllFrames:
        // The processor has seen 124 frames in total, which is not at the
        // window boundary, so it shouldn't emit any histograms.
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
    }
  }
  // Frames presented: 65 damaging, 124 total.

  {
    base::HistogramTester histogram_tester;

    // Frame 66 is janky because It violates the fling continuity rule because
    // Chrome missed 9 VSyncs in the middle of a fast fling.
    {
      AdvanceByVsyncs(10);
      viz::BeginFrameArgs non_damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List non_damaging_metrics;
      non_damaging_metrics.push_back(
          metrics_creator_.CreateInertialGestureScrollUpdate(
              {.timestamp = next_input_generation_ts_,
               .delta = 2.0f,
               .caused_frame_update = false,
               .did_scroll = false,
               .trace_id = TraceId(660),
               .dispatch_args =
                   DispatchBeginFrameArgs::From(non_damaging_args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          non_damaging_metrics, next_presentation_ts_, non_damaging_args);
      switch (GetParam().variant) {
        case TestVariant::kLegacyBehavior:
          break;
        case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        case TestVariant::kNewBehaviorCountAllFrames:
          // The new behavior marks the non-damaging frame as janky.
          expected_results.ExpectIsJanky(660, "MISSED_VSYNC_DURING_FLING(9)");
      }

      AdvanceByVsyncs(1);
      viz::BeginFrameArgs damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List damaging_metrics;
      damaging_metrics.push_back(
          metrics_creator_.CreateInertialGestureScrollUpdate(
              {.timestamp = next_input_generation_ts_,
               .delta = 2.0f,
               .caused_frame_update = true,
               .did_scroll = true,
               .trace_id = TraceId(661),
               .dispatch_args = DispatchBeginFrameArgs::From(damaging_args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          damaging_metrics, next_presentation_ts_, damaging_args);
      switch (GetParam().variant) {
        case TestVariant::kLegacyBehavior:
          // Whereas the legacy behavior marks the subsequent damaging frame as
          // janky (with one more VSync than it should).
          expected_results.ExpectIsJanky(661, "MISSED_VSYNC_DURING_FLING(10)");
          break;
        case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        case TestVariant::kNewBehaviorCountAllFrames:
          expected_results.ExpectIsNotJanky(661);
      }
    }
    // Frames presented: 66 damaging, 126 total.

    // Present 2 more damaging frames.
    for (int damaging_frame = 67; damaging_frame <= 68; damaging_frame++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
          {.timestamp = next_input_generation_ts_,
           .delta = 2.0f,
           .caused_frame_update = true,
           .did_scroll = true,
           .trace_id = TraceId(damaging_frame * 10),
           .dispatch_args = DispatchBeginFrameArgs::From(args)}));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(damaging_frame * 10);
    }
    // Frames presented: 68 damaging, 128 total.

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // Finally, end the scroll.
  {
    base::HistogramTester histogram_tester;

    AdvanceByVsyncs(1);
    viz::BeginFrameArgs end_args = CreateNextBeginFrameArgs();
    EventMetrics::List end_metrics;
    end_metrics.push_back(metrics_creator_.CreateInertialGestureScrollEnd(
        {.timestamp = next_input_generation_ts_,
         .caused_frame_update = false,
         .dispatch_args = DispatchBeginFrameArgs::From(end_args)}));
    processor_.ProcessEventsMetricsForPresentedFrame(
        end_metrics, next_presentation_ts_, end_args);

    switch (GetParam().variant) {
      case TestVariant::kLegacyBehavior:
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
        histogram_tester.ExpectUniqueSample(
            "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
            60 * 100 / 68 /* Frames 2-31, 36-64 & 66 */, 1);
        break;
      case TestVariant::kNewBehaviorCountDamagingFramesOnly:
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
        histogram_tester.ExpectUniqueSample(
            "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
            3 * 100 / 68 /* Frames 11, 36 & 66 */, 1);
        break;
      case TestVariant::kNewBehaviorCountAllFrames:
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
        histogram_tester.ExpectUniqueSample(
            "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
            3 * 100 / 128 /* Frames 11, 36 & 66 */, 1);
    }
  }

  absl::Status status = trace_processor_.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_THAT(QueryTraceProcessor(kTraceQuery),
              ElementsAreArray(std::move(expected_results).Take()));
}

INSTANTIATE_TEST_SUITE_P(
    ScrollJankV4ProcessorTest,
    ScrollJankV4ProcessorTest,
    testing::ValuesIn<ScrollJankV4ProcessorTestCase>({
        {
            .variant = TestVariant::kLegacyBehavior,
            .test_name = "LegacyBehavior",
        },
        {
            .variant = TestVariant::kNewBehaviorCountDamagingFramesOnly,
            .test_name = "NewBehaviorCountDamagingFramesOnly",
        },
        {
            .variant = TestVariant::kNewBehaviorCountAllFrames,
            .test_name = "NewBehaviorCountAllFrames",
        },
    }),
    [](const testing::TestParamInfo<ScrollJankV4ProcessorTest::ParamType>&
           info) { return info.param.test_name; });

}  // namespace cc
