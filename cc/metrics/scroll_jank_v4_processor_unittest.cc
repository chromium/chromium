// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_processor.h"

#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_trace_processor.h"
#include "base/time/time.h"
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

constexpr const char kTraceQuery[] =
    R"(
    INCLUDE PERFETTO MODULE chrome.scroll_jank_v4;

    SELECT
      first_event_latency_id,
      is_janky,
      (
        -- Concatenate `chrome_scroll_jank_v4_reasons` into a single string. For
        -- example, the following values:
        --
        --   | jank_reason | missed_vsyncs |
        --   |-------------|---------------|
        --   | 'REASON_A'  | 1             |
        --   | 'REASON_B'  | 2             |
        --   | 'REASON_C'  | 3             |
        --
        -- are converted to 'REASON_A(1),REASON_B(2),REASON_C(3)'.
        SELECT
          GROUP_CONCAT(
            FORMAT('%s(%d)', jank_reason, missed_vsyncs),
            ','
            ORDER BY jank_reason ASC
          )
        FROM chrome_scroll_jank_v4_reasons AS reasons
        WHERE reasons.id = results.id
      ) AS jank_reasons
    FROM chrome_scroll_jank_v4_results AS results
    ORDER BY first_event_latency_id ASC;
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
      {"first_event_latency_id", "is_janky", "jank_reasons"}};
};

}  // namespace

