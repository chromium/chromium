// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/demo_session_metrics_recorder.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/app_constants/constants.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

// Tests app usage recorded by DemoSessionMetricsRecorder.
// Mocks out the timer to control the sampling cycle. Tests will create and
// activate different window types to test that samples are attributed to the
// correct apps. Tests will also fire the timer continuously without user
// activity to simulate idle time and verify that idle samples are dropped.
class DemoSessionMetricsRecorderTest : public AshTestBase {
 public:
  DemoSessionMetricsRecorderTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  DemoSessionMetricsRecorderTest(const DemoSessionMetricsRecorderTest&) =
      delete;
  DemoSessionMetricsRecorderTest& operator=(
      const DemoSessionMetricsRecorderTest&) = delete;

  ~DemoSessionMetricsRecorderTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Create mock timer to be passed into DemoSessionMetricsRecorder.
    std::unique_ptr<base::RepeatingTimer> mock_timer =
        std::make_unique<base::MockRepeatingTimer>();
    // Store a pointer to the timer before moving it.
    mock_timer_ = static_cast<base::MockRepeatingTimer*>(mock_timer.get());
    metrics_recorder_ =
        std::make_unique<DemoSessionMetricsRecorder>(std::move(mock_timer));

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    metrics_recorder_.reset();
    AshTestBase::TearDown();
  }

  // Fires the timer, if it's running. (If it's stopped, we can assume any
  // amount of time passes here.)
  void FireTimer() {
    if (mock_timer_->IsRunning())
      mock_timer_->Fire();
  }

  // Simulates user activity.
  void SendUserActivity() { metrics_recorder_->OnUserActivity(nullptr); }

  void ClearHistograms() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  // Clears the metrics recorder and the timer to simulate the session ending.
  void DeleteMetricsRecorder() {
    mock_timer_ = nullptr;
    metrics_recorder_.reset();
  }

  // Creates a browser window.
  std::unique_ptr<aura::Window> CreateBrowserWindow() {
    std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 0,
        gfx::Rect(0, 0, 10, 10)));
    window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::BROWSER);
    return window;
  }

  // Creates a browser window associated with a hosted app.
  std::unique_ptr<aura::Window> CreateHostedAppBrowserWindow(
      const std::string& app_id) {
    std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 0,
        gfx::Rect(0, 0, 10, 10)));
    window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::BROWSER);
    window->SetProperty(
        kShelfIDKey,
        new std::string(ShelfID(app_id, std::string()).Serialize()));
    return window;
  }

  // Creates a normal Chrome platform app window.
  std::unique_ptr<aura::Window> CreateChromeAppWindow(
      const std::string& app_id) {
    std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 0,
        gfx::Rect(0, 0, 10, 10)));
    window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
    window->SetProperty(
        kShelfIDKey,
        new std::string(ShelfID(app_id, std::string()).Serialize()));
    return window;
  }

  // Creates a normal ARC++ app window.
  std::unique_ptr<aura::Window> CreateArcWindow(
      const std::string& package_name) {
    std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 0,
        gfx::Rect(0, 0, 10, 10)));
    window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);

    // ARC++ shelf app IDs are hashes of package_name#activity_name formatted as
    // extension IDs. The point is that they are opaque to the metrics recorder.
    const std::string app_id = "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz";
    window->SetProperty(
        kShelfIDKey,
        new std::string(ShelfID(app_id, std::string()).Serialize()));
    if (!package_name.empty())
      window->SetProperty(kArcPackageNameKey, package_name);
    return window;
  }

  // Creates a popup type window.
  std::unique_ptr<aura::Window> CreatePopupWindow() {
    std::unique_ptr<aura::Window> window(
        CreateTestWindowInShellWithDelegateAndType(
            aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(),
            aura::client::WINDOW_TYPE_POPUP, 0, gfx::Rect(0, 0, 10, 10)));
    return window;
  }

  // Simulates user clicking on home button.
  void ClickOnHomeButtion() {
    AshTestBase::LeftClickOn(
        AshTestBase::GetPrimaryShelf()->navigation_widget()->GetHomeButton());
  }

  // Simulates user clicking on the test window.
  void ClickMouseOnTestWindow() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       CreateBrowserWindow().get());
    generator.ClickLeftButton();
  }

  // Simulates user pressing the screen on the test window.
  void GesturePressWindow() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       CreateBrowserWindow().get());
    generator.GestureTapAt(gfx::Point(0, 0));
  }

 protected:
  // Captures histograms.
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  // The test target.
  std::unique_ptr<DemoSessionMetricsRecorder> metrics_recorder_;

  // Owned by metics_recorder_.
  raw_ptr<base::MockRepeatingTimer, DanglingUntriaged> mock_timer_ = nullptr;
};

