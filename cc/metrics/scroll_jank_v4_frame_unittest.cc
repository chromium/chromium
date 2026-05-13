// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_frame.h"

#include <sstream>

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

using BeginFrameArgsForScrollJank =
    ScrollJankV4Frame::BeginFrameArgsForScrollJank;
using ScrollDamage = ScrollJankV4Frame::ScrollDamage;
using DamagingFrame = ScrollJankV4Frame::DamagingFrame;
using NonDamagingFrame = ScrollJankV4Frame::NonDamagingFrame;

constexpr base::TimeTicks MillisecondsTicks(int ms) {
  return base::TimeTicks() + base::Milliseconds(ms);
}

constexpr base::TimeDelta kVsyncInterval = base::Milliseconds(16);

}  // namespace

TEST(ScrollJankV4FrameTest, BeginFrameArgsForScrollJankFromBeginFrameArgs) {
  viz::BeginFrameArgs args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, /* source_id= */ 999, /* sequence_number= */ 42,
      /* frame_time= */ MillisecondsTicks(123),
      /* deadline= */ MillisecondsTicks(123) + kVsyncInterval / 3,
      /* interval= */ kVsyncInterval,
      viz::BeginFrameArgs::BeginFrameArgsType::NORMAL);
  BeginFrameArgsForScrollJank args_for_scroll_jank =
      BeginFrameArgsForScrollJank::From(args, 456);
  EXPECT_EQ(args_for_scroll_jank, (BeginFrameArgsForScrollJank{
                                      .frame_time = MillisecondsTicks(123),
                                      .interval = kVsyncInterval,
                                      .result_id = 456,
                                  }));
}

TEST(ScrollJankV4FrameTest,
     BeginFrameArgsForScrollJankFromDispatchBeginFrameArgs) {
  ScrollEventMetrics::DispatchBeginFrameArgs args = {
      .frame_time = MillisecondsTicks(123), .interval = kVsyncInterval};
  BeginFrameArgsForScrollJank args_for_scroll_jank =
      BeginFrameArgsForScrollJank::From(args, 456);
  EXPECT_EQ(args_for_scroll_jank, (BeginFrameArgsForScrollJank{
                                      .frame_time = MillisecondsTicks(123),
                                      .interval = kVsyncInterval,
                                      .result_id = 456,
                                  }));
}

TEST(ScrollJankV4FrameTest, EmptyRealScrollUpdatesToOstream) {
  std::optional<ScrollJankV4Frame::Stage::ScrollUpdates::Real> updates =
      std::nullopt;

  std::ostringstream out;
  auto& result = out << updates;
  EXPECT_EQ(out.str(), "empty");
  EXPECT_EQ(&result, &out);
}

