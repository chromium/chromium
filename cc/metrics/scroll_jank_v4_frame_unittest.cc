// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_frame.h"

#include <cstdint>
#include <memory>
#include <sstream>

#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_frame_stage.h"
#include "cc/test/event_metrics_test_creator.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/events/types/event_type.h"

namespace cc {

namespace {

using BeginFrameArgsForScrollJank =
    ScrollJankV4Frame::BeginFrameArgsForScrollJank;
using ScrollDamage = ScrollJankV4Frame::ScrollDamage;
using DamagingFrame = ScrollJankV4Frame::DamagingFrame;
using NonDamagingFrame = ScrollJankV4Frame::NonDamagingFrame;
using ScrollUpdates = ScrollJankV4FrameStage::ScrollUpdates;
using ScrollStart = ScrollJankV4FrameStage::ScrollStart;
using ScrollEnd = ScrollJankV4FrameStage::ScrollEnd;
using Real = ScrollUpdates::Real;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr uint64_t kSourceId = 999;
constexpr base::TimeDelta kVsyncInterval = base::Milliseconds(16);

}  // namespace

class ScrollJankV4FrameTest : public testing::Test {
 public:
  ScrollJankV4FrameTest() = default;
  ~ScrollJankV4FrameTest() override = default;

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

TEST_F(ScrollJankV4FrameTest, BeginFrameArgsForScrollJankFrom) {
  viz::BeginFrameArgs args = CreateBeginFrameArgs(42, MillisecondsTicks(123));
  BeginFrameArgsForScrollJank args_for_scroll_jank =
      BeginFrameArgsForScrollJank::From(args);
  EXPECT_EQ(args_for_scroll_jank, (BeginFrameArgsForScrollJank{
                                      .frame_time = MillisecondsTicks(123),
                                      .interval = kVsyncInterval,
                                  }));
}

TEST_F(ScrollJankV4FrameTest, NoFrames) {
  EventMetrics::List events_metrics;
  viz::BeginFrameArgs presented_args =
      CreateBeginFrameArgs(42, MillisecondsTicks(666));
  auto timeline = ScrollJankV4Frame::CalculateTimeline(
      events_metrics, presented_args,
      /* presentation_ts= */ MillisecondsTicks(777));
  EXPECT_THAT(timeline, IsEmpty());
}

TEST_F(ScrollJankV4FrameTest, IgnoreNonScrollEvents) {
  EventMetrics::List events_metrics;
  events_metrics.push_back(
      metrics_creator_.CreateEventMetrics({.type = ui::EventType::kTouchMoved,
                                           .timestamp = MillisecondsTicks(10),
                                           .caused_frame_update = false}));
  events_metrics.push_back(metrics_creator_.CreateEventMetrics(
      {.type = ui::EventType::kTouchReleased,
       .timestamp = MillisecondsTicks(11),
       .caused_frame_update = true}));
  viz::BeginFrameArgs presented_args =
      CreateBeginFrameArgs(42, MillisecondsTicks(666));
  auto timeline = ScrollJankV4Frame::CalculateTimeline(
      events_metrics, presented_args,
      /* presentation_ts= */ MillisecondsTicks(777));
  EXPECT_THAT(timeline, IsEmpty());
}

TEST_F(ScrollJankV4FrameTest, OneNonDamagingFrame) {
  viz::BeginFrameArgs args = CreateBeginFrameArgs(31, MillisecondsTicks(111));
  EventMetrics::List events_metrics;
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(10),
       .delta = 1.0f,
       .caused_frame_update = false,
       .did_scroll = false,
       .begin_frame_args = args}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(11),
       .delta = 2.0f,
       .caused_frame_update = true,
       .did_scroll = false,
       .begin_frame_args = args}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(12),
       .delta = 3.0f,
       .caused_frame_update = false,
       .did_scroll = true,
       .begin_frame_args = args}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(13),
       .delta = 4.0f,
       .caused_frame_update = false,
       .did_scroll = false,
       .begin_frame_args = args}));
  viz::BeginFrameArgs presented_args =
      CreateBeginFrameArgs(42, MillisecondsTicks(666));
  auto timeline = ScrollJankV4Frame::CalculateTimeline(
      events_metrics, presented_args,
      /* presentation_ts= */ MillisecondsTicks(777));
  EXPECT_THAT(
      timeline,
      ElementsAre(ScrollJankV4Frame(
          BeginFrameArgsForScrollJank{.frame_time = MillisecondsTicks(111),
                                      .interval = kVsyncInterval},
          NonDamagingFrame{},
          {ScrollJankV4FrameStage{ScrollUpdates(
              static_cast<ScrollUpdateEventMetrics*>(events_metrics[0].get()),
              Real{
                  .first_input_generation_ts = MillisecondsTicks(10),
                  .last_input_generation_ts = MillisecondsTicks(13),
                  .has_inertial_input = true,
                  .abs_total_raw_delta_pixels = 10.0f,
                  .max_abs_inertial_raw_delta_pixels = 4.0f,
              },
              /* synthetic= */ std::nullopt)}})));
}

