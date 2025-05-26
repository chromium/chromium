// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_DEMO_SESSION_METRICS_RECORDER_H_
#define ASH_METRICS_DEMO_SESSION_METRICS_RECORDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chromeos/ui/base/app_types.h"
#include "ui/aura/window_observer.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "ui/events/event_handler.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace base {
class RepeatingTimer;
}  // namespace base

namespace ash {

// A metrics recorder for demo sessions that samples the active window's app or
// window type. Only used when the device is in Demo Mode.
class ASH_EXPORT DemoSessionMetricsRecorder
    : public ui::UserActivityObserver,
      public wm::ActivationChangeObserver,
      public ui::EventHandler {
 public:
  // These apps are preinstalled in Demo Mode. This list is not exhaustive, and
  // includes first- and third-party Chrome and ARC apps.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class DemoModeApp {
    kBrowser = 0,
    kOtherChromeApp = 1,
    kOtherArcApp = 2,
    kOtherWindow = 3,
    kHighlights = 4,  // Auto-launched Demo Mode app highlighting CrOS features.
    kAsphalt8 = 5,    // Android racing game demo app.
    kCamera = 6,
    kFiles = 7,
    kGetHelp = 8,
    kGoogleKeepChromeApp = 9,
    kGooglePhotos = 10,
    kGoogleSheetsAndroidApp = 11,
    kGoogleSlidesAndroidApp = 12,
    kInfinitePainter = 13,  // Android painting app.
    kMyScriptNebo = 14,     // Android note-taking app.
    kPlayStore = 15,
    kSquid = 16,  // Android note-taking app.
    kWebStore = 17,
    kYouTube = 18,
    kScreensaver = 19,    // Demo Mode screensaver app.
    kAsphalt9 = 20,       // Android racing game demo app.
    kStardewValley = 21,  // Android farming game demo app.
    kKinemaster = 22,     // Android video editing software demo app. nocheck
    kGoogleKeepAndroidApp = 23,
    kAutoCAD = 24,     // Android 2D/3D drawing software demo app.
    kPixlr = 25,       // Android photo editing software demo app.
    kCalculator = 26,  // Essential apps calculator.
    kCalendar = 27,
    kGoogleDocsChromeApp = 28,
    kGoogleSheetsChromeApp = 29,
    kGoogleSlidesChromeApp = 30,
    kYoutubePwa = 31,
    kGoogleDocsPwa = 32,
    kGoogleMeetPwa = 33,
    kGoogleSheetsPwa = 34,
    kSpotify = 35,
    kBeFunky = 36,
    kClipchamp = 37,
    kGeForceNow = 38,
    kZoom = 39,
    kSumo = 40,
    kAdobeSpark = 41,
    kMinecraft = 42,
    // Add future entries above this comment, in sync with enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kMinecraft,
  };

  enum class ExitSessionFrom {
    kShelf = 0,
    kSystemTray = 1,
    kSystemTrayPowerButton = 2,
  };

  // The types of the result of the demo account setup or cleanup request.
  // This enum is tied directly to a UMA enum
  // `DemoModeSignedInAccountRequestResult`, and should always reflect it (do
  // not change one without changing the other). Entries should never be
  // modified or reordered. Entries can only be removed by deprecating it and
  // its value should never be reused. New ones should be added to the end
  // (right before the max value).
  enum class DemoAccountRequestResultCode {
    kSuccess = 0,               // Demo account request success.
    kResponseParsingError = 1,  // Malformat Http response.
    kInvalidCreds = 2,          // Missing required credential for login.
    kEmptyResponse = 3,         // Empty Http response.
    kNetworkError = 4,          // Network error.
    kRequestFailed = 5,         // Server side error or out of quota.
    kCloudPolicyNotConnected =
        6,  // Unable to obtain the DM Token and the Client ID due to the cloud
            // policy not connected.
    kEmptyDMToken = 7,   // The DM Token on the device is empty.
    kEmptyClientID = 8,  // The Client ID on the device is empty.
    kQuotaExhaustedRetriable =
        9,  // Server quota exhausted, might be max QPS reached.
    kQuotaExhaustedNotRetriable =
        10,  // Server quota exhausted, device might be blocked.
    kMaxValue = kQuotaExhaustedNotRetriable,
  };

  // Types of the current demo session.
  //
  // It is worth noting that here is not perfectly accurate on the word
  // "current", because functions in `[demo_login_controller.cc]
  // LoginDemoAccount()` are asyc calls and they may fail with no failure
  // handlers. It may not reflect the actual current session type on failure,
  // but instead, it's more like an "upcoming" session type, also because it's
  // set before entering the session.
  //
  // However, when you get its value in `DemoSessionMetricsRecorder`, it is
  // reflecting the current session type as we're currently in the session.
  enum class SessionType {
    // Classic managed guest session.
    kClassicMGS = 0,
    // Signed-in demo session.
    kSignedInDemoSession = 1,
    // Fallback managed guest session due to the sign-in failure.
    kFallbackMGS = 2,
  };