// Verify samples are correct when one app window is active.
TEST_F(DemoSessionMetricsRecorderTest, ActiveApp) {
  std::unique_ptr<aura::Window> chrome_app_window =
      CreateChromeAppWindow(extension_misc::kHighlightsAppId);

  wm::ActivateWindow(chrome_app_window.get());
  for (int i = 0; i < 5; i++)
    FireTimer();

  SendUserActivity();

  histogram_tester_->ExpectUniqueSample(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kHighlights, 5);
}

// Verify samples are correct when multiple browser windows become active.
TEST_F(DemoSessionMetricsRecorderTest, BrowserWindows) {
  std::unique_ptr<aura::Window> browser_window = CreateBrowserWindow();
  std::unique_ptr<aura::Window> browser_window2 = CreateBrowserWindow();

  // Browser windows should all be treated as the same type.
  wm::ActivateWindow(browser_window.get());
  FireTimer();
  wm::ActivateWindow(browser_window2.get());
  FireTimer();
  FireTimer();

  SendUserActivity();

  histogram_tester_->ExpectUniqueSample(
      "DemoMode.ActiveApp", DemoSessionMetricsRecorder::DemoModeApp::kBrowser,
      3);
}

// Verify samples are correct when multiple windows types become active.
TEST_F(DemoSessionMetricsRecorderTest, AppTypes) {
  std::unique_ptr<aura::Window> browser_window = CreateBrowserWindow();
  std::unique_ptr<aura::Window> chrome_app_window =
      CreateChromeAppWindow(extension_misc::kCalculatorAppId);
  std::unique_ptr<aura::Window> hosted_app_browser_window =
      CreateHostedAppBrowserWindow(extension_misc::kYoutubeAppId);
  std::unique_ptr<aura::Window> arc_window =
      CreateArcWindow("com.google.Photos");

  wm::ActivateWindow(browser_window.get());
  FireTimer();
  SendUserActivity();
  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp", DemoSessionMetricsRecorder::DemoModeApp::kBrowser,
      1);

  wm::ActivateWindow(chrome_app_window.get());
  FireTimer();
  FireTimer();
  SendUserActivity();
  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kCalculator, 2);

  wm::ActivateWindow(hosted_app_browser_window.get());
  FireTimer();
  FireTimer();
  FireTimer();
  SendUserActivity();
  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp", DemoSessionMetricsRecorder::DemoModeApp::kYouTube,
      3);

  wm::ActivateWindow(arc_window.get());
  FireTimer();
  FireTimer();
  FireTimer();
  FireTimer();
  SendUserActivity();
  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kGooglePhotos, 4);

  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 10);
}

// Verify samples are correct when multiple windows types become active.
TEST_F(DemoSessionMetricsRecorderTest, ActiveAppAfterDelayedArcPackageName) {
  // Create an ARC window with an empty package name.
  std::unique_ptr<aura::Window> arc_window = CreateArcWindow("");

  wm::ActivateWindow(arc_window.get());
  FireTimer();
  SendUserActivity();

  // There should be no app activity recorded yet, because there was
  // no package name in the ARC window.
  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 0);

  // Simulate that no package name in the ARC window but metric
  // recording is triggered again. It should not cause any crash.
  FireTimer();

  // Set the package name after window creation/activation.
  arc_window->SetProperty(kArcPackageNameKey,
                          new std::string("com.google.Photos"));

  // Trigger sample reporting by sending user activity.
  SendUserActivity();

  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kGooglePhotos, 1);

  // Set the package name again.  The count shouldn't change because
  // after getting the package name once, we stop observing the
  // window.
  arc_window->SetProperty(kArcPackageNameKey,
                          new std::string("com.google.Photos"));
  // Trigger sample reporting by sending user activity.
  SendUserActivity();

  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kGooglePhotos, 1);

  // Delete the window.
  arc_window.reset();

  // Trigger sample reporting by sending user activity.
  SendUserActivity();

  // The count should not be affected.
  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kGooglePhotos, 1);
}