TEST_F(ScrollJankV4FrameTest, MultipleNonDamagingFrames) {
  viz::BeginFrameArgs args1 = CreateBeginFrameArgs(31, MillisecondsTicks(111));
  viz::BeginFrameArgs args2 = CreateBeginFrameArgs(32, MillisecondsTicks(222));
  viz::BeginFrameArgs args3 = CreateBeginFrameArgs(33, MillisecondsTicks(333));
  EventMetrics::List events_metrics;

  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(10),
       .delta = 1.0f,
       .caused_frame_update = false,
       .did_scroll = false,
       .begin_frame_args = args1}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(11),
       .delta = 2.0f,
       .caused_frame_update = false,
       .did_scroll = false,
       .begin_frame_args = args1}));

  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(12),
       .delta = 10.0f,
       .caused_frame_update = false,
       .did_scroll = true,
       .begin_frame_args = args2}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(13),
       .delta = 20.0f,
       .caused_frame_update = false,
       .did_scroll = true,
       .begin_frame_args = args2}));

  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(14),
       .delta = 100.0f,
       .caused_frame_update = true,
       .did_scroll = false,
       .begin_frame_args = args3}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(15),
       .delta = 200.0f,
       .caused_frame_update = true,
       .did_scroll = false,
       .begin_frame_args = args3}));

  viz::BeginFrameArgs presented_args =
      CreateBeginFrameArgs(42, MillisecondsTicks(666));
  auto timeline = ScrollJankV4Frame::CalculateTimeline(
      events_metrics, presented_args,
      /* presentation_ts= */ MillisecondsTicks(777));
  EXPECT_THAT(
      timeline,
      ElementsAre(
          ScrollJankV4Frame(
              BeginFrameArgsForScrollJank{.frame_time = MillisecondsTicks(111),
                                          .interval = kVsyncInterval},
              NonDamagingFrame{},
              {ScrollJankV4FrameStage{ScrollStart{}},
               ScrollJankV4FrameStage{ScrollUpdates(
                   static_cast<ScrollUpdateEventMetrics*>(
                       events_metrics[0].get()),
                   Real{
                       .first_input_generation_ts = MillisecondsTicks(10),
                       .last_input_generation_ts = MillisecondsTicks(11),
                       .has_inertial_input = false,
                       .abs_total_raw_delta_pixels = 3.0f,
                       .max_abs_inertial_raw_delta_pixels = 0.0f,
                   },
                   /* synthetic= */ std::nullopt)}}),
          ScrollJankV4Frame(
              BeginFrameArgsForScrollJank{.frame_time = MillisecondsTicks(222),
                                          .interval = kVsyncInterval},
              NonDamagingFrame{},
              {ScrollJankV4FrameStage{ScrollUpdates(
                  static_cast<ScrollUpdateEventMetrics*>(
                      events_metrics[2].get()),
                  Real{
                      .first_input_generation_ts = MillisecondsTicks(12),
                      .last_input_generation_ts = MillisecondsTicks(13),
                      .has_inertial_input = false,
                      .abs_total_raw_delta_pixels = 30.0f,
                      .max_abs_inertial_raw_delta_pixels = 0.0f,
                  },
                  /* synthetic= */ std::nullopt)}}),
          ScrollJankV4Frame(
              BeginFrameArgsForScrollJank{.frame_time = MillisecondsTicks(333),
                                          .interval = kVsyncInterval},
              NonDamagingFrame{},
              {ScrollJankV4FrameStage{ScrollUpdates(
                  static_cast<ScrollUpdateEventMetrics*>(
                      events_metrics[4].get()),
                  Real{
                      .first_input_generation_ts = MillisecondsTicks(14),
                      .last_input_generation_ts = MillisecondsTicks(15),
                      .has_inertial_input = true,
                      .abs_total_raw_delta_pixels = 300.0f,
                      .max_abs_inertial_raw_delta_pixels = 200.0f,
                  },
                  /* synthetic= */ std::nullopt)}})));
}

