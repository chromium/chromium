// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/wm_feature_metrics_recorder.h"

#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

constexpr base::TimeDelta kRecordPeriodicMetricsInterval = base::Minutes(30);

}  // namespace

class WMFeatureMetricsRecorderTests : public AshTestBase {
 public:
  WMFeatureMetricsRecorderTests()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  WMFeatureMetricsRecorderTests(const WMFeatureMetricsRecorderTests&) = delete;
  WMFeatureMetricsRecorderTests& operator=(
      const WMFeatureMetricsRecorderTests&) = delete;
  ~WMFeatureMetricsRecorderTests() override = default;

  void FastForwardBy(base::TimeDelta delta) {
    task_environment()->FastForwardBy(delta);
  }
};

// Tests the window layout related metrics can be logged periodically.
TEST_F(WMFeatureMetricsRecorderTests, WindowLayoutMetricsRecorder) {
  UpdateDisplay("1600x1000");
  base::HistogramTester histogram_tester;

  const std::string metrics_prefix =
      WMFeatureMetricsRecorder::GetFeatureMetricsPrefix(
          WMFeatureMetricsRecorder::WMFeatureType::kWindowLayoutState);

  // Create two tests windows.
  auto window1 = CreateAppWindow(gfx::Rect(0, 0, 200, 100));
  EXPECT_EQ(WindowState::Get(window1.get())->GetStateType(),
            chromeos::WindowStateType::kDefault);
  auto window2 = CreateAppWindow(gfx::Rect(0, 0, 1500, 1000));
  EXPECT_EQ(WindowState::Get(window2.get())->GetStateType(),
            chromeos::WindowStateType::kDefault);

  wm::ActivateWindow(window1.get());
  FastForwardBy(kRecordPeriodicMetricsInterval);

  // Check the metrics.
  base::HistogramTester::CountsMap expected_counts;
  expected_counts[metrics_prefix + "WindowNumbers"] = 1;
  expected_counts[metrics_prefix + "AllWindowStates"] = 2;
  expected_counts[metrics_prefix + "AllAppTypes"] = 2;
  expected_counts[metrics_prefix + "AllWindowSizes"] = 2;
  expected_counts[metrics_prefix + "FreeformedWindowSizes"] = 2;
  expected_counts[metrics_prefix + "ActiveWindowState"] = 1;
  expected_counts[metrics_prefix + "ActiveWindowAppType"] = 1;
  expected_counts[metrics_prefix + "ActiveWindowSize"] = 1;
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(metrics_prefix),
              testing::ContainerEq(expected_counts));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(metrics_prefix + "ActiveWindowState"),
      BucketsAre(base::Bucket(chromeos::WindowStateType::kDefault, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(metrics_prefix + "ActiveWindowAppType"),
      BucketsAre(base::Bucket(AppType::SYSTEM_APP, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(metrics_prefix + "ActiveWindowSize"),
      BucketsAre(base::Bucket(
          WMFeatureMetricsRecorder::WindowSizeRange::kXSWidthXSHeight, 1)));

  wm::ActivateWindow(window2.get());
  WindowState::Get(window2.get())->Maximize();
  FastForwardBy(kRecordPeriodicMetricsInterval);

  expected_counts[metrics_prefix + "WindowNumbers"] = 2;
  expected_counts[metrics_prefix + "AllWindowStates"] = 4;
  expected_counts[metrics_prefix + "AllAppTypes"] = 4;
  expected_counts[metrics_prefix + "AllWindowSizes"] = 4;
  expected_counts[metrics_prefix + "FreeformedWindowSizes"] = 3;
  expected_counts[metrics_prefix + "ActiveWindowState"] = 2;
  expected_counts[metrics_prefix + "ActiveWindowAppType"] = 2;
  expected_counts[metrics_prefix + "ActiveWindowSize"] = 2;
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(metrics_prefix),
              testing::ContainerEq(expected_counts));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(metrics_prefix + "ActiveWindowState"),
      BucketsAre(base::Bucket(chromeos::WindowStateType::kMaximized, 1),
                 base::Bucket(chromeos::WindowStateType::kDefault, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(metrics_prefix + "ActiveWindowAppType"),
      BucketsAre(base::Bucket(AppType::SYSTEM_APP, 2)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(metrics_prefix + "ActiveWindowSize"),
      BucketsAre(
          base::Bucket(
              WMFeatureMetricsRecorder::WindowSizeRange::kLWidthLHeight, 1),
          base::Bucket(
              WMFeatureMetricsRecorder::WindowSizeRange::kXSWidthXSHeight, 1)));
}

}  // namespace ash