// Verify popup windows are categorized as kOtherWindow.
TEST_F(DemoSessionMetricsRecorderTest, PopupWindows) {
  std::unique_ptr<aura::Window> chrome_app_window =
      CreateChromeAppWindow(extension_misc::kCalculatorAppId);
  std::unique_ptr<aura::Window> popup_window = CreatePopupWindow();

  wm::ActivateWindow(chrome_app_window.get());
  for (int i = 0; i < 5; i++)
    FireTimer();

  wm::ActivateWindow(popup_window.get());
  for (int i = 0; i < 3; i++)
    FireTimer();

  SendUserActivity();

  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kCalculator, 5);
  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kOtherWindow, 3);
  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 8);
}

// Verify unknown apps are categorized as "other" Chrome/ARC apps.
TEST_F(DemoSessionMetricsRecorderTest, OtherApps) {
  std::unique_ptr<aura::Window> chrome_app_window =
      CreateChromeAppWindow("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  std::unique_ptr<aura::Window> arc_window = CreateArcWindow("com.foo.bar");

  wm::ActivateWindow(chrome_app_window.get());
  FireTimer();

  wm::ActivateWindow(arc_window.get());
  FireTimer();
  FireTimer();

  SendUserActivity();

  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kOtherChromeApp, 1);
  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kOtherArcApp, 2);
  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 3);
}

// Verify samples are discarded after no user activity.
TEST_F(DemoSessionMetricsRecorderTest, DiscardAfterInactivity) {
  std::unique_ptr<aura::Window> chrome_app_window =
      CreateChromeAppWindow(extension_misc::kCalculatorAppId);
  std::unique_ptr<aura::Window> arc_window =
      CreateChromeAppWindow("com.google.Photos");

  wm::ActivateWindow(chrome_app_window.get());
  for (int i = 0; i < 5; i++)
    FireTimer();

  SendUserActivity();

  histogram_tester_->ExpectUniqueSample(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kCalculator, 5);
  ClearHistograms();

  // Have no user activity for 20 seconds.
  for (int i = 0; i < 20; i++)
    FireTimer();

  // After user activity, the active window from the idle time isn't reported.
  SendUserActivity();
  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 0);
}

// Verify sample collection resumes after user activity.
TEST_F(DemoSessionMetricsRecorderTest, ResumeAfterActivity) {
  std::unique_ptr<aura::Window> chrome_app_window =
      CreateChromeAppWindow(extension_misc::kCalculatorAppId);

  wm::ActivateWindow(chrome_app_window.get());

  // Have no user activity for 20 seconds.
  for (int i = 0; i < 20; i++)
    FireTimer();

  // Now send user activity.
  SendUserActivity();
  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 0);

  // Sample collection should resume.
  for (int i = 0; i < 5; i++)
    FireTimer();
  SendUserActivity();
  histogram_tester_->ExpectUniqueSample(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kCalculator, 5);
}

// Verify window activation during idle time doesn't trigger reporting.
TEST_F(DemoSessionMetricsRecorderTest, ActivateWindowWhenIdle) {
  std::unique_ptr<aura::Window> chrome_app_window =
      CreateChromeAppWindow(extension_misc::kCalculatorAppId);
  std::unique_ptr<aura::Window> chrome_app_window2 =
      CreateChromeAppWindow(extension_misc::kGoogleKeepAppId);

  wm::ActivateWindow(chrome_app_window.get());

  // Even if the active window changes, which can happen automatically, these
  // samples shouldn't be reported.
  for (int i = 0; i < 10; i++)
    FireTimer();
  wm::ActivateWindow(chrome_app_window2.get());
  for (int i = 0; i < 10; i++)
    FireTimer();

  SendUserActivity();
  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 0);
}

TEST_F(DemoSessionMetricsRecorderTest, RepeatedUserActivity) {
  std::unique_ptr<aura::Window> chrome_app_window =
      CreateChromeAppWindow(extension_misc::kCalculatorAppId);
  std::unique_ptr<aura::Window> arc_window =
      CreateArcWindow("com.google.Photos");

  wm::ActivateWindow(chrome_app_window.get());

  FireTimer();
  SendUserActivity();
  SendUserActivity();

  // Switching between windows in between samples isn't recorded, even with user
  // action.
  FireTimer();
  SendUserActivity();
  wm::ActivateWindow(arc_window.get());
  SendUserActivity();
  wm::ActivateWindow(chrome_app_window.get());
  SendUserActivity();

  FireTimer();
  wm::ActivateWindow(arc_window.get());
  SendUserActivity();
  SendUserActivity();
  SendUserActivity();

  histogram_tester_->ExpectUniqueSample(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kCalculator, 3);
}