  static constexpr char kUserClicksAndPressesMetric[] =
      "DemoMode.UserClicksAndPresses";

  static void RecordExitSessionAction(ExitSessionFrom recorded_from);

  // Getter of this class' instance.
  static DemoSessionMetricsRecorder* Get();

  // Records the result of the demo account setup request.
  static void ReportDemoAccountSetupResult(
      DemoAccountRequestResultCode result_code);

  // Records the result of the demo account cleanup request.
  static void ReportDemoAccountCleanupResult(
      DemoAccountRequestResultCode result_code);

  // It is used by Demo Mode only, and called by DemoLoginController before
  // entering the session, to set the upcoming session type.
  static void SetCurrentSessionType(SessionType session_type);

  // Get the type of the current demo session.
  static SessionType GetCurrentSessionTypeForTesting();

  // Records cloud policy connections timeout.
  static void RecordCloudPolicyConnectionTimeout();

  // The recorder will create a normal timer by default. Tests should provide a
  // mock timer to control sampling periods.
  explicit DemoSessionMetricsRecorder(
      std::unique_ptr<base::RepeatingTimer> timer = nullptr);

  DemoSessionMetricsRecorder(const DemoSessionMetricsRecorder&) = delete;
  DemoSessionMetricsRecorder& operator=(const DemoSessionMetricsRecorder&) =
      delete;

  ~DemoSessionMetricsRecorder() override;

  // ui::UserActivityObserver:
  void OnUserActivity(const ui::Event* event) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // Records the duration of time the user spent interacting with the current
  // demo mode signed-in shopper session, measured from first user activity to
  // last user activity.
  void ReportShopperSessionDwellTime();

  // Called by DemoModeWindowCloser::OnInstanceUpdate:
  // Passing `app_id_or_package` instead of `aura::Window` here because app
  // information set in window property might now be ready on app creation.
  void OnAppCreation(const std::string& app_id_or_package,
                     const bool is_arc_app);
  void OnAppDestruction(const std::string& app_id_or_package,
                        const bool is_arc_app);

 private:
  // Starts the timer for periodic sampling.
  void StartRecording();

  // Records the active window's app type or, if the user has been inactive for
  // too long, pauses sampling and wipes samples from the inactive period.
  void TakeSampleOrPause();

  // Emits histograms for recorded samples.
  void ReportSamples();

  // Records |app| as being seen while sampling all active apps.
  void RecordActiveAppSample(DemoModeApp app);

  // Indicates whether the specified app_id should be recorded for
  // the unique-apps-launched stat.
  bool ShouldRecordAppLaunch(const std::string& app_id);

  // Records the specified app's launch, subject to the
  // restrictions of ShouldRecordAppLaunch().
  void RecordAppLaunch(const std::string& id, chromeos::AppType app_type);

  // Emits various histograms for unique apps launched.
  void ReportUniqueAppsLaunched();

  // Records the duration of time the user spent interacting with the current
  // demo session, measured from first user activity to last user activity.
  void ReportDwellTime();

  // Records the number of times the user clicks mouse/trackpad and presses
  // screen in the demo session.
  void ReportUserClickesAndPresses();

  // Stores samples as they are collected. Report to UMA if we see user
  // activity soon after. Guaranteed not to grow too large.
  std::vector<DemoModeApp> unreported_samples_;

  // Indicates whether the unique-app-launch stats recording has been enabled.
  bool unique_apps_launched_recording_enabled_ = false;

  // Tracks the ids of apps that have been launched in Demo Mode.
  base::flat_set<std::string> unique_apps_launched_;

  // Used for subscribing to window activation events.
  raw_ptr<wm::ActivationClient> activation_client_ = nullptr;

  // How many periods have elapsed since the last user activity.
  int periods_since_activity_ = 0;

  // Indicates number of user clicks mouse/trackpad and presses screen with
  // demo mode in the current session.
  int user_clicks_and_presses_ = 0;

  base::TimeTicks first_user_activity_;

  base::TimeTicks last_user_activity_;

  base::TimeTicks shopper_session_first_user_activity_;

  std::unique_ptr<base::RepeatingTimer> timer_;

  base::ScopedObservation<ui::UserActivityDetector, ui::UserActivityObserver>
      observation_{this};

  class ActiveAppArcPackageNameObserver;
  class UniqueAppsLaunchedArcPackageNameObserver;

  std::unique_ptr<UniqueAppsLaunchedArcPackageNameObserver>
      unique_apps_arc_package_name_observer_;

  std::unique_ptr<ActiveAppArcPackageNameObserver>
      active_app_arc_package_name_observer_;

  // Tracks the app start time for app defined in `kAppsHistogramSuffix`.
  std::map<DemoModeApp, base::TimeTicks> apps_start_time_;
};

}  // namespace ash

#endif  // ASH_METRICS_DEMO_SESSION_METRICS_RECORDER_H_
