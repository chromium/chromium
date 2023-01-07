// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/jank_metrics.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "cc/metrics/throughput_ukm_reporter.h"
#include "cc/trees/ukm_manager.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const base::TimeDelta kDefaultFrameInterval = base::Milliseconds(16.67);

// All sequence numbers for simulated frame events will start at this number.
// This makes it easier to numerically distinguish sequence numbers versus
// frame tokens, which always start at 1.
const uint32_t kSequenceNumberStartsAt = 100u;

const char* kAllSequencesMetricName = "Graphics.Smoothness.Jank.AllSequences";
const char* kAllAnimationsMetricName = "Graphics.Smoothness.Jank.AllAnimations";
const char* kAllInteractionsMetricName =
    "Graphics.Smoothness.Jank.AllInteractions";

}  // namespace

namespace cc {

class JankMetricsTest : public testing::Test {
 public:
  JankMetricsTest() = default;
  ~JankMetricsTest() override = default;

  // Simulate a series of Submit, NoUpdate, and Presentation events and notify
  // |jank_reporter|, as specified by |frame_sequences|. The exact presentation
  // time of frames can be slightly manipulated by |presentation_time_shifts|.
  void SimulateFrameSequence(
      JankMetrics* jank_reporter,
      const std::array<std::string, 3>& frame_sequences,
      const std::unordered_map<char, double>& presentation_time_shifts = {}) {
    // |frame_sequences| is an array of 3 strings of EQUAL LENGTH, representing
    // the (S)UBMIT, (N)O-UPDATE, (P)RESENTATION events, respectively. In all 3
    // strings:
    //   any char == a vsync interval (16.67ms) with a unique sequence number.
    //   '-' == no event in this vsync interval.
    // In SUBMIT string:
    //   [a-zA-Z] == A SUBMIT occurs at this vsync interval. Each symbol in this
    //   string must be unique.
    // In NO-UPDATE string:
    //   Any non '-' letter == A NO-UPDATE frame is reported at this vsync
    //   interval.
    // In PRESENTATION string:
    //   [a-zA-Z] == A PRESENTATION occurs at this vsync interval. Each
    //   symbol must be unique and MUST HAVE APPEARED in the SUBMIT string.
    //
    // NOTE this test file stylistically denotes the frames that should jank
    // with uppercases (although this is not a strict).
    //
    // Each item in |presentation_time_shifts| maps a presentation frame letter
    // (must have appeared in string P) to how much time (in ms) this
    // presentation deviates from expected. For example: {'a': -3.2} means frame
    // 'a' is presented 3.2ms before expected.
    //
    // e.g.
    // S = "a-b--c--D--"
    // N = "---**------"
    // P = "-a-b---c-D-"
    // presentation_time_shifts = {'c':-8.4, 'D':8.4}
    //
    // means submit at vsync 0, 2, 5, 8, presentation at 1, 3, 7, 9. Due to the
    // no-update frames 3 and 4, no janks will be reported for 'c'. However, the
    // large fluctuation of presentation time of 'c' and 'D', there is a jank
    // at 'D'.
    //
    // Without the no-update frames and presentation_time_shifts, one jank would
    // have been reported at 'c'.
    auto& submits = frame_sequences[0];
    auto& ignores = frame_sequences[1];
    auto& presnts = frame_sequences[2];

    // All three sequences must have the same size.
    EXPECT_EQ(submits.size(), ignores.size());
    EXPECT_EQ(submits.size(), presnts.size());

    // Map submitted frame to their tokens
    std::unordered_map<char, uint32_t> submit_to_token;

    base::TimeTicks start_time = base::TimeTicks::Now();

    // Scan S to collect all symbols
    for (uint32_t frame_token = 1, i = 0; i < submits.size(); ++i) {
      uint32_t sequence_number = kSequenceNumberStartsAt + i;
      if (submits[i] != '-') {
        submit_to_token[submits[i]] = frame_token;
        jank_reporter->AddSubmitFrame(/*frame_token=*/frame_token,
                                      /*sequence_number=*/sequence_number);
        frame_token++;
      }

      if (ignores[i] != '-') {
        jank_reporter->AddFrameWithNoUpdate(
            /*sequence_number=*/sequence_number,
            /*frame_interval=*/kDefaultFrameInterval);
      }

      if (presnts[i] != '-') {
        // The present frame must have been previously submitted
        EXPECT_EQ(submit_to_token.count(presnts[i]), 1u);

        double presentation_offset = 0.0;  // ms
        if (presentation_time_shifts.count(presnts[i]))
          presentation_offset = presentation_time_shifts.at(presnts[i]);

        jank_reporter->AddPresentedFrame(
            /*presented_frame_token=*/submit_to_token[presnts[i]],
            /*current_presentation_timestamp=*/start_time +
                i * kDefaultFrameInterval +
                base::Milliseconds(presentation_offset),
            /*frame_interval=*/kDefaultFrameInterval);
        submit_to_token.erase(presnts[i]);
      }
    }
  }
};

TEST_F(JankMetricsTest, CompositorAnimationOneJankWithMildFluctuation) {
  base::HistogramTester histogram_tester;
  FrameSequenceTrackerType tracker_type =
      FrameSequenceTrackerType::kCompositorAnimation;
  FrameInfo::SmoothEffectDrivingThread thread_type =
      FrameInfo::SmoothEffectDrivingThread::kCompositor;
  JankMetrics jank_reporter{tracker_type, thread_type};

  // One Jank; there are no no-update frames. The fluctuation in presentation of
  // 'd' is not big enough to cause another jank.
  SimulateFrameSequence(&jank_reporter,
                        {
                            /*submit   */ "ab-C-d",
                            /*noupdate */ "------",
                            /*present  */ "ab-C-d",
                        },
                        {{'d', +8.0 /*ms*/}});
  jank_reporter.ReportJankMetrics(100u);

  // One sample of 1 janks reported for "Compositor".
  const char* metric =
      "Graphics.Smoothness.Jank.Compositor.CompositorAnimation";
  const char* invalid_metric =
      "Graphics.Smoothness.Jank.Main.CompositorAnimation";

  histogram_tester.ExpectTotalCount(metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(metric),
              testing::ElementsAre(base::Bucket(1, 1)));

  // Test all-sequence metrics.
  histogram_tester.ExpectTotalCount(kAllSequencesMetricName, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(kAllSequencesMetricName),
              testing::ElementsAre(base::Bucket(1, 1)));

  histogram_tester.ExpectTotalCount(kAllAnimationsMetricName, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(kAllSequencesMetricName),
              testing::ElementsAre(base::Bucket(1, 1)));

  histogram_tester.ExpectTotalCount(kAllInteractionsMetricName, 0u);

  // Stale-frame metrics
  const char* stale_metric = "Graphics.Smoothness.Stale.CompositorAnimation";
  const char* maxstale_metric =
      "Graphics.Smoothness.MaxStale.CompositorAnimation";

  histogram_tester.ExpectTotalCount(stale_metric, 3u);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(stale_metric),
      testing::ElementsAre(base::Bucket(0, 1), base::Bucket(16, 1),
                           base::Bucket(24, 1) /*caused by +8ms fluctuation*/));

