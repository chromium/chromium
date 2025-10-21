// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_decider.h"

#include <optional>
#include <string>

#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
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

using ScrollJankV4Result = ScrollUpdateEventMetrics::ScrollJankV4Result;

/* Matches a result iff `matcher` matches `result->missed_vsyncs_per_reason`. */
::testing::Matcher<const std::optional<ScrollJankV4Result>&>
HasMissedVsyncsPerReasonMatching(
    ::testing::Matcher<const JankReasonArray<int>&> matcher) {
  return ::testing::Optional(
      ::testing::Field(&ScrollJankV4Result::missed_vsyncs_per_reason, matcher));
}

const ::testing::Matcher<const std::optional<ScrollJankV4Result>&>
    kHasNoMissedVsyncs =
        HasMissedVsyncsPerReasonMatching(::testing::Each(::testing::Eq(0)));

::testing::Matcher<const std::optional<ScrollJankV4Result>&> HasMissedVsyncs(
    JankReason reason,
    int missed_vsyncs) {
  JankReasonArray<int> expected_missed_vsyncs = {};
  expected_missed_vsyncs[static_cast<int>(reason)] = missed_vsyncs;
  return HasMissedVsyncsPerReasonMatching(
      ::testing::ElementsAreArray(expected_missed_vsyncs));
}

}  // namespace

class ScrollJankV4DeciderTest : public testing::Test {
 protected:
  ScrollJankV4Decider decider_;
};

/*
Test that regular frame production doesn't cause missed frames.

vsync                         v0      v1      v2
                              |       |       |
input   I0  I1  I2  I3  I4  I5
        |   |   |   |   |   |
F1:     |---------------------| {I0, I1}
F2:             |---------------------| {I2, I3}
F3:                     |---------------------| {I4, I5}
 */
TEST_F(ScrollJankV4DeciderTest, FrameProducedEveryVsync) {
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(103),
          /* last_input_generation_ts= */ MillisSinceEpoch(111),
          /* presentation_ts= */ MillisSinceEpoch(148),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ true,
          /* abs_total_raw_delta_pixels= */ 10.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 10.0f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(119),
          /* last_input_generation_ts= */ MillisSinceEpoch(127),
          /* presentation_ts= */ MillisSinceEpoch(164),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ true,
          /* abs_total_raw_delta_pixels= */ 10.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 10.0f);
  EXPECT_THAT(result2, kHasNoMissedVsyncs);

  std::optional<ScrollJankV4Result> result3 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(135),
          /* last_input_generation_ts= */ MillisSinceEpoch(143),
          /* presentation_ts= */ MillisSinceEpoch(180),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ true,
          /* abs_total_raw_delta_pixels= */ 10.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 10.0f);
  EXPECT_THAT(result3, kHasNoMissedVsyncs);
}

/*
Test that sporadic input timing doesn't cause missed frames when no
frame is expected.
vsync                       v0              v1
                    |       |       |       |
input   I0  I1        I2  I3
        |   |         |   |
F1:     |-------------------| {I0, I1}
F2:                   |---------------------| {I2, I3}
 */
TEST_F(ScrollJankV4DeciderTest, NoFrameProducedForMissingInput) {
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(103),
          /* last_input_generation_ts= */ MillisSinceEpoch(111),
          /* presentation_ts= */ MillisSinceEpoch(148),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(135),
          /* last_input_generation_ts= */ MillisSinceEpoch(143),
          /* presentation_ts= */ MillisSinceEpoch(180),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result2, kHasNoMissedVsyncs);
}

/*
Test that when a frame took too long to be produced shows up in the metric.
vsync                   v0              v1        v2
                        |    |    |     |    |    |
input   I0  I1  I2  I3  I4  I5
        |   |   |   |   |   |
F1:     |---------------| {I0, I1}
F2:             |-----------------------| {I2, I3}
F3:                     |-------------------------| {I4, I5}
 */
