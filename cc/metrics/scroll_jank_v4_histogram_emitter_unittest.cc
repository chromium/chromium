// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_histogram_emitter.h"

#include <memory>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "cc/metrics/event_metrics.h"
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

void ExpectNoScrollJankHistograms(
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
  histogram_tester.ExpectTotalCount(
      "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
}

}  // namespace

class ScrollJankV4HistogramEmitterTest : public testing::Test {
 public:
  void SetUp() override {
    histogram_emitter_ = std::make_unique<ScrollJankV4HistogramEmitter>();
  }

  void TearDown() override { histogram_emitter_ = nullptr; }

 protected:
  std::unique_ptr<ScrollJankV4HistogramEmitter> histogram_emitter_;
};

TEST_F(ScrollJankV4HistogramEmitterTest,
       EmitsFixedWindowHistogramsEvery64Frames) {
  // First window: NO histograms should be emitted for the first 64 frames. Note
  // that the first fixed window contains 65 frames to compensate for the fact
  // that the very first frame can never be janky.
  {
    base::HistogramTester histogram_tester;

    // Frames 1-10: Non-janky.
    for (int i = 1; i <= 10; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    // Frame 11: Janky for ALL reasons.
    histogram_emitter_->OnFrameWithScrollUpdates(MakeMissedVsyncCounts({
        {JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1},
        {JankReason::kMissedVsyncDuringFastScroll, 2},
        {JankReason::kMissedVsyncAtStartOfFling, 3},
        {JankReason::kMissedVsyncDuringFling, 4},
    }));

    // Frames 12-20: Non-janky.
    for (int i = 12; i <= 20; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    // Frames 21-22: Janky due to violating the running consistency rule.
    for (int i = 21; i <= 22; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(MakeMissedVsyncCounts({
          {JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1},
      }));
    }

    // Frames 23-30: Non-janky.
    for (int i = 23; i <= 30; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    // Frame 31-33: Janky due to violating the fast scroll continuity rule.
    for (int i = 31; i <= 33; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(MakeMissedVsyncCounts({
          {JankReason::kMissedVsyncDuringFastScroll, 1},
      }));
    }

    // Frames 34-40: Non-janky.
    for (int i = 34; i <= 40; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    // Frame 41-44: Janky due to violating the fling continuity rule at the
    // transition from a fast scroll.
    for (int i = 41; i <= 44; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(MakeMissedVsyncCounts({
          {JankReason::kMissedVsyncAtStartOfFling, 1},
      }));
    }

    // Frames 45-50: Non-janky.
    for (int i = 45; i <= 50; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    // Frames 51-55: Janky due to violating the fling continuity rule in the
    // middle of a fling.
    for (int i = 51; i <= 55; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(MakeMissedVsyncCounts({
          {JankReason::kMissedVsyncDuringFling, 1},
      }));
    }

    // Frames 56-64: Non-janky.
    for (int i = 56; i <= 64; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    ExpectNoScrollJankHistograms(histogram_tester);
  }

  // UMA histograms SHOULD be emitted for the 65th frame.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);

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
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // Second window: NO histograms should be emitted for the next 63 frames.
  {
    base::HistogramTester histogram_tester;

    for (int i = 1; i <= 63; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    ExpectNoScrollJankHistograms(histogram_tester);
  }

  // UMA histograms SHOULD be emitted for the 64th frame.
  {
    base::HistogramTester histogram_tester;
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);

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
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }
}

TEST_F(ScrollJankV4HistogramEmitterTest,
       EmitsPerScrollHistogramsAtEndOfScroll) {
  // NO histograms for the first scroll should be emitted before it ends.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();  // First scroll.

    // 1 non-janky frame.
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);

    // 5 janky frames.
    histogram_emitter_->OnFrameWithScrollUpdates(MakeMissedVsyncCounts({
        {JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1},
        {JankReason::kMissedVsyncDuringFastScroll, 2},
        {JankReason::kMissedVsyncAtStartOfFling, 3},
        {JankReason::kMissedVsyncDuringFling, 4},
    }));
    histogram_emitter_->OnFrameWithScrollUpdates(MakeMissedVsyncCounts({
        {JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1},
    }));
    histogram_emitter_->OnFrameWithScrollUpdates(MakeMissedVsyncCounts({
        {JankReason::kMissedVsyncDuringFastScroll, 1},
    }));
    histogram_emitter_->OnFrameWithScrollUpdates(MakeMissedVsyncCounts({
        {JankReason::kMissedVsyncAtStartOfFling, 1},
    }));
    histogram_emitter_->OnFrameWithScrollUpdates(MakeMissedVsyncCounts({
        {JankReason::kMissedVsyncDuringFling, 1},
    }));

    // 1 non-janky frame.
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);

    ExpectNoScrollJankHistograms(histogram_tester);
  }

  // UMA histograms for the first scroll SHOULD be emitted when it ends.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollEnded();  // First scroll.

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 5 * 100 / 7, 1);
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

  // NO histograms for the second scroll should be emitted before it ends.
  {
    base::HistogramTester histogram_tester;

    // Robustness test: Under normal circumstances, the histogram emitter should
    // have received a scroll begin event for the second scroll, but it didn't
    // for some unexpected reason. The histogram emitter should be able to
    // handle this situation, using the fact that the first scroll has just
    // ended.

    for (int i = 1; i <= 10; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    ExpectNoScrollJankHistograms(histogram_tester);
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

  // NO histograms should be emitted for the third scroll, even after it ends,
  // because it was empty. NO histograms should also be emitted for the fourth
  // scroll before the histogram emitter is destroyed.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollEnded();    // Third scroll.
    histogram_emitter_->OnScrollStarted();  // Fourth scroll.

    // 1 non-janky frame.
    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);

    // 1 janky frame.
    histogram_emitter_->OnFrameWithScrollUpdates(MakeMissedVsyncCounts({
        {JankReason::kMissedVsyncDuringFastScroll, 4},
        {JankReason::kMissedVsyncDuringFling, 2},
    }));

    // 1 non-janky frame.
    histogram_emitter_->OnFrameWithScrollUpdates(
        kNonJankyFrame);  // Fourth scroll.

    ExpectNoScrollJankHistograms(histogram_tester);
  }

  // UMA histograms for the fourth scroll SHOULD be emitted when the histogram
  // emitter is destroyed.
  {
    base::HistogramTester histogram_tester;

    delete histogram_emitter_.release();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 1 * 100 / 3, 1);
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
}

/*
A combined scenario which tests that the histogram emitter emits both fixed
window and per-scroll UMA histograms. This test also verifies that the emission
of fixed window and per-scroll histograms is independent (e.g. a scroll ending
in the middle of a fixed window shouldn't affect the fixed window calculation
and vice versa).

Presented frame: :1        33:34       65:66       97:98      129:
Fixed windows:   |<-------window 1------>|<-------window 2------>|
Scrolls:         |<scroll 1->|<------scroll 2------->|<scroll 3->|
Delayed frames:  :     1     :     2     :     4     :     8     :
*/
TEST_F(ScrollJankV4HistogramEmitterTest,
       EmitsBothFixedWindowAndPerScrollHistogramsIndependently) {
  // Start of scroll 1, frames 1-32: NO histograms should be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    // Frames 1-10: Non-janky.
    for (int i = 1; i <= 10; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    // Frame 11: Janky for ALL reasons.
    histogram_emitter_->OnFrameWithScrollUpdates(MakeMissedVsyncCounts({
        {JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1},
        {JankReason::kMissedVsyncDuringFastScroll, 2},
        {JankReason::kMissedVsyncAtStartOfFling, 3},
        {JankReason::kMissedVsyncDuringFling, 4},
    }));

    // Frames 12-33: Non-janky.
    for (int i = 12; i <= 33; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    ExpectNoScrollJankHistograms(histogram_tester);
  }

  // End of scroll 1: Per-scroll histogram should be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        1 * 100 / 33 /* Frame 11 */, 1);
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

  // Start of scroll 2, frames 33-64: NO histograms should be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    // Frames 34-50: Non-janky.
    for (int i = 34; i <= 50; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    // Frames 51-52: Janky due to violating the running consistency rule.
    for (int i = 51; i <= 52; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(MakeMissedVsyncCounts({
          {JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 1},
      }));
    }

    // Frames 53-64: Non-janky.
    for (int i = 53; i <= 64; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    ExpectNoScrollJankHistograms(histogram_tester);
  }

  // Frame 65: Fixed window histograms SHOULD be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);

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
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // Frames 66-97: NO histograms should be emitted.
  {
    base::HistogramTester histogram_tester;

    // Frames 66-80: Non-janky.
    for (int i = 66; i <= 80; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    // Frames 81-84: Janky due to violating the fast scroll continuity rule.
    for (int i = 81; i <= 84; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(MakeMissedVsyncCounts({
          {JankReason::kMissedVsyncDuringFastScroll, 17},
      }));
    }

    // Frames 85-97: Non-janky.
    for (int i = 85; i <= 97; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    ExpectNoScrollJankHistograms(histogram_tester);
  }

  // End of scroll 2: Per-scroll histograms SHOULD be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        6 * 100 / 64 /* Frame 51-52 & 81-84 */, 1);
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

  // Start of scroll 3, frames 98-128: NO histograms should be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    // Frames 98-110: Non-janky.
    for (int i = 98; i <= 110; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    // Frame 111-118: Janky due to violating the fling continuity rule at the
    // transition from a fast scroll.
    for (int i = 111; i <= 118; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(MakeMissedVsyncCounts({
          {JankReason::kMissedVsyncAtStartOfFling, 19},
      }));
    }

    // Frames 119-128: Non-janky.
    for (int i = 119; i <= 128; i++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    ExpectNoScrollJankHistograms(histogram_tester);
  }

  // Frame 129: Fixed window histograms SHOULD be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);

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
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }

  // End of scroll 3: Per-scroll histograms SHOULD be emitted.
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        8 * 100 / 32 /* Frames 111-118 */, 1);
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
}

TEST_F(ScrollJankV4HistogramEmitterTest,
       FramesWhichDoNotCountTowardsHistogramFrameCount) {
  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    for (int frame = 1; frame <= 10; frame++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    // A non-janky frame which doesn't count towards the histogram frame count
    // followed by non-janky frame 11 which counts towards the histogram frame
    // count.
    histogram_emitter_->OnFrameWithScrollUpdates(
        kNonJankyFrame, /* counts_towards_histogram_frame_count= */ false);

    for (int frame = 11; frame <= 20; frame++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    // A non-janky frame which doesn't count towards the histogram frame count
    // followed by janky frame 21 which counts towards the histogram frame
    // count.
    histogram_emitter_->OnFrameWithScrollUpdates(
        kNonJankyFrame, /* counts_towards_histogram_frame_count= */ false);
    histogram_emitter_->OnFrameWithScrollUpdates(MakeMissedVsyncCounts(
        {{JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 2}}));

    for (int frame = 22; frame <= 30; frame++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    // A janky frame which doesn't count towards the histogram frame count
    // followed by non-janky frame 31 which counts towards the histogram frame
    // count.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({{JankReason::kMissedVsyncDuringFastScroll, 3}}),
        /* counts_towards_histogram_frame_count= */ false);

    for (int frame = 31; frame <= 40; frame++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    // A janky frame which doesn't count towards the histogram frame count
    // followed by janky frame 41 which counts towards the histogram frame
    // count.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({{JankReason::kMissedVsyncAtStartOfFling, 5}}),
        /* counts_towards_histogram_frame_count= */ false);
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({{JankReason::kMissedVsyncDuringFling, 4}}));

    ExpectNoScrollJankHistograms(histogram_tester);
  }

  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollEnded();

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
        3 * 100 / 41 /* Frames 21, 31 & 41 */, 1);
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

  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    for (int frame = 42; frame <= 50; frame++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    // A janky frame which doesn't count towards the histogram frame count.
    // Since there won't be any more frames before the end of the scroll, this
    // jank will end up "lost".
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts(
            {{JankReason::kMissedVsyncDuringFastScroll, 1000}}),
        /* counts_towards_histogram_frame_count= */ false);

    ExpectNoScrollJankHistograms(histogram_tester);
  }

  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollEnded();

    // Note that the "lost" janky frame above doesn't count towards the
    // per-scroll histogram.
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0, 1);
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

  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnScrollStarted();

    for (int frame = 51; frame <= 60; frame++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    // Multiple frames which don't count towards the histogram frame count
    // followed by non-janky frame 61 which counts towards the histogram frame
    // count.
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts(
            {{JankReason::kMissedVsyncDueToDeceleratingInputFrameDelivery, 8}}),
        /* counts_towards_histogram_frame_count= */ false);
    histogram_emitter_->OnFrameWithScrollUpdates(
        kNonJankyFrame,
        /* counts_towards_histogram_frame_count= */ false);
    histogram_emitter_->OnFrameWithScrollUpdates(
        MakeMissedVsyncCounts({{JankReason::kMissedVsyncDuringFastScroll, 9}}),
        /* counts_towards_histogram_frame_count= */ false);
    histogram_emitter_->OnFrameWithScrollUpdates(
        kNonJankyFrame,
        /* counts_towards_histogram_frame_count= */ false);

    for (int frame = 61; frame <= 64; frame++) {
      histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);
    }

    ExpectNoScrollJankHistograms(histogram_tester);
  }

  {
    base::HistogramTester histogram_tester;

    histogram_emitter_->OnFrameWithScrollUpdates(kNonJankyFrame);

    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
        4 * 100 / 64 /* Frames 21, 31, 41 & 61 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDueToDeceleratingInputFrameDelivery",
        2 * 100 / 64 /* Frames 21 & 61 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFastScroll",
        2 * 100 / 64 /* Frames 31 & 61 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncAtStartOfFling",
        1 * 100 / 64 /* Frame 41 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
        "MissedVsyncDuringFling",
        1 * 100 / 64 /* Frame 41 */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsSum4.FixedWindow",
        2 + 3 + 4 + 5 + 8 + 9 /* Frames 21, 31, 41 (2x) & 61 (2x) */, 1);
    histogram_tester.ExpectUniqueSample(
        "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 9 /* Frame 61 */, 1);
    histogram_tester.ExpectTotalCount(
        "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
  }
}

}  // namespace cc
