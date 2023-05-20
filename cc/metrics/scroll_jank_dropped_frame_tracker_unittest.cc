// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_dropped_frame_tracker.h"

#include <memory>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

constexpr base::TimeDelta kVsyncInterval = base::Milliseconds(16);

struct FrameTimestamps {
  base::TimeTicks first_input_ts;
  base::TimeTicks last_input_ts;
  base::TimeTicks presentation_ts;

  FrameTimestamps(base::TimeTicks first_input,
                  base::TimeTicks last_input,
                  base::TimeTicks presentation)
      : first_input_ts(first_input),
        last_input_ts(last_input),
        presentation_ts(presentation) {}

  FrameTimestamps(int first_input, int last_input, int presentation)
      : first_input_ts(base::TimeTicks() + base::Milliseconds(first_input)),
        last_input_ts(base::TimeTicks() + base::Milliseconds(last_input)),
        presentation_ts(base::TimeTicks() + base::Milliseconds(presentation)) {}
};

constexpr int kHistogramEmitFrequency =
    ScrollJankDroppedFrameTracker::kHistogramEmitFrequency;
constexpr int kFirstWindowSize = kHistogramEmitFrequency + 1;
constexpr const char* kDelayedFramesHistogram =
    ScrollJankDroppedFrameTracker::kDelayedFramesHistogram;
constexpr const char* kMissedVsyncsHistogram =
    ScrollJankDroppedFrameTracker::kMissedVsyncsHistogram;
}  // namespace

class ScrollJankDroppedFrameTrackerTest : public testing::Test {
 public:
  ScrollJankDroppedFrameTrackerTest() = default;

  void SetUp() override {
    histogram_tester = std::make_unique<base::HistogramTester>();
    scroll_jank_dropped_frame_tracker_ =
        std::make_unique<ScrollJankDroppedFrameTracker>();
  }

  FrameTimestamps ProduceAndReportMockFrames(FrameTimestamps prev_frame,
                                             int num_frames) {
    for (int i = 1; i <= num_frames; i++) {
      prev_frame.first_input_ts += kVsyncInterval;
      prev_frame.last_input_ts += kVsyncInterval;
      prev_frame.presentation_ts += kVsyncInterval;

      scroll_jank_dropped_frame_tracker_->ReportLatestPresentationData(
          prev_frame.first_input_ts, prev_frame.last_input_ts,
          prev_frame.presentation_ts, kVsyncInterval);
    }
    return prev_frame;
  }

  void ReportLatestPresentationDataToTracker(const FrameTimestamps& frame) {
    scroll_jank_dropped_frame_tracker_->ReportLatestPresentationData(
        frame.first_input_ts, frame.last_input_ts, frame.presentation_ts,
        kVsyncInterval);
  }

  std::unique_ptr<base::HistogramTester> histogram_tester;

 private:
  std::unique_ptr<ScrollJankDroppedFrameTracker>
      scroll_jank_dropped_frame_tracker_;
};

TEST_F(ScrollJankDroppedFrameTrackerTest, EmitsHistograms) {
  FrameTimestamps f1 = {103, 103, 148};

  FrameTimestamps last_frame =
      ProduceAndReportMockFrames(f1, kHistogramEmitFrequency);

  histogram_tester->ExpectUniqueSample(kDelayedFramesHistogram, 0, 0);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsHistogram, 0, 0);

  // For first window we emit histogram at 65th reported frame.
  last_frame = ProduceAndReportMockFrames(last_frame, 1);

  histogram_tester->ExpectUniqueSample(kDelayedFramesHistogram, 0, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsHistogram, 0, 1);

  // For subsequent windows we emit histogram every 64 frames.
  ProduceAndReportMockFrames(last_frame, kHistogramEmitFrequency);

  histogram_tester->ExpectUniqueSample(kDelayedFramesHistogram, 0, 2);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsHistogram, 0, 2);
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
  const std::vector<int> inputs = {103, 111, 119, 127};
  const std::vector<int> vsyncs = {148, 164};

  FrameTimestamps f1 = {inputs[0], inputs[1], vsyncs[0]};
  FrameTimestamps f2 = {inputs[2], inputs[3], vsyncs[1]};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);

  // To trigger histogram emission.
  int frames_to_emit_histogram = kFirstWindowSize - 2;
  ProduceAndReportMockFrames(f2, frames_to_emit_histogram);

  histogram_tester->ExpectUniqueSample(kDelayedFramesHistogram, 0, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsHistogram, 0, 1);
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
  const std::vector<int> inputs = {103, 111, 135, 143};
  const std::vector<int> vsyncs = {148, 180};

  FrameTimestamps f1 = {103, 111, 148};
  FrameTimestamps f2 = {135, 143, 180};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);

  // To trigger histogram emission.
  int frames_to_emit_histogram = kFirstWindowSize - 2;
  ProduceAndReportMockFrames(f2, frames_to_emit_histogram);

  histogram_tester->ExpectUniqueSample(kDelayedFramesHistogram, 0, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsHistogram, 0, 1);
}

/*
Test that when a frame took too long to be produced shows up in the metric.
vsync                   v0              v1
                        |       |       |
input   I0  I1  I2  I3
        |   |   |   |
F1:     |---------------| {I0, I1}
F2:             |-----------------------| {I2, I3}
 */
TEST_F(ScrollJankDroppedFrameTrackerTest, MissedVsyncWhenInputWasPresent) {
  const std::vector<int> inputs = {103, 111, 119, 127};
  const std::vector<int> vsyncs = {148, 196};

  FrameTimestamps f1 = {inputs[0], inputs[1], vsyncs[0]};
  FrameTimestamps f2 = {inputs[2], inputs[3], vsyncs[1]};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);

  // To trigger histogram emission.
  int frames_to_emit_histogram = kFirstWindowSize - 2;
  FrameTimestamps last_frame_ts =
      ProduceAndReportMockFrames(f2, frames_to_emit_histogram);

  int expected_missed_frames = 1;
  int expected_bucket =
      (100 * expected_missed_frames) / kHistogramEmitFrequency;
  histogram_tester->ExpectUniqueSample(kDelayedFramesHistogram, expected_bucket,
                                       1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsHistogram, 2, 1);

  // The counters were reset for next set of `kHistogramEmitFrequency` frames.
  ProduceAndReportMockFrames(last_frame_ts, kHistogramEmitFrequency);

  histogram_tester->ExpectBucketCount(kDelayedFramesHistogram, 0, 1);
  histogram_tester->ExpectBucketCount(kMissedVsyncsHistogram, 0, 1);
}

}  // namespace cc