  histogram_tester.ExpectTotalCount(maxstale_metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(maxstale_metric),
              testing::ElementsAre(base::Bucket(24, 1)));

  // No reporting for "Main".
  histogram_tester.ExpectTotalCount(invalid_metric, 0u);
}

TEST_F(JankMetricsTest, MainThreadAnimationOneJankWithNoUpdate) {
  base::HistogramTester histogram_tester;
  FrameSequenceTrackerType tracker_type =
      FrameSequenceTrackerType::kMainThreadAnimation;
  FrameInfo::SmoothEffectDrivingThread thread_type =
      FrameInfo::SmoothEffectDrivingThread::kMain;
  JankMetrics jank_reporter{tracker_type, thread_type};

  // There are only 1 jank because of a no-update frame.

  SimulateFrameSequence(&jank_reporter, {
                                            /*submit   */ "ab-c--D",
                                            /*noupdate */ "--*----",
                                            /*present  */ "ab-c--D",
                                        });
  jank_reporter.ReportJankMetrics(100u);

  // One jank is reported for "Main".
  const char* metric = "Graphics.Smoothness.Jank.Main.MainThreadAnimation";
  const char* invalid_metric =
      "Graphics.Smoothness.Jank.Compositor.MainThreadAnimation";

  histogram_tester.ExpectTotalCount(metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(metric),
              testing::ElementsAre(base::Bucket(1, 1)));

  // No jank is reported for "Compositor"
  histogram_tester.ExpectTotalCount(invalid_metric, 0u);

  // Test all-sequence metrics.
  histogram_tester.ExpectTotalCount(kAllSequencesMetricName, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(kAllSequencesMetricName),
              testing::ElementsAre(base::Bucket(1, 1)));

  histogram_tester.ExpectTotalCount(kAllAnimationsMetricName, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(kAllSequencesMetricName),
              testing::ElementsAre(base::Bucket(1, 1)));

  histogram_tester.ExpectTotalCount(kAllInteractionsMetricName, 0u);

  // Stale-frame metrics
  const char* stale_metric = "Graphics.Smoothness.Stale.MainThreadAnimation";
  const char* maxstale_metric =
      "Graphics.Smoothness.MaxStale.MainThreadAnimation";

  histogram_tester.ExpectTotalCount(stale_metric, 3u);
  EXPECT_THAT(histogram_tester.GetAllSamples(stale_metric),
              testing::ElementsAre(base::Bucket(0, 2), base::Bucket(33, 1)));

  histogram_tester.ExpectTotalCount(maxstale_metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(maxstale_metric),
              testing::ElementsAre(base::Bucket(33, 1)));
}

