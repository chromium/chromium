// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_frame_stage.h"

#include <memory>
#include <optional>
#include <sstream>

#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
#include "cc/test/event_metrics_test_creator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/types/event_type.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {

namespace {

using ScrollUpdates = ScrollJankV4FrameStage::ScrollUpdates;
using ScrollEnd = ScrollJankV4FrameStage::ScrollEnd;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

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
       .did_scroll = true}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates{
          .is_scroll_start = true,
          .earliest_event = base::raw_ref(
              static_cast<ScrollUpdateEventMetrics&>(*events_metrics[0])),
          .last_input_generation_ts = MillisecondsTicks(16),
          .has_inertial_input = false,
          .total_raw_delta_pixels = 4,
          .max_abs_inertial_raw_delta_pixels = 0,
      }}));
}

TEST_F(ScrollJankV4FrameStageTest,
       FirstGestureScrollUpdateWhichDidNotCauseFrameUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = false,
       .did_scroll = true}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest, FirstGestureScrollUpdateWhichDidNotScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = true,
       .did_scroll = false}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  // Unlike continued GSUs (regular or inertial), scroll jank should be
  // reported for FGSUs even if they didn't cause a scroll.
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates{
          .is_scroll_start = true,
          .earliest_event = base::raw_ref(
              static_cast<ScrollUpdateEventMetrics&>(*events_metrics[0])),
          .last_input_generation_ts = MillisecondsTicks(16),
          .has_inertial_input = false,
          .total_raw_delta_pixels = 4,
          .max_abs_inertial_raw_delta_pixels = 0,
      }}));
}

TEST_F(ScrollJankV4FrameStageTest,
       FirstGestureScrollUpdateDoNotSkipNonDamagingEvents) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = false,
       .did_scroll = false}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(
      events_metrics, /* skip_non_damaging_events= */ false);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates{
          .is_scroll_start = true,
          .earliest_event = base::raw_ref(
              static_cast<ScrollUpdateEventMetrics&>(*events_metrics[0])),
          .last_input_generation_ts = MillisecondsTicks(16),
          .has_inertial_input = false,
          .total_raw_delta_pixels = 4,
          .max_abs_inertial_raw_delta_pixels = 0,
      }}));
}

TEST_F(ScrollJankV4FrameStageTest,
       GestureScrollUpdateWhichCausedFrameUpdateAndDidScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = true,
       .did_scroll = true}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates{
          .is_scroll_start = false,
          .earliest_event = base::raw_ref(
              static_cast<ScrollUpdateEventMetrics&>(*events_metrics[0])),
          .last_input_generation_ts = MillisecondsTicks(16),
          .has_inertial_input = false,
          .total_raw_delta_pixels = 4,
          .max_abs_inertial_raw_delta_pixels = 0,
      }}));
}

TEST_F(ScrollJankV4FrameStageTest,
       GestureScrollUpdateWhichDidNotCauseFrameUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = false,
       .did_scroll = true}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest, GestureScrollUpdateWhichDidNotScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = true,
       .did_scroll = false}));
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
       .did_scroll = false}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(
      events_metrics, /* skip_non_damaging_events= */ false);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates{
          .is_scroll_start = false,
          .earliest_event = base::raw_ref(
              static_cast<ScrollUpdateEventMetrics&>(*events_metrics[0])),
          .last_input_generation_ts = MillisecondsTicks(16),
          .has_inertial_input = false,
          .total_raw_delta_pixels = 4,
          .max_abs_inertial_raw_delta_pixels = 0,
      }}));
}

TEST_F(ScrollJankV4FrameStageTest,
       InertialGestureScrollUpdateWhichCausedFrameUpdateAndDidScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = true,
       .did_scroll = true}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates{
          .is_scroll_start = false,
          .earliest_event = base::raw_ref(
              static_cast<ScrollUpdateEventMetrics&>(*events_metrics[0])),
          .last_input_generation_ts = MillisecondsTicks(16),
          .has_inertial_input = true,
          .total_raw_delta_pixels = 4,
          .max_abs_inertial_raw_delta_pixels = 4,
      }}));
}

TEST_F(ScrollJankV4FrameStageTest,
       InertialGestureScrollUpdateWhichDidNotCauseFrameUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 4,
       .caused_frame_update = false,
       .did_scroll = true}));
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
       .did_scroll = false}));
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
       .did_scroll = false}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(
      events_metrics, /* skip_non_damaging_events= */ false);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates{
          .is_scroll_start = false,
          .earliest_event = base::raw_ref(
              static_cast<ScrollUpdateEventMetrics&>(*events_metrics[0])),
          .last_input_generation_ts = MillisecondsTicks(16),
          .has_inertial_input = true,
          .total_raw_delta_pixels = 4,
          .max_abs_inertial_raw_delta_pixels = 4,
      }}));
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
       .did_scroll = true}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(2),
       .delta = -32'000,
       .caused_frame_update = true,
       .did_scroll = true}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(7),
       .delta = -1'000,
       .caused_frame_update = true,
       .did_scroll = false}));
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(1),
       .delta = -64'000,
       .caused_frame_update = true,
       .did_scroll = true}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(5),
       .delta = -4'000,
       .caused_frame_update = true,
       .did_scroll = true}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(6),
       .delta = -2'000,
       .caused_frame_update = true,
       .did_scroll = true}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(3),
       .delta = -16'000,
       .caused_frame_update = true,
       .did_scroll = true}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(8),
       .delta = -128'000,
       .caused_frame_update = false,
       .did_scroll = true}));

  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates{
          .is_scroll_start = true,
          .earliest_event = base::raw_ref(
              static_cast<ScrollUpdateEventMetrics&>(*events_metrics[3])),
          .last_input_generation_ts = MillisecondsTicks(7),
          .has_inertial_input = true,
          .total_raw_delta_pixels = -127'000,
          .max_abs_inertial_raw_delta_pixels = 4'000,
      }}));
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
       .did_scroll = true}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(2),
       .delta = -32'000,
       .caused_frame_update = true,
       .did_scroll = true}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(7),
       .delta = -1'000,
       .caused_frame_update = true,
       .did_scroll = false}));
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(1),
       .delta = -64'000,
       .caused_frame_update = true,
       .did_scroll = true}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(5),
       .delta = -4'000,
       .caused_frame_update = true,
       .did_scroll = true}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(6),
       .delta = -2'000,
       .caused_frame_update = true,
       .did_scroll = true}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(3),
       .delta = -16'000,
       .caused_frame_update = true,
       .did_scroll = true}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(8),
       .delta = -128'000,
       .caused_frame_update = false,
       .did_scroll = true}));

  auto stages = ScrollJankV4FrameStage::CalculateStages(
      events_metrics, /* skip_non_damaging_events= */ false);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates{
          .is_scroll_start = true,
          .earliest_event = base::raw_ref(
              static_cast<ScrollUpdateEventMetrics&>(*events_metrics[3])),
          .last_input_generation_ts = MillisecondsTicks(8),
          .has_inertial_input = true,
          .total_raw_delta_pixels = -255'000,
          .max_abs_inertial_raw_delta_pixels = 128'000,
      }}));
}

