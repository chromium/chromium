// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_histogram_emitter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#include "cc/metrics/scroll_jank_os_reporter.h"
#include "cc/metrics/scroll_jank_v4_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

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
 protected:
  std::unique_ptr<ScrollJankV4HistogramEmitter> histogram_emitter_ =
      std::make_unique<ScrollJankV4HistogramEmitter>();
};

TEST_F(ScrollJankV4HistogramEmitterTest,
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
        // Frames 11, 21-22, 31-33, 41-44 & 51-55.
        base::ClampCeil(15 * 100.0 / 64), 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        base::ClampCeil(3 * 100.0 / 64) /* Frames 11 & 21-22 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        base::ClampCeil(4 * 100.0 / 64) /* Frames 11 & 31-33 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncAtStartOfFling",
        base::ClampCeil(5 * 100.0 / 64) /* Frames 11 & 41-44 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFling",
        base::ClampCeil(6 * 100.0 / 64) /* Frames 11 & 51-55 */, 1);
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

TEST_F(ScrollJankV4HistogramEmitterTest,
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
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        base::ClampCeil(5 * 100.0 / 7), 1);
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
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        base::ClampCeil(1 * 100.0 / 3), 1);
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
TEST_F(ScrollJankV4HistogramEmitterTest,
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
        base::ClampCeil(1 * 100.0 / 32) /* Frame 11 */, 1);
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
        base::ClampCeil(3 * 100.0 / 64) /* Frames 11 & 51-52 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        base::ClampCeil(3 * 100.0 / 64) /* Frames 11 & 51-52 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        base::ClampCeil(1 * 100.0 / 64) /* Frame 11 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncAtStartOfFling",
        base::ClampCeil(1 * 100.0 / 64) /* Frame 11 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFling",
        base::ClampCeil(1 * 100.0 / 64) /* Frame 11 */, 1);
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
        base::ClampCeil(6 * 100.0 / 64) /* Frame 51-52 & 81-84 */, 1);
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
        base::ClampCeil(12 * 100.0 / 64) /* Frames 81-84 & 111-118 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        base::ClampCeil(4 * 100.0 / 64) /* Frames 81-84 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncAtStartOfFling",
        base::ClampCeil(8 * 100.0 / 64) /* Frames 111-118 */, 1);
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
        base::ClampCeil(8 * 100.0 / 32) /* Frames 111-118 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }
}

TEST_F(ScrollJankV4HistogramEmitterTest, IgnoresEmptyScrolls) {
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
        base::ClampCeil(1 * 100.0 / 64) /* Frame 64 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        base::ClampCeil(1 * 100.0 / 64) /* Frame 64 */, 1);
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
        base::ClampCeil(1 * 100.0 / 64) /* Frame 64 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }
}

TEST_F(ScrollJankV4HistogramEmitterTest, CountsSingletonScrolls) {
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
        base::ClampCeil(1 * 100.0 / 64) /* Frame 64 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        0, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        base::ClampCeil(1 * 100.0 / 64) /* Frame 64 */, 1);
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
        base::ClampCeil(1 * 100.0 / 54) /* Frame 64 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }
}

// Regression test for https://crbug.com/475797611.
TEST_F(ScrollJankV4HistogramEmitterTest, ShouldNotCrashWhenAllFramesAreJanky) {
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

TEST_F(ScrollJankV4HistogramEmitterTest, CountsNonDamagingFrames) {
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
        base::ClampCeil(2 * 100.0 / 27) /* Frames 14 & 15 */, 1);
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

    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow"),
                base::BucketsAre(
                    // Frames 14 & 15.
                    base::Bucket(base::ClampCeil(2 * 100.0 / 64), 1),
                    // Frame 89.
                    base::Bucket(base::ClampCeil(1 * 100.0 / 64), 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
                    "MissedVsyncDueToDeceleratingInputFrameDelivery"),
                base::BucketsAre(
                    // Frames 14 & 15.
                    base::Bucket(base::ClampCeil(2 * 100.0 / 64), 1),
                    base::Bucket(0, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
            "MissedVsyncDuringFastScroll"),
        base::BucketsAre(base::Bucket(0, 1),
                         // Frame 89.
                         base::Bucket(base::ClampCeil(1 * 100.0 / 64), 1)));
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
        base::ClampCeil(5 * 100.0 / 64) /* Frames 141, 148 & 171-173 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        base::ClampCeil(1 * 100.0 / 64) /* Frame 148 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        base::ClampCeil(4 * 100.0 / 64) /* Frames 141 & 171-173 */, 1);
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
        base::ClampCeil(6 * 100.0 / 200) /* Frames 89, 141, 148 & 171-173 */,
        1);
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
        base::ClampCeil(2 * 100.0 / 64) /* Frames 241 & 242 */, 1);
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
        base::ClampCeil(1 * 100.0 / 64) /* Frame 241 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFling",
        base::ClampCeil(1 * 100.0 / 64) /* Frame 242 */, 1);
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
        base::ClampCeil(2 * 100.0 / 29) /* Frames 241 & 242 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }
}

TEST_F(ScrollJankV4HistogramEmitterTest, IgnoresCompletelyNonDamagingScrolls) {
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
        base::ClampCeil(1 * 100.0 / 32) /* Frame 16 */, 1);
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
        base::ClampCeil(2 * 100.0 / 64) /* Frames 16 & 48 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        base::ClampCeil(1 * 100.0 / 64) /* Frame 16 */, 1);
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
        base::ClampCeil(1 * 100.0 / 64) /* Frame 48 */, 1);
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
        base::ClampCeil(1 * 100.0 / 32) /* Frame 48 */, 1);
    ExpectNoFixedWindowHistograms(histogram_tester);
  }
}

