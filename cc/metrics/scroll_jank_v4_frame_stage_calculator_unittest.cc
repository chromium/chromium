// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_frame_stage_calculator.h"

#include <optional>

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

}  // namespace

class ScrollJankV4FrameStageCalculatorTest : public testing::Test {
 public:
  ScrollJankV4FrameStageCalculatorTest() = default;
  ~ScrollJankV4FrameStageCalculatorTest() override = default;

 protected:
  static base::TimeTicks MillisecondsTicks(int ms) {
    return base::TimeTicks() + base::Milliseconds(ms);
  }

  EventMetricsTestCreator metrics_creator_;
  std::unique_ptr<ScrollJankV4FrameStageCalculator> calculator_ =
      ScrollJankV4FrameStageCalculator::Create();
};

TEST_F(ScrollJankV4FrameStageCalculatorTest, EmptyEventMetricsList) {
  EventMetrics::List events_metrics;
  auto stages = calculator_->CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, FirstGestureScrollUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(16))
                               .SetDelta(4)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  auto stages = calculator_->CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4Frame::Stage{ScrollStart{}},
                  ScrollJankV4Frame::Stage{ScrollUpdates(
                      Real{
                          .first_input_generation_ts = MillisecondsTicks(16),
                          .last_input_generation_ts = MillisecondsTicks(16),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 4,
                          .max_abs_inertial_raw_delta_pixels = 0,
                          .first_input_trace_id = TraceId(42),
                      },
                      /* synthetic= */ std::nullopt)}));
  EXPECT_EQ(events_metrics[0]->AsScroll()->scroll_jank_v4_result_id(),
            kResultId);
}

TEST_F(ScrollJankV4FrameStageCalculatorTest,
       SyntheticFirstGestureScrollUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(16))
                               .SetDelta(4)
                               .SetIsSynthetic(true)
                               .SetTraceId(TraceId(42))
                               .SetDispatchArgs(DispatchBeginFrameArgs{
                                   .frame_time = MillisecondsTicks(24)})
                               .Build());
  auto stages = calculator_->CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4Frame::Stage{ScrollStart{}},
                  ScrollJankV4Frame::Stage{ScrollUpdates(
                      /* real= */ std::nullopt,
                      Synthetic{
                          .first_input_begin_frame_ts = MillisecondsTicks(24),
                          .has_inertial_input = false,
                          .first_input_trace_id = TraceId(42),
                      })}));
  EXPECT_EQ(events_metrics[0]->AsScroll()->scroll_jank_v4_result_id(),
            kResultId);
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, GestureScrollUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(16))
                               .SetDelta(4)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  auto stages = calculator_->CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(stages,
              ElementsAre(ScrollJankV4Frame::Stage{ScrollUpdates(
                  Real{.first_input_generation_ts = MillisecondsTicks(16),
                       .last_input_generation_ts = MillisecondsTicks(16),
                       .has_inertial_input = false,
                       .abs_total_raw_delta_pixels = 4,
                       .max_abs_inertial_raw_delta_pixels = 0,
                       .first_input_trace_id = TraceId(42)},
                  /* synthetic= */ std::nullopt)}));
  EXPECT_EQ(events_metrics[0]->AsScroll()->scroll_jank_v4_result_id(),
            kResultId);
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, SyntheticGestureScrollUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(16))
                               .SetDelta(4)
                               .SetIsSynthetic(true)
                               .SetTraceId(TraceId(42))
                               .SetDispatchArgs(DispatchBeginFrameArgs{
                                   .frame_time = MillisecondsTicks(24)})
                               .Build());
  auto stages = calculator_->CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(stages,
              ElementsAre(ScrollJankV4Frame::Stage{ScrollUpdates(
                  /* real= */ std::nullopt,
                  Synthetic{
                      .first_input_begin_frame_ts = MillisecondsTicks(24),
                      .has_inertial_input = false,
                      .first_input_trace_id = TraceId(42),
                  })}));
  EXPECT_EQ(events_metrics[0]->AsScroll()->scroll_jank_v4_result_id(),
            kResultId);
}

