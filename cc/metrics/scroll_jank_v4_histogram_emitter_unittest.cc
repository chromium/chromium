// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_histogram_emitter.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#include "cc/metrics/scroll_jank_v4_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

using ::testing::_;
using ::testing::VariantWith;

constexpr JankReasonArray<int> MakeMissedVsyncCounts(
    std::initializer_list<std::pair<JankReason, int>> values) {
  JankReasonArray<int> result = {};  // Default initialize to 0
  for (const auto& [reason, missed_vsyncs] : values) {
    result[static_cast<int>(reason)] += missed_vsyncs;
  }
  return result;
}

constexpr JankReasonArray<int> kNonJankyFrame = {};
constexpr bool kDamaging = true;
constexpr bool kNonDamaging = false;

void ExpectNoFixedWindowHistograms(
    const base::HistogramTester& histogram_tester) {
  histogram_tester.ExpectTotalCount(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0);
  histogram_tester.ExpectTotalCount(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      0);
  histogram_tester.ExpectTotalCount(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      0);
  histogram_tester.ExpectTotalCount(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0);
  histogram_tester.ExpectTotalCount(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0);
  histogram_tester.ExpectTotalCount(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0);
  histogram_tester.ExpectTotalCount(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0);
}

void ExpectNoPerScrollHistograms(
    const base::HistogramTester& histogram_tester) {
  histogram_tester.ExpectTotalCount(
      "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
}

void ExpectNoHistograms(const base::HistogramTester& histogram_tester) {
  ExpectNoFixedWindowHistograms(histogram_tester);
  ExpectNoPerScrollHistograms(histogram_tester);
}

}  // namespace

class ScrollJankV4HistogramEmitterTest : public testing::Test {
 public:
  explicit ScrollJankV4HistogramEmitterTest(
      std::string histogram_emission_policy) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kHandleNonDamagingInputsInScrollJankV4Metric,
        {{features::kHistogramEmissionPolicy.name, histogram_emission_policy}});
    histogram_emitter_ = std::make_unique<ScrollJankV4HistogramEmitter>();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ScrollJankV4HistogramEmitter> histogram_emitter_;
};

// Test cases which only involve damaging frames and are therefore common to
// both histogram emission policies, i.e. the expected behavior is independent
// of `features::kHistogramEmissionPolicy`.
class ScrollJankV4HistogramEmitterCommonTest
    : public ScrollJankV4HistogramEmitterTest,
      public testing::WithParamInterface<std::string> {
 public:
  ScrollJankV4HistogramEmitterCommonTest()
      : ScrollJankV4HistogramEmitterTest(GetParam()) {}
};

