// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_decider.h"

#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

constexpr base::TimeDelta kVsyncInterval = base::Milliseconds(16);

constexpr base::TimeTicks MillisSinceEpoch(int64_t millis) {
  return base::TimeTicks() + base::Milliseconds(millis);
}

constexpr base::TimeTicks MicrosSinceEpoch(int64_t micros) {
  return base::TimeTicks() + base::Microseconds(micros);
}

using ScrollDamage = ScrollJankV4Frame::ScrollDamage;
using DamagingFrame = ScrollJankV4Frame::DamagingFrame;
using NonDamagingFrame = ScrollJankV4Frame::NonDamagingFrame;
using ScrollUpdates = ScrollJankV4FrameStage::ScrollUpdates;
using Real = ScrollUpdates::Real;
using Synthetic = ScrollUpdates::Synthetic;
using ScrollJankV4Result = ScrollUpdateEventMetrics::ScrollJankV4Result;

/* Matches a result iff `matcher` matches `result->missed_vsyncs_per_reason`. */
::testing::Matcher<const ScrollJankV4Result&> HasMissedVsyncsPerReasonMatching(
    ::testing::Matcher<const JankReasonArray<int>&> matcher) {
  return ::testing::Field(&ScrollJankV4Result::missed_vsyncs_per_reason,
                          matcher);
}

const ::testing::Matcher<const ScrollJankV4Result&> kHasNoMissedVsyncs =
    HasMissedVsyncsPerReasonMatching(::testing::Each(::testing::Eq(0)));

::testing::Matcher<const ScrollJankV4Result&> HasMissedVsyncs(
    JankReason reason,
    int missed_vsyncs) {
  JankReasonArray<int> expected_missed_vsyncs = {};
  expected_missed_vsyncs[static_cast<int>(reason)] = missed_vsyncs;
  return HasMissedVsyncsPerReasonMatching(
      ::testing::ElementsAreArray(expected_missed_vsyncs));
}

class ScrollJankV4DeciderTest : public testing::Test {
 protected:
  static ScrollJankV4Frame::BeginFrameArgsForScrollJank CreateBeginFrameArgs(
      base::TimeTicks frame_time) {
    return {.frame_time = frame_time, .interval = kVsyncInterval};
  }

  ScrollJankV4Decider decider_;
};

// Type of a frame provided to `ScrollJankV4Decider` along two axes:
//
//   * whether it's damaging or non-damaging,
//   * whether it contains only real scroll updates, only synthetic scroll
//     updates, or both.
struct TestFrameType {
  std::string frame_type_name;
  bool is_damaging;
  bool has_real_inputs;
  bool has_synthetic_inputs;
};

static const std::vector<TestFrameType> kAllFrameTypes = []() {
  std::vector<TestFrameType> frame_types = {};
  for (const auto is_damaging : {false, true}) {
    const char* damage_name = is_damaging ? "Damaging" : "NonDamaging";
    frame_types.push_back({
        .frame_type_name = base::StrCat({damage_name, "RealOnly"}),
        .is_damaging = is_damaging,
        .has_real_inputs = true,
        .has_synthetic_inputs = false,
    });
    frame_types.push_back({
        .frame_type_name = base::StrCat({damage_name, "SyntheticOnly"}),
        .is_damaging = is_damaging,
        .has_real_inputs = false,
        .has_synthetic_inputs = true,
    });
    frame_types.push_back({
        .frame_type_name = base::StrCat({damage_name, "BothRealAndSynthetic"}),
        .is_damaging = is_damaging,
        .has_real_inputs = true,
        .has_synthetic_inputs = true,
    });
  }
  return frame_types;
}();

static const std::vector<TestFrameType> kRealOnlyFrameTypes = []() {
  std::vector<TestFrameType> frame_types = {};
  for (const auto is_damaging : {false, true}) {
    const char* damage_name = is_damaging ? "Damaging" : "NonDamaging";
    frame_types.push_back({
        .frame_type_name = base::StrCat({damage_name, "RealOnly"}),
        .is_damaging = is_damaging,
        .has_real_inputs = true,
        .has_synthetic_inputs = false,
    });
  }
  return frame_types;
}();

// Fixture for tests which are parameterized with one or two `TestFrameType`s.
class ParameterizedScrollJankV4DeciderTest : public ScrollJankV4DeciderTest {
 protected:
  // Recipe which specifies how to construct a frame (call to
  // `ScrollJankV4Decider::DecideJankForFrameWith*()`) for a `TestFrameType`.
  //
  // For example, given the following recipe:
  //
  // ```
  // TestFrameRecipe{
  //   .if_real = Real{R},
  //   .if_synthetic = Synthetic{S},
  //   .if_synthetic_only = {
  //     .future_real_frame_is_fast_scroll_or_sufficiently_fast_fling = F,
  //   },
  //   .if_damaging = DamagingFrame{D},
  //   .args = BeginFrameArgsForScrollJank{A},
  // }
  // ```
  //
  // 1. If combined with the following frame type:
  //
  //    ```
  //    TestFrameType{
  //      .is_damaging = true,
  //      .has_real_inputs = true,
  //      .has_synthetic_inputs = false,
  //    }
  //    ```
  //
  //    the recipe corresponds to the following frame:
  //
  //    ```
  //    decider.DecideJankForFrameWithRealScrollUpdates(
  //      ScrollUpdates(/* earliest_event= */ nullptr, Real{R}, /* synthetic= */
  //      std::nullopt), DamagingFrame{D}, BeginFrameArgsForScrollJank{A});
  //    ```
  //
  // 2. If combined with the following frame type:
  //
  //    ```
  //    TestFrameType{
  //      .is_damaging = false,
  //      .has_real_inputs = false,
  //      .has_synthetic_inputs = true,
  //    }
  //    ```
  //
  //    the recipe corresponds to the following frame:
  //
  //    ```
  //    decider.DecideJankForFrameWithSyntheticScrollUpdatesOnly(
  //      ScrollUpdates(/* earliest_event= */ nullptr, /* real= */ std::nullopt,
  //      Synthetic{S}), NonDamagingFrame{}, BeginFrameArgsForScrollJank{A},
  //      /* future_real_frame_is_fast_scroll_or_sufficiently_fast_fling= */ F);
  //    ```
  //
  // Note: All fields are declared as `std::optional` so that callers wouldn't
  // have to provide parameters which aren't relevant for their test case (e.g.
  // if they exclude frames containing synthetic scroll updates). At the same
  // time, `std::optional` allows `DecideJankForParameterizedFrame()` below to
  // enforce that callers provided all required parameters (instead of silently
  // using their default values).
  struct TestFrameRecipe {
    std::optional<Real> if_real = std::nullopt;
    std::optional<Synthetic> if_synthetic = std::nullopt;
    struct {
      std::optional<bool>
          future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
              std::nullopt;
    } if_synthetic_only;
    std::optional<DamagingFrame> if_damaging = std::nullopt;
    std::optional<ScrollJankV4Frame::BeginFrameArgsForScrollJank> args =
        std::nullopt;
  };

  ScrollJankV4Result DecideJankForParameterizedFrame(
      const TestFrameType& frame_type,
      const TestFrameRecipe& frame_recipe) {
#define GET_FRAME_RECIPE_PARAM_OR_FAIL(param)                           \
  [&]() {                                                               \
    if (!frame_recipe.param.has_value()) {                              \
      ADD_FAILURE() << "Missing ." #param " parameter in frame_recipe"; \
    }                                                                   \
    return *frame_recipe.param;                                         \
  }()
    const ScrollDamage damage =
        frame_type.is_damaging
            ? ScrollDamage{GET_FRAME_RECIPE_PARAM_OR_FAIL(if_damaging)}
            : ScrollDamage{NonDamagingFrame{}};
    const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args =
        GET_FRAME_RECIPE_PARAM_OR_FAIL(args);

    if (frame_type.has_real_inputs) {
      return decider_.DecideJankForFrameWithRealScrollUpdates(
          ScrollUpdates(
              /* earliest_event= */ nullptr,
              GET_FRAME_RECIPE_PARAM_OR_FAIL(if_real),
              frame_type.has_synthetic_inputs
                  ? std::make_optional(
                        GET_FRAME_RECIPE_PARAM_OR_FAIL(if_synthetic))
                  : std::nullopt),
          damage, args);
    }
    return decider_.DecideJankForFrameWithSyntheticScrollUpdatesOnly(
        ScrollUpdates(/* earliest_event= */ nullptr, /* real= */ std::nullopt,
                      GET_FRAME_RECIPE_PARAM_OR_FAIL(if_synthetic)),
        damage, args,
        GET_FRAME_RECIPE_PARAM_OR_FAIL(
            if_synthetic_only
                .future_real_frame_is_fast_scroll_or_sufficiently_fast_fling));
#undef GET_FRAME_RECIPE_PARAM_OR_FAIL
  }
};

// Fixture for tests parameterized with one `TestFrameType`.
//
// Legend for diagrams:
//
//   (p)  = frame with the parameterized type (`GetParam()`)
//   (RD) = real damaging frame
class SinglyParameterizedScrollJankV4DeciderTest
    : public ParameterizedScrollJankV4DeciderTest,
      public testing::WithParamInterface<TestFrameType> {};

INSTANTIATE_TEST_SUITE_P(
    SinglyParameterizedScrollJankV4DeciderTest,
    SinglyParameterizedScrollJankV4DeciderTest,
    testing::ValuesIn(kAllFrameTypes),
    [](const testing::TestParamInfo<
        SinglyParameterizedScrollJankV4DeciderTest::ParamType>& info) {
      return info.param.frame_type_name;
    });

