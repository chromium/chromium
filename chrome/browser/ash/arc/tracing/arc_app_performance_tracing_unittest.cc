// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing.h"

#include <memory>

#include "ash/components/arc/test/arc_task_window_builder.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_handler.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_shell_surface.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_session.h"
#include "chrome/browser/ash/arc/tracing/test/arc_app_performance_tracing_test_helper.h"
#include "chrome/browser/ash/arc/tracing/uma_perf_reporting.h"
#include "chrome/browser/ash/arc/window_predictor/window_predictor_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/app_restore/app_restore_data.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

namespace arc {

namespace {

constexpr char kFocusAppPackage[] = "focus.app.package";
constexpr char kFocusAppActivity[] = "focus.app.package.Activity";
constexpr char kFocusCategory[] = "OnlineGame";
constexpr char kNonFocusAppPackage[] = "nonfocus.app.package";
constexpr char kNonFocusAppActivity[] = "nonfocus.app.package.Activity";

// For 20 frames.
constexpr base::TimeDelta kTestPeriod = base::Seconds(1) / (60 / 20);

constexpr int kMillisecondsToFirstFrame = 500;

struct Application {
  const char* package;
  const char* activity;
  const char* name;
};

std::string GetStatisticName(const std::string& name,
                             const std::string& category) {
  return base::StringPrintf("Arc.Runtime.Performance.%s.%s", name.c_str(),
                            category.c_str());
}

// Reads statistics value from histogram.
int64_t ReadStatistics(const std::string& name, const std::string& category) {
  const base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(GetStatisticName(name, category));
  DCHECK(histogram);

  std::unique_ptr<base::HistogramSamples> samples =
      histogram->SnapshotFinalDelta();
  DCHECK(samples.get());
  DCHECK_EQ(1, samples->TotalCount());
  return samples->sum();
}

// Reads focus statistics value from histogram
int64_t ReadFocusStatistics(const std::string& name) {
  return ReadStatistics(name, kFocusCategory);
}

constexpr std::initializer_list<Application> kApplications{
    {"com.mojang.minecraftpe", "com.mojang.minecraftpe.MainActivity",
     "MinecraftConsumerEdition"},
    {"com.innersloth.spacemafia",
     "com.innersloth.spacemafia.EosUnityPlayerActivity", "AmongUs"},
    {"com.plarium.raidlegends", "com.plarium.unity_app.UnityMainActivity",
     "RaidLegends"},
    {"com.valvesoftware.underlords", "com.valvesoftware.underlords.applauncher",
     "Underlords"},
    {"com.tocaboca.tocalifeworld", "com.tocaboca.activity.TocaBocaMainActivity",
     "TocaLife"},
    {"com.king.candycrushsaga",
     "com.king.candycrushsaga.CandyCrushSagaActivity", "CandyCrush"},
    {"com.playrix.homescapes", "com.playrix.homescapes.GoogleActivity",
     "Homescapes"},
    {"com.ea.gp.fifamobile", "com.ea.gp.fifamobile.FifaMainActivity",
     "FIFAMobile"},
    {"com.miHoYo.GenshinImpact", "com.miHoYo.GetMobileInfo.MainActivity",
     "GenshinImpact"},
};

}  // namespace

// BrowserWithTestWindowTest contains required ash/shell support that would not
// be possible to use directly.
class ArcAppPerformanceTracingTest : public BrowserWithTestWindowTest {
 public:
  ArcAppPerformanceTracingTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ArcAppPerformanceTracingTest(const ArcAppPerformanceTracingTest&) = delete;
  ArcAppPerformanceTracingTest& operator=(const ArcAppPerformanceTracingTest&) =
      delete;

  ~ArcAppPerformanceTracingTest() override = default;

  // testing::Test:
  void SetUp() override {
    ForgetAppMetrics(kFocusCategory);
    for (const auto& app : kApplications) {
      ForgetAppMetrics(app.name);
    }

    BrowserWithTestWindowTest::SetUp();

    arc_test_.SetUp(profile());
    tracing_helper_.SetUp(profile());

    ArcAppPerformanceTracing::SetFocusAppForTesting(
        kFocusAppPackage, kFocusAppActivity, kFocusCategory);

    UmaPerfReporting::SetTracingPeriodForTesting(kTestPeriod);
  }

