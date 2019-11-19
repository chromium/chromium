// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/demo_session_metrics_recorder.h"

#include <iostream>
#include <string>
#include <utility>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shelf/shelf_window_watcher.h"
#include "ash/shell.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

using DemoModeApp = DemoSessionMetricsRecorder::DemoModeApp;

// How often to sample.
constexpr auto kSamplePeriod = base::TimeDelta::FromSeconds(1);

// How many periods to wait for user activity before discarding samples.
// This timeout is low because demo sessions tend to be very short. If we
// recorded samples for a full minute while the device is in between uses, we
// would bias our measurements toward whatever app was used last.
constexpr int kMaxPeriodsWithoutActivity =
    base::TimeDelta::FromSeconds(15) / kSamplePeriod;

// Maps a Chrome app ID to a DemoModeApp value for metrics.
DemoModeApp GetAppFromAppId(const std::string& app_id) {
  // Each version of the Highlights app is bucketed into the same value.
  if (app_id == extension_misc::kHighlightsAppId ||
      app_id == extension_misc::kHighlightsEveAppId ||
      app_id == extension_misc::kHighlightsNocturneAppId ||
      app_id == extension_misc::kHighlightsAltAppId) {
    return DemoModeApp::kHighlights;
  }

  // Each version of the Screensaver app is bucketed into the same value.
  if (app_id == extension_misc::kScreensaverAppId ||
      app_id == extension_misc::kScreensaverEveAppId ||
      app_id == extension_misc::kScreensaverNocturneAppId ||
      app_id == extension_misc::kScreensaverAltAppId) {
    return DemoModeApp::kScreensaver;
  }

  if (app_id == extension_misc::kCameraAppId)
    return DemoModeApp::kCamera;
  if (app_id == extension_misc::kChromeAppId)
    return DemoModeApp::kBrowser;
  if (app_id == extension_misc::kFilesManagerAppId)
    return DemoModeApp::kFiles;
  if (app_id == extension_misc::kGeniusAppId)
    return DemoModeApp::kGetHelp;
  if (app_id == extension_misc::kGoogleKeepAppId)
    return DemoModeApp::kGoogleKeep;
  if (app_id == extensions::kWebStoreAppId)
    return DemoModeApp::kWebStore;
  if (app_id == extension_misc::kYoutubeAppId)
    return DemoModeApp::kYouTube;
  return DemoModeApp::kOtherChromeApp;
}

// Maps an ARC++ package name to a DemoModeApp value for metrics.
DemoModeApp GetAppFromPackageName(const std::string& package_name) {
  // Google apps.
  if (package_name == "com.google.Photos")
    return DemoModeApp::kGooglePhotos;
  if (package_name == "com.google.Sheets")
    return DemoModeApp::kGoogleSheets;
  if (package_name == "com.google.Slides")
    return DemoModeApp::kGoogleSlides;
  if (package_name == "com.android.vending")
    return DemoModeApp::kPlayStore;

  // Third-party apps.
  if (package_name == "com.gameloft.android.ANMP.GloftA8HMD")
    return DemoModeApp::kAsphalt8;
  if (package_name == "com.brakefield.painter")
    return DemoModeApp::kInfinitePainter;
  if (package_name == "com.myscript.nebo.demo")
    return DemoModeApp::kMyScriptNebo;
  if (package_name == "com.steadfastinnovation.android.projectpapyrus")
    return DemoModeApp::kSquid;

  return DemoModeApp::kOtherArcApp;
}

AppType GetAppType(const aura::Window* window) {
  return static_cast<AppType>(window->GetProperty(aura::client::kAppType));
}

bool IsArcWindow(const aura::Window* window) {
  return (GetAppType(window) == AppType::ARC_APP);
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

  AppType app_type = GetAppType(window);
  if (app_type == AppType::ARC_APP) {
    // The ShelfID app id isn't used to identify ARC++ apps since it's a hash of
    // both the package name and the activity.
    const std::string* package_name = GetArcPackageName(window);
    return GetAppFromPackageName(*package_name);
  }

  std::string app_id = GetShelfID(window).app_id;

  // The Chrome "app" in the shelf is just the browser.
  if (app_id == extension_misc::kChromeAppId)
    return DemoModeApp::kBrowser;

  // If the window is the "browser" type, having an app ID other than the
  // default indicates a hosted/bookmark app.
  if (app_type == AppType::CHROME_APP ||
      (app_type == AppType::BROWSER && !app_id.empty())) {
    return GetAppFromAppId(app_id);
  }

  if (app_type == AppType::BROWSER)
    return DemoModeApp::kBrowser;
  return DemoModeApp::kOtherWindow;
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

    scoped_observer_.Remove(window);
  }

  void OnWindowDestroyed(aura::Window* window) override {
    if (scoped_observer_.IsObserving(window))
      scoped_observer_.Remove(window);
  }

  void ObserveWindow(aura::Window* window) { scoped_observer_.Add(window); }

 private:
  DemoSessionMetricsRecorder* metrics_recorder_;
  ScopedObserver<aura::Window, aura::WindowObserver> scoped_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ActiveAppArcPackageNameObserver);
};

// Observes changes in a window's ArcPackageName property for the purpose of
// logging of unique launches of ARC apps.
class DemoSessionMetricsRecorder::UniqueAppsLaunchedArcPackageNameObserver
    : public aura::WindowObserver {
 public:
  explicit UniqueAppsLaunchedArcPackageNameObserver(
      DemoSessionMetricsRecorder* metrics_recorder)
      : metrics_recorder_(metrics_recorder) {}

  // aura::WindowObserver
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key != kArcPackageNameKey)
      return;

    const std::string* package_name = GetArcPackageName(window);

    if (package_name) {
      metrics_recorder_->RecordAppLaunch(*package_name, AppType::ARC_APP);
    } else {
      VLOG(1) << "Got null ARC package name";
    }

    scoped_observer_.Remove(window);
  }

  void OnWindowDestroyed(aura::Window* window) override {
    if (scoped_observer_.IsObserving(window))
      scoped_observer_.Remove(window);
  }

  void ObserveWindow(aura::Window* window) { scoped_observer_.Add(window); }

 private:
  DemoSessionMetricsRecorder* metrics_recorder_;
  ScopedObserver<aura::Window, aura::WindowObserver> scoped_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(UniqueAppsLaunchedArcPackageNameObserver);
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
  observer_.Add(ui::UserActivityDetector::Get());

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
  // Report any remaining stored samples on exit. (If the user went idle, there
  // won't be any.)
  ReportSamples();

  // Unsubscribe from window activation events.
  activation_client_->RemoveObserver(this);

  ReportUniqueAppsLaunched();
}

void DemoSessionMetricsRecorder::RecordAppLaunch(const std::string& id,
                                                 AppType app_type) {
  if (!ShouldRecordAppLaunch(id)) {
    return;
  }
  DemoModeApp app;
  if (app_type == AppType::ARC_APP)
    app = GetAppFromPackageName(id);
  else
    app = GetAppFromAppId(id);

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
  if (gained_active->type() != aura::client::WINDOW_TYPE_NORMAL)
    return;

  AppType app_type = GetAppType(gained_active);

  std::string app_id;
  if (app_type == AppType::ARC_APP) {
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
  // Report samples recorded since the last activity.
  ReportSamples();

  // Restart the timer if the device has been idle.
  if (!timer_->IsRunning())
    StartRecording();
  periods_since_activity_ = 0;
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

  DemoModeApp app = window->type() == aura::client::WINDOW_TYPE_NORMAL
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

}  // namespace ash