// Fixture for tests parameterized with two different `TestFrameType`s. This
// allows tests to check that certain behaviors are preserved even when the
// frame type changes mid-scroll.
//
// Legend for diagrams:
//
//   (a)  = frame with the first parameterized type (`frame_type_a_`)
//   (b)  = frame with the second parameterized type (`frame_type_b_`)
//   (RD) = real damaging frame
class DoublyParameterizedScrollJankV4DeciderTest
    : public ParameterizedScrollJankV4DeciderTest,
      public testing::WithParamInterface<
          std::tuple<TestFrameType, TestFrameType>> {
 public:
  DoublyParameterizedScrollJankV4DeciderTest()
      : frame_type_a_(std::get<0>(GetParam())),
        frame_type_b_(std::get<1>(GetParam())) {}

 protected:
  const TestFrameType frame_type_a_;
  const TestFrameType frame_type_b_;
};
INSTANTIATE_TEST_SUITE_P(
    DoublyParameterizedScrollJankV4DeciderTest,
    DoublyParameterizedScrollJankV4DeciderTest,
    testing::Combine(testing::ValuesIn(kAllFrameTypes),
                     testing::ValuesIn(kAllFrameTypes)),
    [](const testing::TestParamInfo<
        DoublyParameterizedScrollJankV4DeciderTest::ParamType>& info) {
      return base::StrCat({std::get<0>(info.param).frame_type_name, "To",
                           std::get<1>(info.param).frame_type_name});
    });

// Fixture for tests parameterized with two different `TestFrameType`s where the
// SECOND type only contains real scroll updates.
//
// This fixture is used for tests that focus on the transition from a regular
// scroll to an inertial scroll (because inertial scroll updates cannot be
// synthetic).
class FlingTransitionDoublyParameterizedScrollJankV4DeciderTest
    : public DoublyParameterizedScrollJankV4DeciderTest {};
INSTANTIATE_TEST_SUITE_P(
    FlingTransitionDoublyParameterizedScrollJankV4DeciderTest,
    FlingTransitionDoublyParameterizedScrollJankV4DeciderTest,
    testing::Combine(testing::ValuesIn(kAllFrameTypes),
                     testing::ValuesIn(kRealOnlyFrameTypes)),
    [](const testing::TestParamInfo<
        FlingTransitionDoublyParameterizedScrollJankV4DeciderTest::ParamType>&
           info) {
      return base::StrCat({std::get<0>(info.param).frame_type_name, "To",
                           std::get<1>(info.param).frame_type_name});
    });

// Fixture for tests parameterized with two different `TestFrameType`s where
// BOTH types only contain real scroll updates.
//
// This fixture is used for tests that focus on an ongoing inertial scroll
// (because inertial scroll updates cannot be synthetic).
class MidFlingDoublyParameterizedScrollJankV4DeciderTest
    : public DoublyParameterizedScrollJankV4DeciderTest {};
INSTANTIATE_TEST_SUITE_P(
    MidFlingDoublyParameterizedScrollJankV4DeciderTest,
    MidFlingDoublyParameterizedScrollJankV4DeciderTest,
    testing::Combine(testing::ValuesIn(kRealOnlyFrameTypes),
                     testing::ValuesIn(kRealOnlyFrameTypes)),
    [](const testing::TestParamInfo<
        MidFlingDoublyParameterizedScrollJankV4DeciderTest::ParamType>& info) {
      return base::StrCat({std::get<0>(info.param).frame_type_name, "To",
                           std::get<1>(info.param).frame_type_name});
    });

/*
Tests that the decider doesn't mark regular frame production in a fast scroll
with one frame produced every VSync as janky.

VSync    V     V     V     V     V     V     V     V     V
Input     I0 I1 I2 I3:I4 I5:I6 I7:I8 I9:I10  :     :     :
           | |   | | : | | : | | : | | : |I11:     :     :
F1(a):     |---------BF----|     :     : | | :     :     :
F2(a):           |---------BF----|     :     :     :     :
F3(b):                 |---------BF----|     :     :     :
F4(b):                       |---------BF----|     :     :
F5(a):                             |---------BF----|     :
F6(a):                                   |---------BF----|
 */
TEST_P(DoublyParameterizedScrollJankV4DeciderTest,
       FastScrollWithFramesProducedEveryVsync) {
  // 2 frames with `frame_type_a_`.
  ScrollJankV4Result result1 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(103),
                          .last_input_generation_ts = MillisSinceEpoch(111),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 5.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(116)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   true},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(132)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(116)),
      });
  EXPECT_THAT(result1, kHasNoMissedVsyncs);
  ScrollJankV4Result result2 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(119),
                          .last_input_generation_ts = MillisSinceEpoch(127),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 5.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(132)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   true},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(148)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(132)),
      });
  EXPECT_THAT(result2, kHasNoMissedVsyncs);

  // 2 frames with `frame_type_b_`.
  ScrollJankV4Result result3 = DecideJankForParameterizedFrame(
      frame_type_b_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(135),
                          .last_input_generation_ts = MillisSinceEpoch(143),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 5.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(148)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   true},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(164)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(148)),
      });
  EXPECT_THAT(result3, kHasNoMissedVsyncs);
  ScrollJankV4Result result4 = DecideJankForParameterizedFrame(
      frame_type_b_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(151),
                          .last_input_generation_ts = MillisSinceEpoch(159),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 5.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(164)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   true},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(180)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(164)),
      });
  EXPECT_THAT(result4, kHasNoMissedVsyncs);

  // 2 frames with `frame_type_a_` again.
  ScrollJankV4Result result5 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(167),
                          .last_input_generation_ts = MillisSinceEpoch(175),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 5.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(180)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   true},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(196)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(180)),
      });
  EXPECT_THAT(result5, kHasNoMissedVsyncs);
  ScrollJankV4Result result6 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(183),
                          .last_input_generation_ts = MillisSinceEpoch(191),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 5.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(196)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   true},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(212)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(196)),
      });
  EXPECT_THAT(result6, kHasNoMissedVsyncs);
}

/*
Tests that the decider doesn't mark regular frame production in a slow scroll
with one frame produced every VSync as janky.

VSync    V   V   V   V   V   V   V   V   V   V   V   V   V   V   V   V   V
Input     I0      I2     :I4     :I6     :I8     :I10    :       :       :
           |I1     |I3   : |I5   : |I7   : |I9   : |I11  :       :       :
           ||      ||    : ||    : ||    : ||    : ||    :       :       :
F1(a):     |-------------BF------|       :       : ||    :       :       :
F2(a):             |-------------BF------|       :       :       :       :
F3(b):                     |-------------BF------|       :       :       :
F4(b):                             |-------------BF------|       :       :
F5(a):                                     |-------------BF------|       :
F6(a):                                             |-------------BF------|
 */
TEST_P(DoublyParameterizedScrollJankV4DeciderTest,
       SlowScrollWithFramesProducedEveryOtherVsync) {
  // 2 frames with `frame_type_a_`.
  ScrollJankV4Result result1 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(103),
                          .last_input_generation_ts = MillisSinceEpoch(111),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 1.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(116)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   false},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(132)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(116)),
      });
  EXPECT_THAT(result1, kHasNoMissedVsyncs);
  ScrollJankV4Result result2 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(135),
                          .last_input_generation_ts = MillisSinceEpoch(143),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 1.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(148)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   false},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(164)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(148)),
      });
  EXPECT_THAT(result2, kHasNoMissedVsyncs);

  // 2 frames with `frame_type_b_`.
  ScrollJankV4Result result3 = DecideJankForParameterizedFrame(
      frame_type_b_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(167),
                          .last_input_generation_ts = MillisSinceEpoch(175),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 1.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(180)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   false},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(196)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(180)),
      });
  EXPECT_THAT(result3, kHasNoMissedVsyncs);
  ScrollJankV4Result result4 = DecideJankForParameterizedFrame(
      frame_type_b_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(199),
                          .last_input_generation_ts = MillisSinceEpoch(207),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 1.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(212)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   false},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(228)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(212)),
      });
  EXPECT_THAT(result4, kHasNoMissedVsyncs);

  // 2 frames with `frame_type_a_` again.
  ScrollJankV4Result result5 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(231),
                          .last_input_generation_ts = MillisSinceEpoch(239),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 1.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(244)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   false},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(260)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(244)),
      });
  EXPECT_THAT(result5, kHasNoMissedVsyncs);
  ScrollJankV4Result result6 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(263),
                          .last_input_generation_ts = MillisSinceEpoch(271),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 1.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(276)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   false},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(292)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(276)),
      });
  EXPECT_THAT(result6, kHasNoMissedVsyncs);
}

/*
Test that when a frame took too long to be produced shows up in the metric.

time    100     116     132     148     164     180     196     212     228
vsync    |       |       |       |       |       |       |       |       |
input     I0  I1  I2  I3  I4  I5
          |   |   |   |   |   |
F1(RD):   |--------------BF------|
F2(a):            |------------------------------BF------|
F3(b):                    |--------------------------------------BF------|
 */