  void TearDown() override {
    shell_root_surface_.reset();

    tracing_helper_.TearDown();
    arc_test_.TearDown();

    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile* CreateProfile(const std::string& profile_name) override {
    auto* profile = BrowserWithTestWindowTest::CreateProfile(profile_name);
    auto* user = user_manager()->FindUserAndModify(
        AccountId::FromUserEmail(profile_name));
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, profile);
    return profile;
  }

 protected:
  int64_t task_id = 1;
  std::unique_ptr<exo::Surface> shell_root_surface_;

  void ForgetAppMetrics(const std::string& category) {
    base::StatisticsRecorder::ForgetHistogramForTesting(
        GetStatisticName("FPS2", category));
    base::StatisticsRecorder::ForgetHistogramForTesting(
        GetStatisticName("PerceivedFPS2", category));
    base::StatisticsRecorder::ForgetHistogramForTesting(
        GetStatisticName("CommitDeviation2", category));
    base::StatisticsRecorder::ForgetHistogramForTesting(
        GetStatisticName("PresentDeviation2", category));
    base::StatisticsRecorder::ForgetHistogramForTesting(
        GetStatisticName("RenderQuality2", category));
    base::StatisticsRecorder::ForgetHistogramForTesting(
        GetStatisticName("JanksPerMinute2", category));
    base::StatisticsRecorder::ForgetHistogramForTesting(
        GetStatisticName("JanksPercentage2", category));
  }

  // Ensures that tracing is ready to begin, which means up to the point that
  // waiting for the delayed start has just begun.
  views::Widget* PrepareArcAppTracing(const std::string& package_name,
                                      const std::string& activity_name) {
    shell_root_surface_ = std::make_unique<exo::Surface>();
    views::Widget* arc_widget =
        ArcTaskWindowBuilder()
            .SetTaskId(task_id)
            .SetShellRootSurface(shell_root_surface_.get())
            .BuildOwnedByNativeWidget();
    arc_widget->Show();
    DCHECK(arc_widget->GetNativeWindow());

    tracing_helper().GetTracing()->OnWindowActivated(
        wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
        arc_widget->GetNativeWindow(), arc_widget->GetNativeWindow());
    tracing_helper().GetTracing()->OnTaskCreated(
        task_id /* task_Id */, package_name, activity_name,
        std::string() /* intent */, 0 /* session_id */);
    task_id++;
    DCHECK(tracing_helper().GetTracingSession());
    return arc_widget;
  }

  // Ensures that tracing is active.
  views::Widget* StartArcAppTracing(const std::string& package_name,
                                    const std::string& activity_name) {
    auto* arc_widget = PrepareArcAppTracing(package_name, activity_name);
    tracing_helper().GetTracingSession()->FireTimerForTesting();
    DCHECK(tracing_helper().GetTracingSession());
    DCHECK(tracing_helper().GetTracingSession()->tracing_active());
    DCHECK(tracing_helper().GetTracingSession()->HasPresentFrames());
    return arc_widget;
  }

  views::Widget* PrepareArcFocusAppTracing() {
    return PrepareArcAppTracing(kFocusAppPackage, kFocusAppActivity);
  }

  ArcAppPerformanceTracingTestHelper& tracing_helper() {
    return tracing_helper_;
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {TestingProfile::TestingFactory{
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::TestSyncService>();
            })}};
  }

 private:
  ArcAppPerformanceTracingTestHelper tracing_helper_;
  ArcAppTest arc_test_{ArcAppTest::UserManagerMode::kDoNothing};
};

