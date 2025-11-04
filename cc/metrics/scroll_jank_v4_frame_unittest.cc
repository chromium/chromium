// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_frame.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <sstream>

#include "base/memory/raw_ref.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_frame_stage.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/events/types/event_type.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {

namespace {

using ScrollDamage = ScrollJankV4Frame::ScrollDamage;
using DamagingFrame = ScrollJankV4Frame::DamagingFrame;
using NonDamagingFrame = ScrollJankV4Frame::NonDamagingFrame;
using ScrollUpdates = ScrollJankV4FrameStage::ScrollUpdates;
using ScrollEnd = ScrollJankV4FrameStage::ScrollEnd;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::IsEmpty;
using ::testing::Matcher;

constexpr uint64_t kSourceId = 999;

// Matches a reference to begin frame arguments iff `reference->frame_id` equals
// `viz::BeginFrameId(kSourceId, begin_frame_sequence_id)`.
//
// We use this matcher together with `FieldsAre()` to work around the fact that
// `viz::BeginFrameArgs` doesn't have the equality operator defined.
Matcher<const base::raw_ref<const viz::BeginFrameArgs>&>
HasBeginFrameSequenceId(uint64_t begin_frame_sequence_id) {
  viz::BeginFrameId expected_frame_id =
      viz::BeginFrameId(kSourceId, begin_frame_sequence_id);
  return ::testing::Property(
      &base::raw_ref<const viz::BeginFrameArgs>::get,
      ::testing::Field(&viz::BeginFrameArgs::frame_id, expected_frame_id));
}

// Matches a frame iff all of the following are true:
//
//   1. `frame.args->frame_id` equals
//      `viz::BeginFrameId(kSourceId, begin_frame_sequence_id)`.
//   2. `damage` matches `frame.damage`.
//   3. `stages` matches `frame.stages`.
//
// We use this matcher (instead of simple equality) to work around the fact that
// `viz::BeginFrameArgs` doesn't have the equality operator defined.
Matcher<const ScrollJankV4Frame&> ExpectedFrame(
    uint64_t begin_frame_sequence_id,
    Matcher<ScrollDamage> damage,
    Matcher<const ScrollJankV4FrameStage::List&> stages) {
  return FieldsAre(HasBeginFrameSequenceId(begin_frame_sequence_id), damage,
                   stages);
}

}  // namespace