TEST_F(ScrollJankV4HistogramEmitterTest, LimitsNumberOfPendingFixedWindows) {
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

class MockScrollJankOsReporter : public ScrollJankOsReporter {
 public:
  MOCK_METHOD(void,
              ReportScrollJankStats,
              (uint32_t total_frames, uint32_t janky_frames),
              (override));

  base::WeakPtr<MockScrollJankOsReporter> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockScrollJankOsReporter> weak_factory_{this};
};

TEST_F(ScrollJankV4HistogramEmitterTest, ReportsScrollJankStatsToOs) {
  testing::StrictMock<MockScrollJankOsReporter> os_reporter;
  histogram_emitter_->SetOsReporter(os_reporter.GetWeakPtr());

  histogram_emitter_->OnScrollStarted();
  histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
  histogram_emitter_->OnFrameWithScrollUpdates(
      MakeMissedVsyncCounts({{JankReason::kMissedVsyncDuringFling, 2}}),
      kDamaging);

  EXPECT_CALL(os_reporter, ReportScrollJankStats(2, 1)).Times(1);

  histogram_emitter_->OnScrollEnded();
}

TEST_F(ScrollJankV4HistogramEmitterTest, DeletingOsReporterDoesNotCauseCrash) {
  {
    testing::StrictMock<MockScrollJankOsReporter> os_reporter;
    histogram_emitter_->SetOsReporter(os_reporter.GetWeakPtr());
  }

  histogram_emitter_->OnScrollStarted();
  histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
  histogram_emitter_->OnFrameWithScrollUpdates(
      MakeMissedVsyncCounts({{JankReason::kMissedVsyncDuringFling, 2}}),
      kDamaging);

  histogram_emitter_->OnScrollEnded();
}

struct PerScrollPercentageRoundingTestCase {
  std::string test_name;
  bool round_up_feature_enabled;
  int delayed_frames;
  int presented_frames;
  int expected_percentage;
};

class ScrollJankV4HistogramEmitterPerScrollPercentageRoundingTest
    : public ScrollJankV4HistogramEmitterTest,
      public testing::WithParamInterface<PerScrollPercentageRoundingTestCase> {
};

TEST_P(ScrollJankV4HistogramEmitterPerScrollPercentageRoundingTest,
       EmitsExpectedPercentage) {
  const PerScrollPercentageRoundingTestCase& params = GetParam();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureState(
      features::kScrollJankV4MetricRoundPercentageUp,
      params.round_up_feature_enabled);

  base::HistogramTester histogram_tester;
  histogram_emitter_->OnScrollStarted();
  for (int i = 0; i < params.delayed_frames; i++) {
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({{JankReason::kMissedVsyncDuringFastScroll, 1}}),
        kDamaging);
  }
  for (int i = params.delayed_frames; i < params.presented_frames; i++) {
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
  }
  histogram_emitter_->OnScrollEnded();

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
      params.expected_percentage, 1);
}