// Verify remaining samples are recorded on exit.
TEST_F(DemoSessionMetricsRecorderTest, RecordOnExit) {
  std::unique_ptr<aura::Window> chrome_app_window =
      CreateChromeAppWindow(extension_misc::kGoogleKeepAppId);
  std::unique_ptr<aura::Window> arc_window =
      CreateArcWindow("com.google.Photos");

  wm::ActivateWindow(chrome_app_window.get());
  for (int i = 0; i < 2; i++)
    FireTimer();
  wm::ActivateWindow(arc_window.get());
  for (int i = 0; i < 4; i++)
    FireTimer();

  DeleteMetricsRecorder();

  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kGoogleKeepChromeApp, 2);
  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp",
      DemoSessionMetricsRecorder::DemoModeApp::kGooglePhotos, 4);
  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 6);
}

// Verify remaining samples are not recorded on exit because the user became
// idle.
TEST_F(DemoSessionMetricsRecorderTest, IgnoreOnIdleExit) {
  std::unique_ptr<aura::Window> chrome_app_window =
      CreateChromeAppWindow(extension_misc::kFilesManagerAppId);

  wm::ActivateWindow(chrome_app_window.get());
  for (int i = 0; i < 10; i++)
    FireTimer();
  SendUserActivity();

  histogram_tester_->ExpectBucketCount(
      "DemoMode.ActiveApp", DemoSessionMetricsRecorder::DemoModeApp::kFiles,
      10);
  ClearHistograms();

  for (int i = 0; i < 20; i++)
    FireTimer();

  DeleteMetricsRecorder();

  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 0);
}

// Verify remaining samples are not recorded on exit when the user was idle the
// whole time.
TEST_F(DemoSessionMetricsRecorderTest, IgnoreOnIdleSession) {
  std::unique_ptr<aura::Window> chrome_app_window =
      CreateChromeAppWindow(extension_misc::kHighlightsAppId);

  wm::ActivateWindow(chrome_app_window.get());
  for (int i = 0; i < 20; i++)
    FireTimer();

  DeleteMetricsRecorder();

  histogram_tester_->ExpectTotalCount("DemoMode.ActiveApp", 0);
}

TEST_F(DemoSessionMetricsRecorderTest, UniqueAppsLaunchedOnDeletion) {
  // Activate each window twice.  Despite activating each twice,
  // the count should only be incremented once per unique app.
  std::unique_ptr<aura::Window> chrome_app_window =
      CreateChromeAppWindow(extension_misc::kCalculatorAppId);
  wm::ActivateWindow(chrome_app_window.get());
  wm::DeactivateWindow(chrome_app_window.get());
  wm::ActivateWindow(chrome_app_window.get());

  std::unique_ptr<aura::Window> chrome_browser_window =
      CreateChromeAppWindow(app_constants::kChromeAppId);
  wm::ActivateWindow(chrome_browser_window.get());
  wm::DeactivateWindow(chrome_browser_window.get());
  wm::ActivateWindow(chrome_browser_window.get());

  std::unique_ptr<aura::Window> arc_window_1 =
      CreateArcWindow("com.google.Photos");
  wm::ActivateWindow(arc_window_1.get());
  wm::DeactivateWindow(arc_window_1.get());
  wm::ActivateWindow(arc_window_1.get());

  std::unique_ptr<aura::Window> arc_window_2 =
      CreateArcWindow("com.google.Maps");
  wm::ActivateWindow(arc_window_2.get());
  wm::DeactivateWindow(arc_window_2.get());
  wm::ActivateWindow(arc_window_2.get());

  // Popup windows shouldn't be counted at all.
  std::unique_ptr<aura::Window> popup_window = CreatePopupWindow();
  wm::ActivateWindow(popup_window.get());
  wm::DeactivateWindow(popup_window.get());
  wm::ActivateWindow(popup_window.get());

  DeleteMetricsRecorder();

  histogram_tester_->ExpectUniqueSample("DemoMode.UniqueAppsLaunched", 4, 1);
}

