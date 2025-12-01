// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_frame_stage.h"

#include <memory>
#include <optional>
#include <sstream>

#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
#include "cc/test/event_metrics_test_creator.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/types/event_type.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {

namespace {

using ScrollUpdates = ScrollJankV4FrameStage::ScrollUpdates;
using ScrollStart = ScrollJankV4FrameStage::ScrollStart;
using ScrollEnd = ScrollJankV4FrameStage::ScrollEnd;
using Real = ScrollUpdates::Real;
using Synthetic = ScrollUpdates::Synthetic;
using TraceId = EventMetrics::TraceId;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr uint64_t kSourceId = 999;
constexpr base::TimeDelta kVsyncInterval = base::Milliseconds(16);

}  // namespace

class ScrollJankV4FrameStageTest : public testing::Test {
 public:
  ScrollJankV4FrameStageTest() = default;
  ~ScrollJankV4FrameStageTest() override = default;

 protected:
  static base::TimeTicks MillisecondsTicks(int ms) {
    return base::TimeTicks() + base::Milliseconds(ms);
  }

  static viz::BeginFrameArgs CreateBeginFrameArgs(int sequence_id,
                                                  base::TimeTicks frame_time) {
    return viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, kSourceId, sequence_id, frame_time,
        /* deadline= */ frame_time + kVsyncInterval / 3,
        /* interval= */ kVsyncInterval,
        viz::BeginFrameArgs::BeginFrameArgsType::NORMAL);
  }

  EventMetricsTestCreator metrics_creator_;
};

TEST_F(ScrollJankV4FrameStageTest, EmptyEventMetricsList) {
  EventMetrics::List events_metrics;
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest,
       FirstGestureScrollUpdateWhichCausedFrameUpdateAndDidScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(
      stages,
      ElementsAre(
          ScrollJankV4FrameStage{ScrollStart{}},
          ScrollJankV4FrameStage{ScrollUpdates(
              /* earliest_event */ static_cast<ScrollUpdateEventMetrics*>(
                  events_metrics[0].get()),
              Real{
                  .first_input_generation_ts = MillisecondsTicks(16),
                  .last_input_generation_ts = MillisecondsTicks(16),
                  .has_inertial_input = false,
                  .abs_total_raw_delta_pixels = 4,
                  .max_abs_inertial_raw_delta_pixels = 0,
                  .first_input_trace_id = TraceId(42),
              },
              /* synthetic= */ std::nullopt)}));
}

TEST_F(ScrollJankV4FrameStageTest,
       FirstGestureScrollUpdateWhichDidNotCauseFrameUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = false,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest, FirstGestureScrollUpdateWhichDidNotScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = true,
       .did_scroll = false,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  // Unlike continued GSUs (regular or inertial), scroll jank should be
  // reported for FGSUs even if they didn't cause a scroll.
  EXPECT_THAT(
      stages,
      ElementsAre(
          ScrollJankV4FrameStage{ScrollStart{}},
          ScrollJankV4FrameStage{ScrollUpdates(
              static_cast<ScrollUpdateEventMetrics*>(events_metrics[0].get()),
              Real{.first_input_generation_ts = MillisecondsTicks(16),
                   .last_input_generation_ts = MillisecondsTicks(16),
                   .has_inertial_input = false,
                   .abs_total_raw_delta_pixels = 4,
                   .max_abs_inertial_raw_delta_pixels = 0,
                   .first_input_trace_id = TraceId(42)},
              /* synthetic= */ std::nullopt)}));
}

TEST_F(ScrollJankV4FrameStageTest,
       FirstGestureScrollUpdateDoNotSkipNonDamagingEvents) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate({
      .timestamp = MillisecondsTicks(16),
      .delta = 4,
      .caused_frame_update = false,
      .did_scroll = false,
      .is_synthetic = false,
      .trace_id = TraceId(42),
  }));
  auto stages = ScrollJankV4FrameStage::CalculateStages(
      events_metrics, /* skip_non_damaging_events= */ false);
  EXPECT_THAT(
      stages,
      ElementsAre(
          ScrollJankV4FrameStage{ScrollStart{}},
          ScrollJankV4FrameStage{ScrollUpdates(
              static_cast<ScrollUpdateEventMetrics*>(events_metrics[0].get()),
              Real{.first_input_generation_ts = MillisecondsTicks(16),
                   .last_input_generation_ts = MillisecondsTicks(16),
                   .has_inertial_input = false,
                   .abs_total_raw_delta_pixels = 4,
                   .max_abs_inertial_raw_delta_pixels = 0,
                   .first_input_trace_id = TraceId(42)},
              /* synthetic= */ std::nullopt)}));
}