TEST_F(ScrollJankV4DeciderTest, MissedVsyncWhenInputWasPresent) {
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(103),
          /* last_input_generation_ts= */ MillisSinceEpoch(111),
          /* presentation_ts= */ MillisSinceEpoch(148),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(119),
          /* last_input_generation_ts= */ MillisSinceEpoch(127),
          /* presentation_ts= */ MillisSinceEpoch(196),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(
      result2,
      HasMissedVsyncs(
          JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 2));

  std::optional<ScrollJankV4Result> result3 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(135),
          /* last_input_generation_ts= */ MillisSinceEpoch(143),
          /* presentation_ts= */ MillisSinceEpoch(228),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(
      result3,
      HasMissedVsyncs(
          JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1));
}

// Regression test for https://crbug.com/404637348.
TEST_F(ScrollJankV4DeciderTest, ScrollWithZeroVsyncs) {
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(103),
          /* last_input_generation_ts= */ MillisSinceEpoch(111),
          /* presentation_ts= */ MillisSinceEpoch(148),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  // A malformed frame whose presentation timestamp is less than half a vsync
  // greater than than the previous frame's presentation timestamp.
  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(119),
          /* last_input_generation_ts= */ MillisSinceEpoch(127),
          /* presentation_ts= */ MillisSinceEpoch(149),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result2, kHasNoMissedVsyncs);
}

/*
Tests that the decider ignores frames which contain inputs that were generated
after the frame was presented.

VSync V  V  V  V  V  V  V  V  V  V
      :  :  :  :  :  :  :  :  :  :
Input I1    :  I2 I3 :           :
      :     :  :  :  :           :
F1:   |-----:--:--:--|           :
F2:         |<!|  :              :
F3:               |--------------|

F2 was presented before I2 was generated, which is unexpected, so the decider
should completely ignore it. It should then evaluate F3 against F1 only.
*/
TEST_F(ScrollJankV4DeciderTest, InputGeneratedAfterItWasPresented) {
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(100),
          /* last_input_generation_ts= */ MillisSinceEpoch(100),
          /* presentation_ts= */ MillisSinceEpoch(180),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  // A malformed frame which contains an input that was generated after the
  // frame was presented. The decider should completely ignore the frame and not
  // return any result.
  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(148),
          /* last_input_generation_ts= */ MillisSinceEpoch(148),
          /* presentation_ts= */ MillisSinceEpoch(132),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_EQ(result2, std::nullopt);

  // The decider should ignore the malformed frame when assessing subsequent
  // frames.
  std::optional<ScrollJankV4Result> result3 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(164),
          /* last_input_generation_ts= */ MillisSinceEpoch(164),
          /* presentation_ts= */ MillisSinceEpoch(244),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result3, kHasNoMissedVsyncs);
}

/*
Tests that the decider ignores frames which arrive out of order.

VSync V  V  V  V  V  V  V  V
      :  :  :  :  :  :  :  :
Input I1 I2 I3 :     :     :
      :  :  :  :     :     :
F1:   |--:--:--:-----|     :
F2:      |--:--|           :
F3:         |--------------|

F2 was presented before F1, which is unexpected, so the decider should
completely ignore it. It should then evaluate F3 against F1 only.
*/
TEST_F(ScrollJankV4DeciderTest, OutOfOrderFrameTermination) {
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(100),
          /* last_input_generation_ts= */ MillisSinceEpoch(100),
          /* presentation_ts= */ MillisSinceEpoch(180),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  // A malformed frame whose presentation timestamp before the previous frame.
  // The decider should completely ignore it and not return any result.
  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(116),
          /* last_input_generation_ts= */ MillisSinceEpoch(116),
          /* presentation_ts= */ MillisSinceEpoch(148),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_EQ(result2, std::nullopt);

  // The decider should ignore the malformed frame when assessing subsequent
  // frames.
  std::optional<ScrollJankV4Result> result3 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(132),
          /* last_input_generation_ts= */ MillisSinceEpoch(132),
          /* presentation_ts= */ MillisSinceEpoch(212),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result3, kHasNoMissedVsyncs);
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
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(108),
          /* last_input_generation_ts= */ MillisSinceEpoch(108),
          /* presentation_ts= */ MillisSinceEpoch(116),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  decider_.OnScrollEnded();
  decider_.OnScrollStarted();

  // Scroll 2: Inputs 2 and 3 took 40 ms (2.5 VSyncs) to deliver.
  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(124),
          /* last_input_generation_ts= */ MillisSinceEpoch(124),
          /* presentation_ts= */ MillisSinceEpoch(164),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result2, kHasNoMissedVsyncs);
  std::optional<ScrollJankV4Result> result3 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(140),
          /* last_input_generation_ts= */ MillisSinceEpoch(140),
          /* presentation_ts= */ MillisSinceEpoch(180),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result3, kHasNoMissedVsyncs);
}