TEST_P(DoublyParameterizedScrollJankV4DeciderTest,
       MissedVsyncWhenInputWasPresent) {
  ScrollJankV4Result result1 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(103),
                         .last_input_generation_ts = MillisSinceEpoch(111),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 2.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      DamagingFrame{.presentation_ts = MillisSinceEpoch(148)},
      CreateBeginFrameArgs(MillisSinceEpoch(132)));
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  ScrollJankV4Result result2 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(119),
                          .last_input_generation_ts = MillisSinceEpoch(127),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 2.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(148)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   false},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(196)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(180)),
      });
  EXPECT_THAT(
      result2,
      HasMissedVsyncs(
          JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 2));

  ScrollJankV4Result result3 = DecideJankForParameterizedFrame(
      frame_type_b_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(135),
                          .last_input_generation_ts = MillisSinceEpoch(143),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 2.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(196)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   false},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(228)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(212)),
      });
  if (frame_type_a_.has_real_inputs && frame_type_a_.is_damaging) {
    EXPECT_THAT(
        result3,
        HasMissedVsyncs(
            JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1));
  } else {
    EXPECT_THAT(result3, kHasNoMissedVsyncs);
  }
}

// Regression test for https://crbug.com/404637348.
TEST_F(ScrollJankV4DeciderTest, ScrollWithZeroVsyncs) {
  ScrollJankV4Result result1 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(103),
                         .last_input_generation_ts = MillisSinceEpoch(111),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 2.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(148)}},
      CreateBeginFrameArgs(MillisSinceEpoch(132)));
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  // A malformed frame whose presentation timestamp is less than half a vsync
  // greater than than the previous frame's presentation timestamp.
  ScrollJankV4Result result2 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(119),
                         .last_input_generation_ts = MillisSinceEpoch(127),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 2.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(149)}},
      CreateBeginFrameArgs(MillisSinceEpoch(133)));
  EXPECT_THAT(result2, kHasNoMissedVsyncs);
}

/*
Tests that the decider evaluates each scroll separately (i.e. doesn't evaluate a
scroll against a previous scroll).

    Scroll 1 <--|--> Scroll 2
VSync V0  :   V1|     V2      V3      V4 ...     V64     V65     V66     V67
      :   :   : |     :       :       :  ...      :       :       :       :
Input :   I1  : | I2  :   I3  :   I4  :  ... I64  :  I65  :       :       :
          :   : | :   :   :   :   :   :  ...  :   :   :   :       :       :
F1:       |8ms| | :   :   :   :   :   :  ...  :   :   :   :       :       :
F2:             | |-------40ms--------|  ...  :   :   :   :       :       :
F3:             |         |-------40ms---...  :   :   :   :       :       :
F4:             |                 |-40ms-...  :   :   :   :       :       :
...             |                        ...  :   :   :   :       :       :
F62:            |                        ...-40ms-|   :   :       :       :
F63:            |                        ...-40ms---------|       :       :
F64:            |                        ...  |-------40ms--------|       :
F65:            |                        ...          |-------40ms--------|

The decider should NOT evaluate I2/F2 against I1/F1 (because they happened in
different scrolls), so the decider should NOT mark F2 as janky.
*/
TEST_F(ScrollJankV4DeciderTest, EvaluatesEachScrollSeparately) {
  // Scroll 1: First input took only 8 ms (half a VSync) to deliver.
  ScrollJankV4Result result1 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(108),
                         .last_input_generation_ts = MillisSinceEpoch(108),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 4.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(116)}},
      CreateBeginFrameArgs(MillisSinceEpoch(100)));
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  decider_.OnScrollEnded();
  decider_.OnScrollStarted();

  // Scroll 2: Inputs 2 and 3 took 40 ms (2.5 VSyncs) to deliver.
  ScrollJankV4Result result2 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(124),
                         .last_input_generation_ts = MillisSinceEpoch(124),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 4.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(164)}},
      CreateBeginFrameArgs(MillisSinceEpoch(148)));
  EXPECT_THAT(result2, kHasNoMissedVsyncs);
  ScrollJankV4Result result3 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(140),
                         .last_input_generation_ts = MillisSinceEpoch(140),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 4.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(180)}},
      CreateBeginFrameArgs(MillisSinceEpoch(164)));
  EXPECT_THAT(result3, kHasNoMissedVsyncs);
}

/*
Same as `EvaluatesEachScrollSeparately` but without a call to
`ScrollJankV4Decider::OnScrollEnded()`.
*/
TEST_F(ScrollJankV4DeciderTest, EvaluatesEachScrollSeparatelyScrollStartOnly) {
  // Scroll 1: First input took only 8 ms (half a VSync) to deliver.
  ScrollJankV4Result result1 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(108),
                         .last_input_generation_ts = MillisSinceEpoch(108),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 4.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(116)}},
      CreateBeginFrameArgs(MillisSinceEpoch(100)));
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  decider_.OnScrollStarted();

  // Scroll 2: Inputs 2 and 3 took 40 ms (2.5 VSyncs) to deliver.
  ScrollJankV4Result result2 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(124),
                         .last_input_generation_ts = MillisSinceEpoch(124),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 4.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(164)}},
      CreateBeginFrameArgs(MillisSinceEpoch(148)));
  EXPECT_THAT(result2, kHasNoMissedVsyncs);
  ScrollJankV4Result result3 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(140),
                         .last_input_generation_ts = MillisSinceEpoch(140),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 4.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(180)}},
      CreateBeginFrameArgs(MillisSinceEpoch(164)));
  EXPECT_THAT(result3, kHasNoMissedVsyncs);
}

/*
Same as `EvaluatesEachScrollSeparately` but without a call to
`ScrollJankV4Decider::OnScrollStarted()`.
*/
TEST_F(ScrollJankV4DeciderTest, EvaluatesEachScrollSeparatelyScrollEndOnly) {
  // Scroll 1: First input took only 8 ms (half a VSync) to deliver.
  ScrollJankV4Result result1 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(108),
                         .last_input_generation_ts = MillisSinceEpoch(108),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 4.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(116)}},
      CreateBeginFrameArgs(MillisSinceEpoch(100)));
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  decider_.OnScrollEnded();

  // Scroll 2: Inputs 2 and 3 took 40 ms (2.5 VSyncs) to deliver.
  ScrollJankV4Result result2 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(124),
                         .last_input_generation_ts = MillisSinceEpoch(124),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 4.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(164)}},
      CreateBeginFrameArgs(MillisSinceEpoch(148)));
  EXPECT_THAT(result2, kHasNoMissedVsyncs);
  ScrollJankV4Result result3 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(140),
                         .last_input_generation_ts = MillisSinceEpoch(140),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 4.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(180)}},
      CreateBeginFrameArgs(MillisSinceEpoch(164)));
  EXPECT_THAT(result3, kHasNoMissedVsyncs);
}

/*
Tests that the decider doesn't unfairly mark a frame as janky just because
Chrome "got lucky" (quickly presented an input in a frame) once many frames ago.

VSync     V0  :   V1      V2      V3 ... V62     V63     V64  :  V65     V66
          :   :   :       :       :  ...  :       :       :   :   :       :
Input     :   I1  I2      I3      I4 ... I63     I64      :  I65  :       :
              :   :       :       :  ...  :       :       :   :           :
F1(RD):       |8ms|       :       :       :       :       :   :           :
F2(p):            |-16ms--|       :       :       :       :   :           :
F3(p):                    |-16ms--|       :       :       :   :           :
F4(p):                            |--...  :       :       :   :           :
...                                       :       :       :   :           :
F62(p):                              ...--|       :       :   :           :
F63(p):                              ...  |-16ms--|       :   :           :
F64(p):                              ...          |-16ms--|   :           :
F65(RD):                                                      |----24ms---|

The decider should NOT evaluate I65/F65 against I1/F1 (because it happened a
long time ago), so the decider should NOT mark F65 as janky.
*/
TEST_P(SinglyParameterizedScrollJankV4DeciderTest,
       MissedVsyncLongAfterQuickInputFrameDelivery) {
  // First input took only 8 ms (half a VSync) to deliver.
  ScrollJankV4Result result1 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(108),
                         .last_input_generation_ts = MillisSinceEpoch(108),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 2.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(116)}},
      CreateBeginFrameArgs(MillisSinceEpoch(100)));
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  // Inputs 2-64 took 16 ms (one VSync) to deliver.
  for (int i = 2; i <= 64; i++) {
    base::TimeDelta offset = (i - 2) * kVsyncInterval;
    ScrollJankV4Result result = DecideJankForParameterizedFrame(
        GetParam(),
        {
            .if_real =
                Real{
                    .first_input_generation_ts = MillisSinceEpoch(116) + offset,
                    .last_input_generation_ts = MillisSinceEpoch(116) + offset,
                    .has_inertial_input = false,
                    .abs_total_raw_delta_pixels = 2.0f,
                    .max_abs_inertial_raw_delta_pixels = 0.0f},
            .if_synthetic = Synthetic{.first_input_begin_frame_ts =
                                          MillisSinceEpoch(116) + offset},
            .if_synthetic_only =
                {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                     false},
            .if_damaging = DamagingFrame{.presentation_ts =
                                             MillisSinceEpoch(132) + offset},
            .args = CreateBeginFrameArgs(MillisSinceEpoch(116) + offset),
        });
    EXPECT_THAT(result, kHasNoMissedVsyncs);
  }

  // There's one VSync missed between F64 and F65. F65 should be evaluated
  // against the delivery cutoffs of the recent frames (16 ms) rather than the
  // first frame (8 ms). Therefore, it's not reasonable to assume that F65's
  // first input (generated at 1132 ms) could have been included in the missed
  // VSync (presented at 1140 ms), so F65 should NOT be marked as janky.
  ScrollJankV4Result result65 =
      decider_.DecideJankForFrameWithRealScrollUpdates(
          ScrollUpdates(
              /* earliest_event= */ nullptr,
              Real{.first_input_generation_ts = MillisSinceEpoch(1132),
                   .last_input_generation_ts = MillisSinceEpoch(1132),
                   .has_inertial_input = false,
                   .abs_total_raw_delta_pixels = 2.0f,
                   .max_abs_inertial_raw_delta_pixels = 0.0f},
              /* synthetic= */ std::nullopt),
          ScrollDamage{
              DamagingFrame{.presentation_ts = MillisSinceEpoch(1156)}},
          CreateBeginFrameArgs(MillisSinceEpoch(1140)));
  EXPECT_THAT(result65, kHasNoMissedVsyncs);
}

