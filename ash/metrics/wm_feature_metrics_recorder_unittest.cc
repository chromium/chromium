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

using WindowSizeRange = WMFeatureMetricsRecorder::WindowSizeRange;

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
      BucketsAre(base::Bucket(chromeos::AppType::SYSTEM_APP, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(metrics_prefix + "ActiveWindowSize"),
      BucketsAre(base::Bucket(WindowSizeRange::kXSWidthXSHeight, 1)));

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
      BucketsAre(base::Bucket(chromeos::AppType::SYSTEM_APP, 2)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(metrics_prefix + "ActiveWindowSize"),
      BucketsAre(base::Bucket(WindowSizeRange::kLWidthLHeight, 1),
                 base::Bucket(WindowSizeRange::kXSWidthXSHeight, 1)));
}

// Tests that window sizes range can be recorded properly.
TEST_F(WMFeatureMetricsRecorderTests, WindowSizeRangeTest) {
  UpdateDisplay("1600x1000");
  base::HistogramTester histogram_tester;

  const std::string metrics_prefix =
      WMFeatureMetricsRecorder::GetFeatureMetricsPrefix(
          WMFeatureMetricsRecorder::WMFeatureType::kWindowLayoutState);
  auto window = CreateAppWindow(gfx::Rect(0, 0, 200, 100));
  wm::ActivateWindow(window.get());

  struct TestCase {
    const char* msg;
    gfx::Size size;
    WindowSizeRange expectation;
  };
  std::vector<TestCase> test_cases = {
      {"(0-800, 0-600)", gfx::Size(500, 400),
       WindowSizeRange::kXSWidthXSHeight},
      {"(0-800, 600-728)", gfx::Size(500, 700),
       WindowSizeRange::kXSWidthSHeight},
      {"(0-800, 728-900)", gfx::Size(500, 800),
       WindowSizeRange::kXSWidthMHeight},
      {"(0-800, >900)", gfx::Size(500, 1000), WindowSizeRange::kXSWidthLHeight},
      {"(800-1024, 0-600)", gfx::Size(900, 400),
       WindowSizeRange::kSWidthXSHeight},
      {"(800-1024, 600-728)", gfx::Size(900, 700),
       WindowSizeRange::kSWidthSHeight},
      {"(800-1024, 728-900)", gfx::Size(900, 800),
       WindowSizeRange::kSWidthMHeight},
      {"(800-1024, >900)", gfx::Size(900, 1000),
       WindowSizeRange::kSWidthLHeight},
      {"(1024-1400, 0-600)", gfx::Size(1200, 400),
       WindowSizeRange::kMWidthXSHeight},
      {"(1024-1400, 600-728)", gfx::Size(1200, 700),
       WindowSizeRange::kMWidthSHeight},
      {"(1024-1400, 728-900)", gfx::Size(1200, 800),
       WindowSizeRange::kMWidthMHeight},
      {"(1024-1400, >900)", gfx::Size(1200, 1000),
       WindowSizeRange::kMWidthLHeight},
      {">1400, 0-600)", gfx::Size(1500, 400), WindowSizeRange::kLWidthXSHeight},
      {">1400, 600-728)", gfx::Size(1500, 700),
       WindowSizeRange::kLWidthSHeight},
      {">1400, 728-900)", gfx::Size(1500, 800),
       WindowSizeRange::kLWidthMHeight},
      {">1400, >900)", gfx::Size(1500, 1000), WindowSizeRange::kLWidthLHeight}};

  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.msg);
    window->SetBounds(gfx::Rect(test_case.size));
    FastForwardBy(kRecordPeriodicMetricsInterval);
    EXPECT_THAT(
        histogram_tester.GetAllSamples(metrics_prefix + "ActiveWindowSize"),
        BucketsInclude(base::Bucket(test_case.expectation, 1)));
  }
}

}  // namespace ash