/*
Same as `EvaluatesEachScrollSeparately` but without a call to
`ScrollJankV4Decider::OnScrollEnded()`.
*/
TEST_F(ScrollJankV4DeciderTest, EvaluatesEachScrollSeparatelyScrollStartOnly) {
  // Scroll 1: First input took only 8 ms (half a VSync) to deliver.
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(108),
          /* last_input_generation_ts= */ MillisSinceEpoch(108),
          /* presentation_ts= */ MillisSinceEpoch(116),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  decider_.OnScrollStarted();

  // Scroll 2: Inputs 2 and 3 took 40 ms (2.5 VSyncs) to deliver.
  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(124),
          /* last_input_generation_ts= */ MillisSinceEpoch(124),
          /* presentation_ts= */ MillisSinceEpoch(164),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result2, kHasNoMissedVsyncs);
  std::optional<ScrollJankV4Result> result3 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(140),
          /* last_input_generation_ts= */ MillisSinceEpoch(140),
          /* presentation_ts= */ MillisSinceEpoch(180),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result3, kHasNoMissedVsyncs);
}

/*
Same as `EvaluatesEachScrollSeparately` but without a call to
`ScrollJankV4Decider::OnScrollStarted()`.
*/
TEST_F(ScrollJankV4DeciderTest, EvaluatesEachScrollSeparatelyScrollEndOnly) {
  // Scroll 1: First input took only 8 ms (half a VSync) to deliver.
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(108),
          /* last_input_generation_ts= */ MillisSinceEpoch(108),
          /* presentation_ts= */ MillisSinceEpoch(116),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  decider_.OnScrollEnded();

  // Scroll 2: Inputs 2 and 3 took 40 ms (2.5 VSyncs) to deliver.
  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(124),
          /* last_input_generation_ts= */ MillisSinceEpoch(124),
          /* presentation_ts= */ MillisSinceEpoch(164),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result2, kHasNoMissedVsyncs);
  std::optional<ScrollJankV4Result> result3 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(140),
          /* last_input_generation_ts= */ MillisSinceEpoch(140),
          /* presentation_ts= */ MillisSinceEpoch(180),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result3, kHasNoMissedVsyncs);
}

/*
Tests that the decider doesn't unfairly mark a frame as janky just because
Chrome "got lucky" (quickly presented an input in a frame) once many frames ago.

VSync V0  :   V1      V2      V3 ... V62     V63     V64  :  V65     V66
      :   :   :       :       :  ...  :       :       :   :   :       :
Input :   I1  I2      I3      I4 ... I63     I64      :  I65  :       :
          :   :       :       :  ...  :       :       :   :           :
F1:       |8ms|       :       :       :       :       :   :           :
F2:           |-16ms--|       :       :       :       :   :           :
F3:                   |-16ms--|       :       :       :   :           :
F4:                           |--...  :       :       :   :           :
...                                   :       :       :   :           :
F62:                             ...--|       :       :   :           :
F63:                             ...  |-16ms--|       :   :           :
F64:                             ...          |-16ms--|   :           :
F65:                                                      |----24ms---|

The decider should NOT evaluate I65/F65 against I1/F1 (because it happened a
long time ago), so the decider should NOT mark F65 as janky.
*/
TEST_F(ScrollJankV4DeciderTest, MissedVsyncLongAfterQuickInputFrameDelivery) {
  // First input took only 8 ms (half a VSync) to deliver.
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(108),
          /* last_input_generation_ts= */ MillisSinceEpoch(108),
          /* presentation_ts= */ MillisSinceEpoch(116),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  // Inputs 2-64 took 16 ms (one VSync) to deliver.
  for (int i = 2; i <= 64; i++) {
    base::TimeDelta offset = (i - 2) * kVsyncInterval;
    std::optional<ScrollJankV4Result> result =
        decider_.DecideJankForPresentedFrame(
            /* first_input_generation_ts= */ MillisSinceEpoch(116) + offset,
            /* last_input_generation_ts= */ MillisSinceEpoch(116) + offset,
            /* presentation_ts= */ MillisSinceEpoch(132) + offset,
            /* vsync_interval= */ kVsyncInterval,
            /* has_inertial_input= */ false,
            /* abs_total_raw_delta_pixels= */ 2.0f,
            /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
    EXPECT_THAT(result, kHasNoMissedVsyncs);
  }

  // There's one VSync missed between F64 and F65. F65 should be evaluated
  // against the delivery cutoffs of the recent frames (16 ms) rather than the
  // first frame (8 ms). Therefore, it's not reasonable to assume that F65's
  // first input (generated at 1132 ms) could have been included in the missed
  // VSync (presented at 1140 ms), so F65 should NOT be marked as janky.
  std::optional<ScrollJankV4Result> result65 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(1132),
          /* last_input_generation_ts= */ MillisSinceEpoch(1132),
          /* presentation_ts= */ MillisSinceEpoch(1156),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result65, kHasNoMissedVsyncs);
}