/*
Tests that the decider marks a frame as janky if it was delayed compared to the
immediately preceding frame (in which Chrome quickly presented an input in a
frame).

VSync    V0      V1      V2      V3 ... V62     V63  :  V64  :  V65     V66
         :       :       :       :  ...  :       :   :   :   :   :       :
Input    I1      I2      I3      I4 ... I63      :  I64  :  I65  :       :
         :       :       :       :  ...  :       :   :   :   :           :
F1(p):   |-16ms--|       :       :       :       :   :   :   :           :
F2(p):           |-16ms--|       :       :       :   :   :   :           :
F3(p):                   |-16ms--|       :       :   :   :   :           :
F4(p):                           |--...  :       :   :   :   :           :
...                                      :       :   :   :   :           :
F62(p):                             ...--|       :   :   :   :           :
F63(p):                             ...  |-16ms--|   :   :   :           :
F64(RD):                            ...              |8ms|   :           :
F65(RD):                                                     |----24ms---|

The decider SHOULD evaluate I65/F65 against I64/F64 (because it just happened),
so the decider SHOULD mark F65 as janky.
*/
TEST_P(SinglyParameterizedScrollJankV4DeciderTest,
       MissedVsyncImmediatelyAfterQuickInputFrameDelivery) {
  // Inputs 1-63 took 16 ms (one VSync) to deliver.
  for (int i = 1; i <= 63; i++) {
    base::TimeDelta offset = (i - 1) * kVsyncInterval;
    ScrollJankV4Result result = DecideJankForParameterizedFrame(
        GetParam(),
        {
            .if_real =
                Real{
                    .first_input_generation_ts = MillisSinceEpoch(100) + offset,
                    .last_input_generation_ts = MillisSinceEpoch(100) + offset,
                    .has_inertial_input = false,
                    .abs_total_raw_delta_pixels = 2.0f,
                    .max_abs_inertial_raw_delta_pixels = 0.0f},
            .if_synthetic = Synthetic{.first_input_begin_frame_ts =
                                          MillisSinceEpoch(100) + offset},
            .if_synthetic_only =
                {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                     false},
            .if_damaging = DamagingFrame{.presentation_ts =
                                             MillisSinceEpoch(116) + offset},
            .args = CreateBeginFrameArgs(MillisSinceEpoch(100) + offset),
        });
    EXPECT_THAT(result, kHasNoMissedVsyncs);
  }

  // Inputs 64 took only 8 ms (half a VSync) to deliver.
  ScrollJankV4Result result64 =
      decider_.DecideJankForFrameWithRealScrollUpdates(
          ScrollUpdates(
              /* earliest_event= */ nullptr,
              Real{.first_input_generation_ts = MillisSinceEpoch(1116),
                   .last_input_generation_ts = MillisSinceEpoch(1116),
                   .has_inertial_input = false,
                   .abs_total_raw_delta_pixels = 2.0f,
                   .max_abs_inertial_raw_delta_pixels = 0.0f},
              /* synthetic= */ std::nullopt),
          ScrollDamage{
              DamagingFrame{.presentation_ts = MillisSinceEpoch(1124)}},
          CreateBeginFrameArgs(MillisSinceEpoch(1108)));
  EXPECT_THAT(result64, kHasNoMissedVsyncs);

  // There's one VSync missed between F64 and F65. F65 should be evaluated
  // against the delivery cutoffs of the most recent frame (8 ms) rather than
  // the earlier frames (16 ms). Therefore, it's reasonable to assume that F65's
  // first input (generated at 1132 ms) could have been included in the missed
  // VSync (presented at 1140 ms), so F65 SHOULD be marked as janky.
  ScrollJankV4Result result65 =
      decider_.DecideJankForFrameWithRealScrollUpdates(
          ScrollUpdates(
              /* earliest_event= */ nullptr,
              Real{.first_input_generation_ts = MillisSinceEpoch(1132),
                   .last_input_generation_ts = MillisSinceEpoch(1132),
                   .has_inertial_input = false,
                   .abs_total_raw_delta_pixels = 2.0f,
                   .max_abs_inertial_raw_delta_pixels = 0.0f},
              /* synthetic= */ std::nullopt),
          ScrollDamage{
              DamagingFrame{.presentation_ts = MillisSinceEpoch(1156)}},
          CreateBeginFrameArgs(MillisSinceEpoch(1140)));
  EXPECT_THAT(
      result65,
      HasMissedVsyncs(
          JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1));
}

/*
Tests that the decider marks frames which missed one or more VSyncs in the
middle of a fast scroll as janky (even with sparse inputs).

VSync    V V V V V V V V V V V V V V V V V V V V V V V V V V
         : : : : : : : : : : : : : : : : :   : :           :
Input    I1I2  I3I4          I5        : :   : :           :
         : :   : :           :         : :   : :           :
F1(a):   |-----:-:-----------:---------| :   : :           :
F2(a):     |---:-:-----------:-----------|(A): :           :
F3(b):         |-:-----------:---------------| :           :
F4(b):           |-----------:-----------------|    (B)    :
F5(a):                       |-----------------------------|

Assuming I2-I5 are all above the fast scroll threshold (each have at least
3px absolute scroll delta), the decider should mark F3 and F5 janky with 1 (A)
and 5 (B) missed VSyncs respectively.
*/
TEST_P(DoublyParameterizedScrollJankV4DeciderTest,
       MissedVsyncDuringFastScroll) {
  ScrollJankV4Result result1 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(100),
                          .last_input_generation_ts = MillisSinceEpoch(100),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 2.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(324)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   true},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(340)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(324)),
      });
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  ScrollJankV4Result result2 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(116),
                          .last_input_generation_ts = MillisSinceEpoch(116),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 4.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(340)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   true},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(356)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(340)),
      });
  EXPECT_THAT(result2, kHasNoMissedVsyncs);

  // 1 VSync missed between F2 and F3, so F3 should be marked as JANKY UNLESS F1
  // and F2 were both synthetic.
  ScrollJankV4Result result3 = DecideJankForParameterizedFrame(
      frame_type_b_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(148),
                          .last_input_generation_ts = MillisSinceEpoch(148),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 4.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(372)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   true},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(388)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(372)),
      });
  if (frame_type_a_.has_real_inputs) {
    EXPECT_THAT(result3,
                HasMissedVsyncs(JankReason::kMissedVsyncDuringFastScroll, 1));
  } else {
    // If there were no real inputs before F3, then the metric won't consider
    // the scroll to be fast.
    EXPECT_THAT(result3, kHasNoMissedVsyncs);
  }

  ScrollJankV4Result result4 = DecideJankForParameterizedFrame(
      frame_type_b_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(164),
                          .last_input_generation_ts = MillisSinceEpoch(164),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 4.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(388)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   true},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(404)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(388)),
      });
  EXPECT_THAT(result4, kHasNoMissedVsyncs);

  // 5 VSyncs missed between F4 and F5, so F5 should be marked as JANKY UNLESS
  // F1-F4 were all synthetic.
  ScrollJankV4Result result5 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(260),
                          .last_input_generation_ts = MillisSinceEpoch(260),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 4.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(484)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   true},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(500)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(484)),
      });
  if (frame_type_a_.has_real_inputs || frame_type_b_.has_real_inputs) {
    EXPECT_THAT(result5,
                HasMissedVsyncs(JankReason::kMissedVsyncDuringFastScroll, 5));
  } else {
    // If there were no real inputs before F5, then the metric won't consider
    // the scroll to be fast.
    EXPECT_THAT(result5, kHasNoMissedVsyncs);
  }
}

