// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_processor.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "cc/metrics/event_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/452613902): Remove tests that are duplicated in unit tests for
// `ScrollJankV4Decider` and `ScrollJankV4HistogramEmitter`.

namespace cc {

namespace {

constexpr base::TimeDelta kVsyncInterval = base::Milliseconds(16);

constexpr base::TimeTicks MillisSinceEpoch(int64_t millis) {
  return base::TimeTicks() + base::Milliseconds(millis);
}

constexpr base::TimeTicks MicrosSinceEpoch(int64_t micros) {
  return base::TimeTicks() + base::Microseconds(micros);
}

struct FrameTimestamps {
  // Generation timestamp of the earliest input (`ScrollUpdateEventMetrics`) in
  // a frame, i.e. the value of:
  //
  // ```
  // earliest_event.GetDispatchStageTimestamp(
  //     EventMetrics::DispatchStage::kGenerated);
  // ```
  base::TimeTicks earliest_input_generation_ts;

  // Generation timestamp of the latest input (`ScrollUpdateEventMetrics`) in
  // a frame, i.e. the value of `latest_event.last_timestamp()`. If empty,
  // default to `earliest_input_generation_ts`.
  std::optional<base::TimeTicks> last_input_generation_ts;

  base::TimeTicks presentation_ts;

  bool has_inertial_input = false;
  float abs_total_raw_delta_pixels = 0.0f;
  float max_abs_inertial_raw_delta_pixels = 0.0f;
};

constexpr int kHistogramEmitFrequency =
    ScrollJankV4HistogramEmitter::kHistogramEmitFrequency;
constexpr int kFirstWindowSize = kHistogramEmitFrequency + 1;

}  // namespace

class ScrollJankV4ProcessorTest : public testing::Test {
 public:
  ScrollJankV4ProcessorTest() = default;

  void SetUp() override {
    ResetHistogramTester();
    scroll_jank_processor_ = std::make_unique<ScrollJankV4Processor>();
    scroll_jank_processor_->OnScrollStarted();
  }

  void ResetHistogramTester() {
    histogram_tester = std::make_unique<base::HistogramTester>();
  }

  FrameTimestamps ProduceAndReportMockFrames(FrameTimestamps prev_frame,
                                             int num_frames) {
    prev_frame.max_abs_inertial_raw_delta_pixels = 0.0f;
    prev_frame.abs_total_raw_delta_pixels = 0.0f;
    for (int i = 1; i <= num_frames; i++) {
      prev_frame.earliest_input_generation_ts += kVsyncInterval;
      if (prev_frame.last_input_generation_ts.has_value()) {
        *prev_frame.last_input_generation_ts += kVsyncInterval;
      }
      prev_frame.presentation_ts += kVsyncInterval;
      ReportLatestPresentationDataToTracker(prev_frame);
    }
    return prev_frame;
  }

  void ReportLatestPresentationDataToTracker(const FrameTimestamps& frame) {
    base::SimpleTestTickClock tick_clock;
    tick_clock.SetNowTicks(frame.earliest_input_generation_ts);
    auto earliest_event = ScrollUpdateEventMetrics::CreateForTesting(
        ui::EventType::kGestureScrollUpdate, ui::ScrollInputType::kWheel,
        /*is_inertial=*/frame.has_inertial_input,
        ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
        /*delta=*/frame.abs_total_raw_delta_pixels,
        frame.earliest_input_generation_ts, base::TimeTicks(), &tick_clock,
        /*trace_id=*/std::nullopt);
    base::TimeTicks last_input_generation_ts =
        frame.last_input_generation_ts.value_or(
            frame.earliest_input_generation_ts);
    scroll_jank_processor_->OnFramePresented(
        *earliest_event, last_input_generation_ts, frame.presentation_ts,
        kVsyncInterval, frame.has_inertial_input,
        frame.abs_total_raw_delta_pixels,
        frame.max_abs_inertial_raw_delta_pixels);
  }

  std::unique_ptr<base::HistogramTester> histogram_tester;

