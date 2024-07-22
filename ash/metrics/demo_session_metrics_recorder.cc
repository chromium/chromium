// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/demo_session_metrics_recorder.h"

#include <iostream>
#include <string>
#include <utility>

#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shelf/shelf_window_watcher.h"
#include "ash/shell.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_multi_source_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/app_constants/constants.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

using DemoModeApp = DemoSessionMetricsRecorder::DemoModeApp;

// How often to sample.
constexpr auto kSamplePeriod = base::Seconds(1);

// Redefining chromeos::preinstalled_web_apps::kHelpAppId as ash can't depend on
// chrome.
constexpr char kHelpAppId[] = "nbljnnecbjbmifnoehiemkgefbnpoeak";

// How many periods to wait for user activity before discarding samples.
// This timeout is low because demo sessions tend to be very short. If we
// recorded samples for a full minute while the device is in between uses, we
// would bias our measurements toward whatever app was used last.
constexpr int kMaxPeriodsWithoutActivity = base::Seconds(15) / kSamplePeriod;

// Maps a Chrome app ID to a DemoModeApp value for metrics.
DemoModeApp GetAppFromAppId(const std::string& app_id) {
  // Each version of the Highlights app is bucketed into the same value.
  if (app_id == extension_misc::kHighlightsAppId ||
      app_id == extension_misc::kNewHighlightsAppId) {
    return DemoModeApp::kHighlights;
  }

  // Each version of the Screensaver app is bucketed into the same value.
  if (app_id == extension_misc::kScreensaverAppId ||
      app_id == extension_misc::kNewAttractLoopAppId) {
    return DemoModeApp::kScreensaver;
  }

  if (app_id == app_constants::kChromeAppId)
    return DemoModeApp::kBrowser;
  if (app_id == extension_misc::kFilesManagerAppId)
    return DemoModeApp::kFiles;
  if (app_id == extension_misc::kCalculatorAppId)
    return DemoModeApp::kCalculator;
  if (app_id == extension_misc::kCalendarDemoAppId)
    return DemoModeApp::kCalendar;
  if (app_id == extension_misc::kGoogleDocsDemoAppId)
    return DemoModeApp::kGoogleDocsChromeApp;
  if (app_id == extension_misc::kGoogleDocsPwaAppId)
    return DemoModeApp::kGoogleDocsPwa;
  if (app_id == extension_misc::kGoogleMeetPwaAppId)
    return DemoModeApp::kGoogleMeetPwa;
  if (app_id == extension_misc::kGoogleSheetsDemoAppId)
    return DemoModeApp::kGoogleSheetsChromeApp;
  if (app_id == extension_misc::kGoogleSheetsPwaAppId)
    return DemoModeApp::kGoogleSheetsPwa;
  if (app_id == extension_misc::kGoogleSlidesDemoAppId)
    return DemoModeApp::kGoogleSlidesChromeApp;
  if (app_id == kHelpAppId)
    return DemoModeApp::kGetHelp;
  if (app_id == extension_misc::kGoogleKeepAppId)
    return DemoModeApp::kGoogleKeepChromeApp;
  if (app_id == extensions::kWebStoreAppId)
    return DemoModeApp::kWebStore;
  if (app_id == extension_misc::kYoutubeAppId)
    return DemoModeApp::kYouTube;
  if (app_id == extension_misc::kYoutubePwaAppId)
    return DemoModeApp::kYoutubePwa;
  if (app_id == extension_misc::kSpotifyAppId)
    return DemoModeApp::kSpotify;
  if (app_id == extension_misc::kBeFunkyAppId)
    return DemoModeApp::kBeFunky;
  if (app_id == extension_misc::kClipchampAppId)
    return DemoModeApp::kClipchamp;
  if (app_id == extension_misc::kGeForceNowAppId)
    return DemoModeApp::kGeForceNow;
  if (app_id == extension_misc::kZoomAppId)
    return DemoModeApp::kZoom;
  if (app_id == extension_misc::kSumoAppId)
    return DemoModeApp::kSumo;
  if (app_id == extension_misc::kAdobeSparkAppId)
    return DemoModeApp::kAdobeSpark;

  return DemoModeApp::kOtherChromeApp;
}

