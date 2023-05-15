// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/metrics/ambient_session_metrics_recorder.h"

#include <string>
#include <utility>

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/constants/ambient_theme.h"
#include "ash/constants/ambient_video.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
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

class AmbientSessionMetricsRecorderTest
    : public AshTestBase,
      public ::testing::WithParamInterface<AmbientUiSettings> {
 protected:
  AmbientSessionMetricsRecorderTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();
    // Simulate the screensaver being launched in all tests.
    AmbientUiModel::Get()->SetUiVisibility(AmbientUiVisibility::kShouldShow);
  }

  std::string GetMetricNameForTheme(base::StringPiece prefix) {
    return base::StrCat({prefix, GetParam().ToString()});
  }
};

class AmbientSessionMetricsRecorderLottieAnimationTest
    : public AmbientSessionMetricsRecorderTest {
 protected:
  struct Harness {
    static constexpr gfx::Size kTestSize = gfx::Size(100, 100);
    static constexpr base::TimeDelta kTotalAnimationDuration =
        base::Seconds(10);

    explicit Harness(AmbientUiSettings ui_settings)
        : recorder(std::move(ui_settings)),
          animation_1(cc::CreateSkottie(kTestSize,
                                        kTotalAnimationDuration.InSecondsF())),
          animation_2(cc::CreateSkottie(kTestSize,
                                        kTotalAnimationDuration.InSecondsF())) {
    }

    // In the real code, AmbientSessionMetricsRecorder outlives the
    // animations, so simulate that in tests here.
    AmbientSessionMetricsRecorder recorder;
    lottie::Animation animation_1;
    lottie::Animation animation_2;
    gfx::Canvas canvas;
  };
};

INSTANTIATE_TEST_SUITE_P(
    AllUiSettings,
    AmbientSessionMetricsRecorderTest,
    // Just one sample for each category of ui settings
    // is needed.
    testing::Values(AmbientUiSettings(AmbientTheme::kSlideshow),
                    AmbientUiSettings(AmbientTheme::kFeelTheBreeze),
                    AmbientUiSettings(AmbientTheme::kVideo,
                                      AmbientVideo::kNewMexico)));

INSTANTIATE_TEST_SUITE_P(
    AllLottieAnimations,
    AmbientSessionMetricsRecorderLottieAnimationTest,
    testing::Values(AmbientUiSettings(AmbientTheme::kFeelTheBreeze),
                    AmbientUiSettings(AmbientTheme::kFloatOnBy)));

TEST_P(AmbientSessionMetricsRecorderTest, MetricsEngagementTime) {
  constexpr base::TimeDelta kExpectedEngagementTime = base::Minutes(5);
  base::HistogramTester histogram_tester;
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  {
    AmbientSessionMetricsRecorder recorder(GetParam());
    recorder.RegisterScreen(/*animation=*/nullptr);
    task_environment()->FastForwardBy(kExpectedEngagementTime);
  }

  histogram_tester.ExpectTimeBucketCount(
      "Ash.AmbientMode.EngagementTime.ClamshellMode", kExpectedEngagementTime,
      1);
  histogram_tester.ExpectTimeBucketCount(
      base::StrCat({"Ash.AmbientMode.EngagementTime.", GetParam().ToString()}),
      kExpectedEngagementTime, 1);

  // Now do the same sequence in tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  {
    AmbientSessionMetricsRecorder recorder(GetParam());
    recorder.RegisterScreen(/*animation=*/nullptr);
    task_environment()->FastForwardBy(kExpectedEngagementTime);
  }

  histogram_tester.ExpectTimeBucketCount(
      "Ash.AmbientMode.EngagementTime.TabletMode", kExpectedEngagementTime, 1);
  histogram_tester.ExpectTimeBucketCount(
      base::StrCat({"Ash.AmbientMode.EngagementTime.", GetParam().ToString()}),
      kExpectedEngagementTime, 2);
}

TEST_P(AmbientSessionMetricsRecorderTest, MetricsStartupTime) {
  constexpr base::TimeDelta kExpectedStartupTime = base::Seconds(5);

  base::HistogramTester histogram_tester;
  AmbientSessionMetricsRecorder recorder(GetParam());
  task_environment()->FastForwardBy(kExpectedStartupTime);
  recorder.RegisterScreen(/*animation=*/nullptr);
  // Should be ignored. The time that the first screen starts rendering should
  // be when the startup time is recorded.
  task_environment()->FastForwardBy(base::Minutes(1));
  recorder.RegisterScreen(/*animation=*/nullptr);
  histogram_tester.ExpectTimeBucketCount(
      base::StrCat({"Ash.AmbientMode.StartupTime.", GetParam().ToString()}),
      kExpectedStartupTime, 1);
}

TEST_P(AmbientSessionMetricsRecorderTest, MetricsStartupTimeFailedToStart) {
  constexpr base::TimeDelta kFailedStartupTime = base::Minutes(1);
  base::HistogramTester histogram_tester;
  {
    AmbientSessionMetricsRecorder recorder(GetParam());
    task_environment()->FastForwardBy(kFailedStartupTime);
  }
  histogram_tester.ExpectUniqueTimeSample(
      base::StrCat({"Ash.AmbientMode.StartupTime.", GetParam().ToString()}),
      kFailedStartupTime, 1);
}

