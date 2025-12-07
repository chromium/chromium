// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_dropped_frame_tracker.h"

#include <memory>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

constexpr base::TimeDelta kVsyncInterval = base::Milliseconds(16);

constexpr base::TimeTicks MillisSinceEpoch(int64_t millis) {
  return base::TimeTicks() + base::Milliseconds(millis);
}

struct FrameTimestamps {
  base::TimeTicks first_input_ts;
  base::TimeTicks last_input_ts;
  base::TimeTicks presentation_ts;
};

constexpr int kHistogramEmitFrequency =
    ScrollJankDroppedFrameTracker::kHistogramEmitFrequency;
constexpr int kFirstWindowSize = kHistogramEmitFrequency + 1;
constexpr const char* kDelayedFramesWindowHistogram =
    ScrollJankDroppedFrameTracker::kDelayedFramesWindowHistogram;
constexpr const char* kDelayedFramesPerScrollHistogram =
    ScrollJankDroppedFrameTracker::kDelayedFramesPerScrollHistogram;
constexpr const char* kMissedVsyncsSumInWindowHistogram =
    ScrollJankDroppedFrameTracker::kMissedVsyncsSumInWindowHistogram;
constexpr const char* kMissedVsyncsMaxInWindowHistogram =
    ScrollJankDroppedFrameTracker::kMissedVsyncsMaxInWindowHistogram;
constexpr const char* kMissedVsyncsSumPerScrollHistogram =
    ScrollJankDroppedFrameTracker::kMissedVsyncsSumPerScrollHistogram;
constexpr const char* kMissedVsyncsMaxPerScrollHistogram =
    ScrollJankDroppedFrameTracker::kMissedVsyncsMaxPerScrollHistogram;
constexpr const char* kMissedVsyncsPerFrameHistogram =
    ScrollJankDroppedFrameTracker::kMissedVsyncsPerFrameHistogram;
}  // namespace

class ScrollJankDroppedFrameTrackerTest : public testing::Test {
 public:
  ScrollJankDroppedFrameTrackerTest() = default;

  void SetUp() override {
    ResetHistogramTester();
    scroll_jank_dropped_frame_tracker_ =
        std::make_unique<ScrollJankDroppedFrameTracker>();
    scroll_jank_dropped_frame_tracker_->OnScrollStarted();
  }

  void ResetHistogramTester() {
    histogram_tester = std::make_unique<base::HistogramTester>();
  }

  FrameTimestamps ProduceAndReportMockFrames(FrameTimestamps prev_frame,
                                             int num_frames) {
    for (int i = 1; i <= num_frames; i++) {
      prev_frame.first_input_ts += kVsyncInterval;
      prev_frame.last_input_ts += kVsyncInterval;
      prev_frame.presentation_ts += kVsyncInterval;
      ReportLatestPresentationDataToTracker(prev_frame);
    }
    return prev_frame;
  }

  void ReportLatestPresentationDataToTracker(const FrameTimestamps& frame) {
    base::SimpleTestTickClock tick_clock;
    tick_clock.SetNowTicks(frame.first_input_ts);
    auto event = ScrollUpdateEventMetrics::CreateForTesting(
        ui::EventType::kGestureScrollUpdate, ui::ScrollInputType::kWheel,
        /*is_inertial=*/false,
        ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
        /*delta=*/10.0f, frame.first_input_ts, base::TimeTicks(), &tick_clock,
        /*trace_id=*/std::nullopt);
    scroll_jank_dropped_frame_tracker_->ReportLatestPresentationData(
        *event, frame.last_input_ts, frame.presentation_ts, kVsyncInterval);
  }

  std::unique_ptr<base::HistogramTester> histogram_tester;

 protected:
  std::unique_ptr<ScrollJankDroppedFrameTracker>
      scroll_jank_dropped_frame_tracker_;
};