TEST_F(ScrollJankV4FrameStageCalculatorTest,
       SyntheticInertialGestureScrollUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(16))
                               .SetDelta(4)
                               .SetIsSynthetic(true)
                               .SetTraceId(TraceId(42))
                               .SetDispatchArgs(DispatchBeginFrameArgs{
                                   .frame_time = MillisecondsTicks(24)})
                               .Build());
  auto stages = calculator_->CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(stages,
              ElementsAre(ScrollJankV4Frame::Stage{ScrollUpdates(
                  /* real= */ std::nullopt,
                  Synthetic{
                      .first_input_begin_frame_ts = MillisecondsTicks(24),
                      .has_inertial_input = true,
                      .first_input_trace_id = TraceId(42),
                  })}));
  EXPECT_EQ(events_metrics[0]->AsScroll()->scroll_jank_v4_result_id(),
            kResultId);
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, InertialGestureScrollUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(16))
                               .SetDelta(4)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  auto stages = calculator_->CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(stages,
              ElementsAre(ScrollJankV4Frame::Stage{ScrollUpdates(
                  Real{
                      .first_input_generation_ts = MillisecondsTicks(16),
                      .last_input_generation_ts = MillisecondsTicks(16),
                      .has_inertial_input = true,
                      .abs_total_raw_delta_pixels = 4,
                      .max_abs_inertial_raw_delta_pixels = 4,
                      .first_input_trace_id = TraceId(42),
                  },
                  /* synthetic= */ std::nullopt)}));
  EXPECT_EQ(events_metrics[0]->AsScroll()->scroll_jank_v4_result_id(),
            kResultId);
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, GestureScrollEnd) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                               .SetTimestamp(MillisecondsTicks(16))
                               .Build());
  auto stages = calculator_->CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(stages, ElementsAre(ScrollJankV4Frame::Stage{ScrollEnd{}}));
  EXPECT_EQ(events_metrics[0]->AsScroll()->scroll_jank_v4_result_id(),
            kResultId);
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, InertialGestureScrollEnd) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.InertialGestureScrollEndBuilder()
                               .SetTimestamp(MillisecondsTicks(16))
                               .Build());
  auto stages = calculator_->CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(stages, ElementsAre(ScrollJankV4Frame::Stage{ScrollEnd{}}));
  EXPECT_EQ(events_metrics[0]->AsScroll()->scroll_jank_v4_result_id(),
            kResultId);
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, NonScrollEventType) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(
      metrics_creator_.CreateEventBuilder(ui::EventType::kMouseMoved)
          .SetTimestamp(MillisecondsTicks(16))
          .Build());
  auto stages = calculator_->CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageCalculatorTest, MultipleScrollUpdates) {
  EventMetrics::List events_metrics;
  // Intentionally in "random" order to make sure that the calculation doesn't
  // rely on the list being sorted (because the list isn't sorted in general).
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(4))
                               .SetDelta(-8'000)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(44))
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(2))
                               .SetDelta(-32'000)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(22))
                               .Build());
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(7))
                               .SetDelta(-1'000)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(77))
                               .Build());
  events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(1))
                               .SetDelta(-64'000)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(11))
                               .Build());
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(5))
                               .SetDelta(-4'000)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(55))
                               .Build());
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(6))
                               .SetDelta(-2'000)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(66))
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(3))
                               .SetDelta(-16'000)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(33))
                               .Build());
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(8))
                               .SetDelta(-128'000)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(88))
                               .Build());

  auto stages = calculator_->CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4Frame::Stage{ScrollStart{}},
                  ScrollJankV4Frame::Stage{ScrollUpdates(
                      Real{
                          .first_input_generation_ts = MillisecondsTicks(1),
                          .last_input_generation_ts = MillisecondsTicks(8),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 255'000,
                          .max_abs_inertial_raw_delta_pixels = 128'000,
                          .first_input_trace_id = TraceId(11),
                      },
                      /* synthetic= */ std::nullopt)}));
  for (size_t i = 0; i < 8; ++i) {
    EXPECT_EQ(events_metrics[i]->AsScroll()->scroll_jank_v4_result_id(),
              kResultId)
        << "Index " << i;
  }
}

TEST_F(ScrollJankV4FrameStageCalculatorTest,
       MultipleScrollUpdatesIncludingSynthetic) {
  EventMetrics::List events_metrics;
  // Intentionally in "random" order to make sure that the calculation doesn't
  // rely on the list being sorted (functionality isn't sorted in general).
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(4))
                               .SetDelta(-8'000)
                               .SetIsSynthetic(true)
                               .SetTraceId(TraceId(44))
                               .SetDispatchArgs(DispatchBeginFrameArgs{
                                   .frame_time = MillisecondsTicks(24)})
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(2))
                               .SetDelta(-32'000)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(22))
                               .Build());
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(7))
                               .SetDelta(-1'000)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(77))
                               .Build());
  events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(1))
                               .SetDelta(-64'000)
                               .SetIsSynthetic(true)
                               .SetTraceId(TraceId(11))
                               .SetDispatchArgs(DispatchBeginFrameArgs{
                                   .frame_time = MillisecondsTicks(48)})
                               .Build());
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(5))
                               .SetDelta(-4'000)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(55))
                               .Build());
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(6))
                               .SetDelta(-2'000)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(66))
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(3))
                               .SetDelta(-16'000)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(33))
                               .Build());
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(8))
                               .SetDelta(-128'000)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(88))
                               .Build());

  auto stages = calculator_->CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4Frame::Stage{ScrollStart{}},
                  ScrollJankV4Frame::Stage{ScrollUpdates(
                      Real{
                          .first_input_generation_ts = MillisecondsTicks(2),
                          .last_input_generation_ts = MillisecondsTicks(8),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 183'000,
                          .max_abs_inertial_raw_delta_pixels = 128'000,
                          .first_input_trace_id = TraceId(22),
                      },
                      Synthetic{
                          .first_input_begin_frame_ts = MillisecondsTicks(24),
                          .has_inertial_input = false,
                          .first_input_trace_id = TraceId(44),
                      })}));
  for (const auto& event : events_metrics) {
    EXPECT_EQ(event->AsScroll()->scroll_jank_v4_result_id(), kResultId);
  }
}

