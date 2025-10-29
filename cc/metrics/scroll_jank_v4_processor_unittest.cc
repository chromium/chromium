// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_processor.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "cc/metrics/event_metrics.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

constexpr base::TimeDelta kVsyncInterval = base::Milliseconds(16);

constexpr base::TimeTicks MillisSinceEpoch(int64_t millis) {
  return base::TimeTicks() + base::Milliseconds(millis);
}

constexpr JankReasonArray<int> MakeMissedVsyncCounts(
    std::initializer_list<std::pair<JankReason, int>> values) {
  JankReasonArray<int> result = {};  // Default initialize to 0
  for (const auto& [reason, missed_vsyncs] : values) {
    result[static_cast<int>(reason)] += missed_vsyncs;
  }
  return result;
}

constexpr JankReasonArray<int> kNonJankyFrame = {};

}  // namespace

class ScrollJankV4ProcessorTest : public testing::Test {
 public:
  ScrollJankV4ProcessorTest() = default;

 protected:
  std::unique_ptr<ScrollUpdateEventMetrics> CreateScrollUpdateEventMetrics(
      base::TimeTicks timestamp,
      ui::EventType type,
      bool is_inertial,
      ScrollUpdateEventMetrics::ScrollUpdateType scroll_update_type,
      float delta) {
    auto event = ScrollUpdateEventMetrics::CreateForTesting(
        type, ui::ScrollInputType::kTouchscreen, is_inertial,
        scroll_update_type, delta, timestamp,
        /* arrived_in_browser_main_timestamp= */ timestamp +
            base::Nanoseconds(1),
        &test_tick_clock_,
        /* trace_id= */ std::nullopt);
    return event;
  }

  std::unique_ptr<ScrollUpdateEventMetrics> CreateFirstGestureScrollUpdate(
      base::TimeTicks timestamp,
      float delta,
      bool did_scroll) {
    auto event = CreateScrollUpdateEventMetrics(
        timestamp, ui::EventType::kGestureScrollUpdate,
        /* is_inertial= */ false,
        ScrollUpdateEventMetrics::ScrollUpdateType::kStarted, delta);
    EXPECT_EQ(event->type(),
              EventMetrics::EventType::kFirstGestureScrollUpdate);
    event->set_did_scroll(did_scroll);
    return event;
  }

  std::unique_ptr<ScrollUpdateEventMetrics> CreateGestureScrollUpdate(
      base::TimeTicks timestamp,
      float delta,
      bool did_scroll) {
    auto event = CreateScrollUpdateEventMetrics(
        timestamp, ui::EventType::kGestureScrollUpdate,
        /* is_inertial= */ false,
        ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, delta);
    EXPECT_EQ(event->type(), EventMetrics::EventType::kGestureScrollUpdate);
    event->set_did_scroll(did_scroll);
    return event;
  }

  std::unique_ptr<ScrollUpdateEventMetrics> CreateInertialGestureScrollUpdate(
      base::TimeTicks timestamp,
      float delta,
      bool did_scroll) {
    auto event = CreateScrollUpdateEventMetrics(
        timestamp, ui::EventType::kGestureScrollUpdate, /* is_inertial= */ true,
        ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, delta);
    EXPECT_EQ(event->type(),
              EventMetrics::EventType::kInertialGestureScrollUpdate);
    event->set_did_scroll(did_scroll);
    return event;
  }

  std::unique_ptr<ScrollEventMetrics> CreateInertialGestureScrollEnd(
      base::TimeTicks timestamp) {
    auto event = ScrollEventMetrics::CreateForTesting(
        ui::EventType::kGestureScrollEnd, ui::ScrollInputType::kTouchscreen,
        /* is_inertial= */ true, timestamp,
        /* arrived_in_browser_main_timestamp= */ timestamp +
            base::Nanoseconds(1),
        &test_tick_clock_);
    EXPECT_EQ(event->type(),
              EventMetrics::EventType::kInertialGestureScrollEnd);
    event->set_caused_frame_update(false);
    return event;
  }

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
  ScrollJankV4Processor processor_;
  base::SimpleTestTickClock test_tick_clock_;
};

