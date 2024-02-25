// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/metrics/ambient_session_metrics_recorder.h"

#include <string>
#include <string_view>
#include <utility>

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/metrics/ambient_consumer_session_metrics_delegate.h"
#include "ash/constants/ambient_video.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/strings/strcat.h"

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

  std::string GetMetricNameForTheme(std::string_view prefix) {
    return base::StrCat({prefix, GetParam().ToString()});
  }

  std::unique_ptr<AmbientSessionMetricsRecorder::Delegate> CreateDelegate() {
    return std::make_unique<AmbientConsumerSessionMetricsDelegate>(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(
    AllUiSettings,
    AmbientSessionMetricsRecorderTest,
    // Just one sample for each category of ui settings
    // is needed.
    testing::Values(
        AmbientUiSettings(personalization_app::mojom::AmbientTheme::kSlideshow),
        AmbientUiSettings(
            personalization_app::mojom::AmbientTheme::kFeelTheBreeze),
        AmbientUiSettings(personalization_app::mojom::AmbientTheme::kVideo,
                          AmbientVideo::kNewMexico)));

TEST_P(AmbientSessionMetricsRecorderTest, MetricsEngagementTime) {
  constexpr base::TimeDelta kExpectedEngagementTime = base::Minutes(5);
  base::HistogramTester histogram_tester;
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  {
    AmbientSessionMetricsRecorder recorder(CreateDelegate());
    recorder.SetInitStatus(true);
    recorder.RegisterScreen();
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
    AmbientSessionMetricsRecorder recorder(CreateDelegate());
    recorder.SetInitStatus(true);
    recorder.RegisterScreen();
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
  AmbientSessionMetricsRecorder recorder(CreateDelegate());
  task_environment()->FastForwardBy(kExpectedStartupTime);
  recorder.SetInitStatus(true);
  recorder.RegisterScreen();
  // Should be ignored. The time that the first screen starts rendering should
  // be when the startup time is recorded.
  task_environment()->FastForwardBy(base::Minutes(1));
  recorder.RegisterScreen();
  histogram_tester.ExpectTimeBucketCount(
      base::StrCat({"Ash.AmbientMode.StartupTime.", GetParam().ToString()}),
      kExpectedStartupTime, 1);
}

TEST_P(AmbientSessionMetricsRecorderTest, MetricsStartupTimeFailedToStart) {
  constexpr base::TimeDelta kFailedStartupTime = base::Minutes(1);
  base::HistogramTester histogram_tester;
  {
    AmbientSessionMetricsRecorder recorder(CreateDelegate());
    task_environment()->FastForwardBy(kFailedStartupTime);
  }
  histogram_tester.ExpectUniqueTimeSample(
      base::StrCat({"Ash.AmbientMode.StartupTime.", GetParam().ToString()}),
      kFailedStartupTime, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Ash.AmbientMode.Init.", GetParam().ToString()}),
      /*sample=*/0, /*expected_count=*/1);
}

TEST_P(AmbientSessionMetricsRecorderTest, InitStatusSuccess) {
  base::HistogramTester histogram_tester;
  AmbientSessionMetricsRecorder recorder(CreateDelegate());
  recorder.SetInitStatus(true);
  recorder.RegisterScreen();
  recorder.RegisterScreen();
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Ash.AmbientMode.Init.", GetParam().ToString()}),
      /*sample=*/1, /*expected_count=*/1);
}

TEST_P(AmbientSessionMetricsRecorderTest, InitStatusFailed) {
  base::HistogramTester histogram_tester;
  AmbientSessionMetricsRecorder recorder(CreateDelegate());
  recorder.SetInitStatus(false);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Ash.AmbientMode.Init.", GetParam().ToString()}),
      /*sample=*/0, /*expected_count=*/1);
}

TEST_P(AmbientSessionMetricsRecorderTest, RecordsScreenCount) {
  base::HistogramTester histogram_tester;
  {
    AmbientSessionMetricsRecorder recorder(CreateDelegate());
    recorder.SetInitStatus(true);
    recorder.RegisterScreen();
  }
  histogram_tester.ExpectUniqueSample(
      GetMetricNameForTheme("Ash.AmbientMode.ScreenCount."), /*sample=*/1,
      /*expected_bucket_count=*/1);
  {
    AmbientSessionMetricsRecorder recorder(CreateDelegate());
    recorder.SetInitStatus(true);
    recorder.RegisterScreen();
    recorder.RegisterScreen();
  }
  histogram_tester.ExpectBucketCount(
      GetMetricNameForTheme("Ash.AmbientMode.ScreenCount."), /*sample=*/2,
      /*expected_count=*/1);
}

}  // namespace ash