INSTANTIATE_TEST_SUITE_P(
    ScrollJankV4HistogramEmitterPerScrollPercentageRoundingTests,
    ScrollJankV4HistogramEmitterPerScrollPercentageRoundingTest,
    testing::Values(
        PerScrollPercentageRoundingTestCase{
            .test_name = "ZeroDelayedOf101_RoundUp",
            .round_up_feature_enabled = true,
            .delayed_frames = 0,
            .presented_frames = 101,
            .expected_percentage = 0,
        },
        PerScrollPercentageRoundingTestCase{
            .test_name = "ZeroDelayedOf101_RoundDown",
            .round_up_feature_enabled = false,
            .delayed_frames = 0,
            .presented_frames = 101,
            .expected_percentage = 0,
        },
        // Regression test for b/590717751: 1 delayed frame out of 101 (~0.99%)
        // must land in bucket 1 when rounding up, rather than bucket 0.
        PerScrollPercentageRoundingTestCase{
            .test_name = "OneDelayedOf101_RoundUp",
            .round_up_feature_enabled = true,
            .delayed_frames = 1,
            .presented_frames = 101,
            .expected_percentage = 1,
        },
        PerScrollPercentageRoundingTestCase{
            .test_name = "OneDelayedOf101_RoundDown",
            .round_up_feature_enabled = false,
            .delayed_frames = 1,
            .presented_frames = 101,
            .expected_percentage = 0,
        },
        // Exact integer percentages must not be incremented by ceiling.
        PerScrollPercentageRoundingTestCase{
            .test_name = "ExactOnePercent_RoundUp",
            .round_up_feature_enabled = true,
            .delayed_frames = 1,
            .presented_frames = 100,
            .expected_percentage = 1,
        },
        PerScrollPercentageRoundingTestCase{
            .test_name = "ExactOnePercent_RoundDown",
            .round_up_feature_enabled = false,
            .delayed_frames = 1,
            .presented_frames = 100,
            .expected_percentage = 1,
        },
        PerScrollPercentageRoundingTestCase{
            .test_name = "AllDelayedOf101_RoundUp",
            .round_up_feature_enabled = true,
            .delayed_frames = 101,
            .presented_frames = 101,
            .expected_percentage = 100,
        },
        PerScrollPercentageRoundingTestCase{
            .test_name = "AllDelayedOf101_RoundDown",
            .round_up_feature_enabled = false,
            .delayed_frames = 101,
            .presented_frames = 101,
            .expected_percentage = 100,
        }),
    [](const auto& info) { return info.param.test_name; });

struct FixedWindowPercentageRoundingTestCase {
  std::string test_name;
  bool round_up_feature_enabled;
  int delayed_frames_decelerating_input;
  int delayed_frames_fast_scroll;
  int expected_decelerating_input_percentage;
  int expected_fast_scroll_percentage;
  int expected_total_percentage;
};

class ScrollJankV4HistogramEmitterFixedWindowPercentageRoundingTest
    : public ScrollJankV4HistogramEmitterTest,
      public testing::WithParamInterface<
          FixedWindowPercentageRoundingTestCase> {};