TEST_F(ScrollJankV4FrameStageTest,
       ScrollEndForPreviousScrollThenScrollUpdates) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(3),
       .delta = 40,
       .caused_frame_update = true,
       .did_scroll = true}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollEnd(
      {.timestamp = MillisecondsTicks(1), .caused_frame_update = false}));
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(2),
       .delta = 6,
       .caused_frame_update = true,
       .did_scroll = true}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(
      stages,
      ElementsAre(
          ScrollJankV4FrameStage{ScrollEnd{}},
          ScrollJankV4FrameStage{ScrollUpdates{
              .is_scroll_start = true,
              .earliest_event = base::raw_ref(
                  static_cast<ScrollUpdateEventMetrics&>(*events_metrics[2])),
              .last_input_generation_ts = MillisecondsTicks(3),
              .has_inertial_input = false,
              .total_raw_delta_pixels = 46,
              .max_abs_inertial_raw_delta_pixels = 0,
          }}));
}

TEST_F(ScrollJankV4FrameStageTest, ScrollUpdatesThenScrollEndForCurrentScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(1),
       .delta = 40,
       .caused_frame_update = true,
       .did_scroll = true}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollEnd(
      {.timestamp = MillisecondsTicks(3), .caused_frame_update = false}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(2),
       .delta = 6,
       .caused_frame_update = true,
       .did_scroll = true}));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(
      stages,
      ElementsAre(
          ScrollJankV4FrameStage{ScrollUpdates{
              .is_scroll_start = false,
              .earliest_event = base::raw_ref(
                  static_cast<ScrollUpdateEventMetrics&>(*events_metrics[0])),
              .last_input_generation_ts = MillisecondsTicks(2),
              .has_inertial_input = true,
              .total_raw_delta_pixels = 46,
              .max_abs_inertial_raw_delta_pixels = 40,
          }},
          ScrollJankV4FrameStage{ScrollEnd{}}));
}

TEST_F(ScrollJankV4FrameStageTest,
       IgnoreScrollUpdatesThatDidNotCauseFrameUpdate) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(1),
       .delta = 1,
       .caused_frame_update = false,
       .did_scroll = true}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(2),
       .delta = 10,
       .caused_frame_update = true,
       .did_scroll = true}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(3),
       .delta = 100,
       .caused_frame_update = true,
       .did_scroll = true}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(4),
       .delta = 1000,
       .caused_frame_update = false,
       .did_scroll = true}));

  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(
      stages,
      ElementsAre(ScrollJankV4FrameStage{ScrollUpdates{
          .is_scroll_start = false,
          .earliest_event = base::raw_ref(
              static_cast<ScrollUpdateEventMetrics&>(*events_metrics[1])),
          .last_input_generation_ts = MillisecondsTicks(3),
          .has_inertial_input = true,
          .total_raw_delta_pixels = 110,
          .max_abs_inertial_raw_delta_pixels = 100,
      }}));
}

TEST_F(ScrollJankV4FrameStageTest, ScrollUpdatesToOstream) {
  std::unique_ptr<ScrollUpdateEventMetrics> event_metrics =
      metrics_creator_.CreateGestureScrollUpdate(
          {.timestamp = MillisecondsTicks(1),
           .delta = 2,
           .caused_frame_update = true,
           .did_scroll = true});
  auto stage = ScrollJankV4FrameStage{ScrollUpdates{
      .is_scroll_start = false,
      .earliest_event = base::raw_ref<ScrollUpdateEventMetrics>::from_ptr(
          event_metrics.get()),
      .last_input_generation_ts = MillisecondsTicks(3),
      .has_inertial_input = false,
      .total_raw_delta_pixels = 10,
      .max_abs_inertial_raw_delta_pixels = 0,
  }};

  std::ostringstream out;
  auto& result = out << stage;
  EXPECT_THAT(out.str(), ::testing::MatchesRegex(R"(ScrollUpdates\{.*\})"));
  EXPECT_EQ(&result, &out);
}

TEST_F(ScrollJankV4FrameStageTest, ScrollEndToOstream) {
  auto stage = ScrollJankV4FrameStage{ScrollEnd{}};

  std::ostringstream out;
  auto& result = out << stage;
  EXPECT_EQ(out.str(), "ScrollEnd{}");
  EXPECT_EQ(&result, &out);
}

}  // namespace cc
