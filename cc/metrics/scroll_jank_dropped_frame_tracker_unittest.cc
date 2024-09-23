// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_dropped_frame_tracker.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
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
constexpr const char* kDelayedFramesWindowHistogram =
    ScrollJankDroppedFrameTracker::kDelayedFramesWindowHistogram;
constexpr const char* kMissedVsyncsWindowHistogram =
    ScrollJankDroppedFrameTracker::kMissedVsyncsWindowHistogram;
constexpr const char* kDelayedFramesPerScrollHistogram =
    ScrollJankDroppedFrameTracker::kDelayedFramesPerScrollHistogram;
constexpr const char* kMissedVsyncsPerScrollHistogram =
    ScrollJankDroppedFrameTracker::kMissedVsyncsPerScrollHistogram;
constexpr const char* kMissedVsyncsSumInWindowHistogram =
    ScrollJankDroppedFrameTracker::kMissedVsyncsSumInWindowHistogram;
constexpr const char* kMissedVsyncsSumInVsyncWindowHistogram =
    ScrollJankDroppedFrameTracker::kMissedVsyncsSumInVsyncWindowHistogram;
constexpr const char* kMissedVsyncsMaxInWindowHistogram =
    ScrollJankDroppedFrameTracker::kMissedVsyncsMaxInWindowHistogram;
constexpr const char* kMissedVsyncsMaxInVsyncWindowHistogram =
    ScrollJankDroppedFrameTracker::kMissedVsyncsMaxInVsyncWindowHistogram;
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
    histogram_tester = std::make_unique<base::HistogramTester>();
    scroll_jank_dropped_frame_tracker_ =
        std::make_unique<ScrollJankDroppedFrameTracker>();
    scroll_jank_dropped_frame_tracker_->OnScrollStarted();
  }

  FrameTimestamps ProduceAndReportMockFrames(FrameTimestamps prev_frame,
                                             int num_frames) {
    for (int i = 1; i <= num_frames; i++) {
      prev_frame.first_input_ts += kVsyncInterval;
      prev_frame.last_input_ts += kVsyncInterval;
      prev_frame.presentation_ts += kVsyncInterval;

      base::SimpleTestTickClock tick_clock;
      tick_clock.SetNowTicks(prev_frame.first_input_ts);

      auto event = ScrollUpdateEventMetrics::CreateForTesting(
          ui::EventType::kGestureScrollUpdate, ui::ScrollInputType::kWheel,
          /*is_inertial=*/false,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
          /*delta=*/10.0f, prev_frame.first_input_ts, base::TimeTicks(),
          &tick_clock, /*trace_id=*/std::nullopt);

      scroll_jank_dropped_frame_tracker_->ReportLatestPresentationData(
          *event.get(), prev_frame.last_input_ts, prev_frame.presentation_ts,
          kVsyncInterval);
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
        *event.get(), frame.last_input_ts, frame.presentation_ts,
        kVsyncInterval);
  }

  std::unique_ptr<base::HistogramTester> histogram_tester;

 protected:
  std::unique_ptr<ScrollJankDroppedFrameTracker>
      scroll_jank_dropped_frame_tracker_;
};

TEST_F(ScrollJankDroppedFrameTrackerTest, EmitsHistograms) {
  FrameTimestamps f1 = {103, 103, 148};

  FrameTimestamps last_frame =
      ProduceAndReportMockFrames(f1, kHistogramEmitFrequency);

  histogram_tester->ExpectUniqueSample(kDelayedFramesWindowHistogram, 0, 0);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumInWindowHistogram, 0, 0);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsWindowHistogram, 0, 0);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumInVsyncWindowHistogram,
                                       0, 0);

  // For first window we emit histogram at 65th reported frame.
  last_frame = ProduceAndReportMockFrames(last_frame, 1);

  histogram_tester->ExpectUniqueSample(kDelayedFramesWindowHistogram, 0, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumInWindowHistogram, 0, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsWindowHistogram, 0, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumInVsyncWindowHistogram,
                                       0, 1);

  // For subsequent windows we emit histogram every 64 frames.
  ProduceAndReportMockFrames(last_frame, kHistogramEmitFrequency);

  histogram_tester->ExpectUniqueSample(kDelayedFramesWindowHistogram, 0, 2);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumInWindowHistogram, 0, 2);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsWindowHistogram, 0, 2);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumInVsyncWindowHistogram,
                                       0, 2);
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

  // To trigger per window histogram emission.
  int frames_to_emit_histogram = kFirstWindowSize - 2;
  ProduceAndReportMockFrames(f2, frames_to_emit_histogram);

  histogram_tester->ExpectUniqueSample(kDelayedFramesWindowHistogram, 0, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumInWindowHistogram, 0, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsWindowHistogram, 0, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumInVsyncWindowHistogram,
                                       0, 1);
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

  // To trigger per window histogram emission.
  int frames_to_emit_histogram = kFirstWindowSize - 2;
  ProduceAndReportMockFrames(f2, frames_to_emit_histogram);

  histogram_tester->ExpectUniqueSample(kDelayedFramesWindowHistogram, 0, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumInWindowHistogram, 0, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsWindowHistogram, 0, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumInVsyncWindowHistogram,
                                       0, 1);
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
  const std::vector<int> inputs = {103, 111, 119, 127, 135, 143};
  const std::vector<int> vsyncs = {148, 196, 228};

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