TEST_P(ScrollJankV4HistogramEmitterCommonTest,
       EmitsFixedWindowHistogramsEvery64Frames) {
  // First window: Histograms should be emitted after the 64th frame.
  {
    base::HistogramTester histogram_tester;

    // Frames 1-10: Non-janky.
    for (int i = 1; i <= 10; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    // Frame 11: Janky for ALL reasons.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({
            {JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1},
            {JankReason::kMissedVsyncDuringFastScroll, 2},
            {JankReason::kMissedVsyncAtStartOfFling, 3},
            {JankReason::kMissedVsyncDuringFling, 4},
        }),
        kDamaging);

    // Frames 12-20: Non-janky.
    for (int i = 12; i <= 20; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    // Frames 21-22: Janky due to violating the running consistency rule.
    for (int i = 21; i <= 22; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(
          MakeMissedVsyncCounts({
              {JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1},
          }),
          kDamaging);
    }

    // Frames 23-30: Non-janky.
    for (int i = 23; i <= 30; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    // Frame 31-33: Janky due to violating the fast scroll continuity rule.
    for (int i = 31; i <= 33; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(
          MakeMissedVsyncCounts({
              {JankReason::kMissedVsyncDuringFastScroll, 1},
          }),
          kDamaging);
    }

    // Frames 34-40: Non-janky.
    for (int i = 34; i <= 40; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    // Frame 41-44: Janky due to violating the fling continuity rule at the
    // transition from a fast scroll.
    for (int i = 41; i <= 44; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(
          MakeMissedVsyncCounts({
              {JankReason::kMissedVsyncAtStartOfFling, 1},
          }),
          kDamaging);
    }

    // Frames 45-50: Non-janky.
    for (int i = 45; i <= 50; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    // Frames 51-55: Janky due to violating the fling continuity rule in the
    // middle of a fling.
    for (int i = 51; i <= 55; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(
          MakeMissedVsyncCounts({
              {JankReason::kMissedVsyncDuringFling, 1},
          }),
          kDamaging);
    }

    // Frames 56-63: Non-janky.
    for (int i = 56; i <= 63; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    ExpectNoHistograms(histogram_tester);

    // Frame 64: Non-janky.
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
        15 * 100 / 64 /* Frames 11, 21-22, 31-33, 41-44 & 51-55 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        3 * 100 / 64 /* Frames 11 & 21-22 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        4 * 100 / 64 /* Frames 11 & 31-33 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncAtStartOfFling",
        5 * 100 / 64 /* Frames 11 & 41-44 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFling",
        6 * 100 / 64 /* Frames 11 & 51-55 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsSum4.FixedWindow",
        4 + 1 + 2 + 3 + 4 + 5 /* Frames 11, 21-22, 31-33, 41-44 & 51-55 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 4 /* Frame 11 */, 1);
    ExpectNoPerScrollHistograms(histogram_tester);
  }

  // Second window: Histograms should be emitted after the next 64th frame.
  {
    base::HistogramTester histogram_tester;

    for (int i = 1; i <= 63; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    ExpectNoHistograms(histogram_tester);

    // Frame 64.
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncAtStartOfFling",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFling",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 1);
    ExpectNoPerScrollHistograms(histogram_tester);
  }
}
TEST_P(ScrollJankV4HistogramEmitterCommonTest,
       EmitsPerScrollHistogramsAtEndOfScroll) {
  // Scroll 1: Histograms SHOULD be emitted after it ends.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();  // First scroll.

    // 1 non-janky frame.
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);

    // 5 janky frames.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({
            {JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1},
            {JankReason::kMissedVsyncDuringFastScroll, 2},
            {JankReason::kMissedVsyncAtStartOfFling, 3},
            {JankReason::kMissedVsyncDuringFling, 4},
        }),
        kDamaging);
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({
            {JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1},
        }),
        kDamaging);
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({
            {JankReason::kMissedVsyncDuringFastScroll, 1},
        }),
        kDamaging);
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({
            {JankReason::kMissedVsyncAtStartOfFling, 1},
        }),
        kDamaging);
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({
            {JankReason::kMissedVsyncDuringFling, 1},
        }),
        kDamaging);

    // 1 non-janky frame.
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);

    ExpectNoHistograms(histogram_tester);

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 5 * 100 / 7, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }

  // Scroll 2: NO histograms for the second scroll should be emitted before it
  // ends.
  {
    base::HistogramTester histogram_tester;

    // Robustness test: Under normal circumstances, the histogram emitter should
    // have received a scroll begin event for the second scroll, but it didn't
    // for some unexpected reason. The histogram emitter should be able to
    // handle this situation, using the fact that the first scroll has just
    // ended.

    for (int i = 1; i <= 10; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    ExpectNoHistograms(histogram_tester);
  }

  // UMA histograms for the second scroll SHOULD be emitted when the third
  // scroll begins.
  {
    base::HistogramTester histogram_tester;

    // Robustness test: Under normal circumstances, the histogram emitter should
    // have received a scroll end event for the second scroll, but it didn't for
    // some unexpected reason. The histogram emitter should be able to handle
    // this situation, by falling back to emitting UMA histograms for the second
    // scroll when the third scroll begins.
    histogram_emitter_->OnScrollStarted();  // Third scroll.

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }

  // NO histograms should be emitted for the third scroll, even after it ends,
  // because it was empty. NO histograms should also be emitted for the fourth
  // scroll before the histogram emitter is destroyed.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollEnded();    // Third scroll.
    histogram_emitter_->OnScrollStarted();  // Fourth scroll.

    // 1 non-janky frame.
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);

    // 1 janky frame.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({
            {JankReason::kMissedVsyncDuringFastScroll, 4},
            {JankReason::kMissedVsyncDuringFling, 2},
        }),
        kDamaging);

    // 1 non-janky frame.
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame,
                                                 kDamaging);  // Fourth scroll.

    ExpectNoHistograms(histogram_tester);
  }

  // UMA histograms for the fourth scroll SHOULD be emitted when the histogram
  // emitter is destroyed.
  {
    base::HistogramTester histogram_tester;

    delete histogram_emitter_.release();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 1 * 100 / 3, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }
}