/*
Tests that the decider marks a frame as janky if it was delayed compared to the
immediately preceding frame (in which Chrome quickly presented an input in a
frame).

VSync V0      V1      V2      V3 ... V62     V63  :  V64  :  V65     V66
      :       :       :       :  ...  :       :   :   :   :   :       :
Input I1      I2      I3      I4 ... I63      :  I64  :  I65  :       :
      :       :       :       :  ...  :       :   :   :   :           :
F1:   |-16ms--|       :       :       :       :   :   :   :           :
F2:           |-16ms--|       :       :       :   :   :   :           :
F3:                   |-16ms--|       :       :   :   :   :           :
F4:                           |--...  :       :   :   :   :           :
...                                   :       :   :   :   :           :
F62:                             ...--|       :   :   :   :           :
F63:                             ...  |-16ms--|   :   :   :           :
F64:                             ...              |8ms|   :           :
F65:                                                      |----24ms---|

The decider SHOULD evaluate I65/F65 against I64/F64 (because it just happened),
so the decider SHOULD mark F65 as janky.
*/
TEST_F(ScrollJankV4DeciderTest,
       MissedVsyncImmediatelyAfterQuickInputFrameDelivery) {
  // Inputs 1-63 took 16 ms (one VSync) to deliver.
  for (int i = 1; i <= 63; i++) {
    base::TimeDelta offset = (i - 1) * kVsyncInterval;
    std::optional<ScrollJankV4Result> result =
        decider_.DecideJankForPresentedFrame(
            /* first_input_generation_ts= */ MillisSinceEpoch(100) + offset,
            /* last_input_generation_ts= */ MillisSinceEpoch(100) + offset,
            /* presentation_ts= */ MillisSinceEpoch(116) + offset,
            /* vsync_interval= */ kVsyncInterval,
            /* has_inertial_input= */ false,
            /* abs_total_raw_delta_pixels= */ 2.0f,
            /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
    EXPECT_THAT(result, kHasNoMissedVsyncs);
  }

  // Inputs 64 took only 8 ms (half a VSync) to deliver.
  std::optional<ScrollJankV4Result> result64 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(1116),
          /* last_input_generation_ts= */ MillisSinceEpoch(1116),
          /* presentation_ts= */ MillisSinceEpoch(1124),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result64, kHasNoMissedVsyncs);

  // There's one VSync missed between F64 and F65. F65 should be evaluated
  // against the delivery cutoffs of the most recent frame (8 ms) rather than
  // the earlier frames (16 ms). Therefore, it's reasonable to assume that F65's
  // first input (generated at 1132 ms) could have been included in the missed
  // VSync (presented at 1140 ms), so F65 SHOULD be marked as janky.
  std::optional<ScrollJankV4Result> result65 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(1132),
          /* last_input_generation_ts= */ MillisSinceEpoch(1132),
          /* presentation_ts= */ MillisSinceEpoch(1156),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(
      result65,
      HasMissedVsyncs(
          JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1));
}