 protected:
  std::unique_ptr<ScrollJankV4Processor> scroll_jank_processor_;
};

TEST_F(ScrollJankV4ProcessorTest, EmitsHistograms) {
  FrameTimestamps f1 = {.earliest_input_generation_ts = MillisSinceEpoch(103),
                        .presentation_ts = MillisSinceEpoch(148)};

  FrameTimestamps last_frame =
      ProduceAndReportMockFrames(f1, kHistogramEmitFrequency);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 0);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 0);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 0);

  // For first window we emit histogram at 65th reported frame.
  last_frame = ProduceAndReportMockFrames(last_frame, 1);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 1);

  // For subsequent windows we emit histogram every 64 frames.
  ProduceAndReportMockFrames(last_frame, kHistogramEmitFrequency);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 2);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 2);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 2);
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
TEST_F(ScrollJankV4ProcessorTest, FrameProducedEveryVsync) {
  const std::vector<base::TimeTicks> inputs = {
      MillisSinceEpoch(103), MillisSinceEpoch(111), MillisSinceEpoch(119),
      MillisSinceEpoch(127)};
  const std::vector<base::TimeTicks> vsyncs = {MillisSinceEpoch(148),
                                               MillisSinceEpoch(164)};

  FrameTimestamps f1 = {.earliest_input_generation_ts = inputs[0],
                        .last_input_generation_ts = inputs[1],
                        .presentation_ts = vsyncs[0]};
  FrameTimestamps f2 = {.earliest_input_generation_ts = inputs[2],
                        .last_input_generation_ts = inputs[3],
                        .presentation_ts = vsyncs[1]};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);

  // To trigger per window histogram emission.
  int frames_to_emit_histogram = kFirstWindowSize - 2;
  ProduceAndReportMockFrames(f2, frames_to_emit_histogram);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 1);
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
TEST_F(ScrollJankV4ProcessorTest, NoFrameProducedForMissingInput) {
  const std::vector<base::TimeTicks> inputs = {
      MillisSinceEpoch(103), MillisSinceEpoch(111), MillisSinceEpoch(135),
      MillisSinceEpoch(143)};
  const std::vector<base::TimeTicks> vsyncs = {MillisSinceEpoch(148),
                                               MillisSinceEpoch(180)};

  FrameTimestamps f1 = {.earliest_input_generation_ts = inputs[0],
                        .last_input_generation_ts = inputs[1],
                        .presentation_ts = vsyncs[0]};
  FrameTimestamps f2 = {.earliest_input_generation_ts = inputs[2],
                        .last_input_generation_ts = inputs[3],
                        .presentation_ts = vsyncs[1]};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);

  // To trigger per window histogram emission.
  int frames_to_emit_histogram = kFirstWindowSize - 2;
  ProduceAndReportMockFrames(f2, frames_to_emit_histogram);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 1);
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
TEST_F(ScrollJankV4ProcessorTest, MissedVsyncWhenInputWasPresent) {
  const std::vector<base::TimeTicks> inputs = {
      MillisSinceEpoch(103), MillisSinceEpoch(111), MillisSinceEpoch(119),
      MillisSinceEpoch(127), MillisSinceEpoch(135), MillisSinceEpoch(143)};
  const std::vector<base::TimeTicks> vsyncs = {
      MillisSinceEpoch(148), MillisSinceEpoch(196), MillisSinceEpoch(228)};

  FrameTimestamps f1 = {.earliest_input_generation_ts = inputs[0],
                        .last_input_generation_ts = inputs[1],
                        .presentation_ts = vsyncs[0]};
  FrameTimestamps f2 = {.earliest_input_generation_ts = inputs[2],
                        .last_input_generation_ts = inputs[3],
                        .presentation_ts = vsyncs[1]};
  FrameTimestamps f3 = {.earliest_input_generation_ts = inputs[4],
                        .last_input_generation_ts = inputs[5],
                        .presentation_ts = vsyncs[2]};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);
  ReportLatestPresentationDataToTracker(f3);

  // To trigger per window histogram emission.
  int frames_to_emit_histogram = kFirstWindowSize - 3;
  FrameTimestamps last_frame_ts =
      ProduceAndReportMockFrames(f3, frames_to_emit_histogram);

  // F2 and F3 are janky frames.
  const int expected_missed_frames = 2;
  const int expected_delayed_frames_percentage =
      (100 * expected_missed_frames) / kHistogramEmitFrequency;
  // Frame F2 missed 2 vsyncs, F3 missed 1 vsync.
  const int expected_max = 2;
  const int expected_sum = 3;

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
      expected_delayed_frames_percentage, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", expected_sum, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", expected_max, 1);

  // The counters were reset for next set of `kHistogramEmitFrequency` frames.
  ProduceAndReportMockFrames(last_frame_ts, kHistogramEmitFrequency);

  histogram_tester->ExpectBucketCount(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
  histogram_tester->ExpectBucketCount(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 1);
  histogram_tester->ExpectBucketCount(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 1);
}

// Regression test for https://crbug.com/404637348.
TEST_F(ScrollJankV4ProcessorTest, ScrollWithZeroVsyncs) {
  const std::vector<base::TimeTicks> inputs = {
      MillisSinceEpoch(103), MillisSinceEpoch(111), MillisSinceEpoch(119),
      MillisSinceEpoch(127)};
  const std::vector<base::TimeTicks> presentations = {MillisSinceEpoch(148),
                                                      MillisSinceEpoch(149)};

  FrameTimestamps f1 = {.earliest_input_generation_ts = inputs[0],
                        .last_input_generation_ts = inputs[1],
                        .presentation_ts = presentations[0]};
  ReportLatestPresentationDataToTracker(f1);
  scroll_jank_processor_->OnScrollStarted();

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0, 1);

  // A malformed frame whose presentation timestamp is less than half a vsync
  // greater than than the previous frame's presentation timestamp.
  FrameTimestamps f2 = {.earliest_input_generation_ts = inputs[2],
                        .last_input_generation_ts = inputs[3],
                        .presentation_ts = presentations[1]};
  ReportLatestPresentationDataToTracker(f2);
  scroll_jank_processor_->OnScrollStarted();

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0, 2);
}

/*
Tests for the scroll jank metric's per-scroll histograms. To avoid
duplication, all per-scroll tests use the same scenario depicted below.
vsync                   v0              v1        v2
                        |    |    |     |    |    |
input   I0  I1  I2  I3  I4  I5
        |   |   |   |   |    |
F1:     |---------------| {I0, I1}
F2:             |-----------------------| {I2, I3}
F3:                     |-------------------------| {I4, I5}
*/
class ScrollJankV4ProcessorPerScrollTest : public ScrollJankV4ProcessorTest {
 public:
  ScrollJankV4ProcessorPerScrollTest() = default;

