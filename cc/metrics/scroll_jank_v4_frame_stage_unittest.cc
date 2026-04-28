// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_frame_stage.h"

#include <memory>
#include <optional>
#include <sstream>

#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "cc/metrics/event_metrics.h"
#include "cc/test/event_metrics_test_creator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/types/event_type.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {

namespace {

using DispatchBeginFrameArgs = ScrollEventMetrics::DispatchBeginFrameArgs;
using ScrollUpdates = ScrollJankV4FrameStage::ScrollUpdates;
using ScrollStart = ScrollJankV4FrameStage::ScrollStart;
using ScrollEnd = ScrollJankV4FrameStage::ScrollEnd;
using Real = ScrollUpdates::Real;
using Synthetic = ScrollUpdates::Synthetic;
using TraceId = EventMetrics::TraceId;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr uint64_t kResultId = 123456;

}  // namespace

class ScrollJankV4FrameStageTest : public testing::Test {
 public:
  ScrollJankV4FrameStageTest() = default;
  ~ScrollJankV4FrameStageTest() override = default;

 protected:
  static base::TimeTicks MillisecondsTicks(int ms) {
    return base::TimeTicks() + base::Milliseconds(ms);
  }

  EventMetricsTestCreator metrics_creator_;
};

TEST_F(ScrollJankV4FrameStageTest, EmptyEventMetricsList) {
  EventMetrics::List events_metrics;
  auto stages =
      ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest, FirstGestureScrollUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(16))
                               .SetDelta(4)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  auto stages =
      ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(
          ScrollJankV4FrameStage{ScrollStart{}},
          ScrollJankV4FrameStage{ScrollUpdates(
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

TEST_F(ScrollJankV4FrameStageTest, SyntheticFirstGestureScrollUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(16))
                               .SetDelta(4)
                               .SetIsSynthetic(true)
                               .SetTraceId(TraceId(42))
                               .SetDispatchArgs(DispatchBeginFrameArgs{
                                   .frame_time = MillisecondsTicks(24)})
                               .Build());
  auto stages =
      ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollStart{}},
                  ScrollJankV4FrameStage{ScrollUpdates(
                      /* real= */ std::nullopt,
                      Synthetic{
                          .first_input_begin_frame_ts = MillisecondsTicks(24),
                          .has_inertial_input = false,
                          .first_input_trace_id = TraceId(42),
                      })}));
  EXPECT_EQ(events_metrics[0]->AsScroll()->scroll_jank_v4_result_id(),
            kResultId);
}

TEST_F(ScrollJankV4FrameStageTest, GestureScrollUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(16))
                               .SetDelta(4)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  auto stages =
      ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates(
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

TEST_F(ScrollJankV4FrameStageTest, SyntheticGestureScrollUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(16))
                               .SetDelta(4)
                               .SetIsSynthetic(true)
                               .SetTraceId(TraceId(42))
                               .SetDispatchArgs(DispatchBeginFrameArgs{
                                   .frame_time = MillisecondsTicks(24)})
                               .Build());
  auto stages =
      ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(stages,
              ElementsAre(ScrollJankV4FrameStage{ScrollUpdates(
                  /* real= */ std::nullopt,
                  Synthetic{
                      .first_input_begin_frame_ts = MillisecondsTicks(24),
                      .has_inertial_input = false,
                      .first_input_trace_id = TraceId(42),
                  })}));
  EXPECT_EQ(events_metrics[0]->AsScroll()->scroll_jank_v4_result_id(),
            kResultId);
}

TEST_F(ScrollJankV4FrameStageTest, SyntheticInertialGestureScrollUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(16))
                               .SetDelta(4)
                               .SetIsSynthetic(true)
                               .SetTraceId(TraceId(42))
                               .SetDispatchArgs(DispatchBeginFrameArgs{
                                   .frame_time = MillisecondsTicks(24)})
                               .Build());
  auto stages =
      ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(stages,
              ElementsAre(ScrollJankV4FrameStage{ScrollUpdates(
                  /* real= */ std::nullopt,
                  Synthetic{
                      .first_input_begin_frame_ts = MillisecondsTicks(24),
                      .has_inertial_input = true,
                      .first_input_trace_id = TraceId(42),
                  })}));
  EXPECT_EQ(events_metrics[0]->AsScroll()->scroll_jank_v4_result_id(),
            kResultId);
}