/*
A combined scenario which tests that the histogram emitter emits both fixed
window and per-scroll UMA histograms. This test also verifies that the emission
of fixed window and per-scroll histograms is independent (e.g. a scroll ending
in the middle of a fixed window shouldn't affect the fixed window calculation
and vice versa).

Presented frame: :1        32:33       64:65       96:97      128:
Fixed windows:   |<-------window 1------>|<-------window 2------>|
Scrolls:         |<scroll 1->|<------scroll 2------->|<scroll 3->|
Delayed frames:  :     1     :     2     :     4     :     8     :
*/
TEST_P(ScrollJankV4HistogramEmitterCommonTest,
       EmitsBothFixedWindowAndPerScrollHistogramsIndependently) {
  // Scroll 1, frames 1-32: Histograms SHOULD be emitted after the scroll ends.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    // Frames 1-10: Non-janky.
    for (int i = 1; i <= 10; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    // Frame 11: Janky for ALL reasons.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({
            {JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1},
            {JankReason::kMissedVsyncDuringFastScroll, 2},
            {JankReason::kMissedVsyncAtStartOfFling, 3},
            {JankReason::kMissedVsyncDuringFling, 4},
        }),
        kDamaging);

    // Frames 12-32: Non-janky.
    for (int i = 12; i <= 32; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    ExpectNoHistograms(histogram_tester);

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        1 * 100 / 32 /* Frame 11 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }

  // Start of scroll 2, frames 33-64: Fixed window histograms should be emitted
  // after frame 64.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    // Frames 33-50: Non-janky.
    for (int i = 33; i <= 50; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    // Frames 51-52: Janky due to violating the running consistency rule.
    for (int i = 51; i <= 52; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(
          MakeMissedVsyncCounts({
              {JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1},
          }),
          kDamaging);
    }

    // Frames 53-63: Non-janky.
    for (int i = 53; i <= 63; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    ExpectNoHistograms(histogram_tester);

    // Frame 64: Non-janky.
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
        3 * 100 / 64 /* Frames 11 & 51-52 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        3 * 100 / 64 /* Frames 11 & 51-52 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        1 * 100 / 64 /* Frame 11 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncAtStartOfFling",
        1 * 100 / 64 /* Frame 11 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFling",
        1 * 100 / 64 /* Frame 11 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsSum4.FixedWindow",
        4 + 2 /* Frames 11 & 51-52 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 4 /* Frame 11 */, 1);
    ExpectNoPerScrollHistograms(histogram_tester);
  }

  // Frames 65-96, end of scroll 2: Per-scroll histograms SHOULD be emitted
  // after the scroll ends.
  {
    base::HistogramTester histogram_tester;

    // Frames 65-80: Non-janky.
    for (int i = 65; i <= 80; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    // Frames 81-84: Janky due to violating the fast scroll continuity rule.
    for (int i = 81; i <= 84; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(
          MakeMissedVsyncCounts({
              {JankReason::kMissedVsyncDuringFastScroll, 17},
          }),
          kDamaging);
    }

    // Frames 85-96: Non-janky.
    for (int i = 85; i <= 96; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    ExpectNoHistograms(histogram_tester);

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        6 * 100 / 64 /* Frame 51-52 & 81-84 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }

  // Start of scroll 3, frames 97-128: Fixed window histograms SHOULD be emitted
  // after frame 128.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    // Frames 97-110: Non-janky.
    for (int i = 97; i <= 110; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    // Frame 111-118: Janky due to violating the fling continuity rule at the
    // transition from a fast scroll.
    for (int i = 111; i <= 118; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(
          MakeMissedVsyncCounts({
              {JankReason::kMissedVsyncAtStartOfFling, 19},
          }),
          kDamaging);
    }

    // Frames 119-127: Non-janky.
    for (int i = 119; i <= 127; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    ExpectNoHistograms(histogram_tester);

    // Frame 128: Non-janky.
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
        12 * 100 / 64 /* Frames 81-84 & 111-118 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        4 * 100 / 64 /* Frames 81-84 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncAtStartOfFling",
        8 * 100 / 64 /* Frames 111-118 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFling",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsSum4.FixedWindow",
        4 * 17 + 8 * 19 /* Frames 81-84 & 111-118 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsMax4.FixedWindow",
        19 /* Frames 111-118 */, 1);
    ExpectNoPerScrollHistograms(histogram_tester);
  }

  // End of scroll 3: Per-scroll histograms SHOULD be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        8 * 100 / 32 /* Frames 111-118 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }
}