/*
Tests that the decider does NOT mark frames which missed one or more VSyncs as
janky if inputs were sparse and the frames weren't in the middle of a fast
scroll.

VSync    V V V V V V V V V V V V V V V V V V V V V V V V V V
         : : : : : : : : : : : : : : : : :   : :           :
Input    I1I2  I3I4          I5        : :   : :           :
         : :   : :           :         : :   : :           :
F1(a):   |-----:-:-----------:---------| :   : :           :
F2(a):     |---:-:-----------:-----------|(A): :           :
F3(b):         |-:-----------:---------------| :           :
F4(b):           |-----------:-----------------|    (B)    :
F5(a):                       |-----------------------------|

If I2 or I3 is below the fast scroll threshold (has less than 3px absolute
scroll delta), the decider should NOT mark F3 as janky even though it missed 1
VSync (A). Similarly, if I4 or I5 are below the fast scroll threshold (has less
than 3px absolute scroll delta), the decider should NOT mark F5 as janky even
though it missed 5 VSyncs (B).
*/
TEST_P(DoublyParameterizedScrollJankV4DeciderTest,
       MissedVsyncOutsideFastScroll) {
  ScrollJankV4Result result1 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(100),
                          .last_input_generation_ts = MillisSinceEpoch(100),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 4.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(324)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   false},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(340)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(324)),
      });
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  ScrollJankV4Result result2 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(116),
                          .last_input_generation_ts = MillisSinceEpoch(116),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 2.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(340)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   false},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(356)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(340)),
      });
  EXPECT_THAT(result2, kHasNoMissedVsyncs);

  // 1 VSync missed between F2 and F3, BUT F2 has scroll delta below the fast
  // scroll threshold, so F3 should NOT be marked as janky.
  ScrollJankV4Result result3 = DecideJankForParameterizedFrame(
      frame_type_b_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(148),
                          .last_input_generation_ts = MillisSinceEpoch(148),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 4.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(372)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   false},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(388)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(372)),
      });
  EXPECT_THAT(result3, kHasNoMissedVsyncs);

  ScrollJankV4Result result4 = DecideJankForParameterizedFrame(
      frame_type_b_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(164),
                          .last_input_generation_ts = MillisSinceEpoch(164),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 4.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(388)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   false},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(404)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(388)),
      });
  EXPECT_THAT(result4, kHasNoMissedVsyncs);

  // 5 VSyncs missed between F4 and F5, BUT F5 has scroll delta below the fast
  // scroll threshold, so F5 should NOT be marked as janky.
  ScrollJankV4Result result5 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(260),
                          .last_input_generation_ts = MillisSinceEpoch(260),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 2.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(484)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   false},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(500)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(484)),
      });
  EXPECT_THAT(result5, kHasNoMissedVsyncs);
}

/*
Tests that the decider marks frames which missed one or more VSyncs at the
transition from a fast regular scroll to a fast fling as janky.

VSync    V  V  V  V  V  V  V  V  V  V
         :  :  :  :  :  :  :  :  :  :
Input    I1          I2 :           :
         :           :  :           :
F1(a):   |-----------:--|    (A)    :
F2(b):               |--------------|

I1 and I2 are regular and inertial scroll updates respectively. Assuming I1 is
above the fast scroll threshold (has at least 3 px absolute scroll delta) and I2
is above the fling threshold (has at least 0.2 px absolute scroll delta), the
decider should mark F2 as janky with 3 missed VSyncs (A).
*/
TEST_P(FlingTransitionDoublyParameterizedScrollJankV4DeciderTest,
       MissedVsyncAtTransitionFromFastRegularScrollToFastFling) {
  ScrollJankV4Result result1 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(100),
                          .last_input_generation_ts = MillisSinceEpoch(100),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 4.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(164)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   true},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(180)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(164)),
      });
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  // 3 VSync missed between F1 and F2, so F2 should be marked as JANKY UNLESS F1
  // was synthetic.
  ScrollJankV4Result result2 = DecideJankForParameterizedFrame(
      frame_type_b_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(164),
                          .last_input_generation_ts = MillisSinceEpoch(164),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 0.5f,
                          .max_abs_inertial_raw_delta_pixels = 0.5f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(228)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   true},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(244)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(228)),
      });
  if (frame_type_a_.has_real_inputs) {
    EXPECT_THAT(result2,
                HasMissedVsyncs(JankReason::kMissedVsyncAtStartOfFling, 3));
  } else {
    // If there were no real inputs before F2, then the metric won't consider
    // the scroll to be fast.
    EXPECT_THAT(result2, kHasNoMissedVsyncs);
  }
}

/*
Tests that the decider does NOT mark frames which missed one or more VSyncs at
the transition from a slow regular scroll to a fling as janky.

VSync    V  V  V  V  V  V  V  V  V  V
         :  :  :  :  :  :  :  :  :  :
Input    I1          I2 :           :
         :           :  :           :
F1(a):   |-----------:--|    (A)    :
F2(b):               |--------------|

I1 and I2 are regular and inertial scroll updates respectively. Assuming I1 is
below the fast scroll threshold (has less than 3 px absolute scroll delta), the
decider should NOT mark F2 as janky even though it missed 3 VSyncs (A).
*/
TEST_P(FlingTransitionDoublyParameterizedScrollJankV4DeciderTest,
       MissedVsyncAtTransitionFromSlowRegularScrollToFling) {
  ScrollJankV4Result result1 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(100),
                          .last_input_generation_ts = MillisSinceEpoch(100),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 2.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MicrosSinceEpoch(164)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   true},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(180)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(164)),
      });
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  // 3 VSync missed between F1 and F2, BUT F1 has scroll delta below the fast
  // scroll threshold, so F2 should NOT be marked as janky.
  ScrollJankV4Result result2 = DecideJankForParameterizedFrame(
      frame_type_b_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(164),
                          .last_input_generation_ts = MillisSinceEpoch(164),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 0.5f,
                          .max_abs_inertial_raw_delta_pixels = 0.5f},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(244)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(228)),
      });
  EXPECT_THAT(result2, kHasNoMissedVsyncs);
}

/*
Tests that the decider does NOT mark frames which missed one or more VSyncs at
the transition from a regular scroll to a slow fling as janky.

VSync    V  V  V  V  V  V  V  V  V  V
         :  :  :  :  :  :  :  :  :  :
Input    I1          I2 :           :
         :           :  :           :
F1(a):   |-----------:--|    (A)    :
F2(b):               |--------------|

I1 and I2 are regular and inertial scroll updates respectively. Assuming I2 is
below the fling threshold (has less than 0.2 px absolute scroll delta), the
decuder should NOT mark F2 as janky even though it missed 3 VSyncs (A).
*/
TEST_P(FlingTransitionDoublyParameterizedScrollJankV4DeciderTest,
       MissedVsyncAtTransitionFromRegularScrollToSlowFling) {
  ScrollJankV4Result result1 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(100),
                          .last_input_generation_ts = MillisSinceEpoch(100),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 4.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(164)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   false},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(180)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(164)),
      });
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  // 3 VSync missed between F1 and F2, BUT F2 has scroll delta below the fling
  // threshold, so F2 should NOT be marked as janky.
  ScrollJankV4Result result2 = DecideJankForParameterizedFrame(
      frame_type_b_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(164),
                          .last_input_generation_ts = MillisSinceEpoch(164),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 0.1f,
                          .max_abs_inertial_raw_delta_pixels = 0.1f},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(244)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(228)),
      });
  EXPECT_THAT(result2, kHasNoMissedVsyncs);
}

/*
Tests that the decider does NOT mark frames which didn't miss any VSyncs at the
transition from a regular scroll to a fling as janky.

VSync V  V  V  V  V  V  V
      :  :  :  :  :  :  :
Input I1 I2          :  :
      :  :           :  :
F1:   |--:-----------|  :
F2:      |--------------|

I1 and I2 are regular and inertial scroll updates respectively. The decider
should NOT mark F2 as janky because it didn't miss any VSyncs.
*/
TEST_P(FlingTransitionDoublyParameterizedScrollJankV4DeciderTest,
       NoMissedVsyncAtTransitionFromRegularScrollToFling) {
  ScrollJankV4Result result1 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(100),
                          .last_input_generation_ts = MillisSinceEpoch(100),
                          .has_inertial_input = false,
                          .abs_total_raw_delta_pixels = 4.0f,
                          .max_abs_inertial_raw_delta_pixels = 0.0f},
          .if_synthetic =
              Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(164)},
          .if_synthetic_only =
              {.future_real_frame_is_fast_scroll_or_sufficiently_fast_fling =
                   true},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(180)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(164)),
      });
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  // No VSyncs missed between F1 and F2, so F2 should NOT be marked as janky.
  ScrollJankV4Result result2 = DecideJankForParameterizedFrame(
      frame_type_b_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(116),
                          .last_input_generation_ts = MillisSinceEpoch(116),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 0.5f,
                          .max_abs_inertial_raw_delta_pixels = 0.5f},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(196)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(180)),
      });
  EXPECT_THAT(result2, kHasNoMissedVsyncs);
}

/*
Tests that the decider marks frames which missed one or more VSyncs in the
middle of a fast fling as janky.

VSync    V V V V V V V V V V V V V V V V V V V V V V V V V V
         : : : : : : : : : : : : : : : : :   : :           :
Input    I1I2  I3I4          I5        : :   : :           :
         : :   : :           :         : :   : :           :
F1(a):   |-----:-:-----------:---------| :   : :           :
F2(a):     |---:-:-----------:-----------|(A): :           :
F3(b):         |-:-----------:---------------| :           :
F4(b):           |-----------:-----------------|    (B)    :
F5(a):                       |-----------------------------|

I1-I5 are all inertial scroll updates. If I3 and I5 are above the fling
threshold (both have at least 0.2px absolute scroll delta), the decider should
mark F3 and F5 janky with 1 (A) and 5 (B) missed VSyncs respectively.
*/
TEST_P(MidFlingDoublyParameterizedScrollJankV4DeciderTest,
       MissedVsyncDuringFastFling) {
  ScrollJankV4Result result1 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(100),
                          .last_input_generation_ts = MillisSinceEpoch(100),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 0.5f,
                          .max_abs_inertial_raw_delta_pixels = 0.5f},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(340)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(324)),
      });
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  ScrollJankV4Result result2 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(116),
                          .last_input_generation_ts = MillisSinceEpoch(116),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 0.5f,
                          .max_abs_inertial_raw_delta_pixels = 0.5f},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(356)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(340)),
      });
  EXPECT_THAT(result2, kHasNoMissedVsyncs);

  // 1 VSync missed between F2 and F3, so F3 should be marked as JANKY.
  ScrollJankV4Result result3 = DecideJankForParameterizedFrame(
      frame_type_b_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(148),
                          .last_input_generation_ts = MillisSinceEpoch(148),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 0.5f,
                          .max_abs_inertial_raw_delta_pixels = 0.5f},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(388)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(372)),
      });
  EXPECT_THAT(result3, HasMissedVsyncs(JankReason::kMissedVsyncDuringFling, 1));

  ScrollJankV4Result result4 = DecideJankForParameterizedFrame(
      frame_type_b_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(164),
                          .last_input_generation_ts = MillisSinceEpoch(164),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 0.1f,
                          .max_abs_inertial_raw_delta_pixels = 0.1f},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(404)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(388)),
      });
  EXPECT_THAT(result4, kHasNoMissedVsyncs);

  // 5 VSyncs missed between F4 and F5 (EVEN THOUGH F4 has scroll delta below
  // the fling threshold), so F5 should be marked as JANKY.
  ScrollJankV4Result result5 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(260),
                          .last_input_generation_ts = MillisSinceEpoch(260),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 0.5f,
                          .max_abs_inertial_raw_delta_pixels = 0.5f},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(500)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(484)),
      });
  EXPECT_THAT(result5, HasMissedVsyncs(JankReason::kMissedVsyncDuringFling, 5));
}