class ScrollJankV4FrameTest : public testing::Test {
 public:
  ScrollJankV4FrameTest() = default;
  ~ScrollJankV4FrameTest() override = default;

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
      bool did_scroll,
      const viz::BeginFrameArgs& args) {
    auto event = ScrollEventMetrics::CreateForTesting(
        type, ui::ScrollInputType::kTouchscreen, is_inertial, timestamp,
        /* arrived_in_browser_main_timestamp= */ timestamp +
            base::Nanoseconds(1),
        &test_tick_clock_);
    event->set_caused_frame_update(caused_frame_update);
    event->set_did_scroll(did_scroll);
    event->set_begin_frame_args(args);
    return event;
  }

  std::unique_ptr<ScrollUpdateEventMetrics> CreateScrollUpdateEventMetrics(
      base::TimeTicks timestamp,
      ui::EventType type,
      bool is_inertial,
      ScrollUpdateEventMetrics::ScrollUpdateType scroll_update_type,
      float delta,
      bool caused_frame_update,
      bool did_scroll,
      const viz::BeginFrameArgs& args) {
    auto event = ScrollUpdateEventMetrics::CreateForTesting(
        type, ui::ScrollInputType::kTouchscreen, is_inertial,
        scroll_update_type, delta, timestamp,
        /* arrived_in_browser_main_timestamp= */ timestamp +
            base::Nanoseconds(1),
        &test_tick_clock_,
        /* trace_id= */ std::nullopt);
    event->set_caused_frame_update(caused_frame_update);
    event->set_did_scroll(did_scroll);
    event->set_begin_frame_args(args);
    return event;
  }

  std::unique_ptr<ScrollUpdateEventMetrics> CreateFirstGestureScrollUpdate(
      base::TimeTicks timestamp,
      float delta,
      bool caused_frame_update,
      bool did_scroll,
      const viz::BeginFrameArgs& args) {
    auto event = CreateScrollUpdateEventMetrics(
        timestamp, ui::EventType::kGestureScrollUpdate,
        /* is_inertial= */ false,
        ScrollUpdateEventMetrics::ScrollUpdateType::kStarted, delta,
        caused_frame_update, did_scroll, args);
    EXPECT_EQ(event->type(),
              EventMetrics::EventType::kFirstGestureScrollUpdate);
    return event;
  }

  std::unique_ptr<ScrollUpdateEventMetrics> CreateGestureScrollUpdate(
      base::TimeTicks timestamp,
      float delta,
      bool caused_frame_update,
      bool did_scroll,
      const viz::BeginFrameArgs& args) {
    auto event = CreateScrollUpdateEventMetrics(
        timestamp, ui::EventType::kGestureScrollUpdate,
        /* is_inertial= */ false,
        ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, delta,
        caused_frame_update, did_scroll, args);
    EXPECT_EQ(event->type(), EventMetrics::EventType::kGestureScrollUpdate);
    return event;
  }

  std::unique_ptr<ScrollUpdateEventMetrics> CreateInertialGestureScrollUpdate(
      base::TimeTicks timestamp,
      float delta,
      bool caused_frame_update,
      bool did_scroll,
      const viz::BeginFrameArgs& args) {
    auto event = CreateScrollUpdateEventMetrics(
        timestamp, ui::EventType::kGestureScrollUpdate, /* is_inertial= */ true,
        ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, delta,
        caused_frame_update, did_scroll, args);
    EXPECT_EQ(event->type(),
              EventMetrics::EventType::kInertialGestureScrollUpdate);
    return event;
  }

  std::unique_ptr<ScrollEventMetrics> CreateGestureScrollBegin(
      base::TimeTicks timestamp,
      const viz::BeginFrameArgs& args) {
    auto event = CreateScrollEventMetrics(
        timestamp, ui::EventType::kGestureScrollBegin,
        /* is_inertial= */ false, /* caused_frame_update= */ false,
        /* did_scroll= */ false, args);
    EXPECT_EQ(event->type(), EventMetrics::EventType::kGestureScrollBegin);
    return event;
  }

  std::unique_ptr<ScrollEventMetrics> CreateGestureScrollEnd(
      base::TimeTicks timestamp,
      const viz::BeginFrameArgs& args) {
    auto event = CreateScrollEventMetrics(
        timestamp, ui::EventType::kGestureScrollEnd,
        /* is_inertial= */ false, /* caused_frame_update= */ false,
        /* did_scroll= */ false, args);
    EXPECT_EQ(event->type(), EventMetrics::EventType::kGestureScrollEnd);
    return event;
  }

  std::unique_ptr<ScrollEventMetrics> CreateInertialGestureScrollEnd(
      base::TimeTicks timestamp,
      const viz::BeginFrameArgs& args) {
    auto event = CreateScrollEventMetrics(
        timestamp, ui::EventType::kGestureScrollEnd,
        /* is_inertial= */ true, /* caused_frame_update= */ false,
        /* did_scroll= */ false, args);
    EXPECT_EQ(event->type(),
              EventMetrics::EventType::kInertialGestureScrollEnd);
    return event;
  }

  static base::TimeTicks MillisecondsTicks(int ms) {
    return base::TimeTicks() + base::Milliseconds(ms);
  }

  static viz::BeginFrameArgs CreateBeginFrameArgs(int sequence_id) {
    return viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, kSourceId, sequence_id,
        /* frame_time= */ base::TimeTicks() + base::Milliseconds(123450),
        /* deadline= */ base::TimeTicks() + base::Milliseconds(123456),
        /* interval= */ base::Milliseconds(16),
        viz::BeginFrameArgs::BeginFrameArgsType::NORMAL);
  }

  base::SimpleTestTickClock test_tick_clock_;
};