TEST_P(ScrollJankV4HistogramEmitterCommonTest, IgnoresEmptyScrolls) {
  // 10 empty scrolls: NO histograms should be emitted.
  {
    base::HistogramTester histogram_tester;

    for (int s = 1; s <= 10; s++) {
      histogram_emitter_->OnScrollStarted();
      histogram_emitter_->OnScrollEnded();
    }
    ExpectNoHistograms(histogram_tester);
  }

  // Start of scroll 11, frames 1-64: Fixed window histograms SHOULD be emitted
  // after frame 64.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();
    for (int i = 1; i <= 63; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    ExpectNoHistograms(histogram_tester);

    // Frame 64.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({
            {JankReason::kMissedVsyncDuringFastScroll, 5},
        }),
        kDamaging);

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
        1 * 100 / 64 /* Frame 64 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        1 * 100 / 64 /* Frame 64 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncAtStartOfFling",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFling",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 5 /* Frame 64 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 5 /* Frame 64 */, 1);
    ExpectNoPerScrollHistograms(histogram_tester);
  }

  // End of scroll 11: Per-scroll histograms SHOULD be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        1 * 100 / 64 /* Frame 64 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }
}

TEST_P(ScrollJankV4HistogramEmitterCommonTest, CountsSingletonScrolls) {
  // 10 singleton scrolls: Per-scroll histograms SHOULD be emitted for each
  // scroll.
  for (int s = 1; s <= 10; s++) {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }

  // Start of scroll 11, frames 11-64: Fixed window histograms SHOULD be emitted
  // after frame 64.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();
    for (int i = 11; i <= 63; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    ExpectNoHistograms(histogram_tester);

    // Frame 64.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({
            {JankReason::kMissedVsyncDuringFastScroll, 5},
        }),
        kDamaging);

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
        1 * 100 / 64 /* Frame 64 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        1 * 100 / 64 /* Frame 64 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncAtStartOfFling",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFling",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 5 /* Frame 64 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 5 /* Frame 64 */, 1);
    ExpectNoPerScrollHistograms(histogram_tester);
  }

  // End of scroll 11: Per-scroll histograms SHOULD be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        1 * 100 / 54 /* Frame 64 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }
}

// Regression test for https://crbug.com/475797611.
TEST_P(ScrollJankV4HistogramEmitterCommonTest,
       ShouldNotCrashWhenAllFramesAreJanky) {
  base::HistogramTester histogram_tester;

  for (int i = 0; i < 64; i++) {
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts(
            {{JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1}}),
        kDamaging);
  }

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 100, 1);
}