// Unit test of `ScrollJankV4Processor`. Each of the test cases represents a
// possible scenario of input→frame delivery.
class ScrollJankV4ProcessorTest : public testing::Test {
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
TEST_F(ScrollJankV4ProcessorTest, ConsistentDamagingFrameProduction) {
  trace_processor_.StartTrace("input");
  ExpectedTraceResults expected_results;

  // Start a scroll and present frames 1-63.
  {
    base::HistogramTester histogram_tester;

    // Start with a regular scroll with two inputs per frame.
    {
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List first_metrics;
      first_metrics.push_back(
          metrics_creator_.FirstGestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(10))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
              .Build());
      first_metrics.push_back(
          metrics_creator_.FirstGestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ + kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(11))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          first_metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(10);
    }

    for (int i = 2; i <= 50; i++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                            .SetTimestamp(next_input_generation_ts_)
                            .SetDelta(5.0f)
                            .SetCausedFrameUpdate(true)
                            .SetDidScroll(true)
                            .SetTraceId(TraceId(i * 10))
                            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                            .Build());
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ + kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(i * 10 + 1))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(i * 10);
    }

    // Switch to a fling with one input per frame.
    for (int i = 51; i <= 63; i++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                            .SetTimestamp(next_input_generation_ts_)
                            .SetDelta(2.0f)
                            .SetCausedFrameUpdate(true)
                            .SetDidScroll(true)
                            .SetTraceId(TraceId(i * 10))
                            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                            .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(i * 10);
    }

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // Present frame 64 (end of first fixed window).
  {
    base::HistogramTester histogram_tester;

    AdvanceByVsyncs(1);
    viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
    EventMetrics::List last_metrics_in_fixed_window;
    last_metrics_in_fixed_window.push_back(
        metrics_creator_.InertialGestureScrollUpdateBuilder()
            .SetTimestamp(next_input_generation_ts_)
            .SetDelta(2.0f)
            .SetCausedFrameUpdate(true)
            .SetDidScroll(true)
            .SetTraceId(TraceId(640))
            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
            .Build());
    processor_.ProcessEventsMetricsForPresentedFrame(
        last_metrics_in_fixed_window, next_presentation_ts_, args);
    expected_results.ExpectIsNotJanky(640);

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // Present 35 more frames.
  {
    base::HistogramTester histogram_tester;

    for (int i = 65; i <= 99; i++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                            .SetTimestamp(next_input_generation_ts_)
                            .SetDelta(2.0f)
                            .SetCausedFrameUpdate(true)
                            .SetDidScroll(true)
                            .SetTraceId(TraceId(i * 10))
                            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                            .Build());
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
    end_metrics.push_back(
        metrics_creator_.InertialGestureScrollEndBuilder()
            .SetTimestamp(next_input_generation_ts_)
            .SetCausedFrameUpdate(false)
            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
            .Build());
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
TEST_F(ScrollJankV4ProcessorTest, InconsistentDamagingFrameProduction) {
  trace_processor_.StartTrace("input");
  ExpectedTraceResults expected_results;

  // Start a scroll and present frames 1-63.
  {
    base::HistogramTester histogram_tester;

    // Start with a regular scroll with two inputs per frame.
    {
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List first_metrics;
      first_metrics.push_back(
          metrics_creator_.FirstGestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(10))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
              .Build());
      first_metrics.push_back(
          metrics_creator_.FirstGestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ + kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(11))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          first_metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(10);
    }

    for (int i = 2; i <= 10; i++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                            .SetTimestamp(next_input_generation_ts_)
                            .SetDelta(5.0f)
                            .SetCausedFrameUpdate(true)
                            .SetDidScroll(true)
                            .SetTraceId(TraceId(i * 10))
                            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                            .Build());
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ + kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(i * 10 + 1))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
              .Build());
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
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ - kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(110))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
              .Build());
      metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                            .SetTimestamp(next_input_generation_ts_)
                            .SetDelta(5.0f)
                            .SetCausedFrameUpdate(true)
                            .SetDidScroll(true)
                            .SetTraceId(TraceId(111))
                            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                            .Build());
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ + kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(112))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
              .Build());
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
      metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                            .SetTimestamp(next_input_generation_ts_)
                            .SetDelta(5.0f)
                            .SetCausedFrameUpdate(true)
                            .SetDidScroll(true)
                            .SetTraceId(TraceId(i * 10))
                            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                            .Build());
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ + kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(i * 10 + 1))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
              .Build());
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
      metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                            .SetTimestamp(next_input_generation_ts_)
                            .SetDelta(2.0f)
                            .SetCausedFrameUpdate(true)
                            .SetDidScroll(true)
                            .SetTraceId(TraceId(510))
                            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                            .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsJanky(510, "MISSED_VSYNC_AT_START_OF_FLING(5)");
    }

    for (int i = 52; i <= 63; i++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                            .SetTimestamp(next_input_generation_ts_)
                            .SetDelta(2.0f)
                            .SetCausedFrameUpdate(true)
                            .SetDidScroll(true)
                            .SetTraceId(TraceId(i * 10))
                            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                            .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(i * 10);
    }

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // Present frame 64 (end of first fixed window).
  {
    base::HistogramTester histogram_tester;

    AdvanceByVsyncs(1);
    viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
    EventMetrics::List last_metrics_in_fixed_window;
    last_metrics_in_fixed_window.push_back(
        metrics_creator_.InertialGestureScrollUpdateBuilder()
            .SetTimestamp(next_input_generation_ts_)
            .SetDelta(2.0f)
            .SetCausedFrameUpdate(true)
            .SetDidScroll(true)
            .SetTraceId(TraceId(640))
            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
            .Build());
    processor_.ProcessEventsMetricsForPresentedFrame(
        last_metrics_in_fixed_window, next_presentation_ts_, args);
    expected_results.ExpectIsNotJanky(640);

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 2 * 100 / 64,
        1);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // Present 35 more frames.
  {
    base::HistogramTester histogram_tester;

    for (int i = 65; i <= 79; i++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                            .SetTimestamp(next_input_generation_ts_)
                            .SetDelta(2.0f)
                            .SetCausedFrameUpdate(true)
                            .SetDidScroll(true)
                            .SetTraceId(TraceId(i * 10))
                            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                            .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(i * 10);
    }

    // The processor should mark frame 80 as janky. It violates the fling
    // continuity rule because Chrome missed 9 VSyncs in the middle of a fast
    // fling.
    {
      AdvanceByVsyncs(10);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                            .SetTimestamp(next_input_generation_ts_)
                            .SetDelta(2.0f)
                            .SetCausedFrameUpdate(true)
                            .SetDidScroll(true)
                            .SetTraceId(TraceId(800))
                            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                            .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsJanky(800, "MISSED_VSYNC_DURING_FLING(9)");
    }

    for (int i = 81; i <= 99; i++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                            .SetTimestamp(next_input_generation_ts_)
                            .SetDelta(2.0f)
                            .SetCausedFrameUpdate(true)
                            .SetDidScroll(true)
                            .SetTraceId(TraceId(10 * i))
                            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                            .Build());
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
    end_metrics.push_back(
        metrics_creator_.InertialGestureScrollEndBuilder()
            .SetTimestamp(next_input_generation_ts_)
            .SetCausedFrameUpdate(false)
            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
            .Build());
    processor_.ProcessEventsMetricsForPresentedFrame(
        end_metrics, next_presentation_ts_, args);

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 3 * 100 / 99, 1);
  }

  absl::Status status = trace_processor_.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_THAT(QueryTraceProcessor(kTraceQuery),
              ElementsAreArray(std::move(expected_results).Take()));
}