  void ProduceAndReportScrollFrames() {
    const std::vector<base::TimeTicks> inputs = {
        MillisSinceEpoch(103), MillisSinceEpoch(111), MillisSinceEpoch(119),
        MillisSinceEpoch(127), MillisSinceEpoch(135), MillisSinceEpoch(143)};
    const std::vector<base::TimeTicks> vsyncs = {
        MillisSinceEpoch(148), MillisSinceEpoch(196), MillisSinceEpoch(228)};

    FrameTimestamps f1 = {.earliest_input_generation_ts = inputs[0],
                          .last_input_generation_ts = inputs[1],
                          .presentation_ts = vsyncs[0]};
    FrameTimestamps f2 = {.earliest_input_generation_ts = inputs[2],
                          .last_input_generation_ts = inputs[3],
                          .presentation_ts = vsyncs[1]};
    FrameTimestamps f3 = {.earliest_input_generation_ts = inputs[4],
                          .last_input_generation_ts = inputs[5],
                          .presentation_ts = vsyncs[2]};

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
Tests that the scroll jank metric's histograms for a scroll are emitted at
the beginning of the next scroll when the
`EmitPerScrollJankV4MetricAtEndOfScroll` feature is disabled.
*/
TEST_F(ScrollJankV4ProcessorPerScrollTest,
       ShouldEmitMetricsAtStartOfNextScrollWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kEmitPerScrollJankV4MetricAtEndOfScroll);

  ProduceAndReportScrollFrames();

  // The tracker SHOULDN'T emit any metrics at the end of the scroll.
  scroll_jank_processor_->OnScrollEnded();

  histogram_tester->ExpectTotalCount(
      "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);

  // The tracker should emit all metrics at the beginning of the next scroll.
  ResetHistogramTester();
  scroll_jank_processor_->OnScrollStarted();

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
      expected_delayed_frames_percentage, 1);

  // The tracker SHOULDN'T emit any more metrics when it's destroyed.
  ResetHistogramTester();
  delete scroll_jank_processor_.release();

  histogram_tester->ExpectTotalCount(
      "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
}

/*
Tests that the scroll jank metric's histograms for a scroll are emitted at the
end of the scroll when the `EmitPerScrollJankV4MetricAtEndOfScroll` feature is
enabled.
*/
TEST_F(ScrollJankV4ProcessorPerScrollTest,
       ShouldEmitMetricsAtEndOfScrollWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kEmitPerScrollJankV4MetricAtEndOfScroll);

  ProduceAndReportScrollFrames();

  // The tracker should emit all metrics at the end of the scroll.
  scroll_jank_processor_->OnScrollEnded();

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
      expected_delayed_frames_percentage, 1);

  // The tracker SHOULDN'T emit any more metrics at the beginning of the next
  // scroll or when it's destroyed.
  ResetHistogramTester();
  scroll_jank_processor_->OnScrollStarted();
  delete scroll_jank_processor_.release();

  histogram_tester->ExpectTotalCount(
      "Event.ScrollJank.DelayedFramesPercentage4.PerScroll", 0);
}

/*
Tests that the scroll jank metric's histograms for a scroll are
emitted when the tracker is destroyed.
*/
TEST_F(ScrollJankV4ProcessorPerScrollTest, ShouldEmitMetricsWhenDestroyed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kEmitPerScrollJankV4MetricAtEndOfScroll);

  ProduceAndReportScrollFrames();

  // The tracker should emit all metrics when it's destroyed.
  delete scroll_jank_processor_.release();

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.PerScroll",
      expected_delayed_frames_percentage, 1);
}

/*
Tests that the scroll jank metric evaluates each scroll separately (i.e.
doesn't evaluate a scroll against a previous scroll).

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

The metric should NOT evaluate I2/F2 against I1/F1 (because they happened in
different scrolls), so the metric should NOT mark F2 as janky.
*/
TEST_F(ScrollJankV4ProcessorTest, MetricEvaluatesEachScrollSeparately) {
  // Scroll 1: First input took only 8 ms (half a VSync) to deliver.
  FrameTimestamps f1 = {.earliest_input_generation_ts = MillisSinceEpoch(108),
                        .presentation_ts = MillisSinceEpoch(116),
                        .abs_total_raw_delta_pixels = 4.0f};
  ReportLatestPresentationDataToTracker(f1);

  scroll_jank_processor_->OnScrollStarted();
  ResetHistogramTester();

  // Scroll 2: Inputs 2-65 took 40 ms (2.5 VSyncs) to deliver.
  FrameTimestamps f2 = {.earliest_input_generation_ts = MillisSinceEpoch(124),
                        .presentation_ts = MillisSinceEpoch(164)};
  ReportLatestPresentationDataToTracker(f2);
  FrameTimestamps f65 =
      ProduceAndReportMockFrames(f2, kFirstWindowSize - 2 /* f1, f2 */);
  ReportLatestPresentationDataToTracker(f65);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 1);
}