TEST_F(ScrollJankDroppedFrameTrackerTest, EmitsHistograms) {
  FrameTimestamps f1 = {MillisSinceEpoch(103), MillisSinceEpoch(103),
                        MillisSinceEpoch(148)};

  FrameTimestamps last_frame =
      ProduceAndReportMockFrames(f1, kHistogramEmitFrequency);

  histogram_tester->ExpectUniqueSample(kDelayedFramesWindowHistogram, 0, 0);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumInWindowHistogram, 0, 0);

  // For first window we emit histogram at 65th reported frame.
  last_frame = ProduceAndReportMockFrames(last_frame, 1);

  histogram_tester->ExpectUniqueSample(kDelayedFramesWindowHistogram, 0, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumInWindowHistogram, 0, 1);

  // For subsequent windows we emit histogram every 64 frames.
  ProduceAndReportMockFrames(last_frame, kHistogramEmitFrequency);

  histogram_tester->ExpectUniqueSample(kDelayedFramesWindowHistogram, 0, 2);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumInWindowHistogram, 0, 2);
}

/*
Test that regular frame production doesn't cause missed frames.
vsync                   v0      v1
                        |       |
input   I0  I1  I2  I3
        |   |   |   |
F1:     |---------------| {I0, I1}
F2:             |---------------| {I2, I3}
 */
TEST_F(ScrollJankDroppedFrameTrackerTest, FrameProducedEveryVsync) {
  const std::vector<base::TimeTicks> inputs = {
      MillisSinceEpoch(103), MillisSinceEpoch(111), MillisSinceEpoch(119),
      MillisSinceEpoch(127)};
  const std::vector<base::TimeTicks> vsyncs = {MillisSinceEpoch(148),
                                               MillisSinceEpoch(164)};

  FrameTimestamps f1 = {inputs[0], inputs[1], vsyncs[0]};
  FrameTimestamps f2 = {inputs[2], inputs[3], vsyncs[1]};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);

  // To trigger per window histogram emission.
  int frames_to_emit_histogram = kFirstWindowSize - 2;
  ProduceAndReportMockFrames(f2, frames_to_emit_histogram);

  histogram_tester->ExpectUniqueSample(kDelayedFramesWindowHistogram, 0, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumInWindowHistogram, 0, 1);
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
TEST_F(ScrollJankDroppedFrameTrackerTest, NoFrameProducedForMissingInput) {
  const std::vector<base::TimeTicks> inputs = {
      MillisSinceEpoch(103), MillisSinceEpoch(111), MillisSinceEpoch(135),
      MillisSinceEpoch(143)};
  const std::vector<base::TimeTicks> vsyncs = {MillisSinceEpoch(148),
                                               MillisSinceEpoch(180)};

  FrameTimestamps f1 = {MillisSinceEpoch(103), MillisSinceEpoch(111),
                        MillisSinceEpoch(148)};
  FrameTimestamps f2 = {MillisSinceEpoch(135), MillisSinceEpoch(143),
                        MillisSinceEpoch(180)};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);

  // To trigger per window histogram emission.
  int frames_to_emit_histogram = kFirstWindowSize - 2;
  ProduceAndReportMockFrames(f2, frames_to_emit_histogram);

  histogram_tester->ExpectUniqueSample(kDelayedFramesWindowHistogram, 0, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumInWindowHistogram, 0, 1);
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
TEST_F(ScrollJankDroppedFrameTrackerTest, MissedVsyncWhenInputWasPresent) {
  const std::vector<base::TimeTicks> inputs = {
      MillisSinceEpoch(103), MillisSinceEpoch(111), MillisSinceEpoch(119),
      MillisSinceEpoch(127), MillisSinceEpoch(135), MillisSinceEpoch(143)};
  const std::vector<base::TimeTicks> vsyncs = {
      MillisSinceEpoch(148), MillisSinceEpoch(196), MillisSinceEpoch(228)};

  FrameTimestamps f1 = {inputs[0], inputs[1], vsyncs[0]};
  FrameTimestamps f2 = {inputs[2], inputs[3], vsyncs[1]};
  FrameTimestamps f3 = {inputs[4], inputs[5], vsyncs[2]};

  ReportLatestPresentationDataToTracker(f1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsPerFrameHistogram, 0, 1);
  ReportLatestPresentationDataToTracker(f2);
  histogram_tester->ExpectBucketCount(kMissedVsyncsPerFrameHistogram, 2, 1);
  ReportLatestPresentationDataToTracker(f3);
  histogram_tester->ExpectBucketCount(kMissedVsyncsPerFrameHistogram, 1, 1);

  // To trigger per window histogram emission.
  int frames_to_emit_histogram = kFirstWindowSize - 3;
  FrameTimestamps last_frame_ts =
      ProduceAndReportMockFrames(f3, frames_to_emit_histogram);
  histogram_tester->ExpectBucketCount(kMissedVsyncsPerFrameHistogram, 0, 63);

  // F2 and F3 are janky frames.
  const int expected_missed_frames = 2;
  const int expected_delayed_frames_percentage =
      (100 * expected_missed_frames) / kHistogramEmitFrequency;
  // Frame F2 missed 2 vsyncs, F3 missed 1 vsync.
  const int expected_max = 2;
  const int expected_sum = 3;

  histogram_tester->ExpectUniqueSample(kDelayedFramesWindowHistogram,
                                       expected_delayed_frames_percentage, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumInWindowHistogram,
                                       expected_sum, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsMaxInWindowHistogram,
                                       expected_max, 1);

  // The counters were reset for next set of `kHistogramEmitFrequency` frames.
  ProduceAndReportMockFrames(last_frame_ts, kHistogramEmitFrequency);

  histogram_tester->ExpectBucketCount(kDelayedFramesWindowHistogram, 0, 1);
  histogram_tester->ExpectBucketCount(kMissedVsyncsSumInWindowHistogram, 0, 1);
  histogram_tester->ExpectBucketCount(kMissedVsyncsMaxInWindowHistogram, 0, 1);
  // Other non-zero buckets for histogram were tested earlier in the code.
  histogram_tester->ExpectBucketCount(kMissedVsyncsPerFrameHistogram, 0, 127);
}

