// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/touch_ui_controller_stats_tracker.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/pointer/touch_ui_controller.h"

namespace {

constexpr base::TimeDelta kInactivityTimeout = base::Minutes(5);

class SessionEndWaiter
    : public metrics::DesktopSessionDurationTracker::Observer {
 public:
  explicit SessionEndWaiter(metrics::DesktopSessionDurationTracker* tracker)
      : tracker_(tracker) {
    tracker_->AddObserver(this);
  }

  ~SessionEndWaiter() override { tracker_->RemoveObserver(this); }

  void Wait() {
    ASSERT_FALSE(waiting_);
    if (!tracker_->in_session()) {
      return;
    }

    waiting_ = true;
    base::RunLoop run_loop;
    end_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // metrics::DesktopSessionDurationTracker::Observer:
  void OnSessionEnded(base::TimeDelta session_length,
                      base::TimeTicks session_end) override {
    if (!waiting_) {
      return;
    }
    end_closure_.Run();
  }

 private:
  raw_ptr<metrics::DesktopSessionDurationTracker> tracker_;
  base::RepeatingClosure end_closure_;
  bool waiting_ = false;
};

}  // namespace

class SessionStatsTrackerTestBase : public ::testing::Test {
 public:
  SessionStatsTrackerTestBase()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kNoFirstRun);
  }
  ~SessionStatsTrackerTestBase() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("p1");

    metrics::DesktopSessionDurationTracker::Initialize();
    metrics::DesktopSessionDurationTracker::Get()
        ->SetInactivityTimeoutForTesting(kInactivityTimeout);

    stats_tracker_ = std::make_unique<TouchUIControllerStatsTracker>(
        metrics::DesktopSessionDurationTracker::Get(),
        ui::TouchUiController::Get());

    Browser::CreateParams params(profile_, false);
    browser_ = CreateBrowserWithTestWindowForParams(params);
  }

  void TearDown() override {
    browser_.reset();
    stats_tracker_.reset();
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
  }

  void StartSession() {
    BrowserList::SetLastActive(browser_.get());
    task_environment_.RunUntilIdle();
    metrics::DesktopSessionDurationTracker::Get()->OnUserEvent();
  }

  void EndSession() {
    BrowserList::NotifyBrowserNoLongerActive(browser_.get());
    SessionEndWaiter waiter(metrics::DesktopSessionDurationTracker::Get());
    waiter.Wait();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<Profile, DanglingUntriaged> profile_;
  std::unique_ptr<TouchUIControllerStatsTracker> stats_tracker_;

 private:
  TestingProfileManager profile_manager_;
  std::unique_ptr<Browser> browser_;
};

class TouchModeStatsTrackerTest : public SessionStatsTrackerTestBase {};

#if BUILDFLAG(IS_WIN)
class DevicePostureModeStatsTrackerTest : public SessionStatsTrackerTestBase {};
#endif  // BUILDFLAG(IS_WIN)

// An entire session spent in touch mode should be logged accordingly.
TEST_F(TouchModeStatsTrackerTest, TouchSession) {
  ui::TouchUiController::TouchUiScoperForTesting enable_touch_mode(true);
  base::HistogramTester histograms;

  StartSession();
  ASSERT_TRUE(metrics::DesktopSessionDurationTracker::Get()->in_session());
  task_environment_.FastForwardBy(base::Minutes(1));
  EndSession();

  histograms.ExpectUniqueTimeSample(
      TouchUIControllerStatsTracker::kSessionTouchDurationHistogramName,
      base::Minutes(1), 1);
}

// The touch duration logged should be 0 for a non-touch session.
TEST_F(TouchModeStatsTrackerTest, NonTouchSession) {
  ui::TouchUiController::TouchUiScoperForTesting disable_touch_mode(false);
  base::HistogramTester histograms;

  StartSession();
  task_environment_.FastForwardBy(base::Minutes(1));
  EndSession();

  histograms.ExpectUniqueTimeSample(
      TouchUIControllerStatsTracker::kSessionTouchDurationHistogramName,
      base::TimeDelta(), 1);
}

// If the touch mode changes during a session, the logged duration
// should comprise the session time spent in touch mode.
TEST_F(TouchModeStatsTrackerTest, TouchChangesDuringSession) {
  ui::TouchUiController::TouchUiScoperForTesting touch_mode_override(false);

  // Check starting in touch mode.
  {
    base::HistogramTester histograms;
    StartSession();

    task_environment_.FastForwardBy(base::Seconds(15));
    touch_mode_override.UpdateState(true);
    task_environment_.FastForwardBy(base::Seconds(15));
    touch_mode_override.UpdateState(false);
    task_environment_.FastForwardBy(base::Seconds(15));
    touch_mode_override.UpdateState(true);
    task_environment_.FastForwardBy(base::Seconds(15));
    touch_mode_override.UpdateState(false);
    task_environment_.FastForwardBy(base::Seconds(15));

    EndSession();
    histograms.ExpectUniqueTimeSample(
        TouchUIControllerStatsTracker::kSessionTouchDurationHistogramName,
        base::Seconds(30), 1);
  }

  touch_mode_override.UpdateState(true);
  task_environment_.FastForwardBy(base::Seconds(15));

  // Check starting in non-touch mode.
  {
    base::HistogramTester histograms;
    StartSession();

    task_environment_.FastForwardBy(base::Seconds(15));
    touch_mode_override.UpdateState(false);
    task_environment_.FastForwardBy(base::Seconds(15));
    touch_mode_override.UpdateState(true);
    task_environment_.FastForwardBy(base::Seconds(15));

    EndSession();
    histograms.ExpectUniqueTimeSample(
        TouchUIControllerStatsTracker::kSessionTouchDurationHistogramName,
        base::Seconds(30), 1);
  }
}