TEST_P(AmbientSessionMetricsRecorderTest, RecordsScreenCount) {
  base::HistogramTester histogram_tester;
  {
    AmbientSessionMetricsRecorder recorder(GetParam());
    recorder.RegisterScreen(/*animation=*/nullptr);
  }
  histogram_tester.ExpectUniqueSample(
      GetMetricNameForTheme("Ash.AmbientMode.ScreenCount."), /*sample=*/1,
      /*expected_bucket_count=*/1);
  {
    AmbientSessionMetricsRecorder recorder(GetParam());
    recorder.RegisterScreen(/*animation=*/nullptr);
    recorder.RegisterScreen(/*animation=*/nullptr);
  }
  histogram_tester.ExpectBucketCount(
      GetMetricNameForTheme("Ash.AmbientMode.ScreenCount."), /*sample=*/2,
      /*expected_count=*/1);
}

TEST_P(AmbientSessionMetricsRecorderLottieAnimationTest,
       RecordsTimestampOffset) {
  static constexpr base::TimeDelta kFrameInterval = base::Milliseconds(100);
  base::HistogramTester histogram_tester;

  Harness harness(GetParam());
  harness.recorder.RegisterScreen(&harness.animation_1);
  harness.recorder.RegisterScreen(&harness.animation_2);
  harness.animation_1.Start();
  harness.animation_2.Start();
  harness.animation_1.Paint(&harness.canvas, task_environment()->NowTicks(),
                            Harness::kTestSize);
  // Offset of 0.
  harness.animation_2.Paint(&harness.canvas, task_environment()->NowTicks(),
                            Harness::kTestSize);
  task_environment()->FastForwardBy(kFrameInterval);
  // Offset of |kFrameInterval|.
  harness.animation_1.Paint(&harness.canvas, task_environment()->NowTicks(),
                            Harness::kTestSize);
  // Offset of 0.
  harness.animation_2.Paint(&harness.canvas, task_environment()->NowTicks(),
                            Harness::kTestSize);

  histogram_tester.ExpectTimeBucketCount(
      GetMetricNameForTheme("Ash.AmbientMode.MultiScreenOffset."),
      base::TimeDelta(), 2);
  histogram_tester.ExpectTimeBucketCount(
      GetMetricNameForTheme("Ash.AmbientMode.MultiScreenOffset."),
      kFrameInterval, 1);
}

TEST_P(AmbientSessionMetricsRecorderLottieAnimationTest,
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
  harness.animation_1.Paint(&harness.canvas, task_environment()->NowTicks(),
                            Harness::kTestSize);
  // Offset of 0.
  harness.animation_2.Paint(&harness.canvas, task_environment()->NowTicks(),
                            Harness::kTestSize);

  task_environment()->FastForwardBy(Harness::kTotalAnimationDuration / 2);
  // Offset of kTotalAnimationDuration / 2.
  harness.animation_1.Paint(&harness.canvas, task_environment()->NowTicks(),
                            Harness::kTestSize);
  // Offset of 0.
  harness.animation_2.Paint(&harness.canvas, task_environment()->NowTicks(),
                            Harness::kTestSize);

  // Fast forward to just before end of first cycle.
  task_environment()->FastForwardBy((Harness::kTotalAnimationDuration / 2) -
                                    base::Milliseconds(100));
  // Offset of kTotalAnimationDuration / 2) - 100ms
  harness.animation_1.Paint(&harness.canvas, task_environment()->NowTicks(),
                            Harness::kTestSize);
  task_environment()->FastForwardBy(base::Milliseconds(200));
  // Fast forward to just after start of second cycle.
  // Offset of 200 ms (100 ms before end of first cycle to 100 ms past start of
  // second cycle).
  harness.animation_2.Paint(&harness.canvas, task_environment()->NowTicks(),
                            Harness::kTestSize);
  // Offset of 0.
  harness.animation_1.Paint(&harness.canvas, task_environment()->NowTicks(),
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

TEST_P(AmbientSessionMetricsRecorderLottieAnimationTest,
       DoesNotRecordMeanTimestampOffsetForSingleScreen) {
  static constexpr base::TimeDelta kFrameInterval = base::Milliseconds(100);
  base::HistogramTester histogram_tester;

  Harness harness(GetParam());
  harness.recorder.RegisterScreen(&harness.animation_1);
  harness.animation_1.Start();
  harness.animation_1.Paint(&harness.canvas, task_environment()->NowTicks(),
                            Harness::kTestSize);
  task_environment()->FastForwardBy(kFrameInterval);
  harness.animation_1.Paint(&harness.canvas, task_environment()->NowTicks(),
                            Harness::kTestSize);
  histogram_tester.ExpectTotalCount(
      "Ash.AmbientMode.MultiScreenOffset.FeelTheBreeze", 0);
}

}  // namespace ash