TEST_F(ArcAppPerformanceTracingTest, TracingScheduled) {
  constexpr int kTaskId1 = 999;
  constexpr int kTaskId2 = 87111;

  // By default it is inactive.
  EXPECT_FALSE(tracing_helper().GetTracingSession());

  // Report task first.
  tracing_helper().GetTracing()->OnTaskCreated(
      kTaskId1, kFocusAppPackage, kFocusAppActivity, std::string() /* intent */,
      0 /* session_id */);
  EXPECT_FALSE(tracing_helper().GetTracingSession());

  // Create window second.
  exo::Surface shell_root_surface1;

  views::Widget* const arc_widget1 =
      ArcTaskWindowBuilder()
          .SetTaskId(kTaskId1)
          .SetShellRootSurface(&shell_root_surface1)
          .BuildOwnedByNativeWidget();
  arc_widget1->Show();

  ASSERT_TRUE(arc_widget1);
  ASSERT_TRUE(arc_widget1->GetNativeWindow());
  tracing_helper().GetTracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget1->GetNativeWindow(), nullptr /* lost_active */);
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  // Scheduled but not started.
  EXPECT_FALSE(tracing_helper().GetTracingSession()->tracing_active());
  EXPECT_FALSE(tracing_helper().GetTracingSession()->HasPresentFrames());

  // Test reverse order, create window first.
  exo::Surface shell_root_surface2;
  views::Widget* const arc_widget2 =
      ArcTaskWindowBuilder()
          .SetTaskId(kTaskId2)
          .SetShellRootSurface(&shell_root_surface2)
          .BuildOwnedByNativeWidget();
  arc_widget2->Show();

  ASSERT_TRUE(arc_widget2->GetNativeWindow());
  tracing_helper().GetTracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget2->GetNativeWindow(), arc_widget2->GetNativeWindow());
  // Task is not yet created, this also resets previous tracing.
  EXPECT_FALSE(tracing_helper().GetTracingSession());
  // Report task second.
  tracing_helper().GetTracing()->OnTaskCreated(
      kTaskId2, kFocusAppPackage, kFocusAppActivity, std::string() /* intent */,
      0 /* session_id */);
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  // Scheduled but not started.
  EXPECT_FALSE(tracing_helper().GetTracingSession()->tracing_active());
  EXPECT_FALSE(tracing_helper().GetTracingSession()->HasPresentFrames());
  arc_widget1->Close();
  arc_widget2->Close();
}

TEST_F(ArcAppPerformanceTracingTest, TracingNotScheduledForNonFocusApp) {
  exo::Surface shell_root_surface;
  views::Widget* const arc_widget =
      ArcTaskWindowBuilder()
          .SetShellRootSurface(&shell_root_surface)
          .BuildOwnedByNativeWidget();
  arc_widget->Show();

  ASSERT_TRUE(arc_widget->GetNativeWindow());
  tracing_helper().GetTracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget->GetNativeWindow(), arc_widget->GetNativeWindow());
  EXPECT_FALSE(tracing_helper().GetTracingSession());
  tracing_helper().GetTracing()->OnTaskCreated(
      1 /* task_Id */, kNonFocusAppPackage, kNonFocusAppActivity,
      std::string() /* intent */, 0 /* session_id */);
  EXPECT_FALSE(tracing_helper().GetTracingSession());
  arc_widget->Close();
}

constexpr base::TimeDelta kNormalInterval = base::Seconds(1) / 60;

TEST_F(ArcAppPerformanceTracingTest, TracingStoppedOnIdle) {
  views::Widget* const arc_widget = PrepareArcFocusAppTracing();
  tracing_helper().GetTracingSession()->FireTimerForTesting();

  tracing_helper().Commit(shell_root_surface_.get(), PresentType::kSuccessful);

  // Expected updates;
  tracing_helper().AdvanceTickCount(kNormalInterval);
  tracing_helper().Commit(shell_root_surface_.get(), PresentType::kSuccessful);
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_TRUE(tracing_helper().GetTracingSession()->tracing_active());
  EXPECT_TRUE(tracing_helper().GetTracingSession()->HasPresentFrames());

  tracing_helper().AdvanceTickCount(kNormalInterval * 5);
  tracing_helper().Commit(shell_root_surface_.get(), PresentType::kSuccessful);
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_TRUE(tracing_helper().GetTracingSession()->tracing_active());
  EXPECT_TRUE(tracing_helper().GetTracingSession()->HasPresentFrames());

  // Ten or more missed frames is considered a timeout.
  tracing_helper().AdvanceTickCount(kNormalInterval * 10);
  tracing_helper().Commit(shell_root_surface_.get(), PresentType::kSuccessful);
  // Tracing is rescheduled and no longer active.
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_FALSE(tracing_helper().GetTracingSession()->tracing_active());
  EXPECT_FALSE(tracing_helper().GetTracingSession()->HasPresentFrames());
  arc_widget->Close();
}