TEST_F(JankMetricsTest, VideoManyJanksOver300ExpectedFrames) {
  base::HistogramTester histogram_tester;
  FrameSequenceTrackerType tracker_type = FrameSequenceTrackerType::kVideo;
  FrameInfo::SmoothEffectDrivingThread thread_type =
      FrameInfo::SmoothEffectDrivingThread::kCompositor;
  JankMetrics jank_reporter{tracker_type, thread_type};

  // 7 janks.
  SimulateFrameSequence(&jank_reporter,
                        {
                            /*submit   */ "ab-C--DeFGh-IJk---L---------",
                            /*noupdate */ "----------------------------",
                            /*present  */ "---ab-C--De-F--Gh-I---Jk---L",
                        });

  jank_reporter.ReportJankMetrics(300u);

  // Report in the 7/300 ~= 2% bucket for "Compositor"
  const char* metric = "Graphics.Smoothness.Jank.Compositor.Video";
  const char* invalid_metric = "Graphics.Smoothness.Jank.Main.Video";

  histogram_tester.ExpectTotalCount(metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(metric),
              testing::ElementsAre(base::Bucket(2, 1)));

  // No jank is reported for "Main"
  histogram_tester.ExpectTotalCount(invalid_metric, 0u);

  // Test all-sequence metrics. Videos are not counted into AllSequences.
  histogram_tester.ExpectTotalCount(kAllSequencesMetricName, 0u);
  histogram_tester.ExpectTotalCount(kAllAnimationsMetricName, 0u);
  histogram_tester.ExpectTotalCount(kAllInteractionsMetricName, 0u);

  // Stale-frame metrics
  const char* stale_metric = "Graphics.Smoothness.Stale.Video";
  const char* maxstale_metric = "Graphics.Smoothness.MaxStale.Video";

  histogram_tester.ExpectTotalCount(stale_metric, 11u);
  EXPECT_THAT(histogram_tester.GetAllSamples(stale_metric),
              testing::ElementsAre(base::Bucket(0, 4), base::Bucket(16, 3),
                                   base::Bucket(33, 2), base::Bucket(50, 2)));

  histogram_tester.ExpectTotalCount(maxstale_metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(maxstale_metric),
              testing::ElementsAre(base::Bucket(50, 1)));
}

