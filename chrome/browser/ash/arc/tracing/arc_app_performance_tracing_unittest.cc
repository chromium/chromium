// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing.h"

#include <memory>

#include "ash/components/arc/test/arc_task_window_builder.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_handler.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_shell_surface.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_session.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_uma_session.h"
#include "chrome/browser/ash/arc/tracing/test/arc_app_performance_tracing_test_helper.h"
#include "chrome/browser/ash/arc/window_predictor/window_predictor_utils.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/app_restore/app_restore_data.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

#include "ui/display/display.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/native_widget_types.h"

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

}  // namespace

// BrowserWithTestWindowTest contains required ash/shell support that would not
// be possible to use directly.
class ArcAppPerformanceTracingTest : public BrowserWithTestWindowTest {
 public:
  ArcAppPerformanceTracingTest() = default;

  ArcAppPerformanceTracingTest(const ArcAppPerformanceTracingTest&) = delete;
  ArcAppPerformanceTracingTest& operator=(const ArcAppPerformanceTracingTest&) =
      delete;

  ~ArcAppPerformanceTracingTest() override = default;

  // testing::Test:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    arc_test_.SetUp(profile());
    tracing_helper_.SetUp(profile());

    ArcAppPerformanceTracing::SetFocusAppForTesting(
        kFocusAppPackage, kFocusAppActivity, kFocusCategory);

    ArcAppPerformanceTracingUmaSession::SetTracingPeriodForTesting(kTestPeriod);
  }

  void TearDown() override {
    ResetRootSurface();
    tracing_helper_.TearDown();
    arc_test_.TearDown();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  // Ensures that tracing is active.
  views::Widget* StartArcAppTracing(const std::string& package_name,
                                    const std::string& activity_name) {
    shell_root_surface_ = std::make_unique<exo::Surface>();
    views::Widget* arc_widget =
        ArcTaskWindowBuilder()
            .SetShellRootSurface(shell_root_surface_.get())
            .BuildOwnedByNativeWidget();
    arc_widget->Show();
    DCHECK(arc_widget->GetNativeWindow());

    tracing_helper().GetTracing()->OnWindowActivated(
        wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
        arc_widget->GetNativeWindow(), arc_widget->GetNativeWindow());
    tracing_helper().GetTracing()->OnTaskCreated(
        1 /* task_Id */, package_name, activity_name,
        std::string() /* intent */, 0 /* session_id */);
    DCHECK(tracing_helper().GetTracingSession());
    tracing_helper().GetTracingSession()->FireTimerForTesting();
    DCHECK(tracing_helper().GetTracingSession());
    DCHECK(tracing_helper().GetTracingSession()->tracing_active());
    return arc_widget;
  }

  views::Widget* StartArcFocusAppTracing() {
    return StartArcAppTracing(kFocusAppPackage, kFocusAppActivity);
  }

  void ResetRootSurface() { shell_root_surface_.reset(); }

  ArcAppPerformanceTracingTestHelper& tracing_helper() {
    return tracing_helper_;
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{SyncServiceFactory::GetInstance(),
             base::BindRepeating(
                 [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                   return std::make_unique<syncer::TestSyncService>();
                 })}};
  }

 private:
  std::unique_ptr<exo::Surface> shell_root_surface_;
  ArcAppPerformanceTracingTestHelper tracing_helper_;
  ArcAppTest arc_test_;
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

TEST_F(ArcAppPerformanceTracingTest, TracingStoppedOnIdle) {
  views::Widget* const arc_widget = StartArcFocusAppTracing();
  const base::TimeDelta normal_interval = base::Seconds(1) / 60;
  base::Time timestamp = base::Time::Now();
  tracing_helper().GetTracingSession()->OnCommitForTesting(timestamp);
  // Expected updates;
  timestamp += normal_interval;
  tracing_helper().GetTracingSession()->OnCommitForTesting(timestamp);
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_TRUE(tracing_helper().GetTracingSession()->tracing_active());

  timestamp += normal_interval * 5;
  tracing_helper().GetTracingSession()->OnCommitForTesting(timestamp);
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_TRUE(tracing_helper().GetTracingSession()->tracing_active());

  // Too long update.
  timestamp += normal_interval * 10;
  tracing_helper().GetTracingSession()->OnCommitForTesting(timestamp);
  // Tracing is rescheduled and no longer active.
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_FALSE(tracing_helper().GetTracingSession()->tracing_active());
  arc_widget->Close();
}

TEST_F(ArcAppPerformanceTracingTest, StatisticsReported) {
  views::Widget* const arc_widget = StartArcFocusAppTracing();
  EXPECT_FALSE(tracing_helper().GetTracing()->WasReported(kFocusCategory));
  tracing_helper().PlayDefaultSequence();
  tracing_helper().FireTimerForTesting();
  EXPECT_TRUE(tracing_helper().GetTracing()->WasReported(kFocusCategory));
  EXPECT_EQ(45L, ReadFocusStatistics("FPS2"));
  EXPECT_EQ(216L, ReadFocusStatistics("CommitDeviation2"));
  EXPECT_EQ(48L, ReadFocusStatistics("RenderQuality2"));
  arc_widget->Close();
}

TEST_F(ArcAppPerformanceTracingTest,
       MinecraftConsumerEditionStatisticsReported) {
  const std::string package = "com.mojang.minecraftpe";
  const std::string activity = "com.mojang.minecraftpe.MainActivity";
  views::Widget* const arc_widget = StartArcAppTracing(package, activity);

  tracing_helper().PlayDefaultSequence();
  tracing_helper().FireTimerForTesting();
  EXPECT_EQ(45L, ReadStatistics("FPS2", "MinecraftConsumerEdition"));
  EXPECT_EQ(216L,
            ReadStatistics("CommitDeviation2", "MinecraftConsumerEdition"));
  EXPECT_EQ(48L, ReadStatistics("RenderQuality2", "MinecraftConsumerEdition"));
  arc_widget->Close();
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
  views::Widget* const arc_widget = StartArcFocusAppTracing();
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_TRUE(tracing_helper().GetTracingSession()->tracing_active());
  exo::SetShellRootSurface(arc_widget->GetNativeWindow(), nullptr);
  ResetRootSurface();
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_FALSE(tracing_helper().GetTracingSession()->tracing_active());

  // Try to re-active window without surface
  tracing_helper().GetTracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget->GetNativeWindow(), arc_widget->GetNativeWindow());

  EXPECT_FALSE(tracing_helper().GetTracingSession());

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
  ghost_window->SetApplicationId("org.chromium.arc.1");

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

}  // namespace arc