TEST_F(ScrollJankV4FrameTest, OneDamagingFrame) {
  viz::BeginFrameArgs args1 = CreateBeginFrameArgs(31, MillisecondsTicks(111));
  viz::BeginFrameArgs args2 = CreateBeginFrameArgs(32, MillisecondsTicks(222));
  viz::BeginFrameArgs args3 = CreateBeginFrameArgs(33, MillisecondsTicks(333));
  EventMetrics::List events_metrics;

  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(10),
       .delta = 1.0f,
       .caused_frame_update = false,
       .did_scroll = false,
       .begin_frame_args = args1}));
  // events_metrics[1] below is the single damaging input which causes all
  // events to be associated with the presented frame.
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(11),
       .delta = 2.0f,
       .caused_frame_update = true,
       .did_scroll = true,
       .begin_frame_args = args1}));

  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(12),
       .delta = 10.0f,
       .caused_frame_update = false,
       .did_scroll = true,
       .begin_frame_args = args2}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(13),
       .delta = 20.0f,
       .caused_frame_update = false,
       .did_scroll = true,
       .begin_frame_args = args2}));

  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(14),
       .delta = 100.0f,
       .caused_frame_update = true,
       .did_scroll = false,
       .begin_frame_args = args3}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(15),
       .delta = 200.0f,
       .caused_frame_update = true,
       .did_scroll = false,
       .begin_frame_args = args3}));

  viz::BeginFrameArgs presented_args =
      CreateBeginFrameArgs(42, MillisecondsTicks(666));
  auto timeline = ScrollJankV4Frame::CalculateTimeline(
      events_metrics, presented_args,
      /* presentation_ts= */ MillisecondsTicks(777));
  EXPECT_THAT(
      timeline,
      ElementsAre(ScrollJankV4Frame(
          BeginFrameArgsForScrollJank{.frame_time = MillisecondsTicks(666),
                                      .interval = kVsyncInterval},

          DamagingFrame{.presentation_ts = MillisecondsTicks(777)},
          {ScrollJankV4FrameStage{ScrollStart{}},
           ScrollJankV4FrameStage{ScrollUpdates(
               static_cast<ScrollUpdateEventMetrics*>(events_metrics[0].get()),
               Real{
                   .first_input_generation_ts = MillisecondsTicks(10),
                   .last_input_generation_ts = MillisecondsTicks(15),
                   .has_inertial_input = true,
                   .abs_total_raw_delta_pixels = 333.0f,
                   .max_abs_inertial_raw_delta_pixels = 200.0f,
               },
               /* synthetic= */ std::nullopt)}})));
}