TEST_F(ScrollJankV4FrameStageTest, InertialGestureScrollUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(16))
                               .SetDelta(4)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  auto stages =
      ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates(
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

TEST_F(ScrollJankV4FrameStageTest, GestureScrollEnd) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                               .SetTimestamp(MillisecondsTicks(16))
                               .Build());
  auto stages =
      ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(stages, ElementsAre(ScrollJankV4FrameStage{ScrollEnd{}}));
  EXPECT_EQ(events_metrics[0]->AsScroll()->scroll_jank_v4_result_id(),
            kResultId);
}

TEST_F(ScrollJankV4FrameStageTest, InertialGestureScrollEnd) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.InertialGestureScrollEndBuilder()
                               .SetTimestamp(MillisecondsTicks(16))
                               .Build());
  auto stages =
      ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(stages, ElementsAre(ScrollJankV4FrameStage{ScrollEnd{}}));
  EXPECT_EQ(events_metrics[0]->AsScroll()->scroll_jank_v4_result_id(),
            kResultId);
}

TEST_F(ScrollJankV4FrameStageTest, NonScrollEventType) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(
      metrics_creator_.CreateEventBuilder(ui::EventType::kMouseMoved)
          .SetTimestamp(MillisecondsTicks(16))
          .Build());
  auto stages =
      ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest, MultipleScrollUpdates) {
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

  auto stages =
      ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollStart{}},
                  ScrollJankV4FrameStage{ScrollUpdates(
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


TEST_F(ScrollJankV4FrameStageTest, MultipleScrollUpdatesIncludingSynthetic) {
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

  auto stages =
      ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollStart{}},
                  ScrollJankV4FrameStage{ScrollUpdates(
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

TEST_F(ScrollJankV4FrameStageTest,
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
  auto stages =
      ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(
          ScrollJankV4FrameStage{ScrollEnd{}},
          ScrollJankV4FrameStage{ScrollStart{}},
          ScrollJankV4FrameStage{ScrollUpdates(
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

TEST_F(ScrollJankV4FrameStageTest, ScrollUpdatesThenScrollEndForCurrentScroll) {
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
  auto stages =
      ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(
          ScrollJankV4FrameStage{ScrollUpdates(
              Real{
                  .first_input_generation_ts = MillisecondsTicks(1),
                  .last_input_generation_ts = MillisecondsTicks(2),
                  .has_inertial_input = true,
                  .abs_total_raw_delta_pixels = 46,
                  .max_abs_inertial_raw_delta_pixels = 40,
                  .first_input_trace_id = TraceId(11),
              },
              /* synthetic= */ std::nullopt)},
          ScrollJankV4FrameStage{ScrollEnd{}}));
  for (const auto& event : events_metrics) {
    EXPECT_EQ(event->AsScroll()->scroll_jank_v4_result_id(), kResultId);
  }
}

// Verifies that `ScrollJankV4FrameStage::CalculateStages` orders scroll events
// based on the timestamps of their arrival in the renderer compositor.
//
// Timestamp                1      2      3      4
// Inertial scroll update:         IG----AiRC---------...
// Inertial scroll end:     IG------------------AiRC--...
// (IG = input generation, AiRC = arrived in renderer compositor)
//
// Since the IGSE's timestamp of arrival in the renderer compositor (4 ms) is
// greater than that of the IGSU (3 ms), the expected ordering is [IGSU, IGSE].
TEST_F(ScrollJankV4FrameStageTest, OrdersEventsByArrivedInRendererCompositor) {
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
  auto stages =
      ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates(
                      Real{
                          .first_input_generation_ts = MillisecondsTicks(2),
                          .last_input_generation_ts = MillisecondsTicks(2),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 40,
                          .max_abs_inertial_raw_delta_pixels = 40,
                          .first_input_trace_id = TraceId(111),
                      },
                      /* synthetic= */ std::nullopt)},
                  ScrollJankV4FrameStage{ScrollEnd{}}));
  EXPECT_EQ(events_metrics[0]->AsScroll()->scroll_jank_v4_result_id(),
            kResultId);
  EXPECT_EQ(events_metrics[1]->AsScroll()->scroll_jank_v4_result_id(),
            kResultId);
}

TEST_F(ScrollJankV4FrameStageTest, EmptyRealScrollUpdatesToOstream) {
  std::optional<ScrollUpdates::Real> updates = std::nullopt;

  std::ostringstream out;
  auto& result = out << updates;
  EXPECT_EQ(out.str(), "empty");
  EXPECT_EQ(&result, &out);
}

TEST_F(ScrollJankV4FrameStageTest, NonEmptyRealScrollUpdatesToOstream) {
  std::optional<ScrollUpdates::Real> updates = ScrollUpdates::Real{
      .last_input_generation_ts = MillisecondsTicks(3),
      .has_inertial_input = false,
      .abs_total_raw_delta_pixels = 10,
      .max_abs_inertial_raw_delta_pixels = 0,
  };

  std::ostringstream out;
  auto& result = out << updates;
  EXPECT_THAT(out.str(), ::testing::MatchesRegex(R"(Real\{.+\})"));
  EXPECT_EQ(&result, &out);
}

TEST_F(ScrollJankV4FrameStageTest, EmptySyntheticScrollUpdatesToOstream) {
  std::optional<ScrollUpdates::Synthetic> updates = std::nullopt;

  std::ostringstream out;
  auto& result = out << updates;
  EXPECT_EQ(out.str(), "empty");
  EXPECT_EQ(&result, &out);
}

TEST_F(ScrollJankV4FrameStageTest, NonEmptySyntheticScrollUpdatesToOstream) {
  std::optional<ScrollUpdates::Synthetic> updates = ScrollUpdates::Synthetic{
      .first_input_begin_frame_ts = MillisecondsTicks(42),
      .has_inertial_input = false,
  };

  std::ostringstream out;
  auto& result = out << updates;
  EXPECT_THAT(out.str(), ::testing::MatchesRegex(R"(Synthetic\{.+\})"));
  EXPECT_EQ(&result, &out);
}

TEST_F(ScrollJankV4FrameStageTest, ScrollUpdatesToOstream) {
  auto stage = ScrollJankV4FrameStage{ScrollUpdates(
      ScrollUpdates::Real{
          .first_input_generation_ts = MillisecondsTicks(1),
          .last_input_generation_ts = MillisecondsTicks(3),
          .has_inertial_input = false,
          .abs_total_raw_delta_pixels = 10,
          .max_abs_inertial_raw_delta_pixels = 0,
      },
      /* synthetic= */ std::nullopt)};

  std::ostringstream out;
  auto& result = out << stage;
  EXPECT_THAT(out.str(), ::testing::MatchesRegex(R"(ScrollUpdates\{.+\})"));
  EXPECT_EQ(&result, &out);
}

TEST_F(ScrollJankV4FrameStageTest, ScrollStartToOstream) {
  auto stage = ScrollJankV4FrameStage{ScrollStart{}};

  std::ostringstream out;
  auto& result = out << stage;
  EXPECT_EQ(out.str(), "ScrollStart{}");
  EXPECT_EQ(&result, &out);
}

TEST_F(ScrollJankV4FrameStageTest, ScrollEndToOstream) {
  auto stage = ScrollJankV4FrameStage{ScrollEnd{}};

  std::ostringstream out;
  auto& result = out << stage;
  EXPECT_EQ(out.str(), "ScrollEnd{}");
  EXPECT_EQ(&result, &out);
}

TEST_F(ScrollJankV4FrameStageTest,
       FrameStageCalculationResultScrollUpdatesOnly) {
  base::HistogramTester histogram_tester;
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(1))
                               .SetDelta(5)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  events_metrics.push_back(metrics_creator_.InertialGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(2))
                               .SetDelta(5)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.FrameStageCalculationResult",
      ScrollJankV4FrameStage::FrameStageCalculationResult::kScrollUpdatesOnly,
      1);
}

TEST_F(ScrollJankV4FrameStageTest,
       FrameStageCalculationResultScrollUpdatesThenEnd) {
  base::HistogramTester histogram_tester;
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(1))
                               .SetDelta(5)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                               .SetTimestamp(MillisecondsTicks(2))
                               .Build());
  ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.FrameStageCalculationResult",
      ScrollJankV4FrameStage::FrameStageCalculationResult::
          kScrollUpdatesThenEnd,
      1);
}

