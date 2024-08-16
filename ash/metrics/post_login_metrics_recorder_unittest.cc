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
#include "chromeos/ash/components/metrics/login_event_recorder.h"

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

  void DisableDisplay() {
    const std::vector<display::ManagedDisplayInfo> empty;
    display_manager()->OnNativeDisplaysChanged(empty);
    EXPECT_EQ(1U, display_manager()->GetNumDisplays());
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

TEST_F(PostLoginMetricsRecorderTest, ReportAllBrowserWindowsCreated) {
  constexpr char kAllBrowserWindowsCreated[] =
      "Ash.LoginSessionRestore.AllBrowserWindowsCreated";
  constexpr char kLoginPerfAllBrowserWindowsCreated[] =
      "Ash.LoginPerf.AutoRestore.AllBrowserWindowsCreated";

  histogram_tester_->ExpectTotalCount(kAllBrowserWindowsCreated, 0);
  histogram_tester_->ExpectTotalCount(kLoginPerfAllBrowserWindowsCreated, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnSessionRestoreDataLoaded(origin,
                                                /*restore_automatically=*/true);
  metrics_recorder_->OnAllBrowserWindowsCreated(origin + duration);

  histogram_tester_->ExpectTotalCount(kAllBrowserWindowsCreated, 1);
  histogram_tester_->ExpectTimeBucketCount(kAllBrowserWindowsCreated, duration,
                                           1);

  histogram_tester_->ExpectTotalCount(kLoginPerfAllBrowserWindowsCreated, 1);
  histogram_tester_->ExpectTimeBucketCount(kLoginPerfAllBrowserWindowsCreated,
                                           duration, 1);
}

TEST_F(PostLoginMetricsRecorderTest, ReportAllBrowserWindowsShown) {
  constexpr char kAllBrowserWindowsShown[] =
      "Ash.LoginSessionRestore.AllBrowserWindowsShown";
  constexpr char kLoginPerfAllBrowserWindowsShown[] =
      "Ash.LoginPerf.AutoRestore.AllBrowserWindowsShown";

  histogram_tester_->ExpectTotalCount(kAllBrowserWindowsShown, 0);
  histogram_tester_->ExpectTotalCount(kLoginPerfAllBrowserWindowsShown, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnSessionRestoreDataLoaded(origin,
                                                /*restore_automatically=*/true);
  metrics_recorder_->OnAllBrowserWindowsShown(origin + duration);

  histogram_tester_->ExpectTotalCount(kAllBrowserWindowsShown, 1);
  histogram_tester_->ExpectTimeBucketCount(kAllBrowserWindowsShown, duration,
                                           1);

  histogram_tester_->ExpectTotalCount(kLoginPerfAllBrowserWindowsShown, 1);
  histogram_tester_->ExpectTimeBucketCount(kLoginPerfAllBrowserWindowsShown,
                                           duration, 1);
}

TEST_F(PostLoginMetricsRecorderTest, ReportAllBrowserWindowsPresented) {
  constexpr char kAllBrowserWindowsPresented[] =
      "Ash.LoginSessionRestore.AllBrowserWindowsPresented";
  constexpr char kLoginPerfAllBrowserWindowsPresented[] =
      "Ash.LoginPerf.AutoRestore.AllBrowserWindowsPresented";

  histogram_tester_->ExpectTotalCount(kAllBrowserWindowsPresented, 0);
  histogram_tester_->ExpectTotalCount(kLoginPerfAllBrowserWindowsPresented, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnSessionRestoreDataLoaded(origin,
                                                /*restore_automatically=*/true);
  metrics_recorder_->OnAllBrowserWindowsPresented(origin + duration);

  histogram_tester_->ExpectTotalCount(kAllBrowserWindowsPresented, 1);
  histogram_tester_->ExpectTimeBucketCount(kAllBrowserWindowsPresented,
                                           duration, 1);

  histogram_tester_->ExpectTotalCount(kLoginPerfAllBrowserWindowsPresented, 1);
  histogram_tester_->ExpectTimeBucketCount(kLoginPerfAllBrowserWindowsPresented,
                                           duration, 1);
}

TEST_F(PostLoginMetricsRecorderTest,
       ShouldNotReportAllBrowserWindowsPresentedIfNoDisplayIsActive) {
  DisableDisplay();

  constexpr char kAllBrowserWindowsPresented[] =
      "Ash.LoginSessionRestore.AllBrowserWindowsPresented";
  constexpr char kLoginPerfAllBrowserWindowsPresented[] =
      "Ash.LoginPerf.AutoRestore.AllBrowserWindowsPresented";

  histogram_tester_->ExpectTotalCount(kAllBrowserWindowsPresented, 0);
  histogram_tester_->ExpectTotalCount(kLoginPerfAllBrowserWindowsPresented, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnSessionRestoreDataLoaded(origin,
                                                /*restore_automatically=*/true);
  metrics_recorder_->OnAllBrowserWindowsPresented(origin + duration);

  histogram_tester_->ExpectTotalCount(kAllBrowserWindowsPresented, 0);
  histogram_tester_->ExpectTotalCount(kLoginPerfAllBrowserWindowsPresented, 0);
}

class PostLoginMetricsRecorderSessionRestoreTest
    : public PostLoginMetricsRecorderTest,
      public testing::WithParamInterface</*auto_restore=*/bool> {};

// Boolean parameter controls session restore mode.
INSTANTIATE_TEST_SUITE_P(All,
                         PostLoginMetricsRecorderSessionRestoreTest,
                         /*auto_restore=*/testing::Bool());

TEST_P(PostLoginMetricsRecorderSessionRestoreTest, ReportAllShelfIconsLoaded) {
  const bool auto_restore = GetParam();

  constexpr char kAllShelfIconsLoaded[] =
      "Ash.LoginSessionRestore.AllShelfIconsLoaded";

  const std::string kLoginPerfAllShelfIconsLoaded =
      auto_restore ? "Ash.LoginPerf.AutoRestore.AllShelfIconsLoaded"
                   : "Ash.LoginPerf.ManualRestore.AllShelfIconsLoaded";

  histogram_tester_->ExpectTotalCount(kAllShelfIconsLoaded, 0);
  histogram_tester_->ExpectTotalCount(kLoginPerfAllShelfIconsLoaded, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnSessionRestoreDataLoaded(origin, auto_restore);
  metrics_recorder_->OnAllExpectedShelfIconLoaded(origin + duration);

  histogram_tester_->ExpectTotalCount(kAllShelfIconsLoaded, 1);
  histogram_tester_->ExpectTimeBucketCount(kAllShelfIconsLoaded, duration, 1);

  histogram_tester_->ExpectTotalCount(kLoginPerfAllShelfIconsLoaded, 1);
  histogram_tester_->ExpectTimeBucketCount(kLoginPerfAllShelfIconsLoaded,
                                           duration, 1);
}

TEST_P(PostLoginMetricsRecorderSessionRestoreTest,
       ReportShelfLoginAnimationEnd) {
  const bool auto_restore = GetParam();

  constexpr char kShelfLoginAnimationEnd[] =
      "Ash.LoginSessionRestore.ShelfLoginAnimationEnd";
  const std::string kLoginPerfShelfLoginAnimationEnd =
      auto_restore ? "Ash.LoginPerf.AutoRestore.ShelfLoginAnimationEnd"
                   : "Ash.LoginPerf.ManualRestore.ShelfLoginAnimationEnd";

  histogram_tester_->ExpectTotalCount(kShelfLoginAnimationEnd, 0);
  histogram_tester_->ExpectTotalCount(kLoginPerfShelfLoginAnimationEnd, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnSessionRestoreDataLoaded(origin, auto_restore);
  metrics_recorder_->OnShelfAnimationFinished(origin + duration);

  histogram_tester_->ExpectTotalCount(kShelfLoginAnimationEnd, 1);
  histogram_tester_->ExpectTimeBucketCount(kShelfLoginAnimationEnd, duration,
                                           1);

  histogram_tester_->ExpectTotalCount(kLoginPerfShelfLoginAnimationEnd, 1);
  histogram_tester_->ExpectTimeBucketCount(kLoginPerfShelfLoginAnimationEnd,
                                           duration, 1);
}

TEST_P(PostLoginMetricsRecorderSessionRestoreTest, ReportArcUiAvaiableTime) {
  const bool auto_restore = GetParam();

  constexpr char kArcUiAvailableAfterLoginDuration[] =
      "Ash.Login.ArcUiAvailableAfterLogin.Duration";
  const std::string kLoginPerfArcUiAvailableAfterLogin =
      auto_restore ? "Ash.LoginPerf.AutoRestore.ArcUiAvailableAfterLogin"
                   : "Ash.LoginPerf.ManualRestore.ArcUiAvailableAfterLogin";

  histogram_tester_->ExpectTotalCount(kArcUiAvailableAfterLoginDuration, 0);
  histogram_tester_->ExpectTotalCount(kLoginPerfArcUiAvailableAfterLogin, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnSessionRestoreDataLoaded(origin, auto_restore);
  metrics_recorder_->OnArcUiReady(origin + duration);

  histogram_tester_->ExpectTotalCount(kArcUiAvailableAfterLoginDuration, 1);
  histogram_tester_->ExpectTimeBucketCount(kArcUiAvailableAfterLoginDuration,
                                           duration, 1);

  histogram_tester_->ExpectTotalCount(kLoginPerfArcUiAvailableAfterLogin, 1);
  histogram_tester_->ExpectTimeBucketCount(kLoginPerfArcUiAvailableAfterLogin,
                                           duration, 1);
}

TEST_P(PostLoginMetricsRecorderSessionRestoreTest, ReportBootTimeLogin4) {
  const bool auto_restore = GetParam();

  constexpr char kBootTimeLogin4[] = "BootTime.Login4";

  histogram_tester_->ExpectTotalCount(kBootTimeLogin4, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnSessionRestoreDataLoaded(origin, auto_restore);
  metrics_recorder_->OnShelfIconsLoadedAndSessionRestoreDone(origin + duration);

  histogram_tester_->ExpectTotalCount(kBootTimeLogin4, 1);
  histogram_tester_->ExpectTimeBucketCount(kBootTimeLogin4, duration, 1);
}

TEST_P(PostLoginMetricsRecorderSessionRestoreTest, ReportBootTimeLogin3) {
  const bool auto_restore = GetParam();

  constexpr char kBootTimeLogin3[] = "BootTime.Login3";

  histogram_tester_->ExpectTotalCount(kBootTimeLogin3, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnSessionRestoreDataLoaded(origin, auto_restore);
  metrics_recorder_->OnShelfAnimationAndCompositorAnimationDone(origin +
                                                                duration);

  histogram_tester_->ExpectTotalCount(kBootTimeLogin3, 1);
  histogram_tester_->ExpectTimeBucketCount(kBootTimeLogin3, duration, 1);
}

TEST_P(PostLoginMetricsRecorderSessionRestoreTest, ReportTotalDuration) {
  const bool auto_restore = GetParam();

  const std::string kLoginPerfTotalDuration =
      auto_restore ? "Ash.LoginPerf.AutoRestore.TotalDuration"
                   : "Ash.LoginPerf.ManualRestore.TotalDuration";

  histogram_tester_->ExpectTotalCount(kLoginPerfTotalDuration, 0);

  // Assume "LoginStarted" event is recorded in LoginEventRecorder. Otherwise,
  // "TotalDuration" won't be reported.
  LoginEventRecorder::Get()->AddLoginTimeMarker("LoginStarted",
                                                /*send_to_uma=*/false,
                                                /*write_to_file=*/false);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real
  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnSessionRestoreDataLoaded(origin, auto_restore);
  metrics_recorder_->OnShelfAnimationAndCompositorAnimationDone(origin +
                                                                duration);

  histogram_tester_->ExpectTotalCount(kLoginPerfTotalDuration, 1);
}

class PostLoginMetricsRecorderAnimationThroughputMetricsTest
    : public PostLoginMetricsRecorderTest,
      public testing::WithParamInterface<
          std::tuple</*table_mode=*/bool, /*auto_restore=*/bool>> {};

// Boolean parameter controls tablet mode.
INSTANTIATE_TEST_SUITE_P(All,
                         PostLoginMetricsRecorderAnimationThroughputMetricsTest,
                         testing::Combine(/*tablet_mode=*/testing::Bool(),
                                          /*auto_restore=*/testing::Bool()));

TEST_P(PostLoginMetricsRecorderAnimationThroughputMetricsTest,
       ReportAnimationThroughputMetrics) {
  const bool tablet_mode = std::get<0>(GetParam());
  const bool auto_restore = std::get<1>(GetParam());

  EnableTabletMode(tablet_mode);

  const std::string kSuffix = tablet_mode ? "TabletMode" : "ClamshellMode";
  const std::string kSmoothness = "Ash.LoginAnimation.Smoothness." + kSuffix;
  const std::string kJank = "Ash.LoginAnimation.Jank." + kSuffix;
  const std::string kDuration = "Ash.LoginAnimation.Duration2." + kSuffix;

  const std::string kRestoreMode =
      auto_restore ? "AutoRestore" : "ManualRestore";
  const std::string kPostLoginAnimationSmoothness =
      "Ash.LoginPerf." + kRestoreMode + ".PostLoginAnimation.Smoothness." +
      kSuffix;
  const std::string kPostLoginAnimationJank =
      "Ash.LoginPerf." + kRestoreMode + ".PostLoginAnimation.Jank." + kSuffix;
  const std::string kPostLoginAnimationDuration =
      "Ash.LoginPerf." + kRestoreMode + ".PostLoginAnimation.Duration." +
      kSuffix;

  histogram_tester_->ExpectTotalCount(kSmoothness, 0);
  histogram_tester_->ExpectTotalCount(kJank, 0);
  histogram_tester_->ExpectTotalCount(kDuration, 0);

  histogram_tester_->ExpectTotalCount(kPostLoginAnimationSmoothness, 0);
  histogram_tester_->ExpectTotalCount(kPostLoginAnimationJank, 0);
  histogram_tester_->ExpectTotalCount(kPostLoginAnimationDuration, 0);

  const base::TimeTicks origin = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(100);  // not real

  cc::FrameSequenceMetrics::CustomReportData data;
  data.frames_expected_v3 = 1;
  data.frames_dropped_v3 = 0;
  data.jank_count_v3 = 0;

  metrics_recorder_->OnAuthSuccess(origin);
  metrics_recorder_->OnSessionRestoreDataLoaded(origin, auto_restore);
  metrics_recorder_->OnCompositorAnimationFinished(origin + duration, data);

  histogram_tester_->ExpectTotalCount(kSmoothness, 1);
  histogram_tester_->ExpectTotalCount(kJank, 1);
  histogram_tester_->ExpectTotalCount(kDuration, 1);
  histogram_tester_->ExpectTimeBucketCount(kDuration, duration, 1);

  histogram_tester_->ExpectTotalCount(kPostLoginAnimationSmoothness, 1);
  histogram_tester_->ExpectTotalCount(kPostLoginAnimationJank, 1);
  histogram_tester_->ExpectTotalCount(kPostLoginAnimationDuration, 1);
  histogram_tester_->ExpectTimeBucketCount(kPostLoginAnimationDuration,
                                           duration, 1);
}

}  // namespace ash
