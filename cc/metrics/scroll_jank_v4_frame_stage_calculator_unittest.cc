// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_frame_stage_calculator.h"

#include <cstdint>
#include <optional>

#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "cc/test/event_metrics_test_creator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/types/event_type.h"

namespace cc {
namespace {

using DispatchBeginFrameArgs = ScrollEventMetrics::DispatchBeginFrameArgs;
using ScrollUpdates = ScrollJankV4Frame::Stage::ScrollUpdates;
using ScrollStart = ScrollJankV4Frame::Stage::ScrollStart;
using ScrollEnd = ScrollJankV4Frame::Stage::ScrollEnd;
using Real = ScrollUpdates::Real;
using Synthetic = ScrollUpdates::Synthetic;
using TraceId = EventMetrics::TraceId;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr uint64_t kResultId = 123456;

// A matcher which matches a `EventMetrics::List` iff each element in the list
// points to a `ScrollEventMetrics` whose `scroll_jank_v4_result_id()` property
// has value `result_id`.
constexpr ::testing::Matcher<const EventMetrics::List&> AllHaveResultId(
    uint64_t result_id) {
  return ::testing::Each(::testing::Pointee(::testing::Property(
      &EventMetrics::AsScroll,
      ::testing::Property(&ScrollEventMetrics::scroll_jank_v4_result_id,
                          result_id))));
}

class ScrollJankV4FrameStageCalculatorTest : public testing::Test {
 protected:
  static base::TimeTicks MillisecondsTicks(int ms) {
    return base::TimeTicks() + base::Milliseconds(ms);
  }