TEST_F(ScrollJankV4FrameStageTest, SyntheticFirstGestureScrollUpdate) {
  viz::BeginFrameArgs args = CreateBeginFrameArgs(123, MillisecondsTicks(24));
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate({
      .timestamp = MillisecondsTicks(16),
      .delta = 4,
      .caused_frame_update = true,
      .did_scroll = true,
      .is_synthetic = true,
      .trace_id = TraceId(42),
      .begin_frame_args = args,
  }));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(
      stages,
      ElementsAre(
          ScrollJankV4FrameStage{ScrollStart{}},
          ScrollJankV4FrameStage{ScrollUpdates(
              /* earliest_event */ static_cast<ScrollUpdateEventMetrics*>(
                  events_metrics[0].get()),
              /* real= */ std::nullopt,
              Synthetic{
                  .first_input_begin_frame_ts = MillisecondsTicks(24),
                  .first_input_trace_id = TraceId(42),
              })}));
}

TEST_F(ScrollJankV4FrameStageTest,
       GestureScrollUpdateWhichCausedFrameUpdateAndDidScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates(
          static_cast<ScrollUpdateEventMetrics*>(events_metrics[0].get()),
          Real{.first_input_generation_ts = MillisecondsTicks(16),
               .last_input_generation_ts = MillisecondsTicks(16),
               .has_inertial_input = false,
               .abs_total_raw_delta_pixels = 4,
               .max_abs_inertial_raw_delta_pixels = 0,
               .first_input_trace_id = TraceId(42)},
          /* synthetic= */ std::nullopt)}));
}

TEST_F(ScrollJankV4FrameStageTest,
       GestureScrollUpdateWhichDidNotCauseFrameUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = false,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest, GestureScrollUpdateWhichDidNotScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = true,
       .did_scroll = false,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest,
       GestureScrollUpdateDoNotSkipNonDamagingEvents) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = false,
       .did_scroll = false,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(
      events_metrics, /* skip_non_damaging_events= */ false);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates(
          static_cast<ScrollUpdateEventMetrics*>(events_metrics[0].get()),
          Real{
              .first_input_generation_ts = MillisecondsTicks(16),
              .last_input_generation_ts = MillisecondsTicks(16),
              .has_inertial_input = false,
              .abs_total_raw_delta_pixels = 4,
              .max_abs_inertial_raw_delta_pixels = 0,
              .first_input_trace_id = TraceId(42),
          },
          /* synthetic= */ std::nullopt)}));
}

TEST_F(ScrollJankV4FrameStageTest, SyntheticGestureScrollUpdate) {
  viz::BeginFrameArgs args = CreateBeginFrameArgs(123, MillisecondsTicks(24));
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = true,
       .trace_id = TraceId(42),
       .begin_frame_args = args}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates(
          static_cast<ScrollUpdateEventMetrics*>(events_metrics[0].get()),
          /* real= */ std::nullopt,
          Synthetic{
              .first_input_begin_frame_ts = MillisecondsTicks(24),
              .first_input_trace_id = TraceId(42),
          })}));
}

TEST_F(ScrollJankV4FrameStageTest,
       InertialGestureScrollUpdateWhichCausedFrameUpdateAndDidScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates(
          static_cast<ScrollUpdateEventMetrics*>(events_metrics[0].get()),
          Real{
              .first_input_generation_ts = MillisecondsTicks(16),
              .last_input_generation_ts = MillisecondsTicks(16),
              .has_inertial_input = true,
              .abs_total_raw_delta_pixels = 4,
              .max_abs_inertial_raw_delta_pixels = 4,
              .first_input_trace_id = TraceId(42),
          },
          /* synthetic= */ std::nullopt)}));
}

TEST_F(ScrollJankV4FrameStageTest,
       InertialGestureScrollUpdateWhichDidNotCauseFrameUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = false,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest,
       InertialGestureScrollUpdateWhichDidNotScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = true,
       .did_scroll = false,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest,
       InertialGestureScrollUpdateDoNotSkipNonDamagingEvents) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = false,
       .did_scroll = false,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(
      events_metrics, /* skip_non_damaging_events= */ false);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates(
          static_cast<ScrollUpdateEventMetrics*>(events_metrics[0].get()),
          Real{
              .first_input_generation_ts = MillisecondsTicks(16),
              .last_input_generation_ts = MillisecondsTicks(16),
              .has_inertial_input = true,
              .abs_total_raw_delta_pixels = 4,
              .max_abs_inertial_raw_delta_pixels = 4,
              .first_input_trace_id = TraceId(42),
          },
          /* synthetic= */ std::nullopt)}));
}