TEST_F(JankMetricsTest, WheelScrollMainThreadNoJanksWithNoUpdates) {
  base::HistogramTester histogram_tester;
  FrameSequenceTrackerType tracker_type =
      FrameSequenceTrackerType::kWheelScroll;
  FrameInfo::SmoothEffectDrivingThread thread_type =
      FrameInfo::SmoothEffectDrivingThread::kMain;
  JankMetrics jank_reporter{tracker_type, thread_type};

  SimulateFrameSequence(&jank_reporter,
                        {
                            /*submit   */ "ab-c--d------e---------f-",
                            /*noupdate */ "--*-**-******-*********--",
                            /*present  */ "---ab-c-d-----e---------f",
                        });
  jank_reporter.ReportJankMetrics(100u);

  // Expect 2 janks for "Main" and no jank for "Compositor"
  const char* metric = "Graphics.Smoothness.Jank.Main.WheelScroll";
  const char* invalid_metric =
      "Graphics.Smoothness.Jank.Compositor.WheelScroll";

  histogram_tester.ExpectTotalCount(metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(metric),
              testing::ElementsAre(base::Bucket(0, 1)));

  histogram_tester.ExpectTotalCount(invalid_metric, 0u);

  // Test all-sequence metrics.
  histogram_tester.ExpectTotalCount(kAllSequencesMetricName, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(kAllSequencesMetricName),
              testing::ElementsAre(base::Bucket(0, 1)));

  histogram_tester.ExpectTotalCount(kAllAnimationsMetricName, 0u);

  histogram_tester.ExpectTotalCount(kAllInteractionsMetricName, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(kAllInteractionsMetricName),
              testing::ElementsAre(base::Bucket(0, 1)));

  // Stale-frame metrics
  const char* stale_metric = "Graphics.Smoothness.Stale.WheelScroll";
  const char* maxstale_metric = "Graphics.Smoothness.MaxStale.WheelScroll";

  histogram_tester.ExpectTotalCount(stale_metric, 5u);
  EXPECT_THAT(histogram_tester.GetAllSamples(stale_metric),
              testing::ElementsAre(base::Bucket(0, 5)));

  histogram_tester.ExpectTotalCount(maxstale_metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(maxstale_metric),
              testing::ElementsAre(base::Bucket(0, 1)));
}

TEST_F(JankMetricsTest, WheelScrollCompositorTwoJanksWithLargeFluctuation) {
  base::HistogramTester histogram_tester;
  FrameSequenceTrackerType tracker_type =
      FrameSequenceTrackerType::kWheelScroll;
  FrameInfo::SmoothEffectDrivingThread thread_type =
      FrameInfo::SmoothEffectDrivingThread::kCompositor;
  JankMetrics jank_reporter{tracker_type, thread_type};

  // Two janks; there are no no-update frames. The fluctuations in presentation
  // of 'C' and 'D' are just big enough to cause another jank.
  SimulateFrameSequence(&jank_reporter,
                        {
                            /*submit   */ "ab-C-D",
                            /*noupdate */ "------",
                            /*present  */ "ab-C-D",
                        },
                        {{'C', -2.0 /*ms*/}, {'D', +7.0 /*ms*/}});
  jank_reporter.ReportJankMetrics(100u);

  // One sample of 2 janks reported for "Compositor".
  const char* metric = "Graphics.Smoothness.Jank.Compositor.WheelScroll";
  const char* invalid_metric = "Graphics.Smoothness.Jank.Main.WheelScroll";

  histogram_tester.ExpectTotalCount(metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(metric),
              testing::ElementsAre(base::Bucket(2, 1)));

  // No reporting for "Main".
  histogram_tester.ExpectTotalCount(invalid_metric, 0u);

  // Test all-sequence metrics.
  histogram_tester.ExpectTotalCount(kAllSequencesMetricName, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(kAllSequencesMetricName),
              testing::ElementsAre(base::Bucket(2, 1)));

  histogram_tester.ExpectTotalCount(kAllAnimationsMetricName, 0u);

  histogram_tester.ExpectTotalCount(kAllInteractionsMetricName, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(kAllInteractionsMetricName),
              testing::ElementsAre(base::Bucket(2, 1)));

  // Stale-frame metrics
  const char* stale_metric = "Graphics.Smoothness.Stale.WheelScroll";
  const char* maxstale_metric = "Graphics.Smoothness.MaxStale.WheelScroll";

  histogram_tester.ExpectTotalCount(stale_metric, 3u);
  EXPECT_THAT(histogram_tester.GetAllSamples(stale_metric),
              testing::ElementsAre(
                  base::Bucket(0, 1), base::Bucket(14, 1), /*-2ms fluctuation*/
                  base::Bucket(25, 1) /*+7ms - (-2)ms = +9ms fluctuation*/));

  histogram_tester.ExpectTotalCount(maxstale_metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(maxstale_metric),
              testing::ElementsAre(base::Bucket(25, 1)));
}

