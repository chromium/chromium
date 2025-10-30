// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_frame_stage.h"

#include <memory>
#include <optional>
#include <sstream>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
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
  std::unique_ptr<EventMetrics> CreateEventMetrics(base::TimeTicks timestamp,
                                                   ui::EventType type,
                                                   bool caused_frame_update) {
    auto event = EventMetrics::CreateForTesting(
        type, timestamp,
        /* arrived_in_browser_main_timestamp= */ timestamp +
            base::Nanoseconds(1),
        &test_tick_clock_, /* trace_id= */ std::nullopt);
    event->set_caused_frame_update(caused_frame_update);
    return event;
  }

  std::unique_ptr<ScrollEventMetrics> CreateScrollEventMetrics(
      base::TimeTicks timestamp,
      ui::EventType type,
      bool is_inertial,
      bool caused_frame_update,
      bool did_scroll) {
    auto event = ScrollEventMetrics::CreateForTesting(
        type, ui::ScrollInputType::kTouchscreen, is_inertial, timestamp,
        /* arrived_in_browser_main_timestamp= */ timestamp +
            base::Nanoseconds(1),
        &test_tick_clock_);
    event->set_caused_frame_update(caused_frame_update);
    event->set_did_scroll(did_scroll);
    return event;
  }

  std::unique_ptr<ScrollUpdateEventMetrics> CreateScrollUpdateEventMetrics(
      base::TimeTicks timestamp,
      ui::EventType type,
      bool is_inertial,
      ScrollUpdateEventMetrics::ScrollUpdateType scroll_update_type,
      float delta,
      bool caused_frame_update,
      bool did_scroll) {
    auto event = ScrollUpdateEventMetrics::CreateForTesting(
        type, ui::ScrollInputType::kTouchscreen, is_inertial,
        scroll_update_type, delta, timestamp,
        /* arrived_in_browser_main_timestamp= */ timestamp +
            base::Nanoseconds(1),
        &test_tick_clock_,
        /* trace_id= */ std::nullopt);
    event->set_caused_frame_update(caused_frame_update);
    event->set_did_scroll(did_scroll);
    return event;
  }

  std::unique_ptr<ScrollUpdateEventMetrics> CreateFirstGestureScrollUpdate(
      base::TimeTicks timestamp,
      float delta,
      bool caused_frame_update,
      bool did_scroll) {
    auto event = CreateScrollUpdateEventMetrics(
        timestamp, ui::EventType::kGestureScrollUpdate,
        /* is_inertial= */ false,
        ScrollUpdateEventMetrics::ScrollUpdateType::kStarted, delta,
        caused_frame_update, did_scroll);
    EXPECT_EQ(event->type(),
              EventMetrics::EventType::kFirstGestureScrollUpdate);
    return event;
  }

  std::unique_ptr<ScrollUpdateEventMetrics> CreateGestureScrollUpdate(
      base::TimeTicks timestamp,
      float delta,
      bool caused_frame_update,
      bool did_scroll) {
    auto event = CreateScrollUpdateEventMetrics(
        timestamp, ui::EventType::kGestureScrollUpdate,
        /* is_inertial= */ false,
        ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, delta,
        caused_frame_update, did_scroll);
    EXPECT_EQ(event->type(), EventMetrics::EventType::kGestureScrollUpdate);
    return event;
  }

  std::unique_ptr<ScrollUpdateEventMetrics> CreateInertialGestureScrollUpdate(
      base::TimeTicks timestamp,
      float delta,
      bool caused_frame_update,
      bool did_scroll) {
    auto event = CreateScrollUpdateEventMetrics(
        timestamp, ui::EventType::kGestureScrollUpdate, /* is_inertial= */ true,
        ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, delta,
        caused_frame_update, did_scroll);
    EXPECT_EQ(event->type(),
              EventMetrics::EventType::kInertialGestureScrollUpdate);
    return event;
  }

  std::unique_ptr<ScrollEventMetrics> CreateGestureScrollEnd(
      base::TimeTicks timestamp) {
    auto event = CreateScrollEventMetrics(
        timestamp, ui::EventType::kGestureScrollEnd,
        /* is_inertial= */ false, /* caused_frame_update= */ false,
        /* did_scroll= */ false);
    EXPECT_EQ(event->type(), EventMetrics::EventType::kGestureScrollEnd);
    return event;
  }

  std::unique_ptr<ScrollEventMetrics> CreateInertialGestureScrollEnd(
      base::TimeTicks timestamp) {
    auto event = CreateScrollEventMetrics(
        timestamp, ui::EventType::kGestureScrollEnd,
        /* is_inertial= */ true, /* caused_frame_update= */ false,
        /* did_scroll= */ false);
    EXPECT_EQ(event->type(),
              EventMetrics::EventType::kInertialGestureScrollEnd);
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

TEST_F(ScrollJankV4FrameStageTest,
       FirstGestureScrollUpdateWhichCausedFrameUpdateAndDidScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(CreateFirstGestureScrollUpdate(
      MillisecondsTicks(16), 4, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
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
  events_metrics.push_back(CreateFirstGestureScrollUpdate(
      MillisecondsTicks(16), 4, /* caused_frame_update= */ false,
      /* did_scroll= */ true));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest, FirstGestureScrollUpdateWhichDidNotScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(CreateFirstGestureScrollUpdate(
      MillisecondsTicks(16), 4, /* caused_frame_update= */ true,
      /* did_scroll= */ false));
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
  events_metrics.push_back(CreateFirstGestureScrollUpdate(
      MillisecondsTicks(16), 4, /* caused_frame_update= */ false,
      /* did_scroll= */ false));
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
  events_metrics.push_back(CreateGestureScrollUpdate(
      MillisecondsTicks(16), 4, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
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
  events_metrics.push_back(CreateGestureScrollUpdate(
      MillisecondsTicks(16), 4, /* caused_frame_update= */ false,
      /* did_scroll= */ true));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest, GestureScrollUpdateWhichDidNotScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(CreateGestureScrollUpdate(
      MillisecondsTicks(16), 4, /* caused_frame_update= */ true,
      /* did_scroll= */ false));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest,
       GestureScrollUpdateDoNotSkipNonDamagingEvents) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(CreateGestureScrollUpdate(
      MillisecondsTicks(16), 4, /* caused_frame_update= */ false,
      /* did_scroll= */ false));
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
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(16), 4, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
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
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(16), 4, /* caused_frame_update= */ false,
      /* did_scroll= */ true));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest,
       InertialGestureScrollUpdateWhichDidNotScroll) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(16), 4, /* caused_frame_update= */ true,
      /* did_scroll= */ false));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest,
       InertialGestureScrollUpdateDoNotSkipNonDamagingEvents) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(16), 4, /* caused_frame_update= */ false,
      /* did_scroll= */ false));
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
  events_metrics.push_back(CreateEventMetrics(MillisecondsTicks(16),
                                              ui::EventType::kMouseMoved,
                                              /* caused_frame_update= */ true));
  auto stages = ScrollJankV4FrameStage::CalculateStages(events_metrics);
  EXPECT_THAT(stages, IsEmpty());
}