TEST_F(ScrollJankV4FrameStageTest, GestureScrollEnd) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateGestureScrollEnd(
      {.timestamp = MillisecondsTicks(16), .caused_frame_update = false}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, ElementsAre(ScrollJankV4FrameStage{ScrollEnd{}}));
}

TEST_F(ScrollJankV4FrameStageTest, InertialGestureScrollEnd) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollEnd(
      {.timestamp = MillisecondsTicks(16), .caused_frame_update = false}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, ElementsAre(ScrollJankV4FrameStage{ScrollEnd{}}));
}

TEST_F(ScrollJankV4FrameStageTest, NonScrollEventType) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(
      metrics_creator_.CreateEventMetrics({.type = ui::EventType::kMouseMoved,
                                           .timestamp = MillisecondsTicks(16),
                                           .caused_frame_update = true}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest, MultipleScrollUpdates) {
  EventMetrics::List events_metrics;
  // Intentionally in "random" order to make sure that the calculation doesn't
  // rely on the list being sorted (because the list isn't sorted in general).
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(4),
       .delta = -8'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(44)}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(2),
       .delta = -32'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(22)}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(7),
       .delta = -1'000,
       .caused_frame_update = true,
       .did_scroll = false,
       .is_synthetic = false,
       .trace_id = TraceId(77)}));
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(1),
       .delta = -64'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(11)}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(5),
       .delta = -4'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(55)}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(6),
       .delta = -2'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(66)}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(3),
       .delta = -16'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(33)}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(8),
       .delta = -128'000,
       .caused_frame_update = false,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(88)}));

  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(
      stages,
      ElementsAre(
          ScrollJankV4FrameStage{ScrollStart{}},
          ScrollJankV4FrameStage{ScrollUpdates(
              static_cast<ScrollUpdateEventMetrics*>(events_metrics[3].get()),
              Real{
                  .first_input_generation_ts = MillisecondsTicks(1),
                  .last_input_generation_ts = MillisecondsTicks(7),
                  .has_inertial_input = true,
                  .abs_total_raw_delta_pixels = 127'000,
                  .max_abs_inertial_raw_delta_pixels = 4'000,
                  .first_input_trace_id = TraceId(11),
              },
              /* synthetic= */ std::nullopt)}));
}

TEST_F(ScrollJankV4FrameStageTest,
       MultipleScrollUpdatesDoNotSkipNonDamagingEvents) {
  EventMetrics::List events_metrics;
  // Intentionally in "random" order to make sure that the calculation doesn't
  // rely on the list being sorted (because the list isn't sorted in general).
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(4),
       .delta = -8'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(44)}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(2),
       .delta = -32'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(22)}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(7),
       .delta = -1'000,
       .caused_frame_update = true,
       .did_scroll = false,
       .is_synthetic = false,
       .trace_id = TraceId(77)}));
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(1),
       .delta = -64'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(11)}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(5),
       .delta = -4'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(55)}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(6),
       .delta = -2'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(66)}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(3),
       .delta = -16'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(33)}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(8),
       .delta = -128'000,
       .caused_frame_update = false,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(88)}));

  auto stages = ScrollJankV4FrameStage::CalculateStages(
      events_metrics, /* skip_non_damaging_events= */ false);
  EXPECT_THAT(
      stages,
      ElementsAre(
          ScrollJankV4FrameStage{ScrollStart{}},
          ScrollJankV4FrameStage{ScrollUpdates(
              static_cast<ScrollUpdateEventMetrics*>(events_metrics[3].get()),
              Real{
                  .first_input_generation_ts = MillisecondsTicks(1),
                  .last_input_generation_ts = MillisecondsTicks(8),
                  .has_inertial_input = true,
                  .abs_total_raw_delta_pixels = 255'000,
                  .max_abs_inertial_raw_delta_pixels = 128'000,
                  .first_input_trace_id = TraceId(11),
              },
              /* synthetic= */ std::nullopt)}));
}