TEST_F(ScrollJankDroppedFrameTrackerTest, MissedVsyncsPerVsyncWindow) {
  const std::vector<int> inputs = {103, 111, 119, 127, 135, 143};
  const std::vector<int> vsyncs = {148, 196, 228};

  FrameTimestamps f1 = {inputs[0], inputs[1], vsyncs[0]};
  FrameTimestamps f2 = {inputs[2], inputs[3], vsyncs[1]};
  FrameTimestamps f3 = {inputs[4], inputs[5], vsyncs[2]};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);
  ReportLatestPresentationDataToTracker(f3);

  // To trigger per window histogram emission, subtracting 5
  // here because the window is calculated per vsync and 3 vsyncs
  // were missed
  int frames_to_emit_histogram = kFirstWindowSize - 5;
  FrameTimestamps last_frame_ts =
      ProduceAndReportMockFrames(f3, frames_to_emit_histogram);

  // F2 and F3 have 2 and 1 missed vsyncs respectively.
  const int expected_missed_vsyncs = 3;
  const int expected_delayed_frames_percentage =
      (100 * expected_missed_vsyncs) / kHistogramEmitFrequency;
  // Frame F2 missed 2 vsyncs, F3 missed 1 vsync.
  const int expected_sum = 3;
  const int expected_max = 2;
  histogram_tester->ExpectUniqueSample(kMissedVsyncsWindowHistogram,
                                       expected_delayed_frames_percentage, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumInVsyncWindowHistogram,
                                       expected_sum, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsMaxInVsyncWindowHistogram,
                                       expected_max, 1);

  // The counters were reset for next set of `kHistogramEmitFrequency` frames.
  ProduceAndReportMockFrames(last_frame_ts, kHistogramEmitFrequency);

  histogram_tester->ExpectBucketCount(kMissedVsyncsWindowHistogram, 0, 1);
  histogram_tester->ExpectBucketCount(kMissedVsyncsSumInVsyncWindowHistogram, 0,
                                      1);
  histogram_tester->ExpectBucketCount(kMissedVsyncsMaxInVsyncWindowHistogram, 0,
                                      1);
}

struct ScrollTestCase {
  std::string test_name;
  int num_frames;
  std::string suffix;
};

class PerScrollTests : public ScrollJankDroppedFrameTrackerTest,
                       public testing::WithParamInterface<ScrollTestCase> {};