TEST(ScrollJankV4FrameTest, NonEmptyRealScrollUpdatesToOstream) {
  std::optional<ScrollJankV4Frame::Stage::ScrollUpdates::Real> updates =
      ScrollJankV4Frame::Stage::ScrollUpdates::Real{
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

TEST(ScrollJankV4FrameTest, EmptySyntheticScrollUpdatesToOstream) {
  std::optional<ScrollJankV4Frame::Stage::ScrollUpdates::Synthetic> updates =
      std::nullopt;

  std::ostringstream out;
  auto& result = out << updates;
  EXPECT_EQ(out.str(), "empty");
  EXPECT_EQ(&result, &out);
}

TEST(ScrollJankV4FrameTest, NonEmptySyntheticScrollUpdatesToOstream) {
  std::optional<ScrollJankV4Frame::Stage::ScrollUpdates::Synthetic> updates =
      ScrollJankV4Frame::Stage::ScrollUpdates::Synthetic{
          .first_input_begin_frame_ts = MillisecondsTicks(42),
          .has_inertial_input = false,
      };

  std::ostringstream out;
  auto& result = out << updates;
  EXPECT_THAT(out.str(), ::testing::MatchesRegex(R"(Synthetic\{.+\})"));
  EXPECT_EQ(&result, &out);
}

TEST(ScrollJankV4FrameTest, ScrollUpdatesToOstream) {
  auto stage = ScrollJankV4Frame::Stage{ScrollJankV4Frame::Stage::ScrollUpdates(
      ScrollJankV4Frame::Stage::ScrollUpdates::Real{
          .first_input_generation_ts = MillisecondsTicks(1),
          .last_input_generation_ts = MillisecondsTicks(3),
          .has_inertial_input = false,
          .abs_total_raw_delta_pixels = 10,
          .max_abs_inertial_raw_delta_pixels = 0,
      },
      /* synthetic= */ std::nullopt,
      /* scroll_begin_arrival_timestamp= */ std::nullopt)};

  std::ostringstream out;
  auto& result = out << stage;
  EXPECT_THAT(out.str(), ::testing::MatchesRegex(R"(ScrollUpdates\{.+\})"));
  EXPECT_EQ(&result, &out);
}

TEST(ScrollJankV4FrameTest, ScrollStartToOstream) {
  auto stage =
      ScrollJankV4Frame::Stage{ScrollJankV4Frame::Stage::ScrollStart{}};

  std::ostringstream out;
  auto& result = out << stage;
  EXPECT_EQ(out.str(), "ScrollStart{}");
  EXPECT_EQ(&result, &out);
}

TEST(ScrollJankV4FrameTest, ScrollEndToOstream) {
  auto stage = ScrollJankV4Frame::Stage{ScrollJankV4Frame::Stage::ScrollEnd{}};

  std::ostringstream out;
  auto& result = out << stage;
  EXPECT_EQ(out.str(), "ScrollEnd{}");
  EXPECT_EQ(&result, &out);
}

TEST(ScrollJankV4FrameTest, BeginFrameArgsForScrollJankToOstream) {
  std::ostringstream out;
  auto& result =
      out << BeginFrameArgsForScrollJank{.frame_time = MillisecondsTicks(123),
                                         .interval = kVsyncInterval,
                                         .result_id = 456};
  EXPECT_THAT(out.str(),
              ::testing::MatchesRegex(R"(BeginFrameArgsForScrollJank\{.+\})"));
  EXPECT_EQ(&result, &out);
}

TEST(ScrollJankV4FrameTest, DamagingFrameToOstream) {
  std::ostringstream out;
  auto& result =
      out << DamagingFrame{.presentation_ts = MillisecondsTicks(777)};
  EXPECT_THAT(out.str(), ::testing::MatchesRegex(R"(DamagingFrame\{.+\})"));
  EXPECT_EQ(&result, &out);
}

TEST(ScrollJankV4FrameTest, NonDamagingFrameToOstream) {
  std::ostringstream out;
  auto& result = out << NonDamagingFrame{};
  EXPECT_EQ(out.str(), "NonDamagingFrame{}");
  EXPECT_EQ(&result, &out);
}

TEST(ScrollJankV4FrameTest, ScrollDamageToOstream) {
  std::ostringstream out;
  auto& result = out << ScrollDamage{NonDamagingFrame{}};
  EXPECT_THAT(out.str(), ::testing::MatchesRegex(R"(ScrollDamage\{.+\})"));
  EXPECT_EQ(&result, &out);
}

TEST(ScrollJankV4FrameTest, ScrollJankV4FrameToOstream) {
  auto frame = ScrollJankV4Frame(
      BeginFrameArgsForScrollJank{
          .frame_time = MillisecondsTicks(123),
          .interval = base::Milliseconds(8),
          .result_id = 456,
      },
      DamagingFrame{.presentation_ts = MillisecondsTicks(777)},
      {ScrollJankV4Frame::Stage{ScrollJankV4Frame::Stage::ScrollEnd{}},
       ScrollJankV4Frame::Stage{ScrollJankV4Frame::Stage::ScrollEnd{}},
       ScrollJankV4Frame::Stage{ScrollJankV4Frame::Stage::ScrollEnd{}}});

  std::ostringstream out;
  auto& result = out << frame;
  EXPECT_THAT(out.str(), ::testing::MatchesRegex(R"(ScrollJankV4Frame\{.+\})"));
  EXPECT_EQ(&result, &out);
}

}  // namespace cc