// Maps an ARC++ package name to a DemoModeApp value for metrics.
DemoModeApp GetAppFromPackageName(const std::string& package_name) {
  // Google apps.
  if (package_name == "com.google.Photos" ||
      package_name == "com.google.android.apps.photos")
    return DemoModeApp::kGooglePhotos;
  if (package_name == "com.google.Sheets" ||
      package_name == "com.google.android.apps.docs.editors.sheets")
    return DemoModeApp::kGoogleSheetsAndroidApp;
  if (package_name == "com.google.Slides" ||
      package_name == "com.google.android.apps.docs.editors.slides")
    return DemoModeApp::kGoogleSlidesAndroidApp;
  if (package_name == "com.google.android.keep")
    return DemoModeApp::kGoogleKeepAndroidApp;
  if (package_name == "com.android.vending")
    return DemoModeApp::kPlayStore;

  // Third-party apps.
  if (package_name == "com.gameloft.android.ANMP.GloftA8HMD")
    return DemoModeApp::kAsphalt8;
  if (package_name == "com.gameloft.android.ANMP.GloftA9HM" ||
      package_name == "com.gameloft.android.ANMP.GloftA9HMD")
    return DemoModeApp::kAsphalt9;
  if (package_name == "com.chucklefish.stardewvalley" ||
      package_name == "com.chucklefish.stardewvalleydemo")
    return DemoModeApp::kStardewValley;
  if (package_name == "com.nexstreaming.app.kinemasterfree" ||  // nocheck
      package_name ==
          "com.nexstreaming.app.kinemasterfree.demo.chromebook") {  // nocheck
    return DemoModeApp::kKinemaster;                                // nocheck
  }
  if (package_name == "com.pixlr.express" ||
      package_name == "com.pixlr.express.chromebook.demo")
    return DemoModeApp::kPixlr;
  if (package_name == "com.brakefield.painter")
    return DemoModeApp::kInfinitePainter;
  if (package_name == "com.myscript.nebo.demo")
    return DemoModeApp::kMyScriptNebo;
  if (package_name == "com.steadfastinnovation.android.projectpapyrus")
    return DemoModeApp::kSquid;
  if (package_name == "com.autodesk.autocadws.demo")
    return DemoModeApp::kAutoCAD;

  return DemoModeApp::kOtherArcApp;
}

chromeos::AppType GetAppType(const aura::Window* window) {
  return window->GetProperty(chromeos::kAppTypeKey);
}

const std::string* GetArcPackageName(const aura::Window* window) {
  DCHECK(IsArcWindow(window));
  return window->GetProperty(kArcPackageNameKey);
}

bool CanGetAppFromWindow(const aura::Window* window) {
  // For ARC apps we can only get the App if the package
  // name is not null.
  if (IsArcWindow(window)) {
    return GetArcPackageName(window) != nullptr;
  }
  // We can always get the App for non-ARC windows.
  return true;
}

const ShelfID GetShelfID(const aura::Window* window) {
  return ShelfID::Deserialize(window->GetProperty(kShelfIDKey));
}

// Maps the app-like thing in |window| to a DemoModeApp value for metrics.
DemoModeApp GetAppFromWindow(const aura::Window* window) {
  DCHECK(CanGetAppFromWindow(window));

  chromeos::AppType app_type = GetAppType(window);
  if (app_type == chromeos::AppType::ARC_APP) {
    // The ShelfID app id isn't used to identify ARC++ apps since it's a hash
    // of both the package name and the activity.
    const std::string* package_name = GetArcPackageName(window);
    return GetAppFromPackageName(*package_name);
  }

  std::string app_id = GetShelfID(window).app_id;

  // The Chrome "app" in the shelf is just the browser.
  if (app_id == app_constants::kChromeAppId) {
    return DemoModeApp::kBrowser;
  }

  // If the window is the "browser" type, having an app ID other than the
  // default indicates a hosted/bookmark app.
  if (app_type == chromeos::AppType::CHROME_APP ||
      (app_type == chromeos::AppType::BROWSER && !app_id.empty())) {
    return GetAppFromAppId(app_id);
  }

  if (app_type == chromeos::AppType::BROWSER) {
    return DemoModeApp::kBrowser;
  }
  return DemoModeApp::kOtherWindow;
}