/*
Test that the scroll jank v4 metric doesn't mark frame production with
consistent input delivery janky.
*/
TEST_F(ScrollJankV4ProcessorTest, ConsistentFrameProduction) {
  // Start a scroll and present frames 1-64.
  {
    base::HistogramTester histogram_tester;

    // Start with a regular scroll with two inputs per frame.
    {
      EventMetrics::List first_metrics;
      first_metrics.push_back(CreateFirstGestureScrollUpdate(
          next_input_generation_ts_, /* delta= */ 5.0f,
          /* did_scroll= */ true));
      first_metrics.push_back(CreateFirstGestureScrollUpdate(
          next_input_generation_ts_ + kVsyncInterval / 2, /* delta= */ 5.0f,
          /* did_scroll= */ true));
      processor_.ProcessEventsMetricsForPresentedFrame(
          first_metrics, next_presentation_ts_, CreateNextBeginFrameArgs());
      EXPECT_EQ(first_metrics[0]
                    ->AsScrollUpdate()
                    ->scroll_jank_v4()
                    ->missed_vsyncs_per_reason,
                kNonJankyFrame);
    }

    for (int i = 2; i <= 50; i++) {
      AdvanceByVsyncs(1);
      EventMetrics::List metrics;
      metrics.push_back(CreateGestureScrollUpdate(next_input_generation_ts_,
                                                  /* delta= */ 5.0f,
                                                  /* did_scroll= */ true));
      metrics.push_back(CreateGestureScrollUpdate(
          next_input_generation_ts_ + kVsyncInterval / 2,
          /* delta= */ 5.0f, /* did_scroll= */ true));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, CreateNextBeginFrameArgs());
      EXPECT_EQ(metrics[0]
                    ->AsScrollUpdate()
                    ->scroll_jank_v4()
                    ->missed_vsyncs_per_reason,
                kNonJankyFrame);
      EXPECT_EQ(metrics[1]->AsScrollUpdate()->scroll_jank_v4(), std::nullopt);
    }

    // Switch to a fling with one input per frame.
    for (int i = 51; i <= 64; i++) {
      AdvanceByVsyncs(1);
      EventMetrics::List metrics;
      metrics.push_back(
          CreateInertialGestureScrollUpdate(next_input_generation_ts_,
                                            /* delta= */ 2.0f,
                                            /* did_scroll= */ true));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, CreateNextBeginFrameArgs());
      EXPECT_EQ(metrics[0]
                    ->AsScrollUpdate()
                    ->scroll_jank_v4()
                    ->missed_vsyncs_per_reason,
                kNonJankyFrame);
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
    EventMetrics::List last_metrics_in_fixed_window;
    last_metrics_in_fixed_window.push_back(CreateInertialGestureScrollUpdate(
        next_input_generation_ts_,
        /* delta= */ 2.0f, /* did_scroll= */ true));
    processor_.ProcessEventsMetricsForPresentedFrame(
        last_metrics_in_fixed_window, next_presentation_ts_,
        CreateNextBeginFrameArgs());
    EXPECT_EQ(last_metrics_in_fixed_window[0]
                  ->AsScrollUpdate()
                  ->scroll_jank_v4()
                  ->missed_vsyncs_per_reason,
              kNonJankyFrame);

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
      EventMetrics::List metrics;
      metrics.push_back(
          CreateInertialGestureScrollUpdate(next_input_generation_ts_,
                                            /* delta= */ 2.0f,
                                            /* did_scroll= */ true));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, CreateNextBeginFrameArgs());
      EXPECT_EQ(metrics[0]
                    ->AsScrollUpdate()
                    ->scroll_jank_v4()
                    ->missed_vsyncs_per_reason,
                kNonJankyFrame);
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
    EventMetrics::List end_metrics;
    end_metrics.push_back(
        CreateInertialGestureScrollEnd(next_input_generation_ts_));
    processor_.ProcessEventsMetricsForPresentedFrame(
        end_metrics, next_presentation_ts_, CreateNextBeginFrameArgs());

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0, 1);
  }
}

