// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/metrics/ambient_multi_screen_metrics_recorder.h"

#include <string>

#include "ash/constants/ambient_theme.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "cc/test/skia_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/lottie/animation.h"

namespace ash {
namespace {
using ::testing::Eq;
}  // namespace

class AmbientMultiScreenMetricsRecorderTest : public ::testing::Test {
 protected:
  struct Harness {
    static constexpr gfx::Size kTestSize = gfx::Size(100, 100);
    static constexpr base::TimeDelta kTotalAnimationDuration =
        base::Seconds(10);

    explicit Harness(AmbientTheme theme)
        : recorder(theme),
          animation_1(cc::CreateSkottie(kTestSize,
                                        kTotalAnimationDuration.InSecondsF())),
          animation_2(cc::CreateSkottie(kTestSize,
                                        kTotalAnimationDuration.InSecondsF())) {
    }

    // In the real code, AmbientMultiScreenMetricsRecorder outlives the
    // animations, so simulate that in tests here.
    AmbientMultiScreenMetricsRecorder recorder;
    lottie::Animation animation_1;
    lottie::Animation animation_2;
    gfx::Canvas canvas;
  };

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

class AmbientMultiScreenMetricsRecorderTestForAllThemes
    : public AmbientMultiScreenMetricsRecorderTest,
      public ::testing::WithParamInterface<AmbientTheme> {
 protected:
  std::string GetMetricNameForTheme(base::StringPiece prefix) {
    return base::StrCat({prefix, ToString(GetParam())});
  }
};

INSTANTIATE_TEST_SUITE_P(AllAnimationThemes,
                         AmbientMultiScreenMetricsRecorderTestForAllThemes,
                         testing::Values(AmbientTheme::kFeelTheBreeze,
                                         AmbientTheme::kFloatOnBy));

TEST_P(AmbientMultiScreenMetricsRecorderTestForAllThemes, RecordsScreenCount) {
  base::HistogramTester histogram_tester;
  {
    Harness harness(GetParam());
    harness.recorder.RegisterScreen(&harness.animation_1);
    harness.animation_1.Start();
    harness.animation_1.Paint(&harness.canvas, task_environment_.NowTicks(),
                              Harness::kTestSize);
  }
  histogram_tester.ExpectUniqueSample(
      GetMetricNameForTheme("Ash.AmbientMode.ScreenCount."), /*sample=*/1,
      /*expected_bucket_count=*/1);
  {
    Harness harness(GetParam());
    harness.recorder.RegisterScreen(&harness.animation_1);
    harness.recorder.RegisterScreen(&harness.animation_2);
    harness.animation_1.Start();
    harness.animation_1.Paint(&harness.canvas, task_environment_.NowTicks(),
                              Harness::kTestSize);
    harness.animation_2.Start();
    harness.animation_2.Paint(&harness.canvas, task_environment_.NowTicks(),
                              Harness::kTestSize);
  }
  histogram_tester.ExpectBucketCount(
      GetMetricNameForTheme("Ash.AmbientMode.ScreenCount."), /*sample=*/2,
      /*expected_count=*/1);
}

TEST_P(AmbientMultiScreenMetricsRecorderTestForAllThemes,
       RecordsTimestampOffset) {
  static constexpr base::TimeDelta kFrameInterval = base::Milliseconds(100);
  base::HistogramTester histogram_tester;

  Harness harness(GetParam());
  harness.recorder.RegisterScreen(&harness.animation_1);
  harness.recorder.RegisterScreen(&harness.animation_2);
  harness.animation_1.Start();
  harness.animation_2.Start();
  harness.animation_1.Paint(&harness.canvas, task_environment_.NowTicks(),
                            Harness::kTestSize);
  // Offset of 0.
  harness.animation_2.Paint(&harness.canvas, task_environment_.NowTicks(),
                            Harness::kTestSize);
  task_environment_.FastForwardBy(kFrameInterval);
  // Offset of |kFrameInterval|.
  harness.animation_1.Paint(&harness.canvas, task_environment_.NowTicks(),
                            Harness::kTestSize);
  // Offset of 0.
  harness.animation_2.Paint(&harness.canvas, task_environment_.NowTicks(),
                            Harness::kTestSize);

  histogram_tester.ExpectTimeBucketCount(
      GetMetricNameForTheme("Ash.AmbientMode.MultiScreenOffset."),
      base::TimeDelta(), 2);
  histogram_tester.ExpectTimeBucketCount(
      GetMetricNameForTheme("Ash.AmbientMode.MultiScreenOffset."),
      kFrameInterval, 1);
}

TEST_P(AmbientMultiScreenMetricsRecorderTestForAllThemes,
       RecordsMeanTimestampOffsetWithDifferentCycleStartOffsets) {
  base::HistogramTester histogram_tester;

  Harness harness(GetParam());
  harness.recorder.RegisterScreen(&harness.animation_1);
  harness.recorder.RegisterScreen(&harness.animation_2);
  lottie::Animation::PlaybackConfig playback_config(
      {{base::TimeDelta(), Harness::kTotalAnimationDuration},
       {Harness::kTotalAnimationDuration * .25f,
        Harness::kTotalAnimationDuration * .75f}},
      /*initial_offset=*/base::TimeDelta(), /*initial_completed_cycles=*/0,
      lottie::Animation::Style::kLoop);
  harness.animation_1.Start(playback_config);
  harness.animation_2.Start(playback_config);
  harness.animation_1.Paint(&harness.canvas, task_environment_.NowTicks(),
                            Harness::kTestSize);
  // Offset of 0.
  harness.animation_2.Paint(&harness.canvas, task_environment_.NowTicks(),
                            Harness::kTestSize);

  task_environment_.FastForwardBy(Harness::kTotalAnimationDuration / 2);
  // Offset of kTotalAnimationDuration / 2.
  harness.animation_1.Paint(&harness.canvas, task_environment_.NowTicks(),
                            Harness::kTestSize);
  // Offset of 0.
  harness.animation_2.Paint(&harness.canvas, task_environment_.NowTicks(),
                            Harness::kTestSize);

  // Fast forward to just before end of first cycle.
  task_environment_.FastForwardBy((Harness::kTotalAnimationDuration / 2) -
                                  base::Milliseconds(100));
  // Offset of kTotalAnimationDuration / 2) - 100ms
  harness.animation_1.Paint(&harness.canvas, task_environment_.NowTicks(),
                            Harness::kTestSize);
  task_environment_.FastForwardBy(base::Milliseconds(200));
  // Fast forward to just after start of second cycle.
  // Offset of 200 ms (100 ms before end of first cycle to 100 ms past start of
  // second cycle).
  harness.animation_2.Paint(&harness.canvas, task_environment_.NowTicks(),
                            Harness::kTestSize);
  // Offset of 0.
  harness.animation_1.Paint(&harness.canvas, task_environment_.NowTicks(),
                            Harness::kTestSize);

  histogram_tester.ExpectTimeBucketCount(
      GetMetricNameForTheme("Ash.AmbientMode.MultiScreenOffset."),
      base::TimeDelta(), 3);
  histogram_tester.ExpectTimeBucketCount(
      GetMetricNameForTheme("Ash.AmbientMode.MultiScreenOffset."),
      Harness::kTotalAnimationDuration / 2, 2);
  histogram_tester.ExpectTimeBucketCount(
      GetMetricNameForTheme("Ash.AmbientMode.MultiScreenOffset."),
      base::Milliseconds(200), 1);
  histogram_tester.ExpectTotalCount(
      GetMetricNameForTheme("Ash.AmbientMode.MultiScreenOffset."), 6);
}

TEST_P(AmbientMultiScreenMetricsRecorderTestForAllThemes,
       DoesNotRecordMeanTimestampOffsetForSingleScreen) {
  static constexpr base::TimeDelta kFrameInterval = base::Milliseconds(100);
  base::HistogramTester histogram_tester;

  Harness harness(GetParam());
  harness.recorder.RegisterScreen(&harness.animation_1);
  harness.animation_1.Start();
  harness.animation_1.Paint(&harness.canvas, task_environment_.NowTicks(),
                            Harness::kTestSize);
  task_environment_.FastForwardBy(kFrameInterval);
  harness.animation_1.Paint(&harness.canvas, task_environment_.NowTicks(),
                            Harness::kTestSize);
  histogram_tester.ExpectTotalCount(
      "Ash.AmbientMode.MultiScreenOffset.FeelTheBreeze", 0);
}

TEST_F(AmbientMultiScreenMetricsRecorderTest, HandlesNullAnimations) {
  base::HistogramTester histogram_tester;
  {
    AmbientMultiScreenMetricsRecorder recorder(AmbientTheme::kSlideshow);
    recorder.RegisterScreen(nullptr);
    recorder.RegisterScreen(nullptr);
  }
  histogram_tester.ExpectUniqueSample("Ash.AmbientMode.ScreenCount.SlideShow",
                                      /*sample=*/2,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Ash.AmbientMode.MultiScreenOffset.SlideShow", 0);
}

}  // namespace ash