/*
Tests that the scroll jank metric doesn't unfairly mark a frame as janky just
because Chrome "got lucky" (quickly presented an input in a frame) once many
frames ago.

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

The metric should NOT evaluate I65/F65 against I1/F1 (because it happened a
long time ago), so the metric should NOT mark F65 as janky.
*/
TEST_F(ScrollJankV4ProcessorTest, MissedVsyncLongAfterQuickInputFrameDelivery) {
  // First input took only 8 ms (half a VSync) to deliver.
  FrameTimestamps f1 = {.earliest_input_generation_ts = MillisSinceEpoch(108),
                        .presentation_ts = MillisSinceEpoch(116)};
  ReportLatestPresentationDataToTracker(f1);

  // Inputs 2-64 took 16 ms (one VSync) to deliver.
  FrameTimestamps f2 = {.earliest_input_generation_ts = MillisSinceEpoch(116),
                        .presentation_ts = MillisSinceEpoch(132)};
  ReportLatestPresentationDataToTracker(f2);
  FrameTimestamps f64 =
      ProduceAndReportMockFrames(f2, kFirstWindowSize - 3 /* f1, f2 & f65 */);
  ASSERT_EQ(f64.earliest_input_generation_ts, MillisSinceEpoch(1108));
  ASSERT_EQ(f64.presentation_ts, MillisSinceEpoch(1124));

  // There's one VSync missed between F64 and F65. F65 should be evaluated
  // against the delivery cutoffs of the recent frames (16 ms) rather than the
  // first frame (8 ms). Therefore, it's not reasonable to assume that F65's
  // first input (generated at 1132 ms) could have been included in the missed
  // VSync (presented at 1140 ms), so F65 should NOT be marked as janky.
  FrameTimestamps f65 = {.earliest_input_generation_ts = MillisSinceEpoch(1132),
                         .presentation_ts = MillisSinceEpoch(1156)};
  ReportLatestPresentationDataToTracker(f65);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 1);
}

/*
Tests that the scroll jank metric marks a frame as janky if it was delayed
compared to the immediately preceding frame (in which Chrome quickly presented
an input in a frame).

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

The metric SHOULD evaluate I65/F65 against I64/F64 (because it just
happened), so the metric SHOULD mark F65 as janky.
*/
TEST_F(ScrollJankV4ProcessorTest,
       MissedVsyncImmediatelyAfterQuickInputFrameDelivery) {
  // Inputs 1-63 took 16 ms (one VSync) to deliver.
  FrameTimestamps f1 = {.earliest_input_generation_ts = MillisSinceEpoch(100),
                        .presentation_ts = MillisSinceEpoch(116)};
  ReportLatestPresentationDataToTracker(f1);
  FrameTimestamps f63 =
      ProduceAndReportMockFrames(f1, kFirstWindowSize - 3 /* f1, f64 & f65 */);
  ASSERT_EQ(f63.earliest_input_generation_ts, MillisSinceEpoch(1092));
  ASSERT_EQ(f63.presentation_ts, MillisSinceEpoch(1108));

  // Inputs 64 took only 8 ms (half a VSync) to deliver.
  FrameTimestamps f64 = {.earliest_input_generation_ts = MillisSinceEpoch(1116),
                         .presentation_ts = MillisSinceEpoch(1124)};
  ReportLatestPresentationDataToTracker(f64);

  // There's one VSync missed between F64 and F65. F65 should be evaluated
  // against the delivery cutoffs of the most recent frame (8 ms) rather than
  // the earlier frames (16 ms). Therefore, it's reasonable to assume that F65's
  // first input (generated at 1132 ms) could have been included in the missed
  // VSync (presented at 1140 ms), so F65 SHOULD be marked as janky.
  FrameTimestamps f65 = {.earliest_input_generation_ts = MillisSinceEpoch(1132),
                         .presentation_ts = MillisSinceEpoch(1156)};
  ReportLatestPresentationDataToTracker(f65);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
      (1 * 100) / kHistogramEmitFrequency, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      (1 * 100) / kHistogramEmitFrequency, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 1, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 1, 1);

  ResetHistogramTester();
  ProduceAndReportMockFrames(f65, kHistogramEmitFrequency);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 1);
}

/*
Tests that the scroll jank metric marks frames which missed one or more
VSyncs in the middle of a fast scroll as janky (even with sparse inputs).

VSync V V V V V V V V V V V V V V V V V V V V V V V V V V
      : : : : : : : : : : : : : : : : :   : :           :
Input I1I2  I3I4          I5        : :   : :           :
      : :   : :           :         : :   : :           :
F1:   |-----:-:-----------:---------| :   : :           :
F2:     |---:-:-----------:-----------|(A): :           :
F3:         |-:-----------:---------------| :           :
F4:           |-----------:-----------------|    (B)    :
F5:                       |-----------------------------|

Assuming I1-I5 are all above the fast scroll threshold (each have at least 3px
absolute scroll delta), the metric should mark F3 and F5 janky with 1 (A)
and 5 (B) missed VSyncs respectively.
*/
TEST_F(ScrollJankV4ProcessorTest, MissedVsyncDuringFastScroll) {
  FrameTimestamps f1 = {.earliest_input_generation_ts = MillisSinceEpoch(100),
                        .presentation_ts = MillisSinceEpoch(340),
                        .abs_total_raw_delta_pixels = 4.0f};
  FrameTimestamps f2 = {.earliest_input_generation_ts = MillisSinceEpoch(116),
                        .presentation_ts = MillisSinceEpoch(356),
                        .abs_total_raw_delta_pixels = 4.0f};
  // 1 VSync missed between F2 and F3, so F3 should be marked as JANKY.
  FrameTimestamps f3 = {.earliest_input_generation_ts = MillisSinceEpoch(148),
                        .presentation_ts = MillisSinceEpoch(388),
                        .abs_total_raw_delta_pixels = 4.0f};
  FrameTimestamps f4 = {.earliest_input_generation_ts = MillisSinceEpoch(164),
                        .presentation_ts = MillisSinceEpoch(404),
                        .abs_total_raw_delta_pixels = 4.0f};
  // 5 VSyncs missed between F4 and F5, so F5 should be marked as JANKY.
  FrameTimestamps f5 = {.earliest_input_generation_ts = MillisSinceEpoch(260),
                        .presentation_ts = MillisSinceEpoch(500),
                        .abs_total_raw_delta_pixels = 4.0f};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);
  ReportLatestPresentationDataToTracker(f3);
  ReportLatestPresentationDataToTracker(f4);
  ReportLatestPresentationDataToTracker(f5);

  FrameTimestamps last_frame =
      ProduceAndReportMockFrames(f5, kFirstWindowSize - 5);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
      (2 * 100) / kHistogramEmitFrequency, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      (2 * 100) / kHistogramEmitFrequency, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 6, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 5, 1);

  ResetHistogramTester();
  ProduceAndReportMockFrames(last_frame, kHistogramEmitFrequency);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 1);
}