// Note: With the exception of `IgnoreNonScrollEvents`, the test cases below are
// named based on the expected OUTPUTS.

TEST_F(ScrollJankV4FrameTest, NoFrames) {
  EventMetrics::List events_metrics;
  viz::BeginFrameArgs presented_args = CreateBeginFrameArgs(42);
  auto timeline = ScrollJankV4Frame::CalculateTimeline(
      events_metrics, presented_args,
      /* presentation_ts= */ MillisecondsTicks(777));
  EXPECT_THAT(timeline, IsEmpty());
}

TEST_F(ScrollJankV4FrameTest, IgnoreNonScrollEvents) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(
      CreateEventMetrics(MillisecondsTicks(10), ui::EventType::kTouchMoved,
                         /* caused_frame_update= */ false));
  events_metrics.push_back(CreateEventMetrics(MillisecondsTicks(11),
                                              ui::EventType::kTouchReleased,
                                              /* caused_frame_update= */ true));
  viz::BeginFrameArgs presented_args = CreateBeginFrameArgs(42);
  auto timeline = ScrollJankV4Frame::CalculateTimeline(
      events_metrics, presented_args,
      /* presentation_ts= */ MillisecondsTicks(777));
  EXPECT_THAT(timeline, IsEmpty());
}

TEST_F(ScrollJankV4FrameTest, OneNonDamagingFrame) {
  viz::BeginFrameArgs args = CreateBeginFrameArgs(31);
  EventMetrics::List events_metrics;
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(10), /* delta= */ 1.0f,
      /* caused_frame_update= */ false,
      /* did_scroll= */ false, args));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(11), /* delta= */ 2.0f, /* caused_frame_update= */ true,
      /* did_scroll= */ false, args));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(12), /* delta= */ 3.0f,
      /* caused_frame_update= */ false,
      /* did_scroll= */ true, args));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(13), /* delta= */ 4.0f,
      /* caused_frame_update= */ false,
      /* did_scroll= */ false, args));
  viz::BeginFrameArgs presented_args = CreateBeginFrameArgs(42);
  auto timeline = ScrollJankV4Frame::CalculateTimeline(
      events_metrics, presented_args,
      /* presentation_ts= */ MillisecondsTicks(777));
  EXPECT_THAT(
      timeline,
      ElementsAre(ExpectedFrame(
          /* begin_frame_sequence_id= */ 31, ScrollDamage{NonDamagingFrame{}},
          ElementsAre(ScrollJankV4FrameStage{ScrollUpdates{
              .is_scroll_start = false,
              .earliest_event = base::raw_ref(
                  static_cast<ScrollUpdateEventMetrics&>(*events_metrics[0])),
              .last_input_generation_ts = MillisecondsTicks(13),
              .has_inertial_input = true,
              .total_raw_delta_pixels = 10.0f,
              .max_abs_inertial_raw_delta_pixels = 4.0f,
          }}))));
}