TEST_F(ScrollJankV4FrameStageTest, FrameStageCalculationResultScrollEndOnly) {
  base::HistogramTester histogram_tester;
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                               .SetTimestamp(MillisecondsTicks(1))
                               .Build());
  ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.FrameStageCalculationResult",
      ScrollJankV4FrameStage::FrameStageCalculationResult::kScrollEndOnly, 1);
}

TEST_F(ScrollJankV4FrameStageTest,
       FrameStageCalculationResultScrollEndThenStartThenUpdates) {
  base::HistogramTester histogram_tester;
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                               .SetTimestamp(MillisecondsTicks(1))
                               .Build());
  events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(2))
                               .SetDelta(5)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(3))
                               .SetDelta(5)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.FrameStageCalculationResult",
      ScrollJankV4FrameStage::FrameStageCalculationResult::
          kScrollEndThenStartThenUpdates,
      1);
}

TEST_F(ScrollJankV4FrameStageTest,
       FrameStageCalculationResultMultipleScrollEnds) {
  base::HistogramTester histogram_tester;
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                               .SetTimestamp(MillisecondsTicks(1))
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                               .SetTimestamp(MillisecondsTicks(2))
                               .Build());
  ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.FrameStageCalculationResult",
      ScrollJankV4FrameStage::FrameStageCalculationResult::kMultipleScrollEnds,
      1);
}