/*
Tests that the scroll jank metric does NOT mark frames which missed one or
more VSyncs as janky if inputs were sparse and the frames weren't in the middle
of a fast scroll.

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
scroll delta), the metric should NOT mark F3 as janky even though it missed
1 VSync (A). Similarly, if I4 or I5 are below the fast scroll threshold (has
less than 3px absolute scroll delta), the metric should NOT mark F5 as janky
even though it missed 5 VSyncs (B).
*/
TEST_F(ScrollJankV4ProcessorTest, MissedVsyncOutsideFastScroll) {
  FrameTimestamps f1 = {.earliest_input_generation_ts = MillisSinceEpoch(100),
                        .presentation_ts = MillisSinceEpoch(340),
                        .abs_total_raw_delta_pixels = 4.0f};
  FrameTimestamps f2 = {.earliest_input_generation_ts = MillisSinceEpoch(116),
                        .presentation_ts = MillisSinceEpoch(356),
                        .abs_total_raw_delta_pixels = 4.0f};
  // 1 VSync missed between F2 and F3, BUT F3 has scroll delta below the fast
  // scroll threshold, so F3 should NOT be marked as janky.
  FrameTimestamps f3 = {.earliest_input_generation_ts = MillisSinceEpoch(148),
                        .presentation_ts = MillisSinceEpoch(388),
                        .abs_total_raw_delta_pixels = 2.0f};
  FrameTimestamps f4 = {.earliest_input_generation_ts = MillisSinceEpoch(164),
                        .presentation_ts = MillisSinceEpoch(404),
                        .abs_total_raw_delta_pixels = 2.0f};
  // 5 VSyncs missed between F4 and F5, BUT F4 has scroll delta below the fast
  // scroll threshold, so F5 should NOT be marked as janky.
  FrameTimestamps f5 = {.earliest_input_generation_ts = MillisSinceEpoch(260),
                        .presentation_ts = MillisSinceEpoch(500),
                        .abs_total_raw_delta_pixels = 4.0f};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);
  ReportLatestPresentationDataToTracker(f3);
  ReportLatestPresentationDataToTracker(f4);
  ReportLatestPresentationDataToTracker(f5);

  ProduceAndReportMockFrames(f5, kFirstWindowSize - 5);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 1);
}

/*
Tests that the scroll jank metric marks frames which missed one or more
VSyncs at the transition from a fast regular scroll to a fast fling as janky.

VSync V  V  V  V  V  V  V  V  V  V
      :  :  :  :  :  :  :  :  :  :
Input I1          I2 :           :
      :           :  :           :
F1:   |-----------:--|    (A)    :
F2:               |--------------|

I1 and I2 are regular and inertial scroll updates respectively. Assuming I1 is
above the fast scroll threshold (has at least 3 px absolute scroll delta) and I2
is above the fling threshold (has at least 0.2 px absolute scroll delta), the
metric should mark F2 as janky with 3 missed VSyncs (A).
*/
TEST_F(ScrollJankV4ProcessorTest,
       MissedVsyncAtTransitionFromFastRegularScrollToFastFling) {
  FrameTimestamps f1 = {.earliest_input_generation_ts = MillisSinceEpoch(100),
                        .presentation_ts = MillisSinceEpoch(180),
                        .has_inertial_input = false,
                        .abs_total_raw_delta_pixels = 4.0f,
                        .max_abs_inertial_raw_delta_pixels = 0.0f};
  // 3 VSync missed between F1 and F2, so F2 should be marked as JANKY.
  FrameTimestamps f2 = {.earliest_input_generation_ts = MillisSinceEpoch(164),
                        .presentation_ts = MillisSinceEpoch(244),
                        .has_inertial_input = true,
                        .abs_total_raw_delta_pixels = 0.5f,
                        .max_abs_inertial_raw_delta_pixels = 0.5f};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);

  FrameTimestamps last_frame =
      ProduceAndReportMockFrames(f2, kFirstWindowSize - 2);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
      (1 * 100) / kHistogramEmitFrequency, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      (1 * 100) / kHistogramEmitFrequency, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 3, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 3, 1);

  ResetHistogramTester();
  ProduceAndReportMockFrames(last_frame, kHistogramEmitFrequency);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 1);
}