#if BUILDFLAG(IS_WIN)
// An entire session spent in tablet mode should be logged accordingly.
TEST_F(DevicePostureModeStatsTrackerTest, TabletSession) {
  ui::TouchUiController::TouchUiScoperForTesting enable_tablet_mode(
      true, /*tablet_mode_enabled*/ true);
  base::HistogramTester histograms;

  StartSession();
  task_environment_.RunUntilIdle();
  // Have to reset the tablet mode again since
  // IsDeviceInTabletMode() is called again in StartSession()
  // and might override the previous value.
  enable_tablet_mode.UpdateTabletMode(true);
  task_environment_.FastForwardBy(base::Minutes(1));
  EndSession();

  histograms.ExpectUniqueTimeSample(
      TouchUIControllerStatsTracker::kSessionTabletDurationHistogramName,
      base::Minutes(1), 1);
}

// The tablet duration logged should be 0 for a non-tablet session.
TEST_F(DevicePostureModeStatsTrackerTest, NonTabletSession) {
  ui::TouchUiController::TouchUiScoperForTesting disable_tablet_mode(
      true, /*tablet_mode_enabled*/ false);
  base::HistogramTester histograms;

  StartSession();
  task_environment_.RunUntilIdle();
  // Have to reset the tablet mode again since
  // IsDeviceInTabletMode() is called again in StartSession()
  // and might override the previous value.
  disable_tablet_mode.UpdateTabletMode(false);
  task_environment_.FastForwardBy(base::Minutes(1));
  EndSession();

  histograms.ExpectUniqueTimeSample(
      TouchUIControllerStatsTracker::kSessionTabletDurationHistogramName,
      base::TimeDelta(), 1);
}

// If the tablet mode changes during a session, the logged duration
// should comprise the session time spent in tablet mode.
TEST_F(DevicePostureModeStatsTrackerTest, TabletModeChangesDuringSession) {
  ui::TouchUiController::TouchUiScoperForTesting posture_mode_override(
      true, /*tablet_mode_enabled*/ false);

  // Check starting in tablet mode.
  {
    base::HistogramTester histograms;
    StartSession();
    task_environment_.RunUntilIdle();
    // Have to reset the tablet mode again since
    // IsDeviceInTabletMode() is called again in StartSession()
    // and might override the previous value.
    posture_mode_override.UpdateTabletMode(false);

    task_environment_.FastForwardBy(base::Seconds(15));
    posture_mode_override.UpdateTabletMode(true);  // Enter tablet mode
    task_environment_.FastForwardBy(base::Seconds(15));
    posture_mode_override.UpdateTabletMode(false);  // Exit tablet mode
    task_environment_.FastForwardBy(base::Seconds(15));
    posture_mode_override.UpdateTabletMode(true);  // Enter tablet mode again
    task_environment_.FastForwardBy(base::Seconds(15));
    posture_mode_override.UpdateTabletMode(false);  // Exit tablet mode
    task_environment_.FastForwardBy(base::Seconds(15));

    EndSession();
    histograms.ExpectUniqueTimeSample(
        TouchUIControllerStatsTracker::kSessionTabletDurationHistogramName,
        base::Seconds(30), 1);
  }

  posture_mode_override.UpdateTabletMode(true);
  task_environment_.FastForwardBy(base::Seconds(15));

  // Check starting in non-tablet mode.
  {
    base::HistogramTester histograms;
    StartSession();
    task_environment_.RunUntilIdle();
    // Have to reset the tablet mode again since
    // IsDeviceInTabletMode() is called again in StartSession()
    // and might override the previous value.
    posture_mode_override.UpdateTabletMode(true);

    task_environment_.FastForwardBy(base::Seconds(15));
    posture_mode_override.UpdateTabletMode(true);  // Enter tablet mode
    task_environment_.FastForwardBy(base::Seconds(15));
    posture_mode_override.UpdateTabletMode(false);  // Exit tablet mode
    task_environment_.FastForwardBy(base::Seconds(15));

    EndSession();
    histograms.ExpectUniqueTimeSample(
        TouchUIControllerStatsTracker::kSessionTabletDurationHistogramName,
        base::Seconds(30), 1);
  }
}
#endif  // BUILDFLAG(IS_WIN)