  EventMetricsTestCreator metrics_creator_;
  ScrollJankV4FrameStageCalculator calculator_;
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample_;
};

TEST_F(ScrollJankV4FrameStageCalculatorTest, EmptyEventMetricsList) {
  EventMetrics::List events_metrics;
  auto stages = calculator_.CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, RegularScrolls) {
  // Frame 1: 1st GSU of scroll 1.
  base::TimeTicks scroll1_id = MillisecondsTicks(100);
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(105))
                                 .SetScrollBeginArrivalTimestamp(scroll1_id)
                                 .SetDelta(10)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(1))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1001);
    EXPECT_THAT(
        stages,
        ElementsAre(
            ScrollJankV4Frame::Stage{ScrollStart{}},
            ScrollJankV4Frame::Stage{ScrollUpdates(
                Real{.first_input_generation_ts = MillisecondsTicks(105),
                     .last_input_generation_ts = MillisecondsTicks(105),
                     .has_inertial_input = false,
                     .total_raw_delta_pixels = 10,
                     .max_abs_inertial_raw_delta_pixels = 0,
                     .first_input_trace_id = TraceId(1)},
                /* synthetic= */ std::nullopt, scroll1_id)}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1001));
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }

  // Frame 2: 2nd GSU of scroll 1.
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(120))
                                 .SetScrollBeginArrivalTimestamp(scroll1_id)
                                 .SetDelta(20)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(2))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1002);
    EXPECT_THAT(stages,
                ElementsAre(ScrollJankV4Frame::Stage{ScrollUpdates(
                    Real{.first_input_generation_ts = MillisecondsTicks(120),
                         .last_input_generation_ts = MillisecondsTicks(120),
                         .has_inertial_input = false,
                         .total_raw_delta_pixels = 20,
                         .max_abs_inertial_raw_delta_pixels = 0,
                         .first_input_trace_id = TraceId(2)},
                    /* synthetic= */ std::nullopt, scroll1_id)}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1002));
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }

  // Frame 3: GSE of scroll 1 and 1st GSU of scroll 2
  base::TimeTicks scroll2_id = MillisecondsTicks(130);
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                                 .SetTimestamp(MillisecondsTicks(135))
                                 .SetScrollBeginArrivalTimestamp(scroll1_id)
                                 .Build());
    events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(140))
                                 .SetScrollBeginArrivalTimestamp(scroll2_id)
                                 .SetDelta(30)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(3))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1003);
    EXPECT_THAT(
        stages,
        ElementsAre(ScrollJankV4Frame::Stage{ScrollEnd{}},
                    ScrollJankV4Frame::Stage{ScrollStart{}},
                    ScrollJankV4Frame::Stage{ScrollUpdates(
                        Real{
                            .first_input_generation_ts = MillisecondsTicks(140),
                            .last_input_generation_ts = MillisecondsTicks(140),
                            .has_inertial_input = false,
                            .total_raw_delta_pixels = 30,
                            .max_abs_inertial_raw_delta_pixels = 0,
                            .first_input_trace_id = TraceId(3),
                        },
                        /* synthetic= */ std::nullopt, scroll2_id)}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1003));
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }

  // Frame 4: no scroll events.
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(
        metrics_creator_.CreateEventBuilder(ui::EventType::kTouchMoved)
            .SetTimestamp(MillisecondsTicks(190))
            .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1004);
    EXPECT_THAT(stages, IsEmpty());
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues", 0);
  }

  // Frame 5: 2nd-3rd GSU of scroll 2.
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(155))
                                 .SetScrollBeginArrivalTimestamp(scroll2_id)
                                 .SetDelta(40)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(4))
                                 .Build());
    events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(160))
                                 .SetScrollBeginArrivalTimestamp(scroll2_id)
                                 .SetDelta(50)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(5))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1005);
    EXPECT_THAT(stages,
                ElementsAre(ScrollJankV4Frame::Stage{ScrollUpdates(
                    Real{.first_input_generation_ts = MillisecondsTicks(155),
                         .last_input_generation_ts = MillisecondsTicks(160),
                         .has_inertial_input = false,
                         .total_raw_delta_pixels = 90,
                         .max_abs_inertial_raw_delta_pixels = 0,
                         .first_input_trace_id = TraceId(4)},
                    /* synthetic= */ std::nullopt, scroll2_id)}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1005));
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }

  // Frame 6: 4th GSU and GSE of scroll 2
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(175))
                                 .SetScrollBeginArrivalTimestamp(scroll2_id)
                                 .SetDelta(60)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(6))
                                 .Build());
    events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                                 .SetTimestamp(MillisecondsTicks(180))
                                 .SetScrollBeginArrivalTimestamp(scroll2_id)
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1006);
    EXPECT_THAT(
        stages,
        ElementsAre(ScrollJankV4Frame::Stage{ScrollUpdates(
                        Real{
                            .first_input_generation_ts = MillisecondsTicks(175),
                            .last_input_generation_ts = MillisecondsTicks(175),
                            .has_inertial_input = false,
                            .total_raw_delta_pixels = 60,
                            .max_abs_inertial_raw_delta_pixels = 0,
                            .first_input_trace_id = TraceId(6),
                        },
                        /* synthetic= */ std::nullopt, scroll2_id)},
                    ScrollJankV4Frame::Stage{ScrollEnd{}}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1006));
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }

  // Frame 7: no scroll events.
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(
        metrics_creator_.CreateEventBuilder(ui::EventType::kGestureTap)
            .SetTimestamp(MillisecondsTicks(190))
            .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1007);
    EXPECT_THAT(stages, IsEmpty());
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues", 0);
  }

  // Frame 8: 1st GSU of scroll 3.
  base::TimeTicks scroll3_id = MillisecondsTicks(195);
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(205))
                                 .SetScrollBeginArrivalTimestamp(scroll3_id)
                                 .SetDelta(70)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(7))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1008);
    EXPECT_THAT(
        stages,
        ElementsAre(
            ScrollJankV4Frame::Stage{ScrollStart{}},
            ScrollJankV4Frame::Stage{ScrollUpdates(
                Real{.first_input_generation_ts = MillisecondsTicks(205),
                     .last_input_generation_ts = MillisecondsTicks(205),
                     .has_inertial_input = false,
                     .total_raw_delta_pixels = 70,
                     .max_abs_inertial_raw_delta_pixels = 0,
                     .first_input_trace_id = TraceId(7)},
                /* synthetic= */ std::nullopt, scroll3_id)}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1008));
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }

  // Frame 9: 2nd GSU of scroll 3.
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(220))
                                 .SetScrollBeginArrivalTimestamp(scroll3_id)
                                 .SetDelta(80)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(8))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1009);
    EXPECT_THAT(stages,
                ElementsAre(ScrollJankV4Frame::Stage{ScrollUpdates(
                    Real{.first_input_generation_ts = MillisecondsTicks(220),
                         .last_input_generation_ts = MillisecondsTicks(220),
                         .has_inertial_input = false,
                         .total_raw_delta_pixels = 80,
                         .max_abs_inertial_raw_delta_pixels = 0,
                         .first_input_trace_id = TraceId(8)},
                    /* synthetic= */ std::nullopt, scroll3_id)}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1009));
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }

  // Frame 10: Standalone GSE of scroll 3.
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                                 .SetTimestamp(MillisecondsTicks(235))
                                 .SetScrollBeginArrivalTimestamp(scroll3_id)
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1010);
    EXPECT_THAT(stages, ElementsAre(ScrollJankV4Frame::Stage{ScrollEnd{}}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1010));
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues", 0);
  }
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, OverlappingScrolls) {
  // Frame 1: 1st GSU of scroll 1.
  base::TimeTicks scroll1_id = MillisecondsTicks(100);
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(105))
                                 .SetScrollBeginArrivalTimestamp(scroll1_id)
                                 .SetDelta(100)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(1))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1001);
    EXPECT_THAT(
        stages,
        ElementsAre(
            ScrollJankV4Frame::Stage{ScrollStart{}},
            ScrollJankV4Frame::Stage{ScrollUpdates(
                Real{.first_input_generation_ts = MillisecondsTicks(105),
                     .last_input_generation_ts = MillisecondsTicks(105),
                     .has_inertial_input = false,
                     .total_raw_delta_pixels = 100,
                     .max_abs_inertial_raw_delta_pixels = 0,
                     .first_input_trace_id = TraceId(1)},
                /* synthetic= */ std::nullopt, scroll1_id)}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1001));
    histogram_tester.ExpectBucketCount(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }

  // Frame 2: 2nd GSU of scroll 1 and 1st GSU of scroll 2. The calculator should
  // count the frame towards scroll 1 and end scroll 1.
  base::TimeTicks scroll2_id = MillisecondsTicks(115);
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(120))
                                 .SetScrollBeginArrivalTimestamp(scroll2_id)
                                 .SetDelta(200)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(2))
                                 .Build());
    events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(125))
                                 .SetScrollBeginArrivalTimestamp(scroll1_id)
                                 .SetDelta(50)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(3))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1002);
    EXPECT_THAT(
        stages,
        ElementsAre(ScrollJankV4Frame::Stage{ScrollUpdates(
                        Real{
                            .first_input_generation_ts = MillisecondsTicks(125),
                            .last_input_generation_ts = MillisecondsTicks(125),
                            .has_inertial_input = false,
                            .total_raw_delta_pixels = 50,
                            .max_abs_inertial_raw_delta_pixels = 0,
                            .first_input_trace_id = TraceId(3),
                        },
                        /* synthetic= */ std::nullopt, scroll1_id)},
                    ScrollJankV4Frame::Stage{ScrollEnd{}}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1002));
    histogram_tester.ExpectBucketCount(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kOverlappingScrolls,
        1);
  }

  // Frame 3: 2nd GSU of scroll 2. The calculator should treat this as the first
  // frame of scroll 2.
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(135))
                                 .SetScrollBeginArrivalTimestamp(scroll2_id)
                                 .SetDelta(300)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(4))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1003);
    EXPECT_THAT(
        stages,
        ElementsAre(
            ScrollJankV4Frame::Stage{ScrollStart{}},
            ScrollJankV4Frame::Stage{ScrollUpdates(
                Real{.first_input_generation_ts = MillisecondsTicks(135),
                     .last_input_generation_ts = MillisecondsTicks(135),
                     .has_inertial_input = false,
                     .total_raw_delta_pixels = 300,
                     .max_abs_inertial_raw_delta_pixels = 0,
                     .first_input_trace_id = TraceId(4)},
                /* synthetic= */ std::nullopt, scroll2_id)}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1003));
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }
}