TEST_F(ScrollJankV4FrameStageTest, MultipleScrollUpdatesIncludingSynthetic) {
  viz::BeginFrameArgs args24 = CreateBeginFrameArgs(123, MillisecondsTicks(24));
  viz::BeginFrameArgs args48 = CreateBeginFrameArgs(456, MillisecondsTicks(48));
  EventMetrics::List events_metrics;
  // Intentionally in "random" order to make sure that the calculation doesn't
  // rely on the list being sorted (because the list isn't sorted in general).
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(4),
       .delta = -8'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = true,
       .trace_id = TraceId(44),
       .begin_frame_args = args24}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(2),
       .delta = -32'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(22)}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(7),
       .delta = -1'000,
       .caused_frame_update = true,
       .did_scroll = false,
       .is_synthetic = false,
       .trace_id = TraceId(77)}));
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(1),
       .delta = -64'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = true,
       .trace_id = TraceId(11),
       .begin_frame_args = args48}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(5),
       .delta = -4'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(55)}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(6),
       .delta = -2'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(66)}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(3),
       .delta = -16'000,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(33)}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(8),
       .delta = -128'000,
       .caused_frame_update = false,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(88)}));

  auto stages = ScrollJankV4FrameStage::CalculateStages(
      events_metrics, /* skip_non_damaging_events= */ false);
  EXPECT_THAT(
      stages,
      ElementsAre(
          ScrollJankV4FrameStage{ScrollStart{}},
          ScrollJankV4FrameStage{ScrollUpdates(
              static_cast<ScrollUpdateEventMetrics*>(events_metrics[3].get()),
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
                  .first_input_trace_id = TraceId(44),
              })}));
}

TEST_F(ScrollJankV4FrameStageTest,
       ScrollEndForPreviousScrollThenScrollUpdates) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(3),
       .delta = 40,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(33)}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollEnd(
      {.timestamp = MillisecondsTicks(1), .caused_frame_update = false}));
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(2),
       .delta = 6,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(22)}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(
      stages,
      ElementsAre(
          ScrollJankV4FrameStage{ScrollEnd{}},
          ScrollJankV4FrameStage{ScrollStart{}},
          ScrollJankV4FrameStage{ScrollUpdates(
              static_cast<ScrollUpdateEventMetrics*>(events_metrics[2].get()),
              Real{
                  .first_input_generation_ts = MillisecondsTicks(2),
                  .last_input_generation_ts = MillisecondsTicks(3),
                  .has_inertial_input = false,
                  .abs_total_raw_delta_pixels = 46,
                  .max_abs_inertial_raw_delta_pixels = 0,
                  .first_input_trace_id = TraceId(22),
              },
              /* synthetic= */ std::nullopt)}));
}

TEST_F(ScrollJankV4FrameStageTest, ScrollUpdatesThenScrollEndForCurrentScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(1),
       .delta = 40,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(11)}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollEnd(
      {.timestamp = MillisecondsTicks(3), .caused_frame_update = false}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(2),
       .delta = 6,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(22)}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(
      stages,
      ElementsAre(
          ScrollJankV4FrameStage{ScrollUpdates(
              static_cast<ScrollUpdateEventMetrics*>(events_metrics[0].get()),
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
}

TEST_F(ScrollJankV4FrameStageTest,
       IgnoreScrollUpdatesThatDidNotCauseFrameUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(1),
       .delta = 1,
       .caused_frame_update = false,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(11)}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(2),
       .delta = 10,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(22)}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(3),
       .delta = 100,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(33)}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(4),
       .delta = 1000,
       .caused_frame_update = false,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(44)}));

  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates(
          static_cast<ScrollUpdateEventMetrics*>(events_metrics[1].get()),
          Real{
              .first_input_generation_ts = MillisecondsTicks(2),
              .last_input_generation_ts = MillisecondsTicks(3),
              .has_inertial_input = true,
              .abs_total_raw_delta_pixels = 110,
              .max_abs_inertial_raw_delta_pixels = 100,
              .first_input_trace_id = TraceId(22),
          },
          /* synthetic= */ std::nullopt)}));
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
  };

  std::ostringstream out;
  auto& result = out << updates;
  EXPECT_THAT(out.str(), ::testing::MatchesRegex(R"(Synthetic\{.+\})"));
  EXPECT_EQ(&result, &out);
}