TEST_F(ScrollJankV4FrameTest, MultipleNonDamagingFrames) {
  viz::BeginFrameArgs args1 = CreateBeginFrameArgs(31);
  viz::BeginFrameArgs args2 = CreateBeginFrameArgs(32);
  viz::BeginFrameArgs args3 = CreateBeginFrameArgs(33);
  EventMetrics::List events_metrics;

  events_metrics.push_back(
      CreateFirstGestureScrollUpdate(MillisecondsTicks(10), /* delta= */ 1.0f,
                                     /* caused_frame_update= */ false,
                                     /* did_scroll= */ false, args1));
  events_metrics.push_back(
      CreateGestureScrollUpdate(MillisecondsTicks(11), /* delta= */ 2.0f,
                                /* caused_frame_update= */ false,
                                /* did_scroll= */ false, args1));

  events_metrics.push_back(
      CreateGestureScrollUpdate(MillisecondsTicks(12), /* delta= */ 10.0f,
                                /* caused_frame_update= */ false,
                                /* did_scroll= */ true, args2));
  events_metrics.push_back(
      CreateGestureScrollUpdate(MillisecondsTicks(13), /* delta= */ 20.0f,
                                /* caused_frame_update= */ false,
                                /* did_scroll= */ true, args2));

  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(14), /* delta= */ 100.0f,
      /* caused_frame_update= */ true,
      /* did_scroll= */ false, args3));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(15), /* delta= */ 200.0f,
      /* caused_frame_update= */ true,
      /* did_scroll= */ false, args3));

  viz::BeginFrameArgs presented_args = CreateBeginFrameArgs(42);
  auto timeline = ScrollJankV4Frame::CalculateTimeline(
      events_metrics, presented_args,
      /* presentation_ts= */ MillisecondsTicks(777));
  EXPECT_THAT(
      timeline,
      ElementsAre(
          ExpectedFrame(
              /* begin_frame_sequence_id= */ 31,
              ScrollDamage{NonDamagingFrame{}},
              ElementsAre(ScrollJankV4FrameStage{ScrollUpdates{
                  .is_scroll_start = true,
                  .earliest_event =
                      base::raw_ref(static_cast<ScrollUpdateEventMetrics&>(
                          *events_metrics[0])),
                  .last_input_generation_ts = MillisecondsTicks(11),
                  .has_inertial_input = false,
                  .total_raw_delta_pixels = 3.0f,
                  .max_abs_inertial_raw_delta_pixels = 0.0f,
              }})),
          ExpectedFrame(
              /* begin_frame_sequence_id= */ 32,
              ScrollDamage{NonDamagingFrame{}},
              ElementsAre(ScrollJankV4FrameStage{ScrollUpdates{
                  .is_scroll_start = false,
                  .earliest_event =
                      base::raw_ref(static_cast<ScrollUpdateEventMetrics&>(
                          *events_metrics[2])),
                  .last_input_generation_ts = MillisecondsTicks(13),
                  .has_inertial_input = false,
                  .total_raw_delta_pixels = 30.0f,
                  .max_abs_inertial_raw_delta_pixels = 0.0f,
              }})),
          ExpectedFrame(
              /* begin_frame_sequence_id= */ 33,
              ScrollDamage{NonDamagingFrame{}},
              ElementsAre(ScrollJankV4FrameStage{ScrollUpdates{
                  .is_scroll_start = false,
                  .earliest_event =
                      base::raw_ref(static_cast<ScrollUpdateEventMetrics&>(
                          *events_metrics[4])),
                  .last_input_generation_ts = MillisecondsTicks(15),
                  .has_inertial_input = true,
                  .total_raw_delta_pixels = 300.0f,
                  .max_abs_inertial_raw_delta_pixels = 200.0f,
              }}))));
}