/*
Tests that the scroll jank metric's histograms for a scroll are emitted at the
beginning of the next scroll.
vsync                   v0              v1        v2
                        |    |    |     |    |    |
input   I0  I1  I2  I3  I4  I5
        |   |   |   |   |   |
F1:     |---------------| {I0, I1}
F2:             |-----------------------| {I2, I3}
F3:                     |-------------------------| {I4, I5}
*/
TEST_F(ScrollJankDroppedFrameTrackerTest,
       ShouldEmitMetricsAtStartOfNextScroll) {
  const std::vector<base::TimeTicks> inputs = {
      MillisSinceEpoch(103), MillisSinceEpoch(111), MillisSinceEpoch(119),
      MillisSinceEpoch(127), MillisSinceEpoch(135), MillisSinceEpoch(143)};
  const std::vector<base::TimeTicks> vsyncs = {
      MillisSinceEpoch(148), MillisSinceEpoch(196), MillisSinceEpoch(228)};

  FrameTimestamps f1 = {inputs[0], inputs[1], vsyncs[0]};
  FrameTimestamps f2 = {inputs[2], inputs[3], vsyncs[1]};
  FrameTimestamps f3 = {inputs[4], inputs[5], vsyncs[2]};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);
  ReportLatestPresentationDataToTracker(f3);
  const int total_frames = 10;
  ProduceAndReportMockFrames(f3, total_frames - 3);

  histogram_tester->ExpectTotalCount(kMissedVsyncsSumPerScrollHistogram, 0);
  histogram_tester->ExpectTotalCount(kMissedVsyncsMaxPerScrollHistogram, 0);
  histogram_tester->ExpectTotalCount(kDelayedFramesPerScrollHistogram, 0);

  // The tracker should emit all metrics at the beginning of the next scroll.
  ResetHistogramTester();
  scroll_jank_dropped_frame_tracker_->OnScrollStarted();

  // F2 and F3 are janky frames.
  const int expected_missed_frames = 2;
  const int expected_delayed_frames_percentage =
      (100 * expected_missed_frames) / total_frames;
  // Frame F2 missed 2 vsyncs, F3 missed 1 vsync.
  const int expected_max = 2;
  const int expected_sum = 3;

  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumPerScrollHistogram,
                                       expected_sum, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsMaxPerScrollHistogram,
                                       expected_max, 1);
  histogram_tester->ExpectUniqueSample(kDelayedFramesPerScrollHistogram,
                                       expected_delayed_frames_percentage, 1);

  // The tracker SHOULDN'T emit any more metrics when it's destroyed.
  ResetHistogramTester();
  delete scroll_jank_dropped_frame_tracker_.release();

  histogram_tester->ExpectTotalCount(kMissedVsyncsSumPerScrollHistogram, 0);
  histogram_tester->ExpectTotalCount(kMissedVsyncsMaxPerScrollHistogram, 0);
  histogram_tester->ExpectTotalCount(kDelayedFramesPerScrollHistogram, 0);
}