/*
Tests the behavior of the scroll jank v4 metric on consistent input delivery
with both damaging and non-damaging frames.
*/
TEST_F(ScrollJankV4ProcessorTest, ConsistentMixedFrameProduction) {
  trace_processor_.StartTrace("input");
  ExpectedTraceResults expected_results;

  // Start with a regular scroll with two inputs per frame.
  {
    base::HistogramTester histogram_tester;

    {
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List first_metrics;
      first_metrics.push_back(
          metrics_creator_.FirstGestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(10))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
              .Build());
      first_metrics.push_back(
          metrics_creator_.FirstGestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ + kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(11))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
              .Build());
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
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ - kVsyncInterval)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(false)
              .SetDidScroll(false)
              .SetTraceId(TraceId(damaging_frame * 10))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(non_damaging_args))
              .Build());
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ - kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(false)
              .SetDidScroll(false)
              .SetTraceId(TraceId(damaging_frame * 10 + 1))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(non_damaging_args))
              .Build());
      // Two inputs for a presented damaging frame.
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(damaging_frame * 10 + 2))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(damaging_args))
              .Build());
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ + kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(damaging_frame * 10 + 3))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(damaging_args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, damaging_args);
      // There's one non-damaging frame (starting with `metrics[0]`) and one
      // damaging frame (starting with `metrics[2]`). There are no
      // missed VSyncs, so no frames should be marked as janky.
      expected_results.ExpectIsNotJanky(damaging_frame * 10);
      expected_results.ExpectIsNotJanky(damaging_frame * 10 + 2);
    }
    // Frames presented: 31 damaging, 61 total.

    for (int damaging_frame = 32; damaging_frame <= 33; damaging_frame++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                            .SetTimestamp(next_input_generation_ts_)
                            .SetDelta(5.0f)
                            .SetCausedFrameUpdate(true)
                            .SetDidScroll(true)
                            .SetTraceId(TraceId(damaging_frame * 10))
                            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                            .Build());
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ + kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(damaging_frame * 10 + 1))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(damaging_frame * 10);
    }
    // Frames presented: 33 damaging, 63 total.

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
      metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                            .SetTimestamp(next_input_generation_ts_)
                            .SetDelta(5.0f)
                            .SetCausedFrameUpdate(true)
                            .SetDidScroll(true)
                            .SetTraceId(TraceId(340))
                            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                            .Build());
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ + kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(341))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(340);
    }
    // Frames presented: 34 damaging, 64 total.

        // Non-damaging frames count towards the histogram frame count, so
        // the processor should emit fixed window histograms now because it has
        // seen 64 frames in total.
        histogram_tester.ExpectUniqueSample(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
        histogram_tester.ExpectTotalCount(
            "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // Switch to a fling with one input per frame.
  {
    base::HistogramTester histogram_tester;

    // Interleave non-damaging and damaging frames, but this time the
    // non-damaging frames are presented.
    for (int damaging_frame = 35; damaging_frame <= 63; damaging_frame++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs non_damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List non_damaging_metrics;
      non_damaging_metrics.push_back(
          metrics_creator_.InertialGestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_)
              .SetDelta(2.0f)
              .SetCausedFrameUpdate(false)
              .SetDidScroll(false)
              .SetTraceId(TraceId(damaging_frame * 10))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(non_damaging_args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          non_damaging_metrics, next_presentation_ts_, non_damaging_args);
      expected_results.ExpectIsNotJanky(damaging_frame * 10);

      AdvanceByVsyncs(1);
      viz::BeginFrameArgs damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List damaging_metrics;
      damaging_metrics.push_back(
          metrics_creator_.InertialGestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_)
              .SetDelta(2.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(damaging_frame * 10 + 1))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(damaging_args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          damaging_metrics, next_presentation_ts_, damaging_args);
      expected_results.ExpectIsNotJanky(damaging_frame * 10 + 1);
    }
    // Frames presented: 63 damaging, 122 total.

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
    metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                          .SetTimestamp(next_input_generation_ts_)
                          .SetDelta(2.0f)
                          .SetCausedFrameUpdate(true)
                          .SetDidScroll(true)
                          .SetTraceId(TraceId(640))
                          .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                          .Build());
    processor_.ProcessEventsMetricsForPresentedFrame(
        metrics, next_presentation_ts_, args);
    expected_results.ExpectIsNotJanky(640);
    // Frames presented: 64 damaging, 123 total.

    // The processor has seen 123 frames in total, which is not at the
    // window boundary, so it shouldn't emit any histograms.
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // Present 4 more damaging frames.
  {
    base::HistogramTester histogram_tester;

    for (int damaging_frame = 65; damaging_frame <= 68; damaging_frame++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                            .SetTimestamp(next_input_generation_ts_)
                            .SetDelta(2.0f)
                            .SetCausedFrameUpdate(true)
                            .SetDidScroll(true)
                            .SetTraceId(TraceId(damaging_frame * 10))
                            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                            .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(damaging_frame * 10);
    }
    // Frames presented: 68 damaging, 127 total.

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
    end_metrics.push_back(
        metrics_creator_.InertialGestureScrollEndBuilder()
            .SetTimestamp(next_input_generation_ts_)
            .SetCausedFrameUpdate(false)
            .SetDispatchArgs(DispatchBeginFrameArgs::From(end_args))
            .Build());
    processor_.ProcessEventsMetricsForPresentedFrame(
        end_metrics, next_presentation_ts_, end_args);

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
Tests the behavior of the scroll jank v4 metric on inconsistent input delivery
with both damaging and non-damaging frames.
*/
TEST_F(ScrollJankV4ProcessorTest, InconsistentMixedFrameProduction) {
  trace_processor_.StartTrace("input");
  ExpectedTraceResults expected_results;

  // Start with a regular scroll with two inputs per frame.
  {
    base::HistogramTester histogram_tester;

    {
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List first_metrics;
      first_metrics.push_back(
          metrics_creator_.FirstGestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(10))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
              .Build());
      first_metrics.push_back(
          metrics_creator_.FirstGestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ + kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(11))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
              .Build());
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
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ - kVsyncInterval)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(false)
              .SetDidScroll(false)
              .SetTraceId(TraceId(damaging_frame * 10))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(non_damaging_args))
              .Build());
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ - kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(false)
              .SetDidScroll(false)
              .SetTraceId(TraceId(damaging_frame * 10 + 1))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(non_damaging_args))
              .Build());
      // Two inputs for a presented damaging frame.
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(damaging_frame * 10 + 2))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(damaging_args))
              .Build());
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ + kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(damaging_frame * 10 + 3))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(damaging_args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, damaging_args);
      // There's one non-damaging frame (starting with `metrics[0]`) and one
      // damaging frame (starting with `metrics[2]`). There are no
      // missed VSyncs, so no frames should be marked as janky.
      expected_results.ExpectIsNotJanky(damaging_frame * 10);
      expected_results.ExpectIsNotJanky(damaging_frame * 10 + 2);
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
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ - 1.5 * kVsyncInterval)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(false)
              .SetDidScroll(false)
              .SetTraceId(TraceId(110))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(non_damaging_args))
              .Build());
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ - kVsyncInterval)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(false)
              .SetDidScroll(false)
              .SetTraceId(TraceId(111))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(non_damaging_args))
              .Build());
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ - kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(false)
              .SetDidScroll(false)
              .SetTraceId(TraceId(112))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(non_damaging_args))
              .Build());
      // Two inputs for a presented damaging frame.
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(113))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(damaging_args))
              .Build());
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ + kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(114))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(damaging_args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, damaging_args);
      // The non-damaging frame (starting with `metrics[0]`) should be marked as
      // janky because:
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
    // Frames presented: 11 damaging, 21 total.

    for (int damaging_frame = 12; damaging_frame <= 31; damaging_frame++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs non_damaging_args = CreateNextBeginFrameArgs();
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      // Two inputs for a non-damaging frame.
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ - kVsyncInterval)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(false)
              .SetDidScroll(false)
              .SetTraceId(TraceId(damaging_frame * 10))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(non_damaging_args))
              .Build());
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ - kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(false)
              .SetDidScroll(false)
              .SetTraceId(TraceId(damaging_frame * 10 + 1))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(non_damaging_args))
              .Build());
      // Two inputs for a presented damaging frame.
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(damaging_frame * 10 + 2))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(damaging_args))
              .Build());
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ + kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(damaging_frame * 10 + 3))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(damaging_args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, damaging_args);
      // There's one non-damaging frame (starting with `metrics[0]`) and one
      // damaging frame (starting with `metrics[2]`). There are no
      // missed VSyncs, so no frames should be marked as janky.
      expected_results.ExpectIsNotJanky(damaging_frame * 10);
      expected_results.ExpectIsNotJanky(damaging_frame * 10 + 2);
    }
    // Frames presented: 31 damaging, 61 total.

    for (int damaging_frame = 32; damaging_frame <= 33; damaging_frame++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                            .SetTimestamp(next_input_generation_ts_)
                            .SetDelta(5.0f)
                            .SetCausedFrameUpdate(true)
                            .SetDidScroll(true)
                            .SetTraceId(TraceId(damaging_frame * 10))
                            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                            .Build());
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ + kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(damaging_frame * 10 + 1))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(damaging_frame * 10);
    }
    // Frames presented: 33 damaging, 63 total.

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
      metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                            .SetTimestamp(next_input_generation_ts_)
                            .SetDelta(5.0f)
                            .SetCausedFrameUpdate(true)
                            .SetDidScroll(true)
                            .SetTraceId(TraceId(340))
                            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                            .Build());
      metrics.push_back(
          metrics_creator_.GestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_ + kVsyncInterval / 2)
              .SetDelta(5.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(341))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(340);
    }
    // Frames presented: 34 damaging, 64 total.

    // Non-damaging frames count towards the histogram frame count, so
    // the processor should emit fixed window histograms now because it has
    // seen 64 frames in total.
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 1, 1);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // Switch to a fling with one input per frame.
  {
    base::HistogramTester histogram_tester;

    // Interleave non-damaging and damaging frames, but this time the
    // non-damaging frames are presented.
    {
      // Frame 35 is janky. It violates the fling continuity rule because
      // Chrome missed 5 VSyncs at the transition from a fast regular scroll to
      // a fast fling as janky.
      AdvanceByVsyncs(6);
      viz::BeginFrameArgs non_damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List non_damaging_metrics;
      non_damaging_metrics.push_back(
          metrics_creator_.InertialGestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_)
              .SetDelta(2.0f)
              .SetCausedFrameUpdate(false)
              .SetDidScroll(false)
              .SetTraceId(TraceId(350))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(non_damaging_args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          non_damaging_metrics, next_presentation_ts_, non_damaging_args);
      expected_results.ExpectIsJanky(350, "MISSED_VSYNC_AT_START_OF_FLING(5)");

      AdvanceByVsyncs(1);
      viz::BeginFrameArgs damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List damaging_metrics;
      damaging_metrics.push_back(
          metrics_creator_.InertialGestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_)
              .SetDelta(2.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(351))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(damaging_args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          damaging_metrics, next_presentation_ts_, damaging_args);
      expected_results.ExpectIsNotJanky(351);
    }
    // Frames presented: 35 damaging, 66 total.

    // Continue interleaving non-damaging and damaging frames.
    for (int damaging_frame = 36; damaging_frame <= 63; damaging_frame++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs non_damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List non_damaging_metrics;
      non_damaging_metrics.push_back(
          metrics_creator_.InertialGestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_)
              .SetDelta(2.0f)
              .SetCausedFrameUpdate(false)
              .SetDidScroll(false)
              .SetTraceId(TraceId(damaging_frame * 10))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(non_damaging_args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          non_damaging_metrics, next_presentation_ts_, non_damaging_args);
      expected_results.ExpectIsNotJanky(damaging_frame * 10);

      AdvanceByVsyncs(1);
      viz::BeginFrameArgs damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List damaging_metrics;
      damaging_metrics.push_back(
          metrics_creator_.InertialGestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_)
              .SetDelta(2.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(damaging_frame * 10 + 1))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(damaging_args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          damaging_metrics, next_presentation_ts_, damaging_args);
      expected_results.ExpectIsNotJanky(damaging_frame * 10 + 1);
    }
    // Frames presented: 63 damaging, 122 total.

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
        metrics_creator_.InertialGestureScrollUpdateBuilder()
            .SetTimestamp(next_input_generation_ts_)
            .SetDelta(2.0f)
            .SetCausedFrameUpdate(true)
            .SetDidScroll(true)
            .SetTraceId(TraceId(640))
            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
            .Build());
    processor_.ProcessEventsMetricsForPresentedFrame(
        last_metrics_in_fixed_window, next_presentation_ts_, args);
    expected_results.ExpectIsNotJanky(640);
    // Frames presented: 64 damaging, 123 total.

    // The processor has seen 123 frames in total, which is not at the
    // window boundary, so it shouldn't emit any histograms.
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }
  // Frames presented: 64 damaging, 123 total.

  {
    base::HistogramTester histogram_tester;

    // Frame 65 is janky because It violates the fling continuity rule because
    // Chrome missed 9 VSyncs in the middle of a fast fling.
    {
      AdvanceByVsyncs(10);
      viz::BeginFrameArgs non_damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List non_damaging_metrics;
      non_damaging_metrics.push_back(
          metrics_creator_.InertialGestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_)
              .SetDelta(2.0f)
              .SetCausedFrameUpdate(false)
              .SetDidScroll(false)
              .SetTraceId(TraceId(650))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(non_damaging_args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          non_damaging_metrics, next_presentation_ts_, non_damaging_args);
      expected_results.ExpectIsJanky(650, "MISSED_VSYNC_DURING_FLING(9)");

      AdvanceByVsyncs(1);
      viz::BeginFrameArgs damaging_args = CreateNextBeginFrameArgs();
      EventMetrics::List damaging_metrics;
      damaging_metrics.push_back(
          metrics_creator_.InertialGestureScrollUpdateBuilder()
              .SetTimestamp(next_input_generation_ts_)
              .SetDelta(2.0f)
              .SetCausedFrameUpdate(true)
              .SetDidScroll(true)
              .SetTraceId(TraceId(651))
              .SetDispatchArgs(DispatchBeginFrameArgs::From(damaging_args))
              .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          damaging_metrics, next_presentation_ts_, damaging_args);
      expected_results.ExpectIsNotJanky(651);
    }
    // Frames presented: 65 damaging, 125 total.

    // Present 2 more damaging frames.
    for (int damaging_frame = 66; damaging_frame <= 67; damaging_frame++) {
      AdvanceByVsyncs(1);
      viz::BeginFrameArgs args = CreateNextBeginFrameArgs();
      EventMetrics::List metrics;
      metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                            .SetTimestamp(next_input_generation_ts_)
                            .SetDelta(2.0f)
                            .SetCausedFrameUpdate(true)
                            .SetDidScroll(true)
                            .SetTraceId(TraceId(damaging_frame * 10))
                            .SetDispatchArgs(DispatchBeginFrameArgs::From(args))
                            .Build());
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, args);
      expected_results.ExpectIsNotJanky(damaging_frame * 10);
    }
    // Frames presented: 67 damaging, 127 total.

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
    end_metrics.push_back(
        metrics_creator_.InertialGestureScrollEndBuilder()
            .SetTimestamp(next_input_generation_ts_)
            .SetCausedFrameUpdate(false)
            .SetDispatchArgs(DispatchBeginFrameArgs::From(end_args))
            .Build());
    processor_.ProcessEventsMetricsForPresentedFrame(
        end_metrics, next_presentation_ts_, end_args);

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        3 * 100 / 127 /* Frames 11, 35 & 65 */, 1);
  }

  absl::Status status = trace_processor_.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_THAT(QueryTraceProcessor(kTraceQuery),
              ElementsAreArray(std::move(expected_results).Take()));
}

}  // namespace cc