/*
Tests that the decider marks frames which missed one or more VSyncs in the
middle of a fast scroll as janky (even with sparse inputs).

VSync V V V V V V V V V V V V V V V V V V V V V V V V V V
      : : : : : : : : : : : : : : : : :   : :           :
Input I1I2  I3I4          I5        : :   : :           :
      : :   : :           :         : :   : :           :
F1:   |-----:-:-----------:---------| :   : :           :
F2:     |---:-:-----------:-----------|(A): :           :
F3:         |-:-----------:---------------| :           :
F4:           |-----------:-----------------|    (B)    :
F5:                       |-----------------------------|

Assuming I1-I5 are all above the fast scroll threshold (each have at least
3px absolute scroll delta), the decider should mark F3 and F5 janky with 1 (A)
and 5 (B) missed VSyncs respectively.
*/
TEST_F(ScrollJankV4DeciderTest, MissedVsyncDuringFastScroll) {
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(100),
          /* last_input_generation_ts= */ MillisSinceEpoch(100),
          /* presentation_ts= */ MillisSinceEpoch(340),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(116),
          /* last_input_generation_ts= */ MillisSinceEpoch(116),
          /* presentation_ts= */ MillisSinceEpoch(356),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result2, kHasNoMissedVsyncs);

  // 1 VSync missed between F2 and F3, so F3 should be marked as JANKY.
  std::optional<ScrollJankV4Result> result3 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(148),
          /* last_input_generation_ts= */ MillisSinceEpoch(148),
          /* presentation_ts= */ MillisSinceEpoch(388),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result3,
              HasMissedVsyncs(JankReason::kMissedVsyncDuringFastScroll, 1));

  std::optional<ScrollJankV4Result> result4 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(164),
          /* last_input_generation_ts= */ MillisSinceEpoch(164),
          /* presentation_ts= */ MillisSinceEpoch(404),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result4, kHasNoMissedVsyncs);

  // 5 VSyncs missed between F4 and F5, so F5 should be marked as JANKY.
  std::optional<ScrollJankV4Result> result5 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(260),
          /* last_input_generation_ts= */ MillisSinceEpoch(260),
          /* presentation_ts= */ MillisSinceEpoch(500),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result5,
              HasMissedVsyncs(JankReason::kMissedVsyncDuringFastScroll, 5));
}

/*
Tests that the decider does NOT mark frames which missed one or more VSyncs as
janky if inputs were sparse and the frames weren't in the middle of a fast
scroll.

VSync V V V V V V V V V V V V V V V V V V V V V V V V V V
      : : : : : : : : : : : : : : : : :   : :           :
Input I1I2  I3I4          I5        : :   : :           :
      : :   : :           :         : :   : :           :
F1:   |-----:-:-----------:---------| :   : :           :
F2:     |---:-:-----------:-----------|(A): :           :
F3:         |-:-----------:---------------| :           :
F4:           |-----------:-----------------|    (B)    :
F5:                       |-----------------------------|

If I2 or I3 is below the fast scroll threshold (has less than 3px absolute
scroll delta), the decider should NOT mark F3 as janky even though it missed 1
VSync (A). Similarly, if I4 or I5 are below the fast scroll threshold (has less
than 3px absolute scroll delta), the decider should NOT mark F5 as janky even
though it missed 5 VSyncs (B).
*/
TEST_F(ScrollJankV4DeciderTest, MissedVsyncOutsideFastScroll) {
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(100),
          /* last_input_generation_ts= */ MillisSinceEpoch(100),
          /* presentation_ts= */ MillisSinceEpoch(340),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(116),
          /* last_input_generation_ts= */ MillisSinceEpoch(116),
          /* presentation_ts= */ MillisSinceEpoch(356),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result2, kHasNoMissedVsyncs);

  // 1 VSync missed between F2 and F3, BUT F3 has scroll delta below the fast
  // scroll threshold, so F3 should NOT be marked as janky.
  std::optional<ScrollJankV4Result> result3 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(148),
          /* last_input_generation_ts= */ MillisSinceEpoch(148),
          /* presentation_ts= */ MillisSinceEpoch(388),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result3, kHasNoMissedVsyncs);

  std::optional<ScrollJankV4Result> result4 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(164),
          /* last_input_generation_ts= */ MillisSinceEpoch(164),
          /* presentation_ts= */ MillisSinceEpoch(404),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result4, kHasNoMissedVsyncs);

  // 5 VSyncs missed between F4 and F5, BUT F4 has scroll delta below the fast
  // scroll threshold, so F5 should NOT be marked as janky.
  std::optional<ScrollJankV4Result> result5 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(260),
          /* last_input_generation_ts= */ MillisSinceEpoch(260),
          /* presentation_ts= */ MillisSinceEpoch(500),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result5, kHasNoMissedVsyncs);
}