TEST_F(ArcAppPerformanceTracingTest, TracingStoppedOnIdleBeforeFirstFrame) {
  views::Widget* const arc_widget = PrepareArcFocusAppTracing();
  tracing_helper().GetTracingSession()->FireTimerForTesting();

  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_TRUE(tracing_helper().GetTracingSession()->tracing_active());
  EXPECT_TRUE(tracing_helper().GetTracingSession()->HasPresentFrames());

  // Ten or more missed frames is considered a timeout.
  tracing_helper().AdvanceTickCount(kNormalInterval * 10);
  tracing_helper().Commit(shell_root_surface_.get(), PresentType::kSuccessful);

  // Tracing is rescheduled and no longer active.
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_FALSE(tracing_helper().GetTracingSession()->tracing_active());
  EXPECT_FALSE(tracing_helper().GetTracingSession()->HasPresentFrames());

  // Later commits (at normal intervals) will be ignored and not cause problems.
  // We do more than one commit just to be reasonably sure we have given a buggy
  // implementation enough chances to mess up.
  for (int i = 0; i < 3; i++) {
    tracing_helper().AdvanceTickCount(kNormalInterval);
    tracing_helper().Commit(shell_root_surface_.get(),
                            PresentType::kSuccessful);
  }

  // Tracing still not active.
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_FALSE(tracing_helper().GetTracingSession()->tracing_active());
  EXPECT_FALSE(tracing_helper().GetTracingSession()->HasPresentFrames());

  arc_widget->Close();
}

TEST_F(ArcAppPerformanceTracingTest, StatisticsReported) {
  views::Widget* arc_widget = PrepareArcFocusAppTracing();
  EXPECT_EQ(tracing_helper().GetTracingSession()->timer_delay_for_testing(),
            kInitTracingDelay);
  tracing_helper().GetTracingSession()->FireTimerForTesting();

  tracing_helper().PlayDefaultSequence(shell_root_surface_.get());
  tracing_helper().FireTimerForTesting();
  EXPECT_EQ(45L, ReadFocusStatistics("FPS2"));
  EXPECT_EQ(48L, ReadFocusStatistics("PerceivedFPS2"));
  EXPECT_EQ(216L, ReadFocusStatistics("CommitDeviation2"));
  EXPECT_EQ(216L, ReadFocusStatistics("PresentDeviation2"));
  EXPECT_EQ(48L, ReadFocusStatistics("RenderQuality2"));
  EXPECT_EQ(0L, ReadFocusStatistics("JanksPerMinute2"));
  EXPECT_EQ(0L, ReadFocusStatistics("JanksPercentage2"));
  arc_widget->Close();

  arc_widget = PrepareArcFocusAppTracing();
  EXPECT_EQ(tracing_helper().GetTracingSession()->timer_delay_for_testing(),
            kNextTracingDelay);
  arc_widget->Close();
}

TEST_F(ArcAppPerformanceTracingTest, DifferingFPSTypes) {
  // 32 ms interval is 31.25 FPS.
  constexpr base::TimeDelta kCommitInterval = base::Milliseconds(32);
  constexpr int kFrameCount = 100;

  UmaPerfReporting::SetTracingPeriodForTesting(kFrameCount * kCommitInterval);
  views::Widget* arc_widget = PrepareArcFocusAppTracing();

  tracing_helper().GetTracingSession()->FireTimerForTesting();
  for (int frame = 0; frame < kFrameCount; frame++) {
    tracing_helper().AdvanceTickCount(kCommitInterval);
    // One frame of every ten is missed, so perceived FPS is 28.125.
    tracing_helper().Commit(
        shell_root_surface_.get(),
        (frame % 10) == 0 ? PresentType::kDiscarded : PresentType::kSuccessful);
  }

  tracing_helper().FireTimerForTesting();
  EXPECT_EQ(31L, ReadFocusStatistics("FPS2"));
  EXPECT_EQ(28L, ReadFocusStatistics("PerceivedFPS2"));
  arc_widget->Close();
}

TEST_F(ArcAppPerformanceTracingTest, ApplicationStatisticsReported) {
  for (const Application& application : kApplications) {
    views::Widget* const arc_widget =
        StartArcAppTracing(application.package, application.activity);

    tracing_helper().PlayDefaultSequence(shell_root_surface_.get());
    tracing_helper().FireTimerForTesting();
    EXPECT_EQ(45L, ReadStatistics("FPS2", application.name));
    EXPECT_EQ(48L, ReadStatistics("PerceivedFPS2", application.name));
    EXPECT_EQ(216L, ReadStatistics("CommitDeviation2", application.name));
    EXPECT_EQ(216L, ReadStatistics("PresentDeviation2", application.name));
    EXPECT_EQ(48L, ReadStatistics("RenderQuality2", application.name));
    EXPECT_EQ(0L, ReadStatistics("JanksPerMinute2", application.name));
    EXPECT_EQ(0L, ReadStatistics("JanksPercentage2", application.name));
    arc_widget->Close();
  }
}