TEST_F(ScrollJankV4FrameStageCalculatorTest,
       IgnoreUpdatesAfterScrollAlreadyEnded) {
  // Frame 1: 1st GSU and GSE of scroll 1.
  base::TimeTicks scroll1_id = MillisecondsTicks(100);
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(105))
                                 .SetScrollBeginArrivalTimestamp(scroll1_id)
                                 .SetDelta(100)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(1))
                                 .Build());
    events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                                 .SetTimestamp(MillisecondsTicks(110))
                                 .SetScrollBeginArrivalTimestamp(scroll1_id)
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1001);
    EXPECT_THAT(
        stages,
        ElementsAre(
            ScrollJankV4Frame::Stage{ScrollStart{}},
            ScrollJankV4Frame::Stage{ScrollUpdates(
                Real{.first_input_generation_ts = MillisecondsTicks(105),
                     .last_input_generation_ts = MillisecondsTicks(105),
                     .has_inertial_input = false,
                     .total_raw_delta_pixels = 100,
                     .max_abs_inertial_raw_delta_pixels = 0,
                     .first_input_trace_id = TraceId(1)},
                /* synthetic= */ std::nullopt, scroll1_id)},
            ScrollJankV4Frame::Stage{ScrollEnd{}}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1001));
    histogram_tester.ExpectBucketCount(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }

  // Frame 2: 2nd GSU for scroll 1. The calculator should ignore this late GSU
  // because it's already seen a GSE for the same scroll.
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(120))
                                 .SetScrollBeginArrivalTimestamp(scroll1_id)
                                 .SetDelta(200)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(2))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1002);
    EXPECT_THAT(stages, IsEmpty());
    EXPECT_THAT(events_metrics, AllHaveResultId(1002));
    histogram_tester.ExpectBucketCount(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kLateUpdate,
        1);
  }
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, IgnoreUpdatesFromPreviousScrolls) {
  // Frame 1: 1st GSU for scroll 1.
  base::TimeTicks scroll1_id = MillisecondsTicks(100);
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(105))
                                 .SetScrollBeginArrivalTimestamp(scroll1_id)
                                 .SetDelta(100)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(1))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1001);
    EXPECT_THAT(
        stages,
        ElementsAre(
            ScrollJankV4Frame::Stage{ScrollStart{}},
            ScrollJankV4Frame::Stage{ScrollUpdates(
                Real{.first_input_generation_ts = MillisecondsTicks(105),
                     .last_input_generation_ts = MillisecondsTicks(105),
                     .has_inertial_input = false,
                     .total_raw_delta_pixels = 100,
                     .max_abs_inertial_raw_delta_pixels = 0,
                     .first_input_trace_id = TraceId(1)},
                /* synthetic= */ std::nullopt, scroll1_id)}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1001));
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }

  // Frame 2: 1st GSU for scroll 2.
  base::TimeTicks scroll2_id = MillisecondsTicks(110);
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(115))
                                 .SetScrollBeginArrivalTimestamp(scroll2_id)
                                 .SetDelta(200)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(2))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1002);
    EXPECT_THAT(
        stages,
        ElementsAre(
            ScrollJankV4Frame::Stage{ScrollEnd{}},
            ScrollJankV4Frame::Stage{ScrollStart{}},
            ScrollJankV4Frame::Stage{ScrollUpdates(
                Real{.first_input_generation_ts = MillisecondsTicks(115),
                     .last_input_generation_ts = MillisecondsTicks(115),
                     .has_inertial_input = false,
                     .total_raw_delta_pixels = 200,
                     .max_abs_inertial_raw_delta_pixels = 0,
                     .first_input_trace_id = TraceId(2)},
                /* synthetic= */ std::nullopt, scroll2_id)}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1002));
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }

  // Frame 3: 2nd GSU for scroll 1. The calculator should IGNORE this late GSU
  // because it's already seen a GSU for the next scroll.
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(120))
                                 .SetScrollBeginArrivalTimestamp(scroll1_id)
                                 .SetDelta(300)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(3))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1003);
    EXPECT_THAT(stages, IsEmpty());
    EXPECT_THAT(events_metrics, AllHaveResultId(1003));
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kLateUpdate,
        1);
  }

  // Frame 4: 2nd GSU for scroll 2. The calculator should process this GSU
  // because scroll 2 hasn't ended yet.
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(125))
                                 .SetScrollBeginArrivalTimestamp(scroll2_id)
                                 .SetDelta(400)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(4))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1004);
    EXPECT_THAT(stages,
                ElementsAre(ScrollJankV4Frame::Stage{ScrollUpdates(
                    Real{.first_input_generation_ts = MillisecondsTicks(125),
                         .last_input_generation_ts = MillisecondsTicks(125),
                         .has_inertial_input = false,
                         .total_raw_delta_pixels = 400,
                         .max_abs_inertial_raw_delta_pixels = 0,
                         .first_input_trace_id = TraceId(4)},
                    /* synthetic= */ std::nullopt, scroll2_id)}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1004));
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, IgnoreEndsFromPreviousScrolls) {
  // Frame 1: 1st GSU of scroll 1.
  base::TimeTicks scroll1_id = MillisecondsTicks(100);
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(105))
                                 .SetScrollBeginArrivalTimestamp(scroll1_id)
                                 .SetDelta(100)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(1))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1001);
    EXPECT_THAT(
        stages,
        ElementsAre(
            ScrollJankV4Frame::Stage{ScrollStart{}},
            ScrollJankV4Frame::Stage{ScrollUpdates(
                Real{.first_input_generation_ts = MillisecondsTicks(105),
                     .last_input_generation_ts = MillisecondsTicks(105),
                     .has_inertial_input = false,
                     .total_raw_delta_pixels = 100,
                     .max_abs_inertial_raw_delta_pixels = 0,
                     .first_input_trace_id = TraceId(1)},
                /* synthetic= */ std::nullopt, scroll1_id)}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1001));
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }

  // Frame 2: 1st GSU of scroll 2.
  base::TimeTicks scroll2_id = MillisecondsTicks(110);
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(115))
                                 .SetScrollBeginArrivalTimestamp(scroll2_id)
                                 .SetDelta(200)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(2))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1002);
    EXPECT_THAT(
        stages,
        ElementsAre(
            ScrollJankV4Frame::Stage{ScrollEnd{}},
            ScrollJankV4Frame::Stage{ScrollStart{}},
            ScrollJankV4Frame::Stage{ScrollUpdates(
                Real{.first_input_generation_ts = MillisecondsTicks(115),
                     .last_input_generation_ts = MillisecondsTicks(115),
                     .has_inertial_input = false,
                     .total_raw_delta_pixels = 200,
                     .max_abs_inertial_raw_delta_pixels = 0,
                     .first_input_trace_id = TraceId(2)},
                /* synthetic= */ std::nullopt, scroll2_id)}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1002));
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }

  // Frame 3: GSE for scroll 1. The calculator should IGNORE this late GSE
  // because it's already seen a GSU for the next scroll.
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                                 .SetTimestamp(MillisecondsTicks(120))
                                 .SetScrollBeginArrivalTimestamp(scroll1_id)
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1003);
    EXPECT_THAT(stages, IsEmpty());
    EXPECT_THAT(events_metrics, AllHaveResultId(1003));
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues", 0);
  }

  // Frame 4: 2nd GSU for scroll 2. The calculator should process this GSU
  // because scroll 2 hasn't ended yet.
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(125))
                                 .SetScrollBeginArrivalTimestamp(scroll2_id)
                                 .SetDelta(400)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(4))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1004);
    EXPECT_THAT(stages,
                ElementsAre(ScrollJankV4Frame::Stage{ScrollUpdates(
                    Real{.first_input_generation_ts = MillisecondsTicks(125),
                         .last_input_generation_ts = MillisecondsTicks(125),
                         .has_inertial_input = false,
                         .total_raw_delta_pixels = 400,
                         .max_abs_inertial_raw_delta_pixels = 0,
                         .first_input_trace_id = TraceId(4)},
                    /* synthetic= */ std::nullopt, scroll2_id)}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1004));
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, OverlappingScrollsAndLateUpdates) {
  // Frame 1: 1st GSU of scroll 1.
  base::TimeTicks scroll1_id = MillisecondsTicks(100);
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(105))
                                 .SetScrollBeginArrivalTimestamp(scroll1_id)
                                 .SetDelta(10)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(1))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1001);
    EXPECT_THAT(
        stages,
        ElementsAre(
            ScrollJankV4Frame::Stage{ScrollStart{}},
            ScrollJankV4Frame::Stage{ScrollUpdates(
                Real{.first_input_generation_ts = MillisecondsTicks(105),
                     .last_input_generation_ts = MillisecondsTicks(105),
                     .has_inertial_input = false,
                     .total_raw_delta_pixels = 10,
                     .max_abs_inertial_raw_delta_pixels = 0,
                     .first_input_trace_id = TraceId(1)},
                /* synthetic= */ std::nullopt, scroll1_id)}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1001));
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }

  // Frame 2: 1st GSU of scroll 2.
  base::TimeTicks scroll2_id = MillisecondsTicks(110);
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(115))
                                 .SetScrollBeginArrivalTimestamp(scroll2_id)
                                 .SetDelta(20)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(2))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1002);
    EXPECT_THAT(
        stages,
        ElementsAre(
            ScrollJankV4Frame::Stage{ScrollEnd{}},
            ScrollJankV4Frame::Stage{ScrollStart{}},
            ScrollJankV4Frame::Stage{ScrollUpdates(
                Real{.first_input_generation_ts = MillisecondsTicks(115),
                     .last_input_generation_ts = MillisecondsTicks(115),
                     .has_inertial_input = false,
                     .total_raw_delta_pixels = 20,
                     .max_abs_inertial_raw_delta_pixels = 0,
                     .first_input_trace_id = TraceId(2)},
                /* synthetic= */ std::nullopt, scroll2_id)}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1002));
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kNoIssues,
        1);
  }

  // Frame 3: 2nd GSU of scroll 1, 2nd GSU of scroll 2, and 1st GSU of scroll3.
  base::TimeTicks scroll3_id = MillisecondsTicks(120);
  {
    base::HistogramTester histogram_tester;
    EventMetrics::List events_metrics;
    events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(125))
                                 .SetScrollBeginArrivalTimestamp(scroll1_id)
                                 .SetDelta(30)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(3))
                                 .Build());
    events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(130))
                                 .SetScrollBeginArrivalTimestamp(scroll2_id)
                                 .SetDelta(40)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(4))
                                 .Build());
    events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                                 .SetTimestamp(MillisecondsTicks(135))
                                 .SetScrollBeginArrivalTimestamp(scroll3_id)
                                 .SetDelta(50)
                                 .SetIsSynthetic(false)
                                 .SetTraceId(TraceId(5))
                                 .Build());
    auto stages =
        calculator_.CalculateStages(events_metrics, /* result_id= */ 1003);
    EXPECT_THAT(
        stages,
        ElementsAre(
            ScrollJankV4Frame::Stage{ScrollUpdates(
                Real{.first_input_generation_ts = MillisecondsTicks(130),
                     .last_input_generation_ts = MillisecondsTicks(130),
                     .has_inertial_input = false,
                     .total_raw_delta_pixels = 40,
                     .max_abs_inertial_raw_delta_pixels = 0,
                     .first_input_trace_id = TraceId(4)},
                /* synthetic= */ std::nullopt, scroll2_id)},
            ScrollJankV4Frame::Stage{ScrollEnd{}}));
    EXPECT_THAT(events_metrics, AllHaveResultId(1003));
    histogram_tester.ExpectBucketCount(
        "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues",
        ScrollJankV4FrameStageCalculator::ScrollIdBasedCalculationIssues::
            kOverlappingScrollsAndLateUpdate,
        1);
  }
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, RealUpdatesOnly) {
  base::TimeTicks scroll1_id = MillisecondsTicks(100);

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(105))
                               .SetScrollBeginArrivalTimestamp(scroll1_id)
                               .SetDelta(200)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(2))
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(120))
                               .SetScrollBeginArrivalTimestamp(scroll1_id)
                               .SetDelta(300)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(3))
                               .Build());

  auto stages = calculator_.CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4Frame::Stage{ScrollStart{}},
                  ScrollJankV4Frame::Stage{ScrollUpdates(
                      Real{
                          .first_input_generation_ts = MillisecondsTicks(105),
                          .last_input_generation_ts = MillisecondsTicks(120),
                          .has_inertial_input = false,
                          .total_raw_delta_pixels = 500,
                          .max_abs_inertial_raw_delta_pixels = 0,
                          .first_input_trace_id = TraceId(2),
                      },
                      /* synthetic= */ std::nullopt, scroll1_id)}));
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, RealUpdatesOnlyInertial) {
  base::TimeTicks scroll1_id = MillisecondsTicks(100);

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(130))
                               .SetScrollBeginArrivalTimestamp(scroll1_id)
                               .SetDelta(-20)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(2))
                               .Build());
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(110))
                               .SetScrollBeginArrivalTimestamp(scroll1_id)
                               .SetDelta(-30)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(3))
                               .Build());

  auto stages = calculator_.CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4Frame::Stage{ScrollStart{}},
                  ScrollJankV4Frame::Stage{ScrollUpdates(
                      Real{
                          .first_input_generation_ts = MillisecondsTicks(110),
                          .last_input_generation_ts = MillisecondsTicks(130),
                          .has_inertial_input = true,
                          .total_raw_delta_pixels = -50,
                          .max_abs_inertial_raw_delta_pixels = 30,
                          .first_input_trace_id = TraceId(3),
                      },
                      /* synthetic= */ std::nullopt, scroll1_id)}));
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, SyntheticUpdatesOnly) {
  base::TimeTicks scroll1_id = MillisecondsTicks(100);

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(105))
                               .SetScrollBeginArrivalTimestamp(scroll1_id)
                               .SetIsSynthetic(true)
                               .SetDispatchArgs(DispatchBeginFrameArgs{
                                   .frame_time = MillisecondsTicks(110)})
                               .SetTraceId(TraceId(2))
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(104))
                               .SetScrollBeginArrivalTimestamp(scroll1_id)
                               .SetIsSynthetic(true)
                               .SetDispatchArgs(DispatchBeginFrameArgs{
                                   .frame_time = MillisecondsTicks(125)})
                               .SetTraceId(TraceId(3))
                               .Build());

  auto stages = calculator_.CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4Frame::Stage{ScrollStart{}},
                  ScrollJankV4Frame::Stage{ScrollUpdates(
                      /* real= */ std::nullopt,
                      Synthetic{
                          .first_input_begin_frame_ts = MillisecondsTicks(110),
                          .has_inertial_input = false,
                          .first_input_trace_id = TraceId(2),
                      },
                      scroll1_id)}));
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, SyntheticUpdatesOnlyInertial) {
  base::TimeTicks scroll1_id = MillisecondsTicks(100);

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(104))
                               .SetScrollBeginArrivalTimestamp(scroll1_id)
                               .SetIsSynthetic(true)
                               .SetDispatchArgs(DispatchBeginFrameArgs{
                                   .frame_time = MillisecondsTicks(125)})
                               .SetTraceId(TraceId(2))
                               .Build());
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(105))
                               .SetScrollBeginArrivalTimestamp(scroll1_id)
                               .SetIsSynthetic(true)
                               .SetDispatchArgs(DispatchBeginFrameArgs{
                                   .frame_time = MillisecondsTicks(110)})
                               .SetTraceId(TraceId(3))
                               .Build());

  auto stages = calculator_.CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4Frame::Stage{ScrollStart{}},
                  ScrollJankV4Frame::Stage{ScrollUpdates(
                      /* real= */ std::nullopt,
                      Synthetic{
                          .first_input_begin_frame_ts = MillisecondsTicks(110),
                          .has_inertial_input = true,
                          .first_input_trace_id = TraceId(3),
                      },
                      scroll1_id)}));
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, StatsRealAndSyntheticUpdates) {
  base::TimeTicks scroll1_id = MillisecondsTicks(100);

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(105))
                               .SetScrollBeginArrivalTimestamp(scroll1_id)
                               .SetDelta(200)
                               .SetIsSynthetic(false)
                               .SetDispatchArgs(DispatchBeginFrameArgs{
                                   .frame_time = MillisecondsTicks(124)})
                               .SetTraceId(TraceId(2))
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(104))
                               .SetScrollBeginArrivalTimestamp(scroll1_id)
                               .SetDelta(300)
                               .SetIsSynthetic(true)
                               .SetDispatchArgs(DispatchBeginFrameArgs{
                                   .frame_time = MillisecondsTicks(125)})
                               .SetTraceId(TraceId(3))
                               .Build());

  auto stages = calculator_.CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4Frame::Stage{ScrollStart{}},
                  ScrollJankV4Frame::Stage{ScrollUpdates(
                      Real{
                          .first_input_generation_ts = MillisecondsTicks(105),
                          .last_input_generation_ts = MillisecondsTicks(105),
                          .has_inertial_input = false,
                          .total_raw_delta_pixels = 200,
                          .max_abs_inertial_raw_delta_pixels = 0,
                          .first_input_trace_id = TraceId(2),
                      },
                      Synthetic{
                          .first_input_begin_frame_ts = MillisecondsTicks(125),
                          .has_inertial_input = false,
                          .first_input_trace_id = TraceId(3),
                      },
                      scroll1_id)}));
}

}  // namespace
}  // namespace cc
