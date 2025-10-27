// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_frame_stage.h"

#include <memory>
#include <optional>
#include <ostream>
#include <variant>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/events/types/event_type.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {

namespace {

using ScrollUpdates = ScrollJankV4FrameStage::ScrollUpdates;
using ScrollEnd = ScrollJankV4FrameStage::ScrollEnd;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

}  // namespace

// Printer for gtest. Must be in the same namespace as `ScrollJankV4FrameStage`
// (i.e. in cc) so that gtest would find it.
inline void PrintTo(const ScrollJankV4FrameStage& stage, std::ostream* os) {
  std::visit(absl::Overload{
                 [&](const ScrollUpdates& updates) {
                   (*os) << "ScrollUpdates{is_scroll_start: "
                         << updates.is_scroll_start << ", earliest_event: "
                         << updates.earliest_event->GetTypeName() << "@"
                         << &(*updates.earliest_event)
                         << ", last_input_generation_ts: "
                         << updates.last_input_generation_ts
                         << ", has_inertial_input: "
                         << updates.has_inertial_input
                         << ", total_raw_delta_pixels: "
                         << updates.total_raw_delta_pixels
                         << ", max_abs_inertial_raw_delta_pixels: "
                         << updates.max_abs_inertial_raw_delta_pixels << "}";
                 },
                 [&](const ScrollEnd& end) { (*os) << "ScrollEnd{}"; }},
             stage.stage);
}

class ScrollJankV4FrameStageTest : public testing::Test {
 public:
  ScrollJankV4FrameStageTest() = default;
  ~ScrollJankV4FrameStageTest() override = default;

 protected:
  std::unique_ptr<EventMetrics> CreateEventMetrics(base::TimeTicks timestamp,
                                                   ui::EventType type) {
    return EventMetrics::CreateForTesting(
        type, timestamp,
        /* arrived_in_browser_main_timestamp= */ timestamp +
            base::Nanoseconds(1),
        &test_tick_clock_, /* trace_id= */ std::nullopt);
  }

  std::unique_ptr<ScrollEventMetrics> CreateScrollEventMetrics(
      base::TimeTicks timestamp,
      ui::EventType type,
      bool is_inertial) {
    return ScrollEventMetrics::CreateForTesting(
        type, ui::ScrollInputType::kTouchscreen, is_inertial, timestamp,
        /* arrived_in_browser_main_timestamp= */ timestamp +
            base::Nanoseconds(1),
        &test_tick_clock_);
  }

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

  std::unique_ptr<ScrollEventMetrics> CreateGestureScrollEnd(
      base::TimeTicks timestamp) {
    auto event =
        CreateScrollEventMetrics(timestamp, ui::EventType::kGestureScrollEnd,
                                 /* is_inertial= */ false);
    EXPECT_EQ(event->type(), EventMetrics::EventType::kGestureScrollEnd);
    event->set_caused_frame_update(false);
    return event;
  }

  std::unique_ptr<ScrollEventMetrics> CreateInertialGestureScrollEnd(
      base::TimeTicks timestamp) {
    auto event =
        CreateScrollEventMetrics(timestamp, ui::EventType::kGestureScrollEnd,
                                 /* is_inertial= */ true);
    EXPECT_EQ(event->type(),
              EventMetrics::EventType::kInertialGestureScrollEnd);
    event->set_caused_frame_update(false);
    return event;
  }

  static base::TimeTicks MillisecondsTicks(int ms) {
    return base::TimeTicks() + base::Milliseconds(ms);
  }

  base::SimpleTestTickClock test_tick_clock_;
};