// Identical to UmaHistogramLongTimes100, but reports times with second
// granularity instead of millisecond granularity.
// This significantly improves the bucketing if millisecond granularity is
// not required - 90/100 buckets are greater than 10 seconds, compared to
// 43/100 buckets using millisecond accuracy with min=1ms, or
// 72/100 buckets using millisecond accuracy with min=1000ms.
void ReportHistogramLongSecondsTimes100(const char* name,
                                        base::TimeDelta sample) {
  // We use a max of 1 hour = 60 * 60 secs.
  base::UmaHistogramCustomCounts(name,
                                 base::saturated_cast<int>(sample.InSeconds()),
                                 /*min=*/1, /*max=*/60 * 60, /*buckets=*/100);
}

}  // namespace

// Observes for changes in a window's ArcPackageName property for the purpose of
// logging  of active app samples.
class DemoSessionMetricsRecorder::ActiveAppArcPackageNameObserver
    : public aura::WindowObserver {
 public:
  explicit ActiveAppArcPackageNameObserver(
      DemoSessionMetricsRecorder* metrics_recorder)
      : metrics_recorder_(metrics_recorder) {}

  ActiveAppArcPackageNameObserver(const ActiveAppArcPackageNameObserver&) =
      delete;
  ActiveAppArcPackageNameObserver& operator=(
      const ActiveAppArcPackageNameObserver&) = delete;

  // aura::WindowObserver
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key != kArcPackageNameKey)
      return;

    const std::string* package_name = GetArcPackageName(window);

    if (package_name) {
      metrics_recorder_->RecordActiveAppSample(
          GetAppFromPackageName(*package_name));
    } else {
      VLOG(1) << "Got null ARC package name";
    }

    scoped_observations_.RemoveObservation(window);
  }

  void OnWindowDestroyed(aura::Window* window) override {
    if (scoped_observations_.IsObservingSource(window))
      scoped_observations_.RemoveObservation(window);
  }

  void ObserveWindow(aura::Window* window) {
    if (scoped_observations_.IsObservingSource(window)) {
      return;
    }
    scoped_observations_.AddObservation(window);
  }

 private:
  raw_ptr<DemoSessionMetricsRecorder> metrics_recorder_;
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      scoped_observations_{this};
};

// Observes changes in a window's ArcPackageName property for the purpose of
// logging of unique launches of ARC apps.
class DemoSessionMetricsRecorder::UniqueAppsLaunchedArcPackageNameObserver
    : public aura::WindowObserver {
 public:
  explicit UniqueAppsLaunchedArcPackageNameObserver(
      DemoSessionMetricsRecorder* metrics_recorder)
      : metrics_recorder_(metrics_recorder) {}

  UniqueAppsLaunchedArcPackageNameObserver(
      const UniqueAppsLaunchedArcPackageNameObserver&) = delete;
  UniqueAppsLaunchedArcPackageNameObserver& operator=(
      const UniqueAppsLaunchedArcPackageNameObserver&) = delete;

  // aura::WindowObserver
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key != kArcPackageNameKey)
      return;

    const std::string* package_name = GetArcPackageName(window);

    if (package_name) {
      metrics_recorder_->RecordAppLaunch(*package_name,
                                         chromeos::AppType::ARC_APP);
    } else {
      VLOG(1) << "Got null ARC package name";
    }

    DCHECK(scoped_observation_.IsObservingSource(window));
    scoped_observation_.Reset();
  }

  void OnWindowDestroyed(aura::Window* window) override {
    DCHECK(scoped_observation_.IsObservingSource(window));
    scoped_observation_.Reset();
  }

  void ObserveWindow(aura::Window* window) {
    scoped_observation_.Reset();
    scoped_observation_.Observe(window);
  }

 private:
  raw_ptr<DemoSessionMetricsRecorder> metrics_recorder_;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      scoped_observation_{this};
};