/*
Tests that the scroll jank metric does NOT mark frames which missed one or
more VSyncs at the transition from a slow regular scroll to a fling as janky.

VSync V  V  V  V  V  V  V  V  V  V
      :  :  :  :  :  :  :  :  :  :
Input I1          I2 :           :
      :           :  :           :
F1:   |-----------:--|    (A)    :
F2:               |--------------|

I1 and I2 are regular and inertial scroll updates respectively. Assuming I1 is
below the fast scroll threshold (has less than 3 px absolute scroll delta), the
metric should NOT mark F2 as janky even though it missed 3 VSyncs (A).
*/
TEST_F(ScrollJankV4ProcessorTest,
       MissedVsyncAtTransitionFromSlowRegularScrollToFling) {
  FrameTimestamps f1 = {.earliest_input_generation_ts = MillisSinceEpoch(100),
                        .presentation_ts = MillisSinceEpoch(300),
                        .has_inertial_input = false,
                        .abs_total_raw_delta_pixels = 2.0f,
                        .max_abs_inertial_raw_delta_pixels = 0.0f};
  // 3 VSync missed between F1 and F2, BUT F1 has scroll delta below the fast
  // scroll threshold, so F2 should NOT be marked as janky.
  FrameTimestamps f2 = {.earliest_input_generation_ts = MillisSinceEpoch(164),
                        .presentation_ts = MillisSinceEpoch(364),
                        .has_inertial_input = true,
                        .abs_total_raw_delta_pixels = 0.5f,
                        .max_abs_inertial_raw_delta_pixels = 0.5f};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);

  ProduceAndReportMockFrames(f2, kFirstWindowSize - 2);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 1);
}

/*
Tests that the scroll jank metric marks frames which missed one or more
VSyncs at the transition from a regular scroll to a slow fling as janky.

VSync V  V  V  V  V  V  V  V  V  V
      :  :  :  :  :  :  :  :  :  :
Input I1          I2 :           :
      :           :  :           :
F1:   |-----------:--|    (A)    :
F2:               |--------------|

I1 and I2 are regular and inertial scroll updates respectively. Assuming I2 is
below the fling threshold (has less than 0.2 px absolute scroll delta), the
metric should NOT mark F2 as janky even though it missed 3 VSyncs (A).
*/
TEST_F(ScrollJankV4ProcessorTest,
       MissedVsyncAtTransitionFromRegularScrollToSlowFling) {
  FrameTimestamps f1 = {.earliest_input_generation_ts = MillisSinceEpoch(100),
                        .presentation_ts = MillisSinceEpoch(300),
                        .has_inertial_input = false,
                        .abs_total_raw_delta_pixels = 4.0f,
                        .max_abs_inertial_raw_delta_pixels = 0.0f};
  // 3 VSync missed between F1 and F2, BUT F2 has scroll delta below the fling
  // threshold, so F2 should NOT be marked as janky.
  FrameTimestamps f2 = {.earliest_input_generation_ts = MillisSinceEpoch(164),
                        .presentation_ts = MillisSinceEpoch(364),
                        .has_inertial_input = true,
                        .abs_total_raw_delta_pixels = 0.1f,
                        .max_abs_inertial_raw_delta_pixels = 0.1f};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);

  ProduceAndReportMockFrames(f2, kFirstWindowSize - 2);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 1);
}

/*
Tests that the scroll jank metric does NOT mark frames which didn't miss any
VSyncs at the transition from a regular scroll to a fling as janky.

VSync V  V  V  V  V  V  V
      :  :  :  :  :  :  :
Input I1 I2          :  :
      :  :           :  :
F1:   |--:-----------|  :
F2:      |--------------|

I1 and I2 are regular and inertial scroll updates respectively. The metric
should NOT mark F2 as janky because it didn't miss any VSyncs.
*/
TEST_F(ScrollJankV4ProcessorTest,
       NoMissedVsyncAtTransitionFromRegularScrollToFling) {
  FrameTimestamps f1 = {.earliest_input_generation_ts = MillisSinceEpoch(100),
                        .presentation_ts = MillisSinceEpoch(180),
                        .has_inertial_input = false,
                        .abs_total_raw_delta_pixels = 4.0f,
                        .max_abs_inertial_raw_delta_pixels = 0.0f};
  FrameTimestamps f2 = {.earliest_input_generation_ts = MillisSinceEpoch(116),
                        .presentation_ts = MillisSinceEpoch(196),
                        .has_inertial_input = true,
                        .abs_total_raw_delta_pixels = 0.5f,
                        .max_abs_inertial_raw_delta_pixels = 0.5f};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);

  ProduceAndReportMockFrames(f2, kFirstWindowSize - 2);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 1);
}