TEST_F(ArcAppPerformanceTracingTest, TracingNotScheduledWhenAppSyncDisabled) {
  tracing_helper().DisableAppSync();
  exo::Surface shell_root_surface;
  views::Widget* const arc_widget =
      ArcTaskWindowBuilder()
          .SetShellRootSurface(&shell_root_surface)
          .BuildOwnedByNativeWidget();
  arc_widget->Show();
  ASSERT_TRUE(arc_widget->GetNativeWindow());
  tracing_helper().GetTracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget->GetNativeWindow(), arc_widget->GetNativeWindow());
  EXPECT_FALSE(tracing_helper().GetTracingSession());
  tracing_helper().GetTracing()->OnTaskCreated(
      1 /* task_Id */, kFocusAppPackage, kFocusAppActivity,
      std::string() /* intent */, 0 /* session_id */);
  EXPECT_FALSE(tracing_helper().GetTracingSession());
  arc_widget->Close();
}

TEST_F(ArcAppPerformanceTracingTest, TimeToFirstFrameRendered) {
  const std::string app_id =
      ArcAppListPrefs::GetAppId(kFocusAppPackage, kFocusAppActivity);
  exo::Surface shell_root_surface;
  views::Widget* const arc_widget =
      ArcTaskWindowBuilder()
          .SetShellRootSurface(&shell_root_surface)
          .BuildOwnedByNativeWidget();
  arc_widget->Show();
  DCHECK(arc_widget->GetNativeWindow());

  tracing_helper().GetTracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget->GetNativeWindow(), arc_widget->GetNativeWindow());
  tracing_helper().GetTracing()->OnTaskCreated(
      1 /* task_Id */, kFocusAppPackage, kFocusAppActivity,
      std::string() /* intent */, 0 /* session_id */);

  // No report before launch
  base::Time timestamp = base::Time::Now();
  tracing_helper().GetTracing()->HandleActiveAppRendered(timestamp);
  base::HistogramBase* histogram = base::StatisticsRecorder::FindHistogram(
      "Arc.Runtime.Performance.Generic.FirstFrameRendered");
  DCHECK(!histogram);

  // Succesful report after launch
  ArcAppListPrefs::Get(profile())->SetLaunchRequestTimeForTesting(app_id,
                                                                  timestamp);
  timestamp += base::Milliseconds(kMillisecondsToFirstFrame);
  tracing_helper().GetTracing()->HandleActiveAppRendered(timestamp);
  histogram = base::StatisticsRecorder::FindHistogram(
      "Arc.Runtime.Performance.Generic.FirstFrameRendered");
  DCHECK(histogram);

  std::unique_ptr<base::HistogramSamples> samples = histogram->SnapshotDelta();
  DCHECK(samples.get());
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(kMillisecondsToFirstFrame, samples->sum());

  // No double report
  timestamp += base::Milliseconds(kMillisecondsToFirstFrame);
  tracing_helper().GetTracing()->HandleActiveAppRendered(timestamp);

  samples = histogram->SnapshotDelta();
  DCHECK(samples.get());
  EXPECT_EQ(0, samples->TotalCount());

  arc_widget->Close();
}

// This test verifies the case when surface is destroyed before window close.
TEST_F(ArcAppPerformanceTracingTest, DestroySurface) {
  views::Widget* const arc_widget = PrepareArcFocusAppTracing();
  tracing_helper().GetTracingSession()->FireTimerForTesting();

  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_TRUE(tracing_helper().GetTracingSession()->tracing_active());
  EXPECT_TRUE(tracing_helper().GetTracingSession()->HasPresentFrames());
  exo::SetShellRootSurface(arc_widget->GetNativeWindow(), nullptr);
  shell_root_surface_.reset();
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_FALSE(tracing_helper().GetTracingSession()->tracing_active());
  EXPECT_FALSE(tracing_helper().GetTracingSession()->HasPresentFrames());

  // Try to re-active window without surface
  tracing_helper().GetTracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget->GetNativeWindow(), arc_widget->GetNativeWindow());

  EXPECT_FALSE(tracing_helper().GetTracingSession());

  arc_widget->Close();
}