/*
Tests that the decider marks frames which missed one or more VSyncs at the
transition from a fast regular scroll to a fast fling as janky.

VSync V  V  V  V  V  V  V  V  V  V
      :  :  :  :  :  :  :  :  :  :
Input I1          I2 :           :
      :           :  :           :
F1:   |-----------:--|    (A)    :
F2:               |--------------|

I1 and I2 are regular and inertial scroll updates respectively. Assuming I1 is
above the fast scroll threshold (has at least 3 px absolute scroll delta) and I2
is above the fling threshold (has at least 0.2 px absolute scroll delta), the
decider should mark F2 as janky with 3 missed VSyncs (A).
*/
TEST_F(ScrollJankV4DeciderTest,
       MissedVsyncAtTransitionFromFastRegularScrollToFastFling) {
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(100),
          /* last_input_generation_ts= */ MillisSinceEpoch(100),
          /* presentation_ts= */ MillisSinceEpoch(180),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  // 3 VSync missed between F1 and F2, so F2 should be marked as JANKY.
  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(164),
          /* last_input_generation_ts= */ MillisSinceEpoch(164),
          /* presentation_ts= */ MillisSinceEpoch(244),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ true,
          /* abs_total_raw_delta_pixels= */ 0.5f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.5f);
  EXPECT_THAT(result2,
              HasMissedVsyncs(JankReason::kMissedVsyncAtStartOfFling, 3));
}

/*
Tests that the decider does NOT mark frames which missed one or more VSyncs at
the transition from a slow regular scroll to a fling as janky.

VSync V  V  V  V  V  V  V  V  V  V
      :  :  :  :  :  :  :  :  :  :
Input I1          I2 :           :
      :           :  :           :
F1:   |-----------:--|    (A)    :
F2:               |--------------|

I1 and I2 are regular and inertial scroll updates respectively. Assuming I1 is
below the fast scroll threshold (has less than 3 px absolute scroll delta), the
decider should NOT mark F2 as janky even though it missed 3 VSyncs (A).
*/
TEST_F(ScrollJankV4DeciderTest,
       MissedVsyncAtTransitionFromSlowRegularScrollToFling) {
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(100),
          /* last_input_generation_ts= */ MillisSinceEpoch(100),
          /* presentation_ts= */ MillisSinceEpoch(180),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 2.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  // 3 VSync missed between F1 and F2, BUT F1 has scroll delta below the fast
  // scroll threshold, so F2 should NOT be marked as janky.
  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(164),
          /* last_input_generation_ts= */ MillisSinceEpoch(164),
          /* presentation_ts= */ MillisSinceEpoch(244),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ true,
          /* abs_total_raw_delta_pixels= */ 0.5f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.5f);
  EXPECT_THAT(result2, kHasNoMissedVsyncs);
}