TEST_F(ScrollJankV4FrameStageTest,
       FrameStageCalculationResultMultipleScrollStarts) {
  base::HistogramTester histogram_tester;
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(1))
                               .SetDelta(5)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(2))
                               .SetDelta(5)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.FrameStageCalculationResult",
      ScrollJankV4FrameStage::FrameStageCalculationResult::
          kMultipleScrollStarts,
      1);
}

TEST_F(ScrollJankV4FrameStageTest,
       FrameStageCalculationResultScrollStartAfterUpdate) {
  base::HistogramTester histogram_tester;
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(1))
                               .SetDelta(5)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(2))
                               .SetDelta(5)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.FrameStageCalculationResult",
      ScrollJankV4FrameStage::FrameStageCalculationResult::
          kScrollStartAfterUpdate,
      1);
}

TEST_F(ScrollJankV4FrameStageTest,
       FrameStageCalculationResultScrollEndBetweenUpdates) {
  base::HistogramTester histogram_tester;
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(1))
                               .SetDelta(5)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                               .SetTimestamp(MillisecondsTicks(2))
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(3))
                               .SetDelta(5)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.FrameStageCalculationResult",
      ScrollJankV4FrameStage::FrameStageCalculationResult::
          kScrollEndBetweenUpdates,
      1);
}

TEST_F(ScrollJankV4FrameStageTest,
       FrameStageCalculationResultScrollEndThenUpdatesWithoutStart) {
  base::HistogramTester histogram_tester;
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                               .SetTimestamp(MillisecondsTicks(1))
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(2))
                               .SetDelta(5)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.FrameStageCalculationResult",
      ScrollJankV4FrameStage::FrameStageCalculationResult::
          kScrollEndThenUpdatesWithoutStart,
      1);
}

TEST_F(ScrollJankV4FrameStageTest, FrameStageCalculationResultMultipleIssues) {
  base::HistogramTester histogram_tester;
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;

  EventMetrics::List events_metrics;
  // Two scroll ends.
  events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                               .SetTimestamp(MillisecondsTicks(1))
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                               .SetTimestamp(MillisecondsTicks(2))
                               .Build());
  // Two scroll starts.
  events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(3))
                               .SetDelta(5)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(4))
                               .SetDelta(5)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.FrameStageCalculationResult",
      ScrollJankV4FrameStage::FrameStageCalculationResult::kMultipleIssues, 1);
}

TEST_F(ScrollJankV4FrameStageTest,
       FrameStageCalculationResultScrollStartThenUpdates) {
  base::HistogramTester histogram_tester;
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(1))
                               .SetDelta(5)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(2))
                               .SetDelta(5)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.FrameStageCalculationResult",
      ScrollJankV4FrameStage::FrameStageCalculationResult::
          kScrollStartThenUpdates,
      1);
}

TEST_F(ScrollJankV4FrameStageTest,
       FrameStageCalculationResultScrollStartThenUpdatesThenEnd) {
  base::HistogramTester histogram_tester;
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.FirstGestureScrollUpdateBuilder()
                               .SetTimestamp(MillisecondsTicks(1))
                               .SetDelta(5)
                               .SetIsSynthetic(false)
                               .SetTraceId(TraceId(42))
                               .Build());
  events_metrics.push_back(metrics_creator_.GestureScrollEndBuilder()
                               .SetTimestamp(MillisecondsTicks(2))
                               .Build());
  ScrollJankV4FrameStage::CalculateStages(events_metrics, kResultId);

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.FrameStageCalculationResult",
      ScrollJankV4FrameStage::FrameStageCalculationResult::
          kScrollStartThenUpdatesThenEnd,
      1);
}

}  // namespace cc