// Example from `ScrollJankV4Frame::Timeline CalculateTimeline()`'s
// documentation.
TEST_F(ScrollJankV4FrameTest, MultipleNonDamagingFramesAndOneDamagingFrame) {
  viz::BeginFrameArgs args1 = CreateBeginFrameArgs(31, MillisecondsTicks(111));
  viz::BeginFrameArgs args2 = CreateBeginFrameArgs(32, MillisecondsTicks(222));
  viz::BeginFrameArgs args3 = CreateBeginFrameArgs(33, MillisecondsTicks(333));
  viz::BeginFrameArgs args4 = CreateBeginFrameArgs(34, MillisecondsTicks(444));
  viz::BeginFrameArgs args5 = CreateBeginFrameArgs(35, MillisecondsTicks(555));
  EventMetrics::List events_metrics;

  // 1. Non-damaging GSB for BFA1
  // 2. Non-damaging GSU for BFA1
  // 3. Non-damaging GSU for BFA1
  events_metrics.push_back(metrics_creator_.CreateGestureScrollBegin(
      {.timestamp = MillisecondsTicks(10),
       .caused_frame_update = false,
       .begin_frame_args = args1}));
  events_metrics.push_back(metrics_creator_.CreateFirstGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(11),
       .delta = 1.0f,
       .caused_frame_update = false,
       .did_scroll = false,
       .begin_frame_args = args1}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(12),
       .delta = 2.0f,
       .caused_frame_update = false,
       .did_scroll = false,
       .begin_frame_args = args1}));

  // 4. Non-damaging GSE for BFA2
  events_metrics.push_back(metrics_creator_.CreateGestureScrollEnd(
      {.timestamp = MillisecondsTicks(13),
       .caused_frame_update = false,
       .begin_frame_args = args2}));

  // 5. Non-damaging GSU for BFA3
  // 6. Damaging GSU for BFA3
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(14),
       .delta = 10.0f,
       .caused_frame_update = false,
       .did_scroll = false,
       .begin_frame_args = args3}));
  events_metrics.push_back(metrics_creator_.CreateGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(15),
       .delta = 20.0f,
       .caused_frame_update = true,
       .did_scroll = true,
       .begin_frame_args = args3}));

  // 7. Non-damaging GSU for BFA4
  // 8. Non-damaging GSU for BFA4
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(16),
       .delta = 100.0f,
       .caused_frame_update = false,
       .did_scroll = false,
       .begin_frame_args = args4}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(17),
       .delta = 200.0f,
       .caused_frame_update = false,
       .did_scroll = false,
       .begin_frame_args = args4}));

  // 9. Damaging GSU for BFA5
  // 10. Non-damaging GSU for BFA5
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(18),
       .delta = 1000.0f,
       .caused_frame_update = true,
       .did_scroll = true,
       .begin_frame_args = args5}));
  events_metrics.push_back(metrics_creator_.CreateInertialGestureScrollUpdate(
      {.timestamp = MillisecondsTicks(19),
       .delta = 2000.0f,
       .caused_frame_update = false,
       .did_scroll = false,
       .begin_frame_args = args5}));

  viz::BeginFrameArgs presented_args =
      CreateBeginFrameArgs(42, MillisecondsTicks(666));
  auto timeline = ScrollJankV4Frame::CalculateTimeline(
      events_metrics, presented_args,
      /* presentation_ts= */ MillisecondsTicks(777));
  EXPECT_THAT(
      timeline,
      ElementsAre(
          ScrollJankV4Frame(
              BeginFrameArgsForScrollJank{.frame_time = MillisecondsTicks(111),
                                          .interval = kVsyncInterval},
              NonDamagingFrame{},
              {ScrollJankV4FrameStage{ScrollStart{}},
               ScrollJankV4FrameStage{ScrollUpdates(
                   static_cast<ScrollUpdateEventMetrics*>(
                       events_metrics[1].get()),
                   Real{
                       .first_input_generation_ts = MillisecondsTicks(11),
                       .last_input_generation_ts = MillisecondsTicks(12),
                       .has_inertial_input = false,
                       .abs_total_raw_delta_pixels = 3.0f,
                       .max_abs_inertial_raw_delta_pixels = 0.0f,
                   },
                   /* synthetic= */ std::nullopt)}}),
          ScrollJankV4Frame(
              BeginFrameArgsForScrollJank{.frame_time = MillisecondsTicks(222),
                                          .interval = kVsyncInterval},
              NonDamagingFrame{}, {ScrollJankV4FrameStage{ScrollEnd{}}}),
          ScrollJankV4Frame(
              BeginFrameArgsForScrollJank{.frame_time = MillisecondsTicks(666),
                                          .interval = kVsyncInterval},

              DamagingFrame{.presentation_ts = MillisecondsTicks(777)},
              {ScrollJankV4FrameStage{ScrollUpdates(
                  static_cast<ScrollUpdateEventMetrics*>(
                      events_metrics[4].get()),
                  Real{
                      .first_input_generation_ts = MillisecondsTicks(14),
                      .last_input_generation_ts = MillisecondsTicks(19),
                      .has_inertial_input = true,
                      .abs_total_raw_delta_pixels = 3330.0f,
                      .max_abs_inertial_raw_delta_pixels = 2000.0f,
                  },
                  /* synthetic= */ std::nullopt)}})));
}

TEST_F(ScrollJankV4FrameTest, BeginFrameArgsForScrollJankToOstream) {
  std::ostringstream out;
  auto& result =
      out << BeginFrameArgsForScrollJank{.frame_time = MillisecondsTicks(123),
                                         .interval = kVsyncInterval};
  EXPECT_THAT(out.str(),
              ::testing::MatchesRegex(R"(BeginFrameArgsForScrollJank\{.+\})"));
  EXPECT_EQ(&result, &out);
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
  auto frame = ScrollJankV4Frame(
      BeginFrameArgsForScrollJank{
          .frame_time = MillisecondsTicks(123),
          .interval = base::Milliseconds(8),
      },
      DamagingFrame{.presentation_ts = MillisecondsTicks(777)},
      {ScrollJankV4FrameStage{ScrollEnd{}}, ScrollJankV4FrameStage{ScrollEnd{}},
       ScrollJankV4FrameStage{ScrollEnd{}}});

  std::ostringstream out;
  auto& result = out << frame;
  EXPECT_THAT(out.str(), ::testing::MatchesRegex(R"(ScrollJankV4Frame\{.+\})"));
  EXPECT_EQ(&result, &out);
}

}  // namespace cc