TEST_F(DemoSessionMetricsRecorderTest,
       NoUniqueAppsLaunchedOnMissingArcPackageName) {
  // Create an ARC window with no package name set yet
  std::unique_ptr<aura::Window> arc_window_1 = CreateArcWindow("");
  wm::ActivateWindow(arc_window_1.get());

  DeleteMetricsRecorder();

  // There shuld be no unique apps reported if there was no package name.
  histogram_tester_->ExpectUniqueSample("DemoMode.UniqueAppsLaunched", 0, 1);
}

TEST_F(DemoSessionMetricsRecorderTest,
       UniqueAppsLaunchedOnDelayedArcPackageName) {
  // Create an ARC window with no package name set yet.
  std::unique_ptr<aura::Window> arc_window_1 = CreateArcWindow("");
  wm::ActivateWindow(arc_window_1.get());

  // Set the package name after window creation/activation.
  arc_window_1->SetProperty(kArcPackageNameKey,
                            new std::string("com.google.Photos"));

  // Set the package name again. This shouldn't cause a double-recording
  // of the stat.
  arc_window_1->SetProperty(kArcPackageNameKey,
                            new std::string("com.google.Photos"));

  // Delete the window.
  arc_window_1.reset();

  std::unique_ptr<aura::Window> arc_window_2 =
      CreateArcWindow("com.google.Maps");
  wm::ActivateWindow(arc_window_2.get());

  DeleteMetricsRecorder();

  // There should be 2 unique apps reported.
  histogram_tester_->ExpectUniqueSample("DemoMode.UniqueAppsLaunched", 2, 1);
}

TEST_F(DemoSessionMetricsRecorderTest, NoUniqueAppsLaunchedOnDeletion) {
  DeleteMetricsRecorder();

  // There should be no samples if the recorder is deleted with 0 unique apps
  // launched.
  histogram_tester_->ExpectUniqueSample("DemoMode.UniqueAppsLaunched", 0, 1);
}

TEST_F(DemoSessionMetricsRecorderTest, AppLaunched) {
  // Activate each window twice.  Despite activating each twice,
  // the sample should only be incremented once per unique app, except
  // for apps for which we don't have enums, which all get recorded
  // as "other" apps.

  // Chrome browser window
  std::unique_ptr<aura::Window> chrome_browser_window =
      CreateChromeAppWindow(app_constants::kChromeAppId);
  wm::ActivateWindow(chrome_browser_window.get());
  wm::DeactivateWindow(chrome_browser_window.get());
  wm::ActivateWindow(chrome_browser_window.get());

  // Chrome apps
  std::unique_ptr<aura::Window> chrome_app_window_1 =
      CreateChromeAppWindow(extension_misc::kCalculatorAppId);
  wm::ActivateWindow(chrome_app_window_1.get());
  wm::DeactivateWindow(chrome_app_window_1.get());
  wm::ActivateWindow(chrome_app_window_1.get());

  // The following 2 activations should get recorded as kOtherChromeApp
  std::unique_ptr<aura::Window> chrome_app_window_2 =
      CreateChromeAppWindow("otherappid2");
  wm::ActivateWindow(chrome_app_window_2.get());
  wm::DeactivateWindow(chrome_app_window_2.get());
  wm::ActivateWindow(chrome_app_window_2.get());

  std::unique_ptr<aura::Window> chrome_app_window_3 =
      CreateChromeAppWindow("otherappid3");
  wm::ActivateWindow(chrome_app_window_3.get());
  wm::DeactivateWindow(chrome_app_window_3.get());
  wm::ActivateWindow(chrome_app_window_3.get());

  // ARC Apps
  std::unique_ptr<aura::Window> arc_window_1 =
      CreateArcWindow("com.google.Photos");
  wm::ActivateWindow(arc_window_1.get());
  wm::DeactivateWindow(arc_window_1.get());
  wm::ActivateWindow(arc_window_1.get());

  std::unique_ptr<aura::Window> arc_window_2 =
      CreateArcWindow("com.google.Sheets");
  wm::ActivateWindow(arc_window_2.get());
  wm::DeactivateWindow(arc_window_2.get());
  wm::ActivateWindow(arc_window_2.get());

  // The following 2 activations should get recorded as kOtherArcApp
  std::unique_ptr<aura::Window> arc_window_3 =
      CreateArcWindow("com.some.other.App3");
  wm::ActivateWindow(arc_window_3.get());
  wm::DeactivateWindow(arc_window_3.get());
  wm::ActivateWindow(arc_window_3.get());

  std::unique_ptr<aura::Window> arc_window_4 =
      CreateArcWindow("com.some.other.App4");
  wm::ActivateWindow(arc_window_4.get());
  wm::DeactivateWindow(arc_window_4.get());
  wm::ActivateWindow(arc_window_4.get());

  histogram_tester_->ExpectBucketCount(
      "DemoMode.AppLaunched", DemoSessionMetricsRecorder::DemoModeApp::kBrowser,
      1);
  histogram_tester_->ExpectBucketCount(
      "DemoMode.AppLaunched",
      DemoSessionMetricsRecorder::DemoModeApp::kCalculator, 1);
  // We should see 2 "other chrome apps"
  histogram_tester_->ExpectBucketCount(
      "DemoMode.AppLaunched",
      DemoSessionMetricsRecorder::DemoModeApp::kOtherChromeApp, 2);

  histogram_tester_->ExpectBucketCount(
      "DemoMode.AppLaunched",
      DemoSessionMetricsRecorder::DemoModeApp::kGooglePhotos, 1);
  histogram_tester_->ExpectBucketCount(
      "DemoMode.AppLaunched",
      DemoSessionMetricsRecorder::DemoModeApp::kGoogleSheetsAndroidApp, 1);
  // We should see 2 "other arc apps"
  histogram_tester_->ExpectBucketCount(
      "DemoMode.AppLaunched",
      DemoSessionMetricsRecorder::DemoModeApp::kOtherArcApp, 2);
}