TEST_F(JankMetricsTest, TouchScrollCompositorThreadManyJanksLongLatency) {
  base::HistogramTester histogram_tester;
  FrameSequenceTrackerType tracker_type =
      FrameSequenceTrackerType::kTouchScroll;
  FrameInfo::SmoothEffectDrivingThread thread_type =
      FrameInfo::SmoothEffectDrivingThread::kCompositor;

  JankMetrics jank_reporter{tracker_type, thread_type};

  // There are long delays from submit to presentations.
  SimulateFrameSequence(
      &jank_reporter,
      {
          /*submit   */ "abB-c--D--EFgH----------------------",
          /*noupdate */ "---*--------------------------------",
          /*present  */ "----------ab-B--c---D----E-----Fg--H",
      },
      {{'F', -3.0 /*ms*/}});
  jank_reporter.ReportJankMetrics(120u);

  // Expect janks in the 5/120 ~= 4% bucket for "Compositor", and no jank
  // for "Main"
  const char* metric = "Graphics.Smoothness.Jank.Compositor.TouchScroll";
  const char* invalid_metric = "Graphics.Smoothness.Jank.Main.TouchScroll";

  histogram_tester.ExpectTotalCount(metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(metric),
              testing::ElementsAre(base::Bucket(4, 1)));

  histogram_tester.ExpectTotalCount(invalid_metric, 0u);

  // Test all-sequence metrics.
  histogram_tester.ExpectTotalCount(kAllSequencesMetricName, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(kAllSequencesMetricName),
              testing::ElementsAre(base::Bucket(4, 1)));

  histogram_tester.ExpectTotalCount(kAllAnimationsMetricName, 0u);

  histogram_tester.ExpectTotalCount(kAllInteractionsMetricName, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(kAllInteractionsMetricName),
              testing::ElementsAre(base::Bucket(4, 1)));

  // Stale-frame metrics
  const char* stale_metric = "Graphics.Smoothness.Stale.TouchScroll";
  const char* maxstale_metric = "Graphics.Smoothness.MaxStale.TouchScroll";

  histogram_tester.ExpectTotalCount(stale_metric, 8u);
  EXPECT_THAT(histogram_tester.GetAllSamples(stale_metric),
              testing::ElementsAre(
                  base::Bucket(0, 1), base::Bucket(3, 1), /* F to g */
                  base::Bucket(16, 2), base::Bucket(33, 1), base::Bucket(50, 1),
                  base::Bucket(66, 1), base::Bucket(80, 1) /* E to F */));

  histogram_tester.ExpectTotalCount(maxstale_metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(maxstale_metric),
              testing::ElementsAre(base::Bucket(80, 1)));
}

// Test if the jank reporter can correctly merge janks from another jank
// reporter.
TEST_F(JankMetricsTest, RAFMergeJanks) {
  base::HistogramTester histogram_tester;
  FrameSequenceTrackerType tracker_type = FrameSequenceTrackerType::kRAF;
  FrameInfo::SmoothEffectDrivingThread thread_type =
      FrameInfo::SmoothEffectDrivingThread::kMain;

  JankMetrics jank_reporter{tracker_type, thread_type};
  std::unique_ptr<JankMetrics> other_reporter =
      std::make_unique<JankMetrics>(tracker_type, thread_type);

  std::array<std::string, 3> seqs = {
      /*submit   */ "a-b-Cd-e-F--D-",
      /*noupdate */ "-*----*-------",
      /*present  */ "-a-b-Cd-e-F--D",
  };
  SimulateFrameSequence(&jank_reporter, seqs);
  SimulateFrameSequence(other_reporter.get(), seqs);

  jank_reporter.Merge(std::move(other_reporter));
  EXPECT_EQ(jank_reporter.jank_count(), 6);
  EXPECT_TRUE(jank_reporter.max_staleness() > base::Milliseconds(33) &&
              jank_reporter.max_staleness() < base::Milliseconds(34));
  jank_reporter.ReportJankMetrics(100u);

  // Jank / staleness values should be reset after reporting
  EXPECT_EQ(jank_reporter.jank_count(), 0);
  EXPECT_EQ(jank_reporter.max_staleness(), base::Milliseconds(0));

  // Expect 6 janks for "Main" (3 from each reporter)
  const char* metric = "Graphics.Smoothness.Jank.Main.RAF";
  const char* invalid_metric = "Graphics.Smoothness.Jank.Compositor.RAF";

  histogram_tester.ExpectTotalCount(metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(metric),
              testing::ElementsAre(base::Bucket(6, 1)));

  histogram_tester.ExpectTotalCount(invalid_metric, 0u);

  // Test all-sequence metrics.
  // RAF is not included in AllSequences/AllAnimations metrics.
  histogram_tester.ExpectTotalCount(kAllSequencesMetricName, 0u);
  histogram_tester.ExpectTotalCount(kAllAnimationsMetricName, 0u);
  histogram_tester.ExpectTotalCount(kAllInteractionsMetricName, 0u);

  // Stale-frame metrics
  const char* stale_metric = "Graphics.Smoothness.Stale.RAF";
  const char* maxstale_metric = "Graphics.Smoothness.MaxStale.RAF";

  histogram_tester.ExpectTotalCount(stale_metric, 12u);
  EXPECT_THAT(histogram_tester.GetAllSamples(stale_metric),
              testing::ElementsAre(base::Bucket(0, 6), base::Bucket(16, 4),
                                   base::Bucket(33, 2)));

  histogram_tester.ExpectTotalCount(maxstale_metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(maxstale_metric),
              testing::ElementsAre(base::Bucket(33, 1)));
}