/*
Tests that the scroll jank metric marks frames which missed one or more
VSyncs in the middle of a fast fling as janky.

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
threshold (both have at least 0.2px absolute scroll delta), the metric should
mark F3 and F5 janky with 1 (A) and 5 (B) missed VSyncs respectively.
*/
TEST_F(ScrollJankV4ProcessorTest, MissedVsyncDuringFastFling) {
  FrameTimestamps f1 = {.earliest_input_generation_ts = MillisSinceEpoch(100),
                        .presentation_ts = MillisSinceEpoch(340),
                        .has_inertial_input = true,
                        .abs_total_raw_delta_pixels = 0.5f,
                        .max_abs_inertial_raw_delta_pixels = 0.5f};
  FrameTimestamps f2 = {.earliest_input_generation_ts = MillisSinceEpoch(116),
                        .presentation_ts = MillisSinceEpoch(356),
                        .has_inertial_input = true,
                        .abs_total_raw_delta_pixels = 0.5f,
                        .max_abs_inertial_raw_delta_pixels = 0.5f};
  // 1 VSync missed between F2 and F3, so F3 should be marked as JANKY.
  FrameTimestamps f3 = {.earliest_input_generation_ts = MillisSinceEpoch(148),
                        .presentation_ts = MillisSinceEpoch(388),
                        .has_inertial_input = true,
                        .abs_total_raw_delta_pixels = 0.5f,
                        .max_abs_inertial_raw_delta_pixels = 0.5f};
  FrameTimestamps f4 = {.earliest_input_generation_ts = MillisSinceEpoch(164),
                        .presentation_ts = MillisSinceEpoch(404),
                        .has_inertial_input = true,
                        .abs_total_raw_delta_pixels = 0.5f,
                        .max_abs_inertial_raw_delta_pixels = 0.1f};
  // 5 VSyncs missed between F4 and F5 (EVEN THOUGH F4 has scroll delta below
  // the fling threshold), so F5 should be marked as JANKY.
  FrameTimestamps f5 = {.earliest_input_generation_ts = MillisSinceEpoch(260),
                        .presentation_ts = MillisSinceEpoch(500),
                        .has_inertial_input = true,
                        .abs_total_raw_delta_pixels = 0.5f,
                        .max_abs_inertial_raw_delta_pixels = 0.5f};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);
  ReportLatestPresentationDataToTracker(f3);
  ReportLatestPresentationDataToTracker(f4);
  ReportLatestPresentationDataToTracker(f5);

  FrameTimestamps last_frame =
      ProduceAndReportMockFrames(f5, kFirstWindowSize - 5);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
      (2 * 100) / kHistogramEmitFrequency, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      (2 * 100) / kHistogramEmitFrequency, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 6, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 5, 1);

  ResetHistogramTester();
  ProduceAndReportMockFrames(last_frame, kHistogramEmitFrequency);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 1);
}

/*
Tests that the scroll jank metric does NOT mark frames which missed one or
more VSyncs in the middle of a slow fling (typically towards the end of a fling)
as janky.

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
less than 0.2px absolute scroll delta), the metric should NOT mark F3 as
janky even though it missed one VSync (A). Similarly, if I5 is below the fling
threshold (has less than 0.2px absolute scroll delta), the metric should NOT
mark F5 as janky even though it missed 5 VSyncs (B).
*/
TEST_F(ScrollJankV4ProcessorTest, MissedVsyncDuringSlowFling) {
  FrameTimestamps f1 = {.earliest_input_generation_ts = MillisSinceEpoch(100),
                        .presentation_ts = MillisSinceEpoch(300),
                        .has_inertial_input = true,
                        .abs_total_raw_delta_pixels = 0.5f,
                        .max_abs_inertial_raw_delta_pixels = 0.5f};
  FrameTimestamps f2 = {.earliest_input_generation_ts = MillisSinceEpoch(116),
                        .presentation_ts = MillisSinceEpoch(316),
                        .has_inertial_input = true,
                        .abs_total_raw_delta_pixels = 0.5f,
                        .max_abs_inertial_raw_delta_pixels = 0.5f};
  // 1 VSync missed between F2 and F3, BUT F3 has scroll delta below the fling
  // threshold, so F3 should NOT be marked as janky.
  FrameTimestamps f3 = {.earliest_input_generation_ts = MillisSinceEpoch(148),
                        .presentation_ts = MillisSinceEpoch(348),
                        .has_inertial_input = true,
                        .abs_total_raw_delta_pixels = 0.1f,
                        .max_abs_inertial_raw_delta_pixels = 0.1f};
  FrameTimestamps f4 = {.earliest_input_generation_ts = MillisSinceEpoch(164),
                        .presentation_ts = MillisSinceEpoch(364),
                        .has_inertial_input = true,
                        .abs_total_raw_delta_pixels = 0.1f,
                        .max_abs_inertial_raw_delta_pixels = 0.1f};
  // 5 VSyncs missed between F4 and F5, BUT F5 has scroll delta below the fling
  // threshold, so F5 should NOT be marked as janky.
  FrameTimestamps f5 = {.earliest_input_generation_ts = MillisSinceEpoch(260),
                        .presentation_ts = MillisSinceEpoch(460),
                        .has_inertial_input = true,
                        .abs_total_raw_delta_pixels = 0.1f,
                        .max_abs_inertial_raw_delta_pixels = 0.1f};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);
  ReportLatestPresentationDataToTracker(f3);
  ReportLatestPresentationDataToTracker(f4);
  ReportLatestPresentationDataToTracker(f5);

  ProduceAndReportMockFrames(f5, kFirstWindowSize - 5);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 1);
}

struct RunningConsistencyTestCase {
  std::string test_name;
  base::TimeTicks input_ts;
  int expected_delayed_frames;
  int expected_missed_vsyncs;
};

class ScrollJankV4ProcessorRunningConsistentyTests
    : public ScrollJankV4ProcessorTest,
      public testing::WithParamInterface<RunningConsistencyTestCase> {};