TEST_F(DemoSessionMetricsRecorderTest, DwellTime) {
  // Simulate user activity for 10 seconds.
  SendUserActivity();

  task_environment()->FastForwardBy(base::Seconds(5));
  SendUserActivity();

  task_environment()->FastForwardBy(base::Seconds(5));
  SendUserActivity();

  // Simulate a session "timing out" after 60 seconds.
  task_environment()->FastForwardBy(base::Seconds(60));
  DeleteMetricsRecorder();

  // The recorded dwell time should be 10 seconds.
  histogram_tester_->ExpectUniqueSample("DemoMode.DwellTime", 10, 1);
}

// Within the demo session, test user clicks the home button on shelf, clicks on
// the test window twice and presses the screen, then the UserClickesAndPresses
// should be 4.
TEST_F(DemoSessionMetricsRecorderTest,
       UserClicksAndPressesEqualsThreeInDemoSession) {
  TestSessionControllerClient* session =
      AshTestBase::GetSessionControllerClient();
  session->SetIsDemoSession();

  ClickMouseOnTestWindow();
  ClickMouseOnTestWindow();
  ClickOnHomeButtion();
  GesturePressWindow();

  ash::Shell::Get()->metrics()->OnShellShuttingDown();

  // The recorded count UserInteracted should be 4, with one sample recorded.
  histogram_tester_->ExpectUniqueSample(
      DemoSessionMetricsRecorder::kUserClicksAndPressesMetric, 4, 1);
}

// Within the demo session, test user does not do any clicks/presses, then the
// UserClickesAndPresses should be 0.
TEST_F(DemoSessionMetricsRecorderTest,
       UserClicksAndPressesEqualsZeroInDemoSession) {
  TestSessionControllerClient* session =
      AshTestBase::GetSessionControllerClient();
  session->SetIsDemoSession();

  ash::Shell::Get()->metrics()->OnShellShuttingDown();

  // The recorded count UserInteracted should be 0, with one sample recorded.
  histogram_tester_->ExpectUniqueSample(
      DemoSessionMetricsRecorder::kUserClicksAndPressesMetric, 0, 1);
}

// Out of demo session, test user clicks the home button on shelf, clicks on the
// test window twice and presses the screen, then the UserClickesAndPresses
// should be 0.
TEST_F(DemoSessionMetricsRecorderTest,
       UserClicksAndPressesEqualsZeroOutOfDemoSession) {
  ClickMouseOnTestWindow();
  ClickOnHomeButtion();
  GesturePressWindow();

  ash::Shell::Get()->metrics()->OnShellShuttingDown();

  // The recorded count UserInteracted should be 0, and metric should contain 0
  // sample.
  histogram_tester_->ExpectUniqueSample(
      DemoSessionMetricsRecorder::kUserClicksAndPressesMetric, 0, 0);
}

}  // namespace
}  // namespace ash