// Test if jank reporting is correctly disabled for Custom trackers.
TEST_F(JankMetricsTest, CustomNotReported) {
  base::HistogramTester histogram_tester;
  FrameSequenceTrackerType tracker_type = FrameSequenceTrackerType::kCustom;
  FrameInfo::SmoothEffectDrivingThread thread_type =
      FrameInfo::SmoothEffectDrivingThread::kMain;
  JankMetrics jank_reporter{tracker_type, thread_type};

  // There should be 4 janks, but the jank reporter does not track or report
  // them.
  SimulateFrameSequence(&jank_reporter, {
                                            /*submit   */ "ab-C--D---E----F",
                                            /*noupdate */ "----------------",
                                            /*present  */ "ab-C--D---E----F",
                                        });
  jank_reporter.ReportJankMetrics(100u);

  // Expect no jank reports even though the sequence contains jank
  histogram_tester.ExpectTotalCount("Graphics.Smoothness.Jank.Main.Custom", 0u);
  histogram_tester.ExpectTotalCount(
      "Graphics.Smoothness.Jank.Compositor.Custom", 0u);

  // Test all-sequence metrics.
  histogram_tester.ExpectTotalCount(kAllSequencesMetricName, 0u);
  histogram_tester.ExpectTotalCount(kAllAnimationsMetricName, 0u);
  histogram_tester.ExpectTotalCount(kAllInteractionsMetricName, 0u);

  // Stale-frame metrics
  histogram_tester.ExpectTotalCount("Graphics.Smoothness.Stale.Custom", 0u);
  histogram_tester.ExpectTotalCount("Graphics.Smoothness.MaxStale.Custom", 0u);
}

// Test a frame sequence with a long idle period >= 100 frames.
// The presentation interval containing the idle period is excluded from
// jank/stale calculation since the length of the idle period reaches a
// predefined cap.
TEST_F(JankMetricsTest, CompositorAnimationOneJankWithLongIdlePeriod) {
  base::HistogramTester histogram_tester;
  FrameSequenceTrackerType tracker_type =
      FrameSequenceTrackerType::kCompositorAnimation;
  FrameInfo::SmoothEffectDrivingThread thread_type =
      FrameInfo::SmoothEffectDrivingThread::kCompositor;
  JankMetrics jank_reporter{tracker_type, thread_type};

  // One jank at E. The long delay of 100 frames between b and c is considered
  // a long idle period and therefore does not participate in jank/stale
  // calculation.
  SimulateFrameSequence(&jank_reporter,
                        {
                            /*submit   */ std::string("a-b") +
                                std::string(100, '-') + std::string("c--d---E"),
                            /*noupdate */ std::string("---") +
                                std::string(100, '*') + std::string("--------"),
                            /*present  */ std::string("a-b") +
                                std::string(100, '-') + std::string("c--d---E"),
                        },
                        {});
  jank_reporter.ReportJankMetrics(100u);

  // One sample of 1 janks reported for "Compositor".
  const char* metric =
      "Graphics.Smoothness.Jank.Compositor.CompositorAnimation";
  const char* invalid_metric =
      "Graphics.Smoothness.Jank.Main.CompositorAnimation";

  histogram_tester.ExpectTotalCount(metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(metric),
              testing::ElementsAre(base::Bucket(1, 1)));

  // Test all-sequence metrics.
  histogram_tester.ExpectTotalCount(kAllSequencesMetricName, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(kAllSequencesMetricName),
              testing::ElementsAre(base::Bucket(1, 1)));

  histogram_tester.ExpectTotalCount(kAllAnimationsMetricName, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(kAllSequencesMetricName),
              testing::ElementsAre(base::Bucket(1, 1)));

  histogram_tester.ExpectTotalCount(kAllInteractionsMetricName, 0u);

  // Stale-frame metrics
  const char* stale_metric = "Graphics.Smoothness.Stale.CompositorAnimation";
  const char* maxstale_metric =
      "Graphics.Smoothness.MaxStale.CompositorAnimation";

  histogram_tester.ExpectTotalCount(stale_metric, 4u);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(stale_metric),
      testing::ElementsAre(base::Bucket(0, 1),  /* The long frame from b to c*/
                           base::Bucket(16, 1), /* a-b */
                           base::Bucket(33, 1), /* c--d */
                           base::Bucket(50, 1)) /* d---E */);
  histogram_tester.ExpectTotalCount(maxstale_metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(maxstale_metric),
              testing::ElementsAre(base::Bucket(50, 1)));

  // No reporting for "Main".
  histogram_tester.ExpectTotalCount(invalid_metric, 0u);
}