/*
Test that bucketed histograms for scrolls are emitted.
vsync                   v0              v1        v2
                        |    |    |     |    |    |
input   I0  I1  I2  I3  I4  I5
        |   |   |   |   |   |
F1:     |---------------| {I0, I1}
F2:             |-----------------------| {I2, I3}
F3:                     |-------------------------| {I4, I5}
*/
TEST_P(PerScrollTests, MetricsEmittedPerScroll) {
  const ScrollTestCase& params = GetParam();

  const std::vector<int> inputs = {103, 111, 119, 127, 135, 143};
  const std::vector<int> vsyncs = {148, 196, 228};

  FrameTimestamps f1 = {inputs[0], inputs[1], vsyncs[0]};
  FrameTimestamps f2 = {inputs[2], inputs[3], vsyncs[1]};
  FrameTimestamps f3 = {inputs[4], inputs[5], vsyncs[2]};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);
  ReportLatestPresentationDataToTracker(f3);
  CHECK_GE(params.num_frames, 3);
  FrameTimestamps last_ts =
      ProduceAndReportMockFrames(f3, params.num_frames - 3);

  scroll_jank_dropped_frame_tracker_->OnScrollStarted();

  // F2 and F3 are janky frames.
  const int expected_missed_frames = 2;
  const int total_frames = params.num_frames;
  const int expected_delayed_frames_percentage =
      (100 * expected_missed_frames) / total_frames;
  // Frame F2 missed 2 vsyncs, F3 missed 1 vsync.
  const int expected_max = 2;
  const int expected_sum = 3;

  // Emits non-bucketed histograms.
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumPerScrollHistogram,
                                       expected_sum, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsMaxPerScrollHistogram,
                                       expected_max, 1);
  histogram_tester->ExpectUniqueSample(kDelayedFramesPerScrollHistogram,
                                       expected_delayed_frames_percentage, 1);

  // Emits bucketed histograms.
  histogram_tester->ExpectUniqueSample(
      base::StrCat({kMissedVsyncsSumPerScrollHistogram, params.suffix}),
      expected_sum, 1);
  histogram_tester->ExpectUniqueSample(
      base::StrCat({kMissedVsyncsMaxPerScrollHistogram, params.suffix}),
      expected_max, 1);
  histogram_tester->ExpectUniqueSample(
      base::StrCat({kDelayedFramesPerScrollHistogram, params.suffix}),
      expected_delayed_frames_percentage, 1);

  // Produce arbitrary no. of frames.
  ProduceAndReportMockFrames(last_ts, 10);
  // The metrics from last scroll should be emitted when destructor is called.
  delete scroll_jank_dropped_frame_tracker_.release();

  // The counters should have been reset and there wouldn't be any janky frames.
  histogram_tester->ExpectBucketCount(kMissedVsyncsSumPerScrollHistogram, 0, 1);
  histogram_tester->ExpectBucketCount(kMissedVsyncsMaxPerScrollHistogram, 0, 1);
  histogram_tester->ExpectBucketCount(kDelayedFramesPerScrollHistogram, 0, 1);
}

TEST_P(PerScrollTests, VsyncMetricsEmittedPerScroll) {
  const ScrollTestCase& params = GetParam();

  const std::vector<int> inputs = {103, 111, 119, 127, 135, 143};
  const std::vector<int> vsyncs = {148, 196, 228};

  FrameTimestamps f1 = {inputs[0], inputs[1], vsyncs[0]};
  FrameTimestamps f2 = {inputs[2], inputs[3], vsyncs[1]};
  FrameTimestamps f3 = {inputs[4], inputs[5], vsyncs[2]};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);
  ReportLatestPresentationDataToTracker(f3);
  CHECK_GE(params.num_frames, 3);
  FrameTimestamps last_ts =
      // - 6 as 3 presented frames + 3 missed vsyncs need to be subtracted
      ProduceAndReportMockFrames(f3, params.num_frames - 6);

  scroll_jank_dropped_frame_tracker_->OnScrollStarted();

  // Frame F2 missed 2 vsyncs, F3 missed 1 vsync.
  const int expected_max = 2;
  const int expected_sum = 3;

  // F2 and F3 are janky frames.
  const int expected_missed_vsyncs = 3;
  const int total_vsyncs = params.num_frames;
  const int expected_missed_vsyncs_percentage =
      (100 * expected_missed_vsyncs) / total_vsyncs;

  // Emits non-bucketed histograms.
  histogram_tester->ExpectUniqueSample(kMissedVsyncsSumPerScrollHistogram,
                                       expected_sum, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsMaxPerScrollHistogram,
                                       expected_max, 1);
  histogram_tester->ExpectUniqueSample(kMissedVsyncsPerScrollHistogram,
                                       expected_missed_vsyncs_percentage, 1);

  // Emits bucketed histograms.
  histogram_tester->ExpectUniqueSample(
      base::StrCat({kMissedVsyncsPerScrollHistogram, params.suffix}),
      expected_missed_vsyncs_percentage, 1);

  // Produce arbitrary no. of frames.
  ProduceAndReportMockFrames(last_ts, 10);
  // The metrics from last scroll should be emitted when destructor is called.
  delete scroll_jank_dropped_frame_tracker_.release();

  // The counters should have been reset and there wouldn't be any janky frames.
  histogram_tester->ExpectBucketCount(kMissedVsyncsSumPerScrollHistogram, 0, 1);
  histogram_tester->ExpectBucketCount(kMissedVsyncsMaxPerScrollHistogram, 0, 1);
  histogram_tester->ExpectBucketCount(kMissedVsyncsPerScrollHistogram, 0, 1);
}

INSTANTIATE_TEST_SUITE_P(
    PerScrollTests,
    PerScrollTests,
    testing::ValuesIn<ScrollTestCase>({
        {"EmitsSmallScrollHistogram", 10, ".Small"},
        {"EmitsMediumScrollHistogram", 50, ".Medium"},
        {"EmitsLargeScrollHistogram", 65, ".Large"},
    }),
    [](const testing::TestParamInfo<PerScrollTests::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace cc