/*
Tests that the decider marks frames which missed one or more VSyncs at the
transition from a regular scroll to a slow fling as janky.

VSync V  V  V  V  V  V  V  V  V  V
      :  :  :  :  :  :  :  :  :  :
Input I1          I2 :           :
      :           :  :           :
F1:   |-----------:--|    (A)    :
F2:               |--------------|

I1 and I2 are regular and inertial scroll updates respectively. Assuming I2 is
below the fling threshold (has less than 0.2 px absolute scroll delta), the
decuder should NOT mark F2 as janky even though it missed 3 VSyncs (A).
*/
TEST_F(ScrollJankV4DeciderTest,
       MissedVsyncAtTransitionFromRegularScrollToSlowFling) {
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(100),
          /* last_input_generation_ts= */ MillisSinceEpoch(100),
          /* presentation_ts= */ MillisSinceEpoch(180),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  // 3 VSync missed between F1 and F2, BUT F2 has scroll delta below the fling
  // threshold, so F2 should NOT be marked as janky.
  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(164),
          /* last_input_generation_ts= */ MillisSinceEpoch(164),
          /* presentation_ts= */ MillisSinceEpoch(244),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ true,
          /* abs_total_raw_delta_pixels= */ 0.1f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.1f);
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
TEST_F(ScrollJankV4DeciderTest,
       NoMissedVsyncAtTransitionFromRegularScrollToFling) {
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(100),
          /* last_input_generation_ts= */ MillisSinceEpoch(100),
          /* presentation_ts= */ MillisSinceEpoch(180),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 4.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  // 3 VSync missed between F1 and F2, so F2 should be marked as JANKY.
  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(116),
          /* last_input_generation_ts= */ MillisSinceEpoch(116),
          /* presentation_ts= */ MillisSinceEpoch(196),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ true,
          /* abs_total_raw_delta_pixels= */ 0.5f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.5f);
  EXPECT_THAT(result2, kHasNoMissedVsyncs);
}

/*
Tests that the decider marks frames which missed one or more VSyncs in the
middle of a fast fling as janky.

VSync V V V V V V V V V V V V V V V V V V V V V V V V V V
      : : : : : : : : : : : : : : : : :   : :           :
Input I1I2  I3I4          I5        : :   : :           :
      : :   : :           :         : :   : :           :
F1:   |-----:-:-----------:---------| :   : :           :
F2:     |---:-:-----------:-----------|(A): :           :
F3:         |-:-----------:---------------| :           :
F4:           |-----------:-----------------|    (B)    :
F5:                       |-----------------------------|

I1-I5 are all inertial scroll updates. If I3 and I5 are above the fling
threshold (both have at least 0.2px absolute scroll delta), the decider should
mark F3 and F5 janky with 1 (A) and 5 (B) missed VSyncs respectively.
*/
TEST_F(ScrollJankV4DeciderTest, MissedVsyncDuringFastFling) {
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(100),
          /* last_input_generation_ts= */ MillisSinceEpoch(100),
          /* presentation_ts= */ MillisSinceEpoch(340),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ true,
          /* abs_total_raw_delta_pixels= */ 0.5f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.5f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(116),
          /* last_input_generation_ts= */ MillisSinceEpoch(116),
          /* presentation_ts= */ MillisSinceEpoch(356),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ true,
          /* abs_total_raw_delta_pixels= */ 0.5f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.5f);
  EXPECT_THAT(result2, kHasNoMissedVsyncs);

  // 1 VSync missed between F2 and F3, so F3 should be marked as JANKY.
  std::optional<ScrollJankV4Result> result3 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(148),
          /* last_input_generation_ts= */ MillisSinceEpoch(148),
          /* presentation_ts= */ MillisSinceEpoch(388),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ true,
          /* abs_total_raw_delta_pixels= */ 0.5f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.5f);
  EXPECT_THAT(result3, HasMissedVsyncs(JankReason::kMissedVsyncDuringFling, 1));

  std::optional<ScrollJankV4Result> result4 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(164),
          /* last_input_generation_ts= */ MillisSinceEpoch(164),
          /* presentation_ts= */ MillisSinceEpoch(404),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ true,
          /* abs_total_raw_delta_pixels= */ 0.1f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.1f);
  EXPECT_THAT(result4, kHasNoMissedVsyncs);

  // 5 VSyncs missed between F4 and F5 (EVEN THOUGH F4 has scroll delta below
  // the fling threshold), so F5 should be marked as JANKY.
  std::optional<ScrollJankV4Result> result5 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(260),
          /* last_input_generation_ts= */ MillisSinceEpoch(260),
          /* presentation_ts= */ MillisSinceEpoch(500),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ true,
          /* abs_total_raw_delta_pixels= */ 0.5f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.5f);
  EXPECT_THAT(result5, HasMissedVsyncs(JankReason::kMissedVsyncDuringFling, 5));
}