DemoSessionMetricsRecorder::DemoSessionMetricsRecorder(
    std::unique_ptr<base::RepeatingTimer> timer)
    : timer_(std::move(timer)),
      unique_apps_arc_package_name_observer_(
          std::make_unique<UniqueAppsLaunchedArcPackageNameObserver>(this)),
      active_app_arc_package_name_observer_(
          std::make_unique<ActiveAppArcPackageNameObserver>(this)) {
  // Outside of tests, use a normal repeating timer.
  if (!timer_.get())
    timer_ = std::make_unique<base::RepeatingTimer>();

  StartRecording();
  observation_.Observe(ui::UserActivityDetector::Get());

  // Subscribe to window activation updates.  Even though this gets us
  // notifications for all window activations, we ignore the ARC
  // notifications because they don't contain the app_id.  We handle
  // accounting for ARC windows with OnTaskCreated.
  if (Shell::Get()->GetPrimaryRootWindow()) {
    activation_client_ = Shell::Get()->focus_controller();
    activation_client_->AddObserver(this);
  }
}

DemoSessionMetricsRecorder::~DemoSessionMetricsRecorder() {
  // TODO(mlcui): Investigate whether the metrics emitted here are gracefully
  // handled during session / device shutdown.

  // Report any remaining stored samples on exit. (If the user went idle, there
  // won't be any.)
  ReportSamples();

  ReportDwellTime();

  ReportUserClickesAndPresses();

  // Unsubscribe from window activation events.
  activation_client_->RemoveObserver(this);

  ReportUniqueAppsLaunched();
}

void DemoSessionMetricsRecorder::RecordAppLaunch(const std::string& id,
                                                 chromeos::AppType app_type) {
  if (!ShouldRecordAppLaunch(id)) {
    return;
  }
  DemoModeApp app;
  if (app_type == chromeos::AppType::ARC_APP) {
    app = GetAppFromPackageName(id);
  } else {
    app = GetAppFromAppId(id);
  }

  if (!unique_apps_launched_.contains(id)) {
    unique_apps_launched_.insert(id);
    // Only log each app launch once.  This is determined by
    // checking the package_name instead of the DemoApp enum,
    // because the DemoApp enum collapses unknown apps into
    // a single enum.
    UMA_HISTOGRAM_ENUMERATION("DemoMode.AppLaunched", app);
  }
}

// Indicates whether the specified app_id should be recorded for
// the unique-apps-launched stat.
bool DemoSessionMetricsRecorder::ShouldRecordAppLaunch(
    const std::string& app_id) {
  return unique_apps_launched_recording_enabled_ &&
         GetAppFromAppId(app_id) != DemoModeApp::kHighlights &&
         GetAppFromAppId(app_id) != DemoModeApp::kScreensaver;
}

void DemoSessionMetricsRecorder::OnWindowActivated(ActivationReason reason,
                                                   aura::Window* gained_active,
                                                   aura::Window* lost_active) {
  if (!gained_active)
    return;

  // Don't count popup windows.
  if (gained_active->GetType() != aura::client::WINDOW_TYPE_NORMAL)
    return;

  chromeos::AppType app_type = GetAppType(gained_active);

  std::string app_id;
  if (app_type == chromeos::AppType::ARC_APP) {
    const std::string* package_name = GetArcPackageName(gained_active);

    if (!package_name) {
      // The package name property for the window has not been set yet.
      // Listen for changes to the window properties so we can
      // be informed when the package name gets set.
      if (!gained_active->HasObserver(
              unique_apps_arc_package_name_observer_.get())) {
        unique_apps_arc_package_name_observer_->ObserveWindow(gained_active);
      }
      return;
    }
    app_id = *package_name;
  } else {
    // This is a non-ARC window, so we just get the shelf ID, which should
    // be unique per app.
    app_id = GetShelfID(gained_active).app_id;
  }

  // Some app_ids are empty, i.e the "You will be signed out
  // in X seconds" modal dialog in Demo Mode, so skip those.
  if (app_id.empty())
    return;

  RecordAppLaunch(app_id, app_type);
}