INSTANTIATE_TEST_SUITE_P(
    ScrollJankV4HistogramEmitterCommonTest,
    ScrollJankV4HistogramEmitterCommonTest,
    testing::ValuesIn<std::string>({
        features::kEmitForAllScrolls,
        features::kEmitForDamagingScrolls,
    }),
    [](const testing::TestParamInfo<
        ScrollJankV4HistogramEmitterCommonTest::ParamType>& info) {
      return info.param;
    });

class ScrollJankV4HistogramEmitterEmitForAllScrollsTest
    : public ScrollJankV4HistogramEmitterTest {
 public:
  ScrollJankV4HistogramEmitterEmitForAllScrollsTest()
      : ScrollJankV4HistogramEmitterTest(features::kEmitForAllScrolls) {}
};

TEST_F(ScrollJankV4HistogramEmitterEmitForAllScrollsTest,
       CountsNonDamagingFrames) {
  // Start of scroll, frames 1-64: Fixed window histograms SHOULD be emitted
  // after frame 64.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    for (int i = 1; i <= 20; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    for (int i = 21; i <= 40; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame,
                                                   kNonDamaging);
    }

    // Frame 41: Janky damaging frame.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts(
            {{JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 3}}),
        kDamaging);

    for (int i = 42; i <= 50; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    // Frame 51: Janky damaging frame.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({{JankReason::kMissedVsyncDuringFastScroll, 5}}),
        kNonDamaging);

    for (int i = 52; i <= 63; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame,
                                                   kNonDamaging);
    }

    ExpectNoHistograms(histogram_tester);

    // Frame 64: Non-janky non-damaging frame.
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kNonDamaging);

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
        2 * 100 / 64 /* Frames 41 & 51 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        1 * 100 / 64 /* Frame 41 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        1 * 100 / 64 /* Frame 51 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncAtStartOfFling",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFling",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsSum4.FixedWindow",
        3 + 5 /* Frames 41 & 51 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 5 /* Frame 51 */, 1);
    ExpectNoPerScrollHistograms(histogram_tester);
  }

  // End of scroll: Per-scroll histograms SHOULD be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        2 * 100 / 54 /* Frames 41 & 51 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }
}

TEST_F(ScrollJankV4HistogramEmitterEmitForAllScrollsTest,
       CountsCompletelyNonDamagingScrolls) {
  // Scroll 1 with both damaging and non-damaging frames: Per-scroll histotgrams
  // SHOULD be emitted after the scroll ends.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    for (int i = 1; i <= 10; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame,
                                                   kNonDamaging);
    }

    for (int i = 11; i <= 19; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    // Frame 20: Janky.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts(
            {{JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 4}}),
        kNonDamaging);

    ExpectNoHistograms(histogram_tester);

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        1 * 100 / 20 /* Frame 20 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }

  // COMPLETELY NON-DAMAGING scroll 2: Per-scroll histograms SHOULD be emitted
  // after the scroll ends.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    for (int i = 21; i <= 29; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame,
                                                   kNonDamaging);
    }

    // Frame 30: Janky.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({{JankReason::kMissedVsyncDuringFastScroll, 8}}),
        kNonDamaging);

    ExpectNoHistograms(histogram_tester);

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        1 * 100 / 10 /* Frame 30 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }

  // Start of scroll 3 with both damaging and non-damaging frames: Fixed window
  // histograms SHOULD be emitted after frame 64.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    for (int i = 31; i <= 50; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    for (int i = 51; i <= 63; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame,
                                                   kNonDamaging);
    }

    ExpectNoHistograms(histogram_tester);

    // Frame 64.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({{JankReason::kMissedVsyncAtStartOfFling, 6}}),
        kDamaging);

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
        3 * 100 / 64 /* Frames 20, 30 & 64 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        1 * 100 / 64 /* Frame 20 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        1 * 100 / 64 /* Frame 30 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncAtStartOfFling",
        1 * 100 / 64 /* Frame 64 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFling",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsSum4.FixedWindow",
        4 + 8 + 6 /* Frames 20, 30 & 64 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 8 /* Frame 30 */, 1);
    ExpectNoPerScrollHistograms(histogram_tester);
  }

  // End of scroll 3: Per-scroll histograms SHOULD be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        1 * 100 / 34 /* Frame 64 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }
}