TEST_F(ScrollJankV4FrameStageTest, MultipleScrollUpdates) {
  EventMetrics::List events_metrics;
  // Intentionally in "random" order to make sure that the calculation doesn't
  // rely on the list being sorted (because the list isn't sorted in general).
  events_metrics.push_back(CreateGestureScrollUpdate(
      MillisecondsTicks(4), -8'000, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
  events_metrics.push_back(CreateGestureScrollUpdate(
      MillisecondsTicks(2), -32'000, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(7), -1'000, /* caused_frame_update= */ true,
      /* did_scroll= */ false));
  events_metrics.push_back(CreateFirstGestureScrollUpdate(
      MillisecondsTicks(1), -64'000, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(5), -4'000, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(6), -2'000, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
  events_metrics.push_back(CreateGestureScrollUpdate(
      MillisecondsTicks(3), -16'000, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(8), -128'000, /* caused_frame_update= */ false,
      /* did_scroll= */ true));

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
  events_metrics.push_back(CreateGestureScrollUpdate(
      MillisecondsTicks(4), -8'000, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
  events_metrics.push_back(CreateGestureScrollUpdate(
      MillisecondsTicks(2), -32'000, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(7), -1'000, /* caused_frame_update= */ true,
      /* did_scroll= */ false));
  events_metrics.push_back(CreateFirstGestureScrollUpdate(
      MillisecondsTicks(1), -64'000, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(5), -4'000, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(6), -2'000, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
  events_metrics.push_back(CreateGestureScrollUpdate(
      MillisecondsTicks(3), -16'000, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(8), -128'000, /* caused_frame_update= */ false,
      /* did_scroll= */ true));

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
  events_metrics.push_back(CreateGestureScrollUpdate(
      MillisecondsTicks(3), 40, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
  events_metrics.push_back(CreateGestureScrollEnd(MillisecondsTicks(1)));
  events_metrics.push_back(CreateFirstGestureScrollUpdate(
      MillisecondsTicks(2), 6, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
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
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(1), 40, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
  events_metrics.push_back(CreateGestureScrollEnd(MillisecondsTicks(3)));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(2), 6, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
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
  events_metrics.push_back(CreateGestureScrollUpdate(
      MillisecondsTicks(1), 1, /* caused_frame_update= */ false,
      /* did_scroll= */ true));
  events_metrics.push_back(CreateGestureScrollUpdate(
      MillisecondsTicks(2), 10, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(3), 100, /* caused_frame_update= */ true,
      /* did_scroll= */ true));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(4), 1000, /* caused_frame_update= */ false,
      /* did_scroll= */ true));

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
      CreateGestureScrollUpdate(MillisecondsTicks(1), 2,
                                /* caused_frame_update= */ true,
                                /* did_scroll= */ true);
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