TEST_F(ScrollJankV4FrameTest, OneDamagingFrame) {
  viz::BeginFrameArgs args1 = CreateBeginFrameArgs(31);
  viz::BeginFrameArgs args2 = CreateBeginFrameArgs(31);
  viz::BeginFrameArgs args3 = CreateBeginFrameArgs(32);
  EventMetrics::List events_metrics;

  events_metrics.push_back(
      CreateFirstGestureScrollUpdate(MillisecondsTicks(10), /* delta= */ 1.0f,
                                     /* caused_frame_update= */ false,
                                     /* did_scroll= */ false, args1));
  // events_metrics[1] below is the single damaging input which causes all
  // events to be associated with the presented frame.
  events_metrics.push_back(
      CreateGestureScrollUpdate(MillisecondsTicks(11), /* delta= */ 2.0f,
                                /* caused_frame_update= */ true,
                                /* did_scroll= */ true, args1));

  events_metrics.push_back(
      CreateGestureScrollUpdate(MillisecondsTicks(12), /* delta= */ 10.0f,
                                /* caused_frame_update= */ false,
                                /* did_scroll= */ true, args2));
  events_metrics.push_back(
      CreateGestureScrollUpdate(MillisecondsTicks(13), /* delta= */ 20.0f,
                                /* caused_frame_update= */ false,
                                /* did_scroll= */ true, args2));

  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(14), /* delta= */ 100.0f,
      /* caused_frame_update= */ true,
      /* did_scroll= */ false, args3));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(15), /* delta= */ 200.0f,
      /* caused_frame_update= */ true,
      /* did_scroll= */ false, args3));

  viz::BeginFrameArgs presented_args = CreateBeginFrameArgs(42);
  auto timeline = ScrollJankV4Frame::CalculateTimeline(
      events_metrics, presented_args,
      /* presentation_ts= */ MillisecondsTicks(777));
  EXPECT_THAT(
      timeline,
      ElementsAre(ExpectedFrame(
          /* begin_frame_sequence_id= */ 42,
          ScrollDamage{
              DamagingFrame{.presentation_ts = MillisecondsTicks(777)}},
          ElementsAre(ScrollJankV4FrameStage{ScrollUpdates{
              .is_scroll_start = true,
              .earliest_event = base::raw_ref(
                  static_cast<ScrollUpdateEventMetrics&>(*events_metrics[0])),
              .last_input_generation_ts = MillisecondsTicks(15),
              .has_inertial_input = true,
              .total_raw_delta_pixels = 333.0f,
              .max_abs_inertial_raw_delta_pixels = 200.0f,
          }}))));
}

// Example from `ScrollJankV4Frame::Timeline CalculateTimeline()`'s
// documentation.
TEST_F(ScrollJankV4FrameTest, MultipleNonDamagingFramesAndOneDamagingFrame) {
  viz::BeginFrameArgs args1 = CreateBeginFrameArgs(31);
  viz::BeginFrameArgs args2 = CreateBeginFrameArgs(32);
  viz::BeginFrameArgs args3 = CreateBeginFrameArgs(33);
  viz::BeginFrameArgs args4 = CreateBeginFrameArgs(34);
  viz::BeginFrameArgs args5 = CreateBeginFrameArgs(35);
  EventMetrics::List events_metrics;

  // 1. Non-damaging GSB for BFA1
  // 2. Non-damaging GSU for BFA1
  // 3. Non-damaging GSU for BFA1
  events_metrics.push_back(
      CreateGestureScrollBegin(MillisecondsTicks(10), args1));
  events_metrics.push_back(
      CreateFirstGestureScrollUpdate(MillisecondsTicks(11), /* delta= */ 1.0f,
                                     /* caused_frame_update= */ false,
                                     /* did_scroll= */ false, args1));
  events_metrics.push_back(
      CreateGestureScrollUpdate(MillisecondsTicks(12), /* delta= */ 2.0f,
                                /* caused_frame_update= */ false,
                                /* did_scroll= */ false, args1));

  // 4. Non-damaging GSE for BFA2
  events_metrics.push_back(
      CreateGestureScrollEnd(MillisecondsTicks(13), args2));

  // 5. Non-damaging GSU for BFA3
  // 6. Damaging GSU for BFA3
  events_metrics.push_back(
      CreateGestureScrollUpdate(MillisecondsTicks(14), /* delta= */ 10.0f,
                                /* caused_frame_update= */ false,
                                /* did_scroll= */ false, args3));
  events_metrics.push_back(
      CreateGestureScrollUpdate(MillisecondsTicks(15), /* delta= */ 20.0f,
                                /* caused_frame_update= */ true,
                                /* did_scroll= */ true, args3));

  // 7. Non-damaging GSU for BFA4
  // 8. Non-damaging GSU for BFA4
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(16), /* delta= */ 100.0f,
      /* caused_frame_update= */ false,
      /* did_scroll= */ false, args4));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(17), /* delta= */ 200.0f,
      /* caused_frame_update= */ false,
      /* did_scroll= */ false, args4));

  // 9. Damaging GSU for BFA5
  // 10. Non-damaging GSU for BFA5
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(18), /* delta= */ 1000.0f,
      /* caused_frame_update= */ true,
      /* did_scroll= */ true, args5));
  events_metrics.push_back(CreateInertialGestureScrollUpdate(
      MillisecondsTicks(19), /* delta= */ 2000.0f,
      /* caused_frame_update= */ false,
      /* did_scroll= */ false, args5));

  viz::BeginFrameArgs presented_args = CreateBeginFrameArgs(42);
  auto timeline = ScrollJankV4Frame::CalculateTimeline(
      events_metrics, presented_args,
      /* presentation_ts= */ MillisecondsTicks(777));
  EXPECT_THAT(
      timeline,
      ElementsAre(
          ExpectedFrame(
              /* begin_frame_sequence_id= */ 31,
              ScrollDamage{NonDamagingFrame{}},
              ElementsAre(ScrollJankV4FrameStage{ScrollUpdates{
                  .is_scroll_start = true,
                  .earliest_event =
                      base::raw_ref(static_cast<ScrollUpdateEventMetrics&>(
                          *events_metrics[1])),
                  .last_input_generation_ts = MillisecondsTicks(12),
                  .has_inertial_input = false,
                  .total_raw_delta_pixels = 3.0f,
                  .max_abs_inertial_raw_delta_pixels = 0.0f,
              }})),
          ExpectedFrame(
              /* begin_frame_sequence_id= */ 32,
              ScrollDamage{NonDamagingFrame{}},
              ElementsAre(ScrollJankV4FrameStage{ScrollEnd{}})),
          ExpectedFrame(
              /* begin_frame_sequence_id= */ 42,
              ScrollDamage{
                  DamagingFrame{.presentation_ts = MillisecondsTicks(777)}},
              ElementsAre(ScrollJankV4FrameStage{ScrollUpdates{
                  .is_scroll_start = false,
                  .earliest_event =
                      base::raw_ref(static_cast<ScrollUpdateEventMetrics&>(
                          *events_metrics[4])),
                  .last_input_generation_ts = MillisecondsTicks(19),
                  .has_inertial_input = true,
                  .total_raw_delta_pixels = 3330.0f,
                  .max_abs_inertial_raw_delta_pixels = 2000.0f,
              }}))));
}