class ScrollJankV4HistogramEmitterEmitForDamagingScrollsTest
    : public ScrollJankV4HistogramEmitterTest {
 public:
  ScrollJankV4HistogramEmitterEmitForDamagingScrollsTest()
      : ScrollJankV4HistogramEmitterTest(features::kEmitForDamagingScrolls) {}
};

TEST_F(ScrollJankV4HistogramEmitterEmitForDamagingScrollsTest,
       CountsNonDamagingFrames) {
  // Scroll 1 with frames 1-27 (all damaging): Per-scroll histograms SHOULD be
  // emitted after the scroll ends.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    for (int i = 1; i <= 13; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    // Frames 14 & 15: Janky damaging frames.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts(
            {{JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1}}),
        kDamaging);
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts(
            {{JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 2}}),
        kDamaging);

    for (int i = 16; i <= 27; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    ExpectNoHistograms(histogram_tester);

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        2 * 100 / 27 /* Frames 14 & 15 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }

  // Start of scroll 2 with frames 28-147 (all non-damaging): NO histograms
  // should be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    for (int i = 28; i <= 88; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame,
                                                   kNonDamaging);
    }

    // Frame 89: Janky non-damaging frame.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({{JankReason::kMissedVsyncDuringFastScroll, 4}}),
        kNonDamaging);

    for (int i = 90; i <= 140; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame,
                                                   kNonDamaging);
    }

    // Frame 141: Janky non-damaging frame.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({{JankReason::kMissedVsyncDuringFastScroll, 8}}),
        kNonDamaging);

    for (int i = 142; i <= 147; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame,
                                                   kNonDamaging);
    }

    // `histogram_emitter_` doesn't yet know whether scroll 2 contains any
    // damaging frames, so even though it has already encountered 147 frames, it
    // hasn't emitted any fixed window UMA histograms.
    ExpectNoHistograms(histogram_tester);
  }

  // Frame 148 (damaging): Fixed window histograms for frames 1-64 and 65-128
  // SHOULD be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts(
            {{JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery,
              16}}),
        kDamaging);

    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow"),
        base::BucketsAre(base::Bucket(2 * 100 / 64, 1) /* Frames 14 & 15 */,
                         base::Bucket(1 * 100 / 64, 1) /* Frame 89 */
                         ));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
            "MissedVsyncDueToDeceleratingInputFrameDelivery"),
        base::BucketsAre(base::Bucket(2 * 100 / 64, 1) /* Frames 14 & 15 */,
                         base::Bucket(0, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
                    "MissedVsyncDuringFastScroll"),
                base::BucketsAre(base::Bucket(0, 1),
                                 base::Bucket(1 * 100 / 64, 1) /* Frame 89 */
                                 ));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
                    "MissedVsyncAtStartOfFling"),
                base::BucketsAre(base::Bucket(0, 2)));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
                    "MissedVsyncDuringFling"),
                base::BucketsAre(base::Bucket(0, 2)));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "Event.ScrollJank.MissedVsyncsSum4.FixedWindow"),
                base::BucketsAre(base::Bucket(1 + 2, 1) /* Frames 14 & 15 */,
                                 base::Bucket(4, 1) /* Frame 89 */));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "Event.ScrollJank.MissedVsyncsMax4.FixedWindow"),
                base::BucketsAre(base::Bucket(2, 1) /* Frame 15 */,
                                 base::Bucket(4, 1) /* Frame 89 */));
    ExpectNoPerScrollHistograms(histogram_tester);
  }

  // Frames 149-192 (non-damaging): Fixed window histograms for frames 129-192
  // SHOULD be emitted after frame 192.
  {
    base::HistogramTester histogram_tester;

    for (int i = 149; i <= 170; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame,
                                                   kNonDamaging);
    }

    // Frames 171-173: Janky non-damaging frame.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({{JankReason::kMissedVsyncDuringFastScroll, 32}}),
        kNonDamaging);
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({{JankReason::kMissedVsyncDuringFastScroll, 64}}),
        kNonDamaging);
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts(
            {{JankReason::kMissedVsyncDuringFastScroll, 128}}),
        kNonDamaging);

    for (int i = 174; i <= 191; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame,
                                                   kNonDamaging);
    }

    ExpectNoHistograms(histogram_tester);

    // Frame 192: Non-janky non-damaging frame.
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
        5 * 100 / 64 /* Frames 141, 148 & 171-173 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        1 * 100 / 64 /* Frame 148 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        4 * 100 / 64 /* Frames 141 & 171-173 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncAtStartOfFling",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFling",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsSum4.FixedWindow",
        8 + 16 + 32 + 64 + 128 /* Frames 141, 148 & 171-173 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 128 /* Frame 173 */,
        1);
    ExpectNoPerScrollHistograms(histogram_tester);
  }

  // Frames 193-227 (non-damaging), end of scroll 2: Per-scroll histograms
  // SHOULD be emitted after the scroll ends.
  {
    base::HistogramTester histogram_tester;

    for (int i = 193; i <= 227; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame,
                                                   kNonDamaging);
    }

    ExpectNoHistograms(histogram_tester);

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        6 * 100 / 200 /* Frames 89, 141, 148 & 171-173 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }

  // Start of scroll 3 with frames 228-256 (all damaging): Fixed window
  // histograms for frames 193-256 SHOULD be emitted after frame 256.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    for (int i = 228; i <= 240; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    // Frames 241-242: Janky damaging frames.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({{JankReason::kMissedVsyncAtStartOfFling, 256}}),
        kDamaging);
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({{JankReason::kMissedVsyncDuringFling, 512}}),
        kDamaging);

    for (int i = 243; i <= 255; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    ExpectNoHistograms(histogram_tester);

    // Frame 256: Non-janky damaging frame.
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
        2 * 100 / 64 /* Frames 241 & 242 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncAtStartOfFling",
        1 * 100 / 64 /* Frame 241 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFling",
        1 * 100 / 64 /* Frame 242 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsSum4.FixedWindow",
        256 + 512 /* Frames 241 & 242 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 512 /* Frame 242 */,
        1);
    ExpectNoPerScrollHistograms(histogram_tester);
  }

  // End of scroll 3: Per-scroll histograms SHOULD be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        2 * 100 / 29 /* Frames 241 & 242 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }
}