// Test a frame sequence with an idle period < 100 frames.
// The jank and stale are still calculated normally in this case.
TEST_F(JankMetricsTest, CompositorAnimationTwoJanksWithModerateIdlePeriod) {
  base::HistogramTester histogram_tester;
  FrameSequenceTrackerType tracker_type =
      FrameSequenceTrackerType::kCompositorAnimation;
  FrameInfo::SmoothEffectDrivingThread thread_type =
      FrameInfo::SmoothEffectDrivingThread::kCompositor;
  JankMetrics jank_reporter{tracker_type, thread_type};

  // Two janks at D and E. The long delay of 99 no-update frames does not
  // exceed the capacity of the no-update frame queue and therefore is not
  // excluded from jank/stale calculation.
  SimulateFrameSequence(&jank_reporter,
                        {
                            /*submit   */ std::string("a-b-") +
                                std::string(99, '-') + std::string("c--D---E"),
                            /*noupdate */ std::string("----") +
                                std::string(99, '*') + std::string("--------"),
                            /*present  */ std::string("a-b-") +
                                std::string(99, '-') + std::string("c--D---E"),
                        },
                        {});
  jank_reporter.ReportJankMetrics(100u);

  // One sample of 2 janks reported for "Compositor".
  const char* metric =
      "Graphics.Smoothness.Jank.Compositor.CompositorAnimation";
  const char* invalid_metric =
      "Graphics.Smoothness.Jank.Main.CompositorAnimation";

  histogram_tester.ExpectTotalCount(metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(metric),
              testing::ElementsAre(base::Bucket(2, 1)));

  // Test all-sequence metrics.
  histogram_tester.ExpectTotalCount(kAllSequencesMetricName, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(kAllSequencesMetricName),
              testing::ElementsAre(base::Bucket(2, 1)));

  histogram_tester.ExpectTotalCount(kAllAnimationsMetricName, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(kAllSequencesMetricName),
              testing::ElementsAre(base::Bucket(2, 1)));

  histogram_tester.ExpectTotalCount(kAllInteractionsMetricName, 0u);

  // Stale-frame metrics
  const char* stale_metric = "Graphics.Smoothness.Stale.CompositorAnimation";
  const char* maxstale_metric =
      "Graphics.Smoothness.MaxStale.CompositorAnimation";

  histogram_tester.ExpectTotalCount(stale_metric, 4u);
  EXPECT_THAT(histogram_tester.GetAllSamples(stale_metric),
              testing::ElementsAre(base::Bucket(16, 2), /* a-b & b-c */
                                   base::Bucket(33, 1), /* c--d */
                                   base::Bucket(50, 1)) /* d---E */);
  histogram_tester.ExpectTotalCount(maxstale_metric, 1u);
  EXPECT_THAT(histogram_tester.GetAllSamples(maxstale_metric),
              testing::ElementsAre(base::Bucket(50, 1)));

  // No reporting for "Main".
  histogram_tester.ExpectTotalCount(invalid_metric, 0u);
}

}  // namespace cc