/*
Tests that the decider does NOT mark frames which missed one or more VSyncs in
the middle of a slow fling (typically towards the end of a fling) as janky.

VSync    V V V V V V V V V V V V V V V V V V V V V V V V V V
         : : : : : : : : : : : : : : : : :   : :           :
Input    I1I2  I3I4          I5        : :   : :           :
         : :   : :           :         : :   : :           :
F1(a):   |-----:-:-----------:---------| :   : :           :
F2(a):     |---:-:-----------:-----------|(A): :           :
F3(b):         |-:-----------:---------------| :           :
F4(b):           |-----------:-----------------|    (B)    :
F5(a):                       |-----------------------------|

I1-I5 are all inertial scroll updates. If I3 is below the fling threshold (has
less than 0.2px absolute scroll delta), the decider should NOT mark F3 as janky
even though it missed one VSync (A). Similarly, if I5 is below the fling
threshold (has less than 0.2px absolute scroll delta), the decider should NOT
mark F5 as janky even though it missed 5 VSyncs (B).
*/
TEST_P(MidFlingDoublyParameterizedScrollJankV4DeciderTest,
       MissedVsyncDuringSlowFling) {
  ScrollJankV4Result result1 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(100),
                          .last_input_generation_ts = MillisSinceEpoch(100),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 0.5f,
                          .max_abs_inertial_raw_delta_pixels = 0.5f},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(300)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(284)),
      });
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  ScrollJankV4Result result2 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(116),
                          .last_input_generation_ts = MillisSinceEpoch(116),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 0.5f,
                          .max_abs_inertial_raw_delta_pixels = 0.5f},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(316)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(300)),
      });
  EXPECT_THAT(result2, kHasNoMissedVsyncs);

  // 1 VSync missed between F2 and F3, BUT F3 has scroll delta below the fling
  // threshold, so F3 should NOT be marked as janky.
  ScrollJankV4Result result3 = DecideJankForParameterizedFrame(
      frame_type_b_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(148),
                          .last_input_generation_ts = MillisSinceEpoch(148),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 0.1f,
                          .max_abs_inertial_raw_delta_pixels = 0.1f},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(348)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(332)),
      });
  EXPECT_THAT(result3, kHasNoMissedVsyncs);

  ScrollJankV4Result result4 = DecideJankForParameterizedFrame(
      frame_type_b_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(164),
                          .last_input_generation_ts = MillisSinceEpoch(164),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 0.1f,
                          .max_abs_inertial_raw_delta_pixels = 0.1f},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(364)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(348)),
      });
  EXPECT_THAT(result4, kHasNoMissedVsyncs);

  // 5 VSyncs missed between F4 and F5, BUT F5 has scroll delta below the fling
  // threshold, so F5 should NOT be marked as janky.
  ScrollJankV4Result result5 = DecideJankForParameterizedFrame(
      frame_type_a_,
      {
          .if_real = Real{.first_input_generation_ts = MillisSinceEpoch(260),
                          .last_input_generation_ts = MillisSinceEpoch(260),
                          .has_inertial_input = true,
                          .abs_total_raw_delta_pixels = 0.1f,
                          .max_abs_inertial_raw_delta_pixels = 0.1f},
          .if_damaging =
              DamagingFrame{.presentation_ts = MillisSinceEpoch(460)},
          .args = CreateBeginFrameArgs(MillisSinceEpoch(444)),
      });
  EXPECT_THAT(result5, kHasNoMissedVsyncs);
}

struct ScrollJankV4DeciderRunningConsistencyTestCase {
  std::string test_name;
  base::TimeTicks input_ts;
  int expected_missed_vsyncs;
};

class ScrollJankV4DeciderRunningConsistentyTests
    : public ScrollJankV4DeciderTest,
      public testing::WithParamInterface<
          ScrollJankV4DeciderRunningConsistencyTestCase> {};

/*
A parameterized test which verifies that the decider correctly calculates the
number of missed VSyncs (taking into account the discount factor and stability
correction).

     100   116   132   148   164   180   196   212   228   244   260
VSync V     V     V     V     V     V     V     V     V     V     V
      :     :     :     :     :     :     :     :     :     :     :
Input I1 I2 I3 I4 I5 I6       |     :     :                       :
      :  :  :  :  :  :        |     :     :                       :
F1:   |-----:--:--:--:-{I1,I2}|     :     :                       :
F2:         |-----:--:-------{I3,I4}|     :                       :
F3:               |--------------{I5,I6}--|                       :
F4:                     ?  ?  ?  ?  ?  ?  ?  ?  ------------------|
                     [ M=3 ](M=2 ](M=1 ](---------- M=0 ----------]

The test is parameterized by the generation timestamp of I7. I7's generation
timestamp directly influences whether the decider will mark F4 as janky and, if
so, with how many missed VSyncs. Intuitively, the later I7 arrives, the less
opportunity Chrome will have to present it in F4, so Chrome will have missed
fewer VSyncs.

We can see that delivery cut-off for each of F1-F3 (the duration between the
generation timestamp of the last input included in a frame and the frame's
presentation timestamp) is roughly 3.5 VSyncs. This implies approximately the
following (without taking the discount factor, stability correction and exact
timestamps into account):

  * If I7 was generated later than 4.5 VSyncs before F4 was presented (M=0),
    then the decider should mark it as non-janky.
  * If I7 was generated between 5.5 (exclusive) and 4.5 (inclusive) VSyncs
    before F4 was presented (M=1), then the decider should mark it as janky with
    1 missed VSync.
  * If I7 was generated between 6.5 (exclusive) and 5.5 (inclusive) VSyncs
    before F4 was presented (M=2), then the decider should mark it as janky with
    2 missed VSyncs.
  * If I7 was generated 6.5 VSyncs before F4 was presented or earlier (M=3),
    then the decider should mark it as janky with 3 missed VSyncs.
*/
TEST_P(ScrollJankV4DeciderRunningConsistentyTests,
       MissedVsyncDueToDeceleratingInputFrameDelivery) {
  const ScrollJankV4DeciderRunningConsistencyTestCase& params = GetParam();

  // F1: 164 - 108.1 = 55.9 ms delivery cutoff.
  ScrollJankV4Result result1 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(
          /* earliest_event= */ nullptr,
          Real{.first_input_generation_ts = MillisSinceEpoch(100),
               .last_input_generation_ts = MicrosSinceEpoch(108100),
               .has_inertial_input = false,
               .abs_total_raw_delta_pixels = 0.0f,
               .max_abs_inertial_raw_delta_pixels = 0.0f},
          /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(164)}},
      CreateBeginFrameArgs(MillisSinceEpoch(148)));
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  // F2: 180 - 124 = 56 ms delivery cutoff.
  ScrollJankV4Result result2 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(116),
                         .last_input_generation_ts = MillisSinceEpoch(124),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 0.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(180)}},
      CreateBeginFrameArgs(MillisSinceEpoch(164)));
  EXPECT_THAT(result2, kHasNoMissedVsyncs);

  // F3: 196 - 139.8 = 56.2 ms delivery cutoff
  ScrollJankV4Result result3 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(
          /* earliest_event= */ nullptr,
          Real{.first_input_generation_ts = MillisSinceEpoch(132),
               .last_input_generation_ts = MicrosSinceEpoch(139800),
               .has_inertial_input = false,
               .abs_total_raw_delta_pixels = 0.0f,
               .max_abs_inertial_raw_delta_pixels = 0.0f},
          /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(196)}},
      CreateBeginFrameArgs(MillisSinceEpoch(180)));
  EXPECT_THAT(result3, kHasNoMissedVsyncs);

  // 3 VSyncs missed between F3 and F4. Whether the first input in F4 could have
  // been presented one or more VSyncs earlier is determined by:
  //
  //     floor((
  //       `f4.presentation_ts`
  //         + (`kDiscountFactor` + `kStabilityCorrection`) * `kVsyncInterval`
  //         - min(
  //             `f1.presentation_ts` - `f1.last_input_ts`
  //               + 6 * `kDiscountFactor` * `kVsyncInterval`,
  //             `f2.presentation_ts` - `f2.last_input_ts`
  //               + 5 * `kDiscountFactor` * `kVsyncInterval`,
  //             `f3.presentation_ts` - `f3.last_input_ts`
  //               + 4 * `kDiscountFactor` * `kVsyncInterval`,
  //           )
  //         - `params.input_ts`
  //     ) / ((1 - `kDiscountFactor`) * `kVsyncInterval`))
  //   = floor((
  //       260 + 6% * 16
  //         - min(55.9 + 6% * 16, 56 + 5% * 16, 56.2 + 4% * 16)
  //         - `params.input_ts`
  //     ) / (99% * 16))
  //   = floor((
  //       260 + 0.96 - min(56.86, 56.8, 56.84) - `params.input_ts`
  //     ) / 15.84)
  //   = floor((260 + 0.96 - 56.8 - `params.input_ts`) / 15.84)
  //   = floor((204.16 - `params.input_ts`) / 15.84)
  //
  // For example, if `params.input_ts` (I7's generation timestamp) is 157 ms,
  // then the formula above resolves to floor(2.98) = 2, which means that F4
  // should be marked as JANKY with 2 missed VSyncs.
  ScrollJankV4Result result4 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = params.input_ts,
                         .last_input_generation_ts = params.input_ts,
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 0.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(260)}},
      CreateBeginFrameArgs(MillisSinceEpoch(244)));
  EXPECT_THAT(result4,
              HasMissedVsyncs(
                  JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery,
                  params.expected_missed_vsyncs));
}