TEST_F(ScrollJankV4HistogramEmitterEmitForDamagingScrollsTest,
       IgnoresCompletelyNonDamagingScrolls) {
  // Scroll 1 with 32 damaging frames: Per-scroll histograms SHOULD be emitted
  // after the scroll ends.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    for (int i = 1; i <= 15; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    // Frame 16: Janky damaging frame
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts(
            {{JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 2}}),
        kDamaging);

    for (int i = 17; i <= 32; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    ExpectNoHistograms(histogram_tester);

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        1 * 100 / 32 /* Frame 16 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }

  // COMPLETELY NON-DAMAGING scroll 2: NO histograms should be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    for (int non_damaging = 1; non_damaging <= 50; non_damaging++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame,
                                                   kNonDamaging);
    }
    // Note: This jank will be lost because it's within a completely
    // non-damaging scroll.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({{JankReason::kMissedVsyncDuringFastScroll, 3}}),
        kNonDamaging);
    for (int non_damaging = 52; non_damaging <= 100; non_damaging++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame,
                                                   kNonDamaging);
    }

    histogram_emitter_->OnScrollEnded();

    ExpectNoHistograms(histogram_tester);
  }

  // Start of scroll 3 with 32 damaging frames: Fixed window histograms SHOULD
  // be emitted after frame 64.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    for (int i = 33; i <= 47; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    // Frame 48: Janky damaging frame
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({{JankReason::kMissedVsyncDuringFling, 4}}),
        kDamaging);

    for (int i = 49; i <= 63; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
    }

    ExpectNoHistograms(histogram_tester);

    // Frame 64: Non-janky damaging frame.
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
        2 * 100 / 64 /* Frames 16 & 48 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        1 * 100 / 64 /* Frame 16 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncAtStartOfFling",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFling",
        1 * 100 / 64 /* Frame 48 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsSum4.FixedWindow",
        2 + 4 /* Frames 16 & 48 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 4 /* Frame 48 */, 1);
    ExpectNoPerScrollHistograms(histogram_tester);
  }

  // End of scroll 3: Per-scroll histograms SHOULD be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        1 * 100 / 32 /* Frame 48 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }
}