void DemoSessionMetricsRecorder::OnUserActivity(const ui::Event* event) {
  // Record the first and last time activity was observed.
  if (first_user_activity_.is_null()) {
    first_user_activity_ = base::TimeTicks::Now();
  }
  last_user_activity_ = base::TimeTicks::Now();

  // Report samples recorded since the last activity.
  ReportSamples();

  // Restart the timer if the device has been idle.
  if (!timer_->IsRunning())
    StartRecording();
  periods_since_activity_ = 0;
}

void DemoSessionMetricsRecorder::OnMouseEvent(ui::MouseEvent* event) {
  // If event type is mouse/trackpad clicking, increase the metric by one.
  if (event->type() == ui::EventType::kMousePressed) {
    user_clicks_and_presses_++;
  }
}

void DemoSessionMetricsRecorder::OnTouchEvent(ui::TouchEvent* event) {
  // If event type is screen pressing, increase the metric by one.
  if (event->type() == ui::EventType::kTouchPressed) {
    user_clicks_and_presses_++;
  }
}

void DemoSessionMetricsRecorder::StartRecording() {
  unique_apps_launched_recording_enabled_ = true;
  timer_->Start(FROM_HERE, kSamplePeriod, this,
                &DemoSessionMetricsRecorder::TakeSampleOrPause);
}

void DemoSessionMetricsRecorder::RecordActiveAppSample(DemoModeApp app) {
  unreported_samples_.push_back(app);
}

void DemoSessionMetricsRecorder::TakeSampleOrPause() {
  // After enough inactive time, assume the user left.
  if (++periods_since_activity_ > kMaxPeriodsWithoutActivity) {
    // These samples were collected since the last user activity.
    unreported_samples_.clear();
    timer_->Stop();
    return;
  }

  aura::Window* window = Shell::Get()->activation_client()->GetActiveWindow();
  if (!window)
    return;

  // If there is no ARC package name available, set up a listener
  // to be informed when it is available.
  if (IsArcWindow(window) && !CanGetAppFromWindow(window)) {
    active_app_arc_package_name_observer_->ObserveWindow(window);
    return;
  }

  DemoModeApp app = window->GetType() == aura::client::WINDOW_TYPE_NORMAL
                        ? GetAppFromWindow(window)
                        : DemoModeApp::kOtherWindow;
  RecordActiveAppSample(app);
}

void DemoSessionMetricsRecorder::ReportSamples() {
  for (DemoModeApp app : unreported_samples_)
    UMA_HISTOGRAM_ENUMERATION("DemoMode.ActiveApp", app);
  unreported_samples_.clear();
}

void DemoSessionMetricsRecorder::ReportUniqueAppsLaunched() {
  if (unique_apps_launched_recording_enabled_)
    UMA_HISTOGRAM_COUNTS_100("DemoMode.UniqueAppsLaunched",
                             unique_apps_launched_.size());
  unique_apps_launched_.clear();
}

void DemoSessionMetricsRecorder::ReportDwellTime() {
  if (!first_user_activity_.is_null()) {
    DCHECK(!last_user_activity_.is_null());
    DCHECK_LE(first_user_activity_, last_user_activity_);

    base::TimeDelta dwell_time = last_user_activity_ - first_user_activity_;
    ReportHistogramLongSecondsTimes100("DemoMode.DwellTime", dwell_time);
  }
  first_user_activity_ = base::TimeTicks();
  last_user_activity_ = base::TimeTicks();
}

void DemoSessionMetricsRecorder::ReportUserClickesAndPresses() {
  UMA_HISTOGRAM_COUNTS_1000(
      DemoSessionMetricsRecorder::kUserClicksAndPressesMetric,
      user_clicks_and_presses_);
}

}  // namespace ash