TEST_F(ScrollJankV4FrameTest, DamagingFrameToOstream) {
  std::ostringstream out;
  auto& result =
      out << DamagingFrame{.presentation_ts = MillisecondsTicks(777)};
  EXPECT_THAT(out.str(), ::testing::MatchesRegex(R"(DamagingFrame\{.+\})"));
  EXPECT_EQ(&result, &out);
}

TEST_F(ScrollJankV4FrameTest, NonDamagingFrameToOstream) {
  std::ostringstream out;
  auto& result = out << NonDamagingFrame{};
  EXPECT_EQ(out.str(), "NonDamagingFrame{}");
  EXPECT_EQ(&result, &out);
}

TEST_F(ScrollJankV4FrameTest, ScrollDamageToOstream) {
  std::ostringstream out;
  auto& result = out << ScrollDamage{NonDamagingFrame{}};
  EXPECT_THAT(out.str(), ::testing::MatchesRegex(R"(ScrollDamage\{.+\})"));
  EXPECT_EQ(&result, &out);
}

TEST_F(ScrollJankV4FrameTest, ScrollJankV4FrameToOstream) {
  viz::BeginFrameArgs args = CreateBeginFrameArgs(42);
  auto frame = ScrollJankV4Frame(
      base::raw_ref<const viz::BeginFrameArgs>::from_ptr(&args),
      DamagingFrame{.presentation_ts = MillisecondsTicks(777)},
      {ScrollJankV4FrameStage{ScrollEnd{}}, ScrollJankV4FrameStage{ScrollEnd{}},
       ScrollJankV4FrameStage{ScrollEnd{}}});

  std::ostringstream out;
  auto& result = out << frame;
  EXPECT_THAT(out.str(), ::testing::MatchesRegex(R"(ScrollJankV4Frame\{.+\})"));
  EXPECT_EQ(&result, &out);
}

}  // namespace cc