INSTANTIATE_TEST_SUITE_P(
    ScrollJankV4DeciderRunningConsistentyTests,
    ScrollJankV4DeciderRunningConsistentyTests,
    // The expected number of missed VSyncs is (see above):
    //
    //   V = floor((204.16 - `params.input_ts`) / 15.84)
    //
    // Given a fixed number of missed VSyncs V, this can be re-arranged as:
    //
    //   (204.16 - `params.input_ts`) / 15.84 in [V, V + 1)
    //   (204.16 - `params.input_ts`) in [15.84 * V, 15.84 * (V + 1))
    //   `params.input_ts` in (204.16 - 15.84 * (V + 1), 204.16 - 15.84 * V]
    //   `params.input_ts` in (188.32 - 15.84 * V, 204.16 - 15.84 * V]
    //
    // Going back to the diagram above the
    // `MissedVsyncDueToDeceleratingInputFrameDeliveryV4` test case, we get the
    // following logic:
    //
    //   * If `params.input_ts` > 188.32 ms, F4 is not janky (M=0).
    //   * If 172.48 ms < `params.input_ts` <= 188.32 ms, F4 is janky with 1
    //     missed VSync (M=1).
    //   * If 156.64 ms < `params.input_ts` <= 172.48 ms, F4 is janky with 2
    //     missed VSyncs (M=2).
    //   * If `params.input_ts` <= 156.64 ms, F4 is janky with 3 missed VSyncs
    //     (M=3).
    //
    // The parameters below corresponds to the boundaries in the above logic.
    testing::ValuesIn<ScrollJankV4DeciderRunningConsistencyTestCase>({
        {.test_name = "MaxInputTimestampFor3MissedVsyncs",
         .input_ts = MicrosSinceEpoch(156640),
         .expected_missed_vsyncs = 3},
        {.test_name = "MinInputTimestampFor2MissedVsyncs",
         .input_ts = MicrosSinceEpoch(156641),
         .expected_missed_vsyncs = 2},
        {.test_name = "MaxInputTimestampFor2MissedVsyncs",
         .input_ts = MicrosSinceEpoch(172480),
         .expected_missed_vsyncs = 2},
        {.test_name = "MinInputTimestampFor1MissedVsync",
         .input_ts = MicrosSinceEpoch(172481),
         .expected_missed_vsyncs = 1},
        {.test_name = "MaxInputTimestampFor1MissedVsync",
         .input_ts = MicrosSinceEpoch(188320),
         .expected_missed_vsyncs = 1},
        {.test_name = "MinInputTimestampFor0MissedVsyncs",
         .input_ts = MicrosSinceEpoch(188321),
         .expected_missed_vsyncs = 0},
    }),
    [](const testing::TestParamInfo<
        ScrollJankV4DeciderRunningConsistentyTests::ParamType>& info) {
      return info.param.test_name;
    });

/*
Tests that the decider can handle a scenario where the scroll starts with
non-damaging frames.

                   <--- regular scroll | fling --->
VSync V     V     V     V     V     V     V     V     V     V     V     V     V
Input  I0 I1 I2 I3:     :I4 I5:     :    I6     :          I7    I8           :
        | |   | | :     : | | :     :     |     :           |     |           :
F1:     |---------BF----|     :     :     :     :           :     :           :
F2:           |---------BF-xxx:     :     :     :           :     :           :
F3:                       |---------BF-xxx:     :           :     :           :
F4:                           :     :     |BFxxx:           :     :           :
F5:                           :     :           :           |BFxxx:           :
F6:                           :     :           :           :     |BF---------|
                              <jank->           <---jank---->

Assuming I2+I3 and I4+I5 are above the fast scroll threshold (each pair has at
least 3px absolute total scroll delta), the decider should mark F3 as janky with
1 missed VSync. Furthermore, assuming and I7 is above the fling threshold (has
at least 0.2 px absolute scroll delta), the decider should mark F5 as janky with
2 missed VSyncs.
 */
TEST_F(ScrollJankV4DeciderTest, JankyNonDamagingFrames) {
  ScrollJankV4Result result1 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(103),
                         .last_input_generation_ts = MillisSinceEpoch(111),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 5.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(148)}},
      CreateBeginFrameArgs(MillisSinceEpoch(132)));
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  ScrollJankV4Result result2 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(119),
                         .last_input_generation_ts = MillisSinceEpoch(127),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 5.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{NonDamagingFrame{}},
      CreateBeginFrameArgs(MillisSinceEpoch(148)));
  EXPECT_THAT(result2, kHasNoMissedVsyncs);

  ScrollJankV4Result result3 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(151),
                         .last_input_generation_ts = MillisSinceEpoch(159),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 5.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{NonDamagingFrame{}},
      CreateBeginFrameArgs(MillisSinceEpoch(180)));
  EXPECT_THAT(result3,
              HasMissedVsyncs(JankReason::kMissedVsyncDuringFastScroll, 1));

  ScrollJankV4Result result4 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(196),
                         .last_input_generation_ts = MillisSinceEpoch(196),
                         .has_inertial_input = true,
                         .abs_total_raw_delta_pixels = 2.0f,
                         .max_abs_inertial_raw_delta_pixels = 2.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{NonDamagingFrame{}},
      CreateBeginFrameArgs(MillisSinceEpoch(196)));
  EXPECT_THAT(result4, kHasNoMissedVsyncs);

  ScrollJankV4Result result5 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(244),
                         .last_input_generation_ts = MillisSinceEpoch(244),
                         .has_inertial_input = true,
                         .abs_total_raw_delta_pixels = 2.0f,
                         .max_abs_inertial_raw_delta_pixels = 2.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{NonDamagingFrame{}},
      CreateBeginFrameArgs(MillisSinceEpoch(244)));
  EXPECT_THAT(result5, HasMissedVsyncs(JankReason::kMissedVsyncDuringFling, 2));

  ScrollJankV4Result result6 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(260),
                         .last_input_generation_ts = MillisSinceEpoch(260),
                         .has_inertial_input = true,
                         .abs_total_raw_delta_pixels = 2.0f,
                         .max_abs_inertial_raw_delta_pixels = 2.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(292)}},
      CreateBeginFrameArgs(MillisSinceEpoch(260)));
  EXPECT_THAT(result6, kHasNoMissedVsyncs);
}

/*
Tests that the decider can mark a non-damaging frame as janky due to the running
consistency rule.

VSync V0    V1    V2    V3    V4    V5    V6    V7    V8    V9    V10
Input  :I0 I1:I2 I3:     :I4 I5:I6 I7:I8 I9:     :           :     :
       : | | : | | :     : | | : | | : | | :     :           :     :
F1:      |---------BF----|     : |   : |   :     :           :     :
F2:            |---------------BF-xxx: |   :     :           :     :
F3                       : |---------BF----|     :           :     :
F4:                      :     : |---------BF-xxx:           :     :
F5:                      :     :       |---------------------BF-zzz:
                         <jank->                 <---jank---->

The decider should mark F2 and F5 janky with 1 and 2 missed VSyncs respectively.
Rationale: I2 should have been included in the begin frame that started at V3.
I8 should have been included in the begin frame that started at V7.
 */
TEST_F(ScrollJankV4DeciderTest,
       JankyNonDamagingFramesViolatingRunningConsistency) {
  ScrollJankV4Result result1 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(103),
                         .last_input_generation_ts = MillisSinceEpoch(111),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 0.1f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(148)}},
      CreateBeginFrameArgs(MillisSinceEpoch(132)));
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  ScrollJankV4Result result2 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(119),
                         .last_input_generation_ts = MillisSinceEpoch(127),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 0.1f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{NonDamagingFrame{}},
      CreateBeginFrameArgs(MillisSinceEpoch(164)));
  EXPECT_THAT(
      result2,
      HasMissedVsyncs(
          JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1));

  ScrollJankV4Result result3 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(151),
                         .last_input_generation_ts = MillisSinceEpoch(159),
                         .has_inertial_input = true,
                         .abs_total_raw_delta_pixels = 0.1f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{DamagingFrame{.presentation_ts = MillisSinceEpoch(196)}},
      CreateBeginFrameArgs(MillisSinceEpoch(180)));
  EXPECT_THAT(result3, kHasNoMissedVsyncs);

  ScrollJankV4Result result4 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(167),
                         .last_input_generation_ts = MillisSinceEpoch(175),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 0.1f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{NonDamagingFrame{}},
      CreateBeginFrameArgs(MillisSinceEpoch(196)));
  EXPECT_THAT(result4, kHasNoMissedVsyncs);

  ScrollJankV4Result result5 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(183),
                         .last_input_generation_ts = MillisSinceEpoch(191),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 0.1f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      ScrollDamage{NonDamagingFrame{}},
      CreateBeginFrameArgs(MillisSinceEpoch(244)));
  EXPECT_THAT(
      result5,
      HasMissedVsyncs(
          JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 2));
}