TEST_F(ScrollJankV4FrameStageTest, EmptyEventMetricsList) {
  EventMetrics::List events_metrics;
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest, FirstGestureScrollUpdateWhichDidScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(
      CreateFirstGestureScrollUpdate(MillisecondsTicks(16), 4, true));
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

TEST_F(ScrollJankV4FrameStageTest, FirstGestureScrollUpdateWhichDidNotScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(
      CreateFirstGestureScrollUpdate(MillisecondsTicks(16), 4, false));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  // Unlike continued GSUs (regular or inertial), scroll jank should always be
  // reported for FGSUs (even if they didn't cause a scroll).
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

TEST_F(ScrollJankV4FrameStageTest, GestureScrollUpdateWhichDidScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(
      CreateGestureScrollUpdate(MillisecondsTicks(16), 4, true));
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

TEST_F(ScrollJankV4FrameStageTest, GestureScrollUpdateWhichDidNotScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(
      CreateGestureScrollUpdate(MillisecondsTicks(16), 4, false));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest, InertialGestureScrollUpdateWhichDidScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(
      CreateInertialGestureScrollUpdate(MillisecondsTicks(16), 4, true));
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
       InertialGestureScrollUpdateWhichDidNotScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(
      CreateInertialGestureScrollUpdate(MillisecondsTicks(16), 4, false));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest, GestureScrollEnd) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(CreateGestureScrollEnd(MillisecondsTicks(16)));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, ElementsAre(ScrollJankV4FrameStage{ScrollEnd{}}));
}

TEST_F(ScrollJankV4FrameStageTest, InertialGestureScrollEnd) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(
      CreateInertialGestureScrollEnd(MillisecondsTicks(16)));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, ElementsAre(ScrollJankV4FrameStage{ScrollEnd{}}));
}

TEST_F(ScrollJankV4FrameStageTest, NonScrollEventType) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(
      CreateEventMetrics(MillisecondsTicks(16), ui::EventType::kMouseMoved));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest, MultipleScrollUpdates) {
  EventMetrics::List events_metrics;
  // Intentionally in "random" order to make sure that the calculation doesn't
  // rely on the list being sorted (because the list isn't sorted in general).
  events_metrics.push_back(
      CreateGestureScrollUpdate(MillisecondsTicks(4), -8'000, true));
  events_metrics.push_back(
      CreateGestureScrollUpdate(MillisecondsTicks(2), -32'000, true));
  events_metrics.push_back(
      CreateInertialGestureScrollUpdate(MillisecondsTicks(7), -1'000, false));
  events_metrics.push_back(
      CreateFirstGestureScrollUpdate(MillisecondsTicks(1), -64'000, true));
  events_metrics.push_back(
      CreateInertialGestureScrollUpdate(MillisecondsTicks(5), -4'000, true));
  events_metrics.push_back(
      CreateInertialGestureScrollUpdate(MillisecondsTicks(6), -2'000, true));
  events_metrics.push_back(
      CreateGestureScrollUpdate(MillisecondsTicks(3), -16'000, true));

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
       ScrollEndForPreviousScrollThenScrollUpdates) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(
      CreateGestureScrollUpdate(MillisecondsTicks(3), 40, true));
  events_metrics.push_back(CreateGestureScrollEnd(MillisecondsTicks(1)));
  events_metrics.push_back(
      CreateFirstGestureScrollUpdate(MillisecondsTicks(2), 6, true));
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
  events_metrics.push_back(
      CreateInertialGestureScrollUpdate(MillisecondsTicks(1), 40, true));
  events_metrics.push_back(CreateGestureScrollEnd(MillisecondsTicks(3)));
  events_metrics.push_back(
      CreateInertialGestureScrollUpdate(MillisecondsTicks(2), 6, true));
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
  events_metrics.push_back(
      CreateGestureScrollUpdate(MillisecondsTicks(1), 1, true));
  events_metrics[0]->set_caused_frame_update(false);
  events_metrics.push_back(
      CreateGestureScrollUpdate(MillisecondsTicks(2), 10, true));
  events_metrics.push_back(
      CreateInertialGestureScrollUpdate(MillisecondsTicks(3), 100, true));
  events_metrics.push_back(
      CreateInertialGestureScrollUpdate(MillisecondsTicks(4), 1000, true));
  events_metrics[3]->set_caused_frame_update(false);

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

}  // namespace cc