/*
Tests for the scroll jank metric's per-scroll histograms. To avoid duplication,
all per-scroll tests use the same scenario depicted below.
vsync                   v0              v1        v2
                        |    |    |     |    |    |
input   I0  I1  I2  I3  I4  I5
        |   |   |   |   |    |
F1:     |---------------| {I0, I1}
F2:             |-----------------------| {I2, I3}
F3:                     |-------------------------| {I4, I5}
*/
class ScrollJankDroppedFrameTrackerPerScrollTest
    : public ScrollJankDroppedFrameTrackerTest {
 public:
  ScrollJankDroppedFrameTrackerPerScrollTest() = default;

  void ProduceAndReportScrollFrames() {
    const std::vector<base::TimeTicks> inputs = {
        MillisSinceEpoch(103), MillisSinceEpoch(111), MillisSinceEpoch(119),
        MillisSinceEpoch(127), MillisSinceEpoch(135), MillisSinceEpoch(143)};
    const std::vector<base::TimeTicks> vsyncs = {
        MillisSinceEpoch(148), MillisSinceEpoch(196), MillisSinceEpoch(228)};

    FrameTimestamps f1 = {inputs[0], inputs[1], vsyncs[0]};
    FrameTimestamps f2 = {inputs[2], inputs[3], vsyncs[1]};
    FrameTimestamps f3 = {inputs[4], inputs[5], vsyncs[2]};

    ReportLatestPresentationDataToTracker(f1);
    ReportLatestPresentationDataToTracker(f2);
    ReportLatestPresentationDataToTracker(f3);

    ProduceAndReportMockFrames(f3, total_frames - 3);
  }

  static const int total_frames = 10;

  // F2 and F3 are janky frames.
  static const int expected_missed_frames = 2;
  static const int expected_delayed_frames_percentage =
      (100 * expected_missed_frames) / 10;

  // Frame F2 missed 2 vsyncs, F3 missed 1 vsync.
  static const int expected_missed_vsyncs_sum = 3;
  static const int expected_missed_vsyncs_max = 2;
};

/*
Tests that the scroll jank metric's histograms for a scroll are emitted when the
tracker is destroyed.
*/
TEST_F(ScrollJankDroppedFrameTrackerPerScrollTest,
       ShouldEmitMetricsWhenDestroyed) {
  ProduceAndReportScrollFrames();

  // The tracker should emit all metrics when it's destroyed.
  delete scroll_jank_dropped_frame_tracker_.release();

  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumPerScrollHistogram,
                                       expected_missed_vsyncs_sum, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsMaxPerScrollHistogram,
                                       expected_missed_vsyncs_max, 1);
  histogram_tester->ExpectUniqueSample(kDelayedFramesPerScrollHistogram,
                                       expected_delayed_frames_percentage, 1);
}

}  // namespace cc