TEST_F(ArcAppPerformanceTracingTest, DetachDisplayDuringTrace) {
  views::Widget* const arc_widget = PrepareArcFocusAppTracing();
  tracing_helper().GetTracingSession()->FireTimerForTesting();

  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_TRUE(tracing_helper().GetTracingSession()->tracing_active());
  EXPECT_TRUE(tracing_helper().GetTracingSession()->HasPresentFrames());

  auto* dm = ash::Shell::Get()->display_manager();
  display::test::DisplayManagerTestApi display_manager(dm);
  auto prim_info = dm->GetDisplayInfo(dm->first_display_id());
  display_manager.UpdateDisplayWithDisplayInfoList({});

  EXPECT_FALSE(tracing_helper().GetTracingSession());

  display_manager.UpdateDisplayWithDisplayInfoList({prim_info});
  EXPECT_TRUE(tracing_helper().GetTracingSession());
  arc_widget->Close();
}

TEST_F(ArcAppPerformanceTracingTest, NoTracingForArcGhostWindow) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          ash::Shell::GetPrimaryRootWindow());
  std::unique_ptr<ash::full_restore::ArcGhostWindowHandler>
      ghost_window_handler =
          std::make_unique<ash::full_restore::ArcGhostWindowHandler>();

  app_restore::AppRestoreData restore_data;
  restore_data.display_id = display.id();
  auto ghost_window = ash::full_restore::ArcGhostWindowShellSurface::Create(
      "app_id" /* app_id */, GhostWindowType::kFullRestore, 1 /* window_id */,
      gfx::Rect(10, 10, 100, 100) /* bounds */, &restore_data,
      base::BindRepeating([]() {}));
  ASSERT_TRUE(ghost_window);

  // Associate ghost window with real app.
  ghost_window->SetApplicationId("org.chromium.arc.session.1");

  // This creates window.
  ghost_window->SetSystemUiVisibility(false /* autohide */);
  ASSERT_TRUE(ghost_window->GetWidget());
  ASSERT_TRUE(ghost_window->GetWidget()->GetNativeWindow());
  ghost_window->GetWidget()->GetNativeWindow()->Show();

  tracing_helper().GetTracing()->OnTaskCreated(
      1 /* task_Id */, kFocusAppPackage, kFocusAppActivity,
      std::string() /* intent */, 0 /* session_id */);

  tracing_helper().GetTracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      ghost_window->GetWidget()->GetNativeWindow() /* gained_active */,
      nullptr /* lost_active */);

  // Ghost window should not trigger tracing sessions.
  DCHECK(!tracing_helper().GetTracingSession());
}

TEST_F(ArcAppPerformanceTracingTest, GhostWindowTurnsIntoTaskWindow) {
  // TODO(b/312215591): Use ghost window utilities to simulate the
  // transformation of a ghost window into a task window?
  constexpr int kTaskId = 9486;
  constexpr char kAppId[] = "org.chromium.arc.9486";

  // By default it is inactive.
  EXPECT_FALSE(tracing_helper().GetTracingSession());

  auto ghost_surface = std::make_unique<exo::Surface>();

  views::Widget* const widget = ArcTaskWindowBuilder()
                                    .SetTaskId(kTaskId)
                                    .SetShellRootSurface(ghost_surface.get())
                                    .BuildOwnedByNativeWidget();
  exo::SetShellApplicationId(widget->GetNativeWindow(),
                             "org.chromium.arc.session.1");
  tracing_helper().GetTracing()->OnTaskCreated(
      kTaskId, kFocusAppPackage, kFocusAppActivity, std::string() /* intent */,
      0 /* session_id */);

  exo::SetShellRootSurface(widget->GetNativeWindow(), ghost_surface.get());
  widget->Show();

  ASSERT_FALSE(tracing_helper().GetTracingSession());

  auto task_surface = std::make_unique<exo::Surface>();
  exo::SetShellRootSurface(widget->GetNativeWindow(), ghost_surface.get());
  exo::SetShellApplicationId(widget->GetNativeWindow(), kAppId);

  ASSERT_TRUE(tracing_helper().GetTracingSession());

  widget->Close();
}

}  // namespace arc