/*
Tests that the decider does NOT mark frames which missed one or more VSyncs in
the middle of a slow fling (typically towards the end of a fling) as janky.

VSync V V V V V V V V V V V V V V V V V V V V V V V V V V
      : : : : : : : : : : : : : : : : :   : :           :
Input I1I2  I3I4          I5        : :   : :           :
      : :   : :           :         : :   : :           :
F1:   |-----:-:-----------:---------| :   : :           :
F2:     |---:-:-----------:-----------|(A): :           :
F3:         |-:-----------:---------------| :           :
F4:           |-----------:-----------------|    (B)    :
F5:                       |-----------------------------|

I1-I5 are all inertial scroll updates. If I3 is below the fling threshold (has
less than 0.2px absolute scroll delta), the decider should NOT mark F3 as janky
even though it missed one VSync (A). Similarly, if I5 is below the fling
threshold (has less than 0.2px absolute scroll delta), the decider should NOT
mark F5 as janky even though it missed 5 VSyncs (B).
*/
TEST_F(ScrollJankV4DeciderTest, MissedVsyncDuringSlowFling) {
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(100),
          /* last_input_generation_ts= */ MillisSinceEpoch(100),
          /* presentation_ts= */ MillisSinceEpoch(300),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ true,
          /* abs_total_raw_delta_pixels= */ 0.5f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.5f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(116),
          /* last_input_generation_ts= */ MillisSinceEpoch(116),
          /* presentation_ts= */ MillisSinceEpoch(316),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ true,
          /* abs_total_raw_delta_pixels= */ 0.5f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.5f);
  EXPECT_THAT(result2, kHasNoMissedVsyncs);

  // 1 VSync missed between F2 and F3, BUT F3 has scroll delta below the fling
  // threshold, so F3 should NOT be marked as janky.
  std::optional<ScrollJankV4Result> result3 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(148),
          /* last_input_generation_ts= */ MillisSinceEpoch(148),
          /* presentation_ts= */ MillisSinceEpoch(348),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ true,
          /* abs_total_raw_delta_pixels= */ 0.1f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.1f);
  EXPECT_THAT(result3, kHasNoMissedVsyncs);

  std::optional<ScrollJankV4Result> result4 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(164),
          /* last_input_generation_ts= */ MillisSinceEpoch(164),
          /* presentation_ts= */ MillisSinceEpoch(364),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ true,
          /* abs_total_raw_delta_pixels= */ 0.1f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.1f);
  EXPECT_THAT(result4, kHasNoMissedVsyncs);

  // 5 VSyncs missed between F4 and F5, BUT F5 has scroll delta below the fling
  // threshold, so F5 should NOT be marked as janky.
  std::optional<ScrollJankV4Result> result5 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(260),
          /* last_input_generation_ts= */ MillisSinceEpoch(260),
          /* presentation_ts= */ MillisSinceEpoch(460),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ true,
          /* abs_total_raw_delta_pixels= */ 0.1f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.1f);
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
  std::optional<ScrollJankV4Result> result1 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(100),
          /* last_input_generation_ts= */ MicrosSinceEpoch(108100),
          /* presentation_ts= */ MillisSinceEpoch(164),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 0.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result1, kHasNoMissedVsyncs);

  // F2: 180 - 124 = 56 ms delivery cutoff.
  std::optional<ScrollJankV4Result> result2 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(116),
          /* last_input_generation_ts= */ MillisSinceEpoch(124),
          /* presentation_ts= */ MillisSinceEpoch(180),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 0.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
  EXPECT_THAT(result2, kHasNoMissedVsyncs);

  // F3: 196 - 139.8 = 56.2 ms delivery cutoff
  std::optional<ScrollJankV4Result> result3 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ MillisSinceEpoch(132),
          /* last_input_generation_ts= */ MicrosSinceEpoch(139800),
          /* presentation_ts= */ MillisSinceEpoch(196),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 0.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
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
  std::optional<ScrollJankV4Result> result4 =
      decider_.DecideJankForPresentedFrame(
          /* first_input_generation_ts= */ params.input_ts,
          /* last_input_generation_ts= */ params.input_ts,
          /* presentation_ts= */ MillisSinceEpoch(260),
          /* vsync_interval= */ kVsyncInterval,
          /* has_inertial_input= */ false,
          /* abs_total_raw_delta_pixels= */ 0.0f,
          /* max_abs_inertial_raw_delta_pixels= */ 0.0f);
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

}  // namespace cc