/*
Test that the scroll jank v4 metric marks "hiccups" in frame production with
inconsistent input delivery janky.
*/
TEST_F(ScrollJankV4ProcessorTest, InconsistentFrameProduction) {
  // Start a scroll and present frames 1-64.
  {
    base::HistogramTester histogram_tester;

    // Start with a regular scroll with two inputs per frame.
    {
      EventMetrics::List first_metrics;
      first_metrics.push_back(CreateFirstGestureScrollUpdate(
          next_input_generation_ts_, /* delta= */ 5.0f,
          /* did_scroll= */ true));
      first_metrics.push_back(CreateFirstGestureScrollUpdate(
          next_input_generation_ts_ + kVsyncInterval / 2, /* delta= */ 5.0f,
          /* did_scroll= */ true));
      processor_.ProcessEventsMetricsForPresentedFrame(
          first_metrics, next_presentation_ts_, CreateNextBeginFrameArgs());
      EXPECT_EQ(first_metrics[0]
                    ->AsScrollUpdate()
                    ->scroll_jank_v4()
                    ->missed_vsyncs_per_reason,
                kNonJankyFrame);
    }

    for (int i = 2; i <= 10; i++) {
      AdvanceByVsyncs(1);
      EventMetrics::List metrics;
      metrics.push_back(CreateGestureScrollUpdate(next_input_generation_ts_,
                                                  /* delta= */ 5.0f,
                                                  /* did_scroll= */ true));
      metrics.push_back(CreateGestureScrollUpdate(
          next_input_generation_ts_ + kVsyncInterval / 2,
          /* delta= */ 5.0f, /* did_scroll= */ true));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, CreateNextBeginFrameArgs());
      EXPECT_EQ(metrics[0]
                    ->AsScrollUpdate()
                    ->scroll_jank_v4()
                    ->missed_vsyncs_per_reason,
                kNonJankyFrame);
      EXPECT_EQ(metrics[1]->AsScrollUpdate()->scroll_jank_v4(), std::nullopt);
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
      EventMetrics::List metrics;
      metrics.push_back(CreateGestureScrollUpdate(
          next_input_generation_ts_ - kVsyncInterval / 2,
          /* delta= */ 5.0f, /* did_scroll= */ true));
      metrics.push_back(CreateGestureScrollUpdate(next_input_generation_ts_,
                                                  /* delta= */ 5.0f,
                                                  /* did_scroll= */ true));
      metrics.push_back(CreateGestureScrollUpdate(
          next_input_generation_ts_ + kVsyncInterval / 2,
          /* delta= */ 5.0f, /* did_scroll= */ true));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, CreateNextBeginFrameArgs());
      EXPECT_EQ(
          metrics[0]
              ->AsScrollUpdate()
              ->scroll_jank_v4()
              ->missed_vsyncs_per_reason,
          MakeMissedVsyncCounts(
              {{JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1},
               {JankReason::kMissedVsyncDuringFastScroll, 2}}));
      EXPECT_EQ(metrics[1]->AsScrollUpdate()->scroll_jank_v4(), std::nullopt);
      EXPECT_EQ(metrics[2]->AsScrollUpdate()->scroll_jank_v4(), std::nullopt);
    }

    for (int i = 12; i <= 50; i++) {
      AdvanceByVsyncs(1);
      EventMetrics::List metrics;
      metrics.push_back(CreateGestureScrollUpdate(next_input_generation_ts_,
                                                  /* delta= */ 5.0f,
                                                  /* did_scroll= */ true));
      metrics.push_back(CreateGestureScrollUpdate(
          next_input_generation_ts_ + kVsyncInterval / 2,
          /* delta= */ 5.0f, /* did_scroll= */ true));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, CreateNextBeginFrameArgs());
      EXPECT_EQ(metrics[0]
                    ->AsScrollUpdate()
                    ->scroll_jank_v4()
                    ->missed_vsyncs_per_reason,
                kNonJankyFrame);
      EXPECT_EQ(metrics[1]->AsScrollUpdate()->scroll_jank_v4(), std::nullopt);
    }

    // Switch to a fling with one input per frame.
    {
      // The processor should mark frame 51 as janky. It violates the fling
      // continuity rule because Chrome missed 5 VSyncs at the transition from a
      // fast regular scroll to a fast fling as janky.
      AdvanceByVsyncs(6);
      EventMetrics::List metrics;
      metrics.push_back(
          CreateInertialGestureScrollUpdate(next_input_generation_ts_,
                                            /* delta= */ 2.0f,
                                            /* did_scroll= */ true));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, CreateNextBeginFrameArgs());
      EXPECT_EQ(
          metrics[0]
              ->AsScrollUpdate()
              ->scroll_jank_v4()
              ->missed_vsyncs_per_reason,
          MakeMissedVsyncCounts({{JankReason::kMissedVsyncAtStartOfFling, 5}}));
    }

    for (int i = 52; i <= 64; i++) {
      AdvanceByVsyncs(1);
      EventMetrics::List metrics;
      metrics.push_back(
          CreateInertialGestureScrollUpdate(next_input_generation_ts_,
                                            /* delta= */ 2.0f,
                                            /* did_scroll= */ true));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, CreateNextBeginFrameArgs());
      EXPECT_EQ(metrics[0]
                    ->AsScrollUpdate()
                    ->scroll_jank_v4()
                    ->missed_vsyncs_per_reason,
                kNonJankyFrame);
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
    EventMetrics::List last_metrics_in_fixed_window;
    last_metrics_in_fixed_window.push_back(CreateInertialGestureScrollUpdate(
        next_input_generation_ts_,
        /* delta= */ 2.0f, /* did_scroll= */ true));
    processor_.ProcessEventsMetricsForPresentedFrame(
        last_metrics_in_fixed_window, next_presentation_ts_,
        CreateNextBeginFrameArgs());
    EXPECT_EQ(last_metrics_in_fixed_window[0]
                  ->AsScrollUpdate()
                  ->scroll_jank_v4()
                  ->missed_vsyncs_per_reason,
              kNonJankyFrame);

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
      EventMetrics::List metrics;
      metrics.push_back(
          CreateInertialGestureScrollUpdate(next_input_generation_ts_,
                                            /* delta= */ 2.0f,
                                            /* did_scroll= */ true));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, CreateNextBeginFrameArgs());
      EXPECT_EQ(metrics[0]
                    ->AsScrollUpdate()
                    ->scroll_jank_v4()
                    ->missed_vsyncs_per_reason,
                kNonJankyFrame);
    }

    // The processor should mark frame 81 as janky. It violates the fling
    // continuity rule because Chrome missed 9 VSyncs in the middle of a fast
    // fling.
    {
      AdvanceByVsyncs(10);
      EventMetrics::List metrics;
      metrics.push_back(
          CreateInertialGestureScrollUpdate(next_input_generation_ts_,
                                            /* delta= */ 2.0f,
                                            /* did_scroll= */ true));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, CreateNextBeginFrameArgs());
      EXPECT_EQ(
          metrics[0]
              ->AsScrollUpdate()
              ->scroll_jank_v4()
              ->missed_vsyncs_per_reason,
          MakeMissedVsyncCounts({{JankReason::kMissedVsyncDuringFling, 9}}));
    }

    for (int i = 82; i <= 100; i++) {
      AdvanceByVsyncs(1);
      EventMetrics::List metrics;
      metrics.push_back(
          CreateInertialGestureScrollUpdate(next_input_generation_ts_,
                                            /* delta= */ 2.0f,
                                            /* did_scroll= */ true));
      processor_.ProcessEventsMetricsForPresentedFrame(
          metrics, next_presentation_ts_, CreateNextBeginFrameArgs());
      EXPECT_EQ(metrics[0]
                    ->AsScrollUpdate()
                    ->scroll_jank_v4()
                    ->missed_vsyncs_per_reason,
                kNonJankyFrame);
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
    EventMetrics::List end_metrics;
    end_metrics.push_back(
        CreateInertialGestureScrollEnd(next_input_generation_ts_));
    processor_.ProcessEventsMetricsForPresentedFrame(
        end_metrics, next_presentation_ts_, CreateNextBeginFrameArgs());

    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 3 * 100 / 100,
        1);
  }
}

}  // namespace cc