TEST_P(ScrollJankV4HistogramEmitterFixedWindowPercentageRoundingTest,
       EmitsExpectedPercentages) {
  const FixedWindowPercentageRoundingTestCase& params = GetParam();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureState(
      features::kScrollJankV4MetricRoundPercentageUp,
      params.round_up_feature_enabled);

  base::HistogramTester histogram_tester;
  histogram_emitter_->OnScrollStarted();
  for (int i = 0; i < params.delayed_frames_decelerating_input; i++) {
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts(
            {{JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1}}),
        kDamaging);
  }
  for (int i = 0; i < params.delayed_frames_fast_scroll; i++) {
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({{JankReason::kMissedVsyncDuringFastScroll, 1}}),
        kDamaging);
  }
  const int remaining_frames =
      ScrollJankV4HistogramEmitter::kHistogramEmitFrequency -
      params.delayed_frames_decelerating_input -
      params.delayed_frames_fast_scroll;
  ASSERT_GE(remaining_frames, 0);
  for (int i = 0; i < remaining_frames; i++) {
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame, kDamaging);
  }

  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
      params.expected_total_percentage, 1);
  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      params.expected_decelerating_input_percentage, 1);
  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      params.expected_fast_scroll_percentage, 1);
  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0, 1);
  histogram_tester.ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0, 1);
}

INSTANTIATE_TEST_SUITE_P(
    ScrollJankV4HistogramEmitterFixedWindowPercentageRoundingTests,
    ScrollJankV4HistogramEmitterFixedWindowPercentageRoundingTest,
    testing::Values(
        FixedWindowPercentageRoundingTestCase{
            .test_name = "ZeroDelayed_RoundUp",
            .round_up_feature_enabled = true,
            .delayed_frames_decelerating_input = 0,
            .delayed_frames_fast_scroll = 0,
            .expected_decelerating_input_percentage = 0,
            .expected_fast_scroll_percentage = 0,
            .expected_total_percentage = 0,
        },
        FixedWindowPercentageRoundingTestCase{
            .test_name = "ZeroDelayed_RoundDown",
            .round_up_feature_enabled = false,
            .delayed_frames_decelerating_input = 0,
            .delayed_frames_fast_scroll = 0,
            .expected_decelerating_input_percentage = 0,
            .expected_fast_scroll_percentage = 0,
            .expected_total_percentage = 0,
        },
        // 16 / 64 = 25% exact; 1 / 64 = 1.5625%; 17 / 64 = 26.5625%.
        FixedWindowPercentageRoundingTestCase{
            .test_name = "MixedExactAndFractionalSubreasons_RoundUp",
            .round_up_feature_enabled = true,
            .delayed_frames_decelerating_input = 16,
            .delayed_frames_fast_scroll = 1,
            .expected_decelerating_input_percentage = 25,
            .expected_fast_scroll_percentage = 2,
            .expected_total_percentage = 27,
        },
        FixedWindowPercentageRoundingTestCase{
            .test_name = "MixedExactAndFractionalSubreasons_RoundDown",
            .round_up_feature_enabled = false,
            .delayed_frames_decelerating_input = 16,
            .delayed_frames_fast_scroll = 1,
            .expected_decelerating_input_percentage = 25,
            .expected_fast_scroll_percentage = 1,
            .expected_total_percentage = 26,
        },
        FixedWindowPercentageRoundingTestCase{
            .test_name = "All64Delayed_RoundUp",
            .round_up_feature_enabled = true,
            .delayed_frames_decelerating_input = 64,
            .delayed_frames_fast_scroll = 0,
            .expected_decelerating_input_percentage = 100,
            .expected_fast_scroll_percentage = 0,
            .expected_total_percentage = 100,
        },
        FixedWindowPercentageRoundingTestCase{
            .test_name = "All64Delayed_RoundDown",
            .round_up_feature_enabled = false,
            .delayed_frames_decelerating_input = 64,
            .delayed_frames_fast_scroll = 0,
            .expected_decelerating_input_percentage = 100,
            .expected_fast_scroll_percentage = 0,
            .expected_total_percentage = 100,
        }),
    [](const auto& info) { return info.param.test_name; });

}  // namespace cc