/*
A parameterized test which verifies that the scroll jank metric correctly
calculates the number of missed VSyncs (taking into account the discount factor
and stability correction).

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
timestamp directly influences whether the metric metric will mark F4 as janky
and, if so, with how many missed VSyncs. Intuitively, the later I7 arrives, the
less opportunity Chrome will have to present it in F4, so Chrome will have
missed fewer VSyncs.

We can see that delivery cut-off for each of F1-F3 (the duration between the
generation timestamp of the last input included in a frame and the frame's
presentation timestamp) is roughly 3.5 VSyncs. This implies approximately the
following (without taking the discount factor, stability correction and exact
timestamps into account):

  * If I7 was generated later than 4.5 VSyncs before F4 was presented (M=0),
    then the metric should mark it as non-janky.
  * If I7 was generated between 5.5 (exclusive) and 4.5 (inclusive) VSyncs
    before F4 was presented (M=1), then the scroll metric should mark it as
    janky with 1 missed VSync.
  * If I7 was generated between 6.5 (exclusive) and 5.5 (inclusive) VSyncs
    before F4 was presented (M=2), then the scroll metric should mark it as
    janky with 2 missed VSyncs.
  * If I7 was generated 6.5 VSyncs before F4 was presented or earlier (M=3),
    then the scroll metric should mark it as janky with 3 missed VSyncs.
*/
TEST_P(ScrollJankV4ProcessorRunningConsistentyTests,
       MissedVsyncDueToDeceleratingInputFrameDelivery) {
  const RunningConsistencyTestCase& params = GetParam();

  // F1: 164 - 108.1 = 55.9 ms delivery cutoff.
  FrameTimestamps f1 = {.earliest_input_generation_ts = MillisSinceEpoch(100),
                        .last_input_generation_ts = MicrosSinceEpoch(108100),
                        .presentation_ts = MillisSinceEpoch(164)};
  // F2: 180 - 124 = 56 ms delivery cutoff.
  FrameTimestamps f2 = {.earliest_input_generation_ts = MillisSinceEpoch(116),
                        .last_input_generation_ts = MillisSinceEpoch(124),
                        .presentation_ts = MillisSinceEpoch(180)};
  // F3: 196 - 139.8 = 56.2 ms delivery cutoff
  FrameTimestamps f3 = {.earliest_input_generation_ts = MillisSinceEpoch(132),
                        .last_input_generation_ts = MicrosSinceEpoch(139800),
                        .presentation_ts = MillisSinceEpoch(196)};
  // 3 VSyncs missed between F3 and F4. Whether the first input in F4 could have
  // been presented one or more VSyncs earlier is determined by:
  //
  //     floor((
  //       `f4.presentation_ts`
  //         + (`kDiscountFactor` + `kStabilityCorrection`) * `kVsyncInterval`
  //         - min(
  //             `f1.presentation_ts` - `f1.last_input_generation_ts`
  //               + 6 * `kDiscountFactor` * `kVsyncInterval`,
  //             `f2.presentation_ts` - `f2.last_input_generation_ts`
  //               + 5 * `kDiscountFactor` * `kVsyncInterval`,
  //             `f3.presentation_ts` - `f3.last_input_generation_ts`
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
  FrameTimestamps f4 = {.earliest_input_generation_ts = params.input_ts,
                        .presentation_ts = MillisSinceEpoch(260)};

  ReportLatestPresentationDataToTracker(f1);
  ReportLatestPresentationDataToTracker(f2);
  ReportLatestPresentationDataToTracker(f3);
  ReportLatestPresentationDataToTracker(f4);

  FrameTimestamps last_frame =
      ProduceAndReportMockFrames(f4, kFirstWindowSize - 4);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow",
      (params.expected_delayed_frames * 100) / kHistogramEmitFrequency, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      (params.expected_delayed_frames * 100) / kHistogramEmitFrequency, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow",
      params.expected_missed_vsyncs, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow",
      params.expected_missed_vsyncs, 1);

  ResetHistogramTester();
  ProduceAndReportMockFrames(last_frame, kHistogramEmitFrequency);

  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDueToDeceleratingInputFrameDelivery",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling",
      0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow", 0, 1);
  histogram_tester->ExpectUniqueSample(
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow", 0, 1);
}

INSTANTIATE_TEST_SUITE_P(
    ScrollJankV4ProcessorRunningConsistentyTests,
    ScrollJankV4ProcessorRunningConsistentyTests,
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
    // `MissedVsyncDueToDeceleratingInputFrameDelivery` test case, we get the
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
    testing::ValuesIn<RunningConsistencyTestCase>({
        {.test_name = "MaxInputTimestampFor3MissedVsyncs",
         .input_ts = MicrosSinceEpoch(156640),
         .expected_delayed_frames = 1,
         .expected_missed_vsyncs = 3},
        {.test_name = "MinInputTimestampFor2MissedVsyncs",
         .input_ts = MicrosSinceEpoch(156641),
         .expected_delayed_frames = 1,
         .expected_missed_vsyncs = 2},
        {.test_name = "MaxInputTimestampFor2MissedVsyncs",
         .input_ts = MicrosSinceEpoch(172480),
         .expected_delayed_frames = 1,
         .expected_missed_vsyncs = 2},
        {.test_name = "MinInputTimestampFor1MissedVsync",
         .input_ts = MicrosSinceEpoch(172481),
         .expected_delayed_frames = 1,
         .expected_missed_vsyncs = 1},
        {.test_name = "MaxInputTimestampFor1MissedVsync",
         .input_ts = MicrosSinceEpoch(188320),
         .expected_delayed_frames = 1,
         .expected_missed_vsyncs = 1},
        {.test_name = "MinInputTimestampFor0MissedVsyncs",
         .input_ts = MicrosSinceEpoch(188321),
         .expected_delayed_frames = 0,
         .expected_missed_vsyncs = 0},
    }),
    [](const testing::TestParamInfo<
        ScrollJankV4ProcessorRunningConsistentyTests::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace cc