TEST_F(ScrollJankV4FrameStageCalculatorTest,
       ScrollEndForPreviousScrollThenScrollUpdates) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(3))
                               .SetDelta(40)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(33))
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                               .SetTimestamp(MillisecondsTicks(1))
                               .Build());
  events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(2))
                               .SetDelta(6)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(22))
                               .Build());
  auto stages = calculator_->CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4Frame::Stage{ScrollEnd{}},
                  ScrollJankV4Frame::Stage{ScrollStart{}},
                  ScrollJankV4Frame::Stage{ScrollUpdates(
                      Real{
                          .first_input_generation_ts = MillisecondsTicks(2),
                          .last_input_generation_ts = MillisecondsTicks(3),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 46,
                          .max_abs_inertial_raw_delta_pixels = 0,
                          .first_input_trace_id = TraceId(22),
                      },
                      /* synthetic= */ std::nullopt)}));
  for (const auto& event : events_metrics) {
    EXPECT_EQ(event->AsScroll()->scroll_jank_v4_result_id(), kResultId);
  }
}

TEST_F(ScrollJankV4FrameStageCalculatorTest,
       ScrollUpdatesThenScrollEndForCurrentScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(1))
                               .SetDelta(40)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(11))
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                               .SetTimestamp(MillisecondsTicks(3))
                               .Build());
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(2))
                               .SetDelta(6)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(22))
                               .Build());
  auto stages = calculator_->CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4Frame::Stage{ScrollUpdates(
                      Real{
                          .first_input_generation_ts = MillisecondsTicks(1),
                          .last_input_generation_ts = MillisecondsTicks(2),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 46,
                          .max_abs_inertial_raw_delta_pixels = 40,
                          .first_input_trace_id = TraceId(11),
                      },
                      /* synthetic= */ std::nullopt)},
                  ScrollJankV4Frame::Stage{ScrollEnd{}}));
  for (const auto& event : events_metrics) {
    EXPECT_EQ(event->AsScroll()->scroll_jank_v4_result_id(), kResultId);
  }
}

// Verifies that `calculator_->CalculateStages` orders scroll events
// based on the timestamps of their arrival in the renderer compositor.
//
// Timestamp                1      2      3      4
// Inertial scroll update:         IG----AiRC---------...
// Inertial scroll end:     IG------------------AiRC--...
// (IG = input generation, AiRC = arrived in renderer compositor)
//
// Since the IGSE's timestamp of arrival in the renderer compositor (4 ms) is
// greater than that of the IGSU (3 ms), the expected ordering is [IGSU, IGSE].
TEST_F(ScrollJankV4FrameStageCalculatorTest,
       OrdersEventsByArrivedInRendererCompositor) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(
      metrics_creator_.InertialGestureScrollUpdateBuilder()
          .SetTimestamp(MillisecondsTicks(2))
          .SetArrivedInRendererCompositorTimestamp(MillisecondsTicks(3))
          .SetDelta(40)
          .SetIsSynthetic(false)
          .SetTraceId(TraceId(111))
          .Build());
  events_metrics.push_back(
      metrics_creator_.InertialGestureScrollEndBuilder()
          .SetTimestamp(MillisecondsTicks(1))
          .SetArrivedInRendererCompositorTimestamp(MillisecondsTicks(4))
          .Build());
  auto stages = calculator_->CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4Frame::Stage{ScrollUpdates(
                      Real{
                          .first_input_generation_ts = MillisecondsTicks(2),
                          .last_input_generation_ts = MillisecondsTicks(2),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 40,
                          .max_abs_inertial_raw_delta_pixels = 40,
                          .first_input_trace_id = TraceId(111),
                      },
                      /* synthetic= */ std::nullopt)},
                  ScrollJankV4Frame::Stage{ScrollEnd{}}));
  EXPECT_EQ(events_metrics[0]->AsScroll()->scroll_jank_v4_result_id(),
            kResultId);
  EXPECT_EQ(events_metrics[1]->AsScroll()->scroll_jank_v4_result_id(),
            kResultId);
}

}  // namespace cc