TEST_F(ScrollJankV4FrameStageTest,
       ScrollUpdatesWithNonNullEarliestEventToOstream) {
  std::unique_ptr<ScrollUpdateEventMetrics> earliest_event =
      metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = MillisecondsTicks(1),
           .delta = 2,
           .caused_frame_update = true,
           .did_scroll = true,
           .is_synthetic = false,
           .trace_id = TraceId(42)});
  auto stage = ScrollJankV4FrameStage{
      ScrollUpdates(earliest_event.get(),
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

TEST_F(ScrollJankV4FrameStageTest,
       ScrollUpdatesWithNullEarliestEventToOstream) {
  auto stage = ScrollJankV4FrameStage{
      ScrollUpdates(/* earliest_event= */ nullptr,
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
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(1),
       .delta = 5,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(2),
       .delta = 5,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  ScrollJankV4FrameStage::CalculateStages(events_metrics);

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
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(1),
       .delta = 5,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollEnd(
      {.timestamp = MillisecondsTicks(2), .caused_frame_update = false}));
  ScrollJankV4FrameStage::CalculateStages(events_metrics);

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
  events_metrics.push_back(metrics_creator_.CreateGestureScrollEnd(
      {.timestamp = MillisecondsTicks(1), .caused_frame_update = false}));
  ScrollJankV4FrameStage::CalculateStages(events_metrics);

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.FrameStageCalculationResult",
      ScrollJankV4FrameStage::FrameStageCalculationResult::kScrollEndOnly, 1);
}

TEST_F(ScrollJankV4FrameStageTest,
       FrameStageCalculationResultScrollEndThenStartThenUpdates) {
  base::HistogramTester histogram_tester;
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateGestureScrollEnd(
      {.timestamp = MillisecondsTicks(1), .caused_frame_update = false}));
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(2),
       .delta = 5,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(3),
       .delta = 5,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  ScrollJankV4FrameStage::CalculateStages(events_metrics);

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
  events_metrics.push_back(metrics_creator_.CreateGestureScrollEnd(
      {.timestamp = MillisecondsTicks(1), .caused_frame_update = false}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollEnd(
      {.timestamp = MillisecondsTicks(2), .caused_frame_update = false}));
  ScrollJankV4FrameStage::CalculateStages(events_metrics);

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
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(1),
       .delta = 5,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(2),
       .delta = 5,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  ScrollJankV4FrameStage::CalculateStages(events_metrics);

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
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(1),
       .delta = 5,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(2),
       .delta = 5,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  ScrollJankV4FrameStage::CalculateStages(events_metrics);

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
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(1),
       .delta = 5,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollEnd(
      {.timestamp = MillisecondsTicks(2), .caused_frame_update = false}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(3),
       .delta = 5,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  ScrollJankV4FrameStage::CalculateStages(events_metrics);

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
  events_metrics.push_back(metrics_creator_.CreateGestureScrollEnd(
      {.timestamp = MillisecondsTicks(1), .caused_frame_update = false}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(2),
       .delta = 5,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  ScrollJankV4FrameStage::CalculateStages(events_metrics);

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
  events_metrics.push_back(metrics_creator_.CreateGestureScrollEnd(
      {.timestamp = MillisecondsTicks(1), .caused_frame_update = false}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollEnd(
      {.timestamp = MillisecondsTicks(2), .caused_frame_update = false}));
  // Two scroll starts.
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(3),
       .delta = 5,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(4),
       .delta = 5,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  ScrollJankV4FrameStage::CalculateStages(events_metrics);

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.FrameStageCalculationResult",
      ScrollJankV4FrameStage::FrameStageCalculationResult::kMultipleIssues, 1);
}

TEST_F(ScrollJankV4FrameStageTest,
       FrameStageCalculationResultScrollStartThenUpdates) {
  base::HistogramTester histogram_tester;
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;

  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(1),
       .delta = 5,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(2),
       .delta = 5,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  ScrollJankV4FrameStage::CalculateStages(events_metrics);

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
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(1),
       .delta = 5,
       .caused_frame_update = true,
       .did_scroll = true,
       .is_synthetic = false,
       .trace_id = TraceId(42)}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollEnd(
      {.timestamp = MillisecondsTicks(2), .caused_frame_update = true}));
  ScrollJankV4FrameStage::CalculateStages(events_metrics);

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.FrameStageCalculationResult",
      ScrollJankV4FrameStage::FrameStageCalculationResult::
          kScrollStartThenUpdatesThenEnd,
      1);
}

}  // namespace cc