TEST_F(ScrollJankV4HistogramEmitterEmitForDamagingScrollsTest,
       LimitsNumberOfPendingFixedWindows) {
  base::HistogramTester histogram_tester;

  histogram_emitter_->OnScrollStarted();

  // If the emitter didn't limit the number of pending fixed windows, there
  // would be 100 pending windows.
  for (int i = 1; i <= 6400; i++) {
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kNonDamaging);
  }

  ExpectNoHistograms(histogram_tester);

  histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);

  // However, the emitter limits the number of pending fixed windows to 20.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow"),
              base::BucketsAre(base::Bucket(0, 20)));
}

// Test cases which verify that `ScrollJankV4HistogramEmitter` selects the
// correct histogram emission policy depending on
// `features::kHistogramEmissionPolicy`.
class ScrollJankV4HistogramEmitterPolicySelectionTest : public testing::Test {
 protected:
  // Individual test cases cannot access private declarations within
  // `ScrollJankV4HistogramEmitter`, only this test fixture can (because it's a
  // friend of `ScrollJankV4HistogramEmitter`), so we need to re-export the
  // relevant nested classes.
  using EmitForAllScrolls = ScrollJankV4HistogramEmitter::EmitForAllScrolls;
  using EmitForDamagingScrolls =
      ScrollJankV4HistogramEmitter::EmitForDamagingScrolls;

  template <typename ExpectedInnerEmitterType>
  void ExpectThatCreateInnerEmitterReturnsType() {
    // This call cannot be inlined for the same visibility reasons.
    EXPECT_THAT(ScrollJankV4HistogramEmitter::CreateInnerEmitter(),
                VariantWith<ExpectedInnerEmitterType>(_));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ScrollJankV4HistogramEmitterPolicySelectionTest, FeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kHandleNonDamagingInputsInScrollJankV4Metric);

  ExpectThatCreateInnerEmitterReturnsType<EmitForAllScrolls>();
}

TEST_F(ScrollJankV4HistogramEmitterPolicySelectionTest,
       PolicyParamIsEmitForAllScrolls) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kHandleNonDamagingInputsInScrollJankV4Metric,
      {{features::kHistogramEmissionPolicy.name,
        features::kEmitForAllScrolls}});

  ExpectThatCreateInnerEmitterReturnsType<EmitForAllScrolls>();
}

TEST_F(ScrollJankV4HistogramEmitterPolicySelectionTest,
       PolicyParamIsEmitForDamagingScrolls) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kHandleNonDamagingInputsInScrollJankV4Metric,
      {{features::kHistogramEmissionPolicy.name,
        features::kEmitForDamagingScrolls}});

  ExpectThatCreateInnerEmitterReturnsType<EmitForDamagingScrolls>();
}

TEST_F(ScrollJankV4HistogramEmitterPolicySelectionTest, PolicyParamIsInvalid) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kHandleNonDamagingInputsInScrollJankV4Metric,
      {
          {features::kHistogramEmissionPolicy.name, "invalid"},
      });

  ExpectThatCreateInnerEmitterReturnsType<EmitForDamagingScrolls>();
}

TEST_F(ScrollJankV4HistogramEmitterPolicySelectionTest,
       PolicyParamIsNotProvided) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kHandleNonDamagingInputsInScrollJankV4Metric);

  ExpectThatCreateInnerEmitterReturnsType<EmitForDamagingScrolls>();
}

}  // namespace cc