/*
Tests a scenario where a frame that contains both real and synthetic scroll
updates is marked as janky because the real scroll updates violate the running
consistency rule.

time  100     116     132     148     164
vsync  |       |       |       |       |
input   I0  I1  I2  I3         S1
        |   |   |   |          :
F1:     |------BF------|       :
F2:             |--------------BF------|
                               <-jank-->

F2 contains real inputs I2 and I3 generated at 119 ms and 127 ms. It also
contains a synthetic input S1 that was predicted at 148 ms. The decider should
mark F2 as janky with 1 missed VSync because Chrome should have presented the
real inputs 1 VSync earlier.
 */
TEST_F(ScrollJankV4DeciderTest,
       BothRealAndSyntheticFrameJankyDueToRealScrollUpdates) {
  ScrollJankV4Result result1 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(103),
                         .last_input_generation_ts = MillisSinceEpoch(111),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 2.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      DamagingFrame{.presentation_ts = MillisSinceEpoch(132)},
      CreateBeginFrameArgs(MillisSinceEpoch(116)));
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  ScrollJankV4Result result2 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(
          /* earliest_event= */ nullptr,
          Real{.first_input_generation_ts = MillisSinceEpoch(119),
               .last_input_generation_ts = MillisSinceEpoch(127),
               .has_inertial_input = false,
               .abs_total_raw_delta_pixels = 2.0f,
               .max_abs_inertial_raw_delta_pixels = 0.0f},
          Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(148)}),
      DamagingFrame{.presentation_ts = MillisSinceEpoch(164)},
      CreateBeginFrameArgs(MillisSinceEpoch(148)));
  EXPECT_THAT(
      result2,
      HasMissedVsyncs(
          JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1));
}

/*
Tests a scenario where a frame that contains both real and synthetic scroll
updates is marked as janky because the real scroll updates violate the running
consistency rule.

time  100     116     132     148     164
vsync  |       |       |       |       |
input   I0  I1         S1   I3
        |   |          |I2  |
        |   |          ||   |
F1:     |------BF-------|   |
F2:                    |-------BF------|
                               <-jank-->

F2 contains real inputs I2 and I3 generated at 135 ms and 143 ms. It also
contains a synthetic input that was predicted at 132 ms. The metric extrapolates
the synthetic input's generation timestamp to 127 ms (assuming the same duration
between input generation and begin frame timestamps as the previous real input
I1). Based on past performance, Chrome should have been able to present the
synthetic input at 148 ms. The decider should therefore mark F2 as janky with 1
missed VSync.
 */
TEST_F(ScrollJankV4DeciderTest,
       BothRealAndSyntheticFrameJankyDueToRealSyntheticUpdates) {
  ScrollJankV4Result result1 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{.first_input_generation_ts = MillisSinceEpoch(103),
                         .last_input_generation_ts = MillisSinceEpoch(111),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 2.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt),
      DamagingFrame{.presentation_ts = MillisSinceEpoch(132)},
      CreateBeginFrameArgs(MillisSinceEpoch(116)));
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  ScrollJankV4Result result2 = decider_.DecideJankForFrameWithRealScrollUpdates(
      ScrollUpdates(
          /* earliest_event= */ nullptr,
          Real{.first_input_generation_ts = MillisSinceEpoch(135),
               .last_input_generation_ts = MillisSinceEpoch(143),
               .has_inertial_input = false,
               .abs_total_raw_delta_pixels = 2.0f,
               .max_abs_inertial_raw_delta_pixels = 0.0f},
          Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(132)}),
      DamagingFrame{.presentation_ts = MillisSinceEpoch(164)},
      CreateBeginFrameArgs(MillisSinceEpoch(148)));
  EXPECT_THAT(
      result2,
      HasMissedVsyncs(
          JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1));
}

TEST_F(ScrollJankV4DeciderTest, IsValidFrame) {
  EXPECT_TRUE(ScrollJankV4Decider::IsValidFrame(
      ScrollUpdates(
          /* earliest_event= */ nullptr,
          Real{
              .first_input_generation_ts = MillisSinceEpoch(80),
              .last_input_generation_ts = MillisSinceEpoch(80),
          },
          Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(80)}),
      DamagingFrame{.presentation_ts = MillisSinceEpoch(100)},
      CreateBeginFrameArgs(MillisSinceEpoch(90))));
}

TEST_F(ScrollJankV4DeciderTest,
       IsNotValidFrameWithArgsFrameTimeAfterPresentation) {
  // Violates `args.frame_time < presentation_ts`.
  EXPECT_FALSE(ScrollJankV4Decider::IsValidFrame(
      ScrollUpdates(
          /* earliest_event= */ nullptr,
          Real{
              .first_input_generation_ts = MillisSinceEpoch(80),
              .last_input_generation_ts = MillisSinceEpoch(80),
          },
          Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(80)}),
      DamagingFrame{.presentation_ts = MillisSinceEpoch(100)},
      CreateBeginFrameArgs(MillisSinceEpoch(101))));
}

TEST_F(ScrollJankV4DeciderTest,
       IsNotValidFrameWithInputGenerationAfterPresentation) {
  // Real update violates `last_input_generation_ts < presentation_ts`.
  EXPECT_FALSE(ScrollJankV4Decider::IsValidFrame(
      ScrollUpdates(
          /* earliest_event= */ nullptr,
          Real{
              .first_input_generation_ts = MillisSinceEpoch(80),
              .last_input_generation_ts = MillisSinceEpoch(101),
          },
          Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(80)}),
      DamagingFrame{.presentation_ts = MillisSinceEpoch(100)},
      CreateBeginFrameArgs(MillisSinceEpoch(90))));
}

TEST_F(ScrollJankV4DeciderTest,
       IsNotValidFrameWithInputBeginFrameAfterPresentation) {
  // Synthetic update violates `first_input_begin_frame_ts < presentation_ts`.
  EXPECT_FALSE(ScrollJankV4Decider::IsValidFrame(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{
                        .first_input_generation_ts = MillisSinceEpoch(80),
                        .last_input_generation_ts = MillisSinceEpoch(80),
                    },
                    Synthetic{
                        .first_input_begin_frame_ts = MillisSinceEpoch(101),
                    }),
      DamagingFrame{.presentation_ts = MillisSinceEpoch(100)},
      CreateBeginFrameArgs(MillisSinceEpoch(90))));
}

TEST_F(ScrollJankV4DeciderTest, IsNotValidFrameWithFirstInputAfterLastInput) {
  // Real update violates
  // `first_input_generation_ts <= last_input_generation_ts`.
  EXPECT_FALSE(ScrollJankV4Decider::IsValidFrame(
      ScrollUpdates(
          /* earliest_event= */ nullptr,
          Real{
              .first_input_generation_ts = MillisSinceEpoch(81),
              .last_input_generation_ts = MillisSinceEpoch(80),
          },
          Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(80)}),
      DamagingFrame{.presentation_ts = MillisSinceEpoch(100)},
      CreateBeginFrameArgs(MillisSinceEpoch(90))));
}

TEST_F(ScrollJankV4DeciderTest,
       IsNotValidFrameWithFirstInputBeginFrameAfterArgsFrameTime) {
  // Synthetic update violates `first_input_begin_frame_ts <= args.frame_time`.
  EXPECT_FALSE(ScrollJankV4Decider::IsValidFrame(
      ScrollUpdates(/* earliest_event= */ nullptr,
                    Real{
                        .first_input_generation_ts = MillisSinceEpoch(80),
                        .last_input_generation_ts = MillisSinceEpoch(80),
                    },
                    Synthetic{
                        .first_input_begin_frame_ts = MillisSinceEpoch(91),
                    }),
      DamagingFrame{.presentation_ts = MillisSinceEpoch(100)},
      CreateBeginFrameArgs(MillisSinceEpoch(90))));
}

TEST_F(ScrollJankV4DeciderTest, IsFastScroll) {
  EXPECT_TRUE(ScrollJankV4Decider::IsFastScroll(
      Real{.abs_total_raw_delta_pixels = 4.0}));
}

TEST_F(ScrollJankV4DeciderTest, IsNotFastScroll) {
  EXPECT_FALSE(ScrollJankV4Decider::IsFastScroll(
      Real{.abs_total_raw_delta_pixels = 2.0}));
}

TEST_F(ScrollJankV4DeciderTest, IsSufficientlyFastFling) {
  EXPECT_TRUE(ScrollJankV4Decider::IsSufficientlyFastFling(Real{
      .has_inertial_input = true, .max_abs_inertial_raw_delta_pixels = 0.3}));
}

TEST_F(ScrollJankV4DeciderTest, IsNotSufficientlyFastFling) {
  EXPECT_FALSE(ScrollJankV4Decider::IsSufficientlyFastFling(Real{
      .has_inertial_input = true, .max_abs_inertial_raw_delta_pixels = 0.1}));
}

}  // namespace
}  // namespace cc
