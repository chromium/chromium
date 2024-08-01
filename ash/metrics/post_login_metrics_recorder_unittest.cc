// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/post_login_metrics_recorder.h"

#include <memory>
#include <string>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "cc/metrics/frame_sequence_metrics.h"

namespace ash {

class PostLoginMetricsRecorderTest : public AshTestBase {
 public:
  PostLoginMetricsRecorderTest() = default;
  PostLoginMetricsRecorderTest(const PostLoginMetricsRecorderTest&) = delete;
  PostLoginMetricsRecorderTest& operator=(const PostLoginMetricsRecorderTest&) =
      delete;
  ~PostLoginMetricsRecorderTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    metrics_recorder_ = std::make_unique<PostLoginMetricsRecorder>(
        /*login_unlock_throughput_recorder=*/nullptr);
  }

  void EnableTabletMode(bool enable) {
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(enable);
  }

 protected:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<PostLoginMetricsRecorder> metrics_recorder_;
};

TEST_F(PostLoginMetricsRecorderTest, ReportLoggedInStateChanged) {
  constexpr char kLoggedInStateChanged[] = "Ash.Login.LoggedInStateChanged";

  histogram_tester_->ExpectTotalCount(kLoggedInStateChanged, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnUserLoggedIn(origin + duration,
                                    /*is_ash_restarted=*/false,
                                    /*is_regular_user_or_owner=*/true);

  histogram_tester_->ExpectTotalCount(kLoggedInStateChanged, 1);
  histogram_tester_->ExpectTimeBucketCount(kLoggedInStateChanged, duration, 1);
}

TEST_F(PostLoginMetricsRecorderTest, ReportAllShelfIconsLoaded) {
  constexpr char kAllShelfIconsLoaded[] =
      "Ash.LoginSessionRestore.AllShelfIconsLoaded";

  histogram_tester_->ExpectTotalCount(kAllShelfIconsLoaded, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnAllExpectedShelfIconLoaded(origin + duration);

  histogram_tester_->ExpectTotalCount(kAllShelfIconsLoaded, 1);
  histogram_tester_->ExpectTimeBucketCount(kAllShelfIconsLoaded, duration, 1);
}

TEST_F(PostLoginMetricsRecorderTest, ReportAllBrowserWindowsCreated) {
  constexpr char kAllBrowserWindowsCreated[] =
      "Ash.LoginSessionRestore.AllBrowserWindowsCreated";

  histogram_tester_->ExpectTotalCount(kAllBrowserWindowsCreated, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnAllBrowserWindowsCreated(origin + duration);

  histogram_tester_->ExpectTotalCount(kAllBrowserWindowsCreated, 1);
  histogram_tester_->ExpectTimeBucketCount(kAllBrowserWindowsCreated, duration,
                                           1);
}

TEST_F(PostLoginMetricsRecorderTest, ReportAllBrowserWindowsShown) {
  constexpr char kAllBrowserWindowsShown[] =
      "Ash.LoginSessionRestore.AllBrowserWindowsShown";

  histogram_tester_->ExpectTotalCount(kAllBrowserWindowsShown, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnAllBrowserWindowsShown(origin + duration);

  histogram_tester_->ExpectTotalCount(kAllBrowserWindowsShown, 1);
  histogram_tester_->ExpectTimeBucketCount(kAllBrowserWindowsShown, duration,
                                           1);
}

TEST_F(PostLoginMetricsRecorderTest, ReportAllBrowserWindowsPresented) {
  constexpr char kAllBrowserWindowsPresented[] =
      "Ash.LoginSessionRestore.AllBrowserWindowsPresented";

  histogram_tester_->ExpectTotalCount(kAllBrowserWindowsPresented, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnAllBrowserWindowsPresented(origin + duration);

  histogram_tester_->ExpectTotalCount(kAllBrowserWindowsPresented, 1);
  histogram_tester_->ExpectTimeBucketCount(kAllBrowserWindowsPresented,
                                           duration, 1);
}

TEST_F(PostLoginMetricsRecorderTest, ReportShelfLoginAnimationEnd) {
  constexpr char kShelfLoginAnimationEnd[] =
      "Ash.LoginSessionRestore.ShelfLoginAnimationEnd";

  histogram_tester_->ExpectTotalCount(kShelfLoginAnimationEnd, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnShelfAnimationFinished(origin + duration);

  histogram_tester_->ExpectTotalCount(kShelfLoginAnimationEnd, 1);
  histogram_tester_->ExpectTimeBucketCount(kShelfLoginAnimationEnd, duration,
                                           1);
}

TEST_F(PostLoginMetricsRecorderTest, ReportArcUiAvaiableTime) {
  constexpr char kArcUiAvailableAfterLoginDuration[] =
      "Ash.Login.ArcUiAvailableAfterLogin.Duration";

  histogram_tester_->ExpectTotalCount(kArcUiAvailableAfterLoginDuration, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnArcUiReady(origin + duration);

  histogram_tester_->ExpectTotalCount(kArcUiAvailableAfterLoginDuration, 1);
  histogram_tester_->ExpectTimeBucketCount(kArcUiAvailableAfterLoginDuration,
                                           duration, 1);
}

TEST_F(PostLoginMetricsRecorderTest, ReportBootTimeLogin4) {
  constexpr char kBootTimeLogin4[] = "BootTime.Login4";

  histogram_tester_->ExpectTotalCount(kBootTimeLogin4, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnShelfIconsLoadedAndSessionRestoreDone(origin + duration);

  histogram_tester_->ExpectTotalCount(kBootTimeLogin4, 1);
  histogram_tester_->ExpectTimeBucketCount(kBootTimeLogin4, duration, 1);
}

TEST_F(PostLoginMetricsRecorderTest, ReportBootTimeLogin3) {
  constexpr char kBootTimeLogin3[] = "BootTime.Login3";

  histogram_tester_->ExpectTotalCount(kBootTimeLogin3, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnShelfAnimationAndCompositorAnimationDone(origin +
                                                                duration);

  histogram_tester_->ExpectTotalCount(kBootTimeLogin3, 1);
  histogram_tester_->ExpectTimeBucketCount(kBootTimeLogin3, duration, 1);
}

class PostLoginMetricsRecorderAnimationThroughputMetricsTest
    : public PostLoginMetricsRecorderTest,
      public testing::WithParamInterface<bool> {};

// Boolean parameter controls tablet mode.
INSTANTIATE_TEST_SUITE_P(All,
                         PostLoginMetricsRecorderAnimationThroughputMetricsTest,
                         testing::Bool());

TEST_P(PostLoginMetricsRecorderAnimationThroughputMetricsTest,
       ReportAnimationThroughputMetrics) {
  EnableTabletMode(GetParam());

  const std::string kSuffix = GetParam() ? "TabletMode" : "ClamshellMode";
  const std::string kSmoothness = "Ash.LoginAnimation.Smoothness." + kSuffix;
  const std::string kJank = "Ash.LoginAnimation.Jank." + kSuffix;
  const std::string kDuration = "Ash.LoginAnimation.Duration2." + kSuffix;

  histogram_tester_->ExpectTotalCount(kSmoothness, 0);
  histogram_tester_->ExpectTotalCount(kJank, 0);
  histogram_tester_->ExpectTotalCount(kDuration, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real

  cc::FrameSequenceMetrics::CustomReportData data;
  data.frames_expected_v3 = 1;
  data.frames_dropped_v3 = 0;
  data.jank_count_v3 = 0;

  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnCompositorAnimationFinished(origin + duration, data);

  histogram_tester_->ExpectTotalCount(kSmoothness, 1);
  histogram_tester_->ExpectTotalCount(kJank, 1);
  histogram_tester_->ExpectTotalCount(kDuration, 1);
  histogram_tester_->ExpectTimeBucketCount(kDuration, duration, 1);
}

}  // namespace ash
