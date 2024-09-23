// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_METRICS_HELPER_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_METRICS_HELPER_H_

#include <map>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"

namespace ash {

// Handles metrics for OOBE.
class OobeMetricsHelper {
 public:
  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class ScreenShownStatus { kSkipped = 0, kShown = 1, kMaxValue = kShown };

  // The type of flow completed when pre-login OOBE is completed.
  enum class CompletedPreLoginOobeFlowType {
    kAutoEnrollment = 0,
    kDemo = 1,
    kRegular = 2
  };

  // Observer that is notified on certain OOBE recording events.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override = default;

    // Invoked when `screen shown status` metrics are being reported.
    virtual void OnScreenShownStatusChanged(OobeScreenId screen,
                                            ScreenShownStatus status) {}

    // Invoked when `screen exit` metrics are being reported.
    virtual void OnScreenExited(OobeScreenId screen,
                                const std::string& exit_reason) {}

    // Invoked when `pre login OOBE flow start` metrics are being reported.
    virtual void OnPreLoginOobeFirstStarted() {}

    // Invoked when `pre login OOBE flow complete` metrics are being reported.
    virtual void OnPreLoginOobeCompleted(
        CompletedPreLoginOobeFlowType flow_type) {}

    // Invoked when `onboarding flow start` metrics are being reported.
    virtual void OnOnboardingStarted() {}

    // Invoked when `onboarding complete` metrics are being reported.
    virtual void OnOnboardingCompleted() {}

    // Invoked when `device registered` metrics are being reported.
    virtual void OnDeviceRegistered() {}

    // Invoked when `GAIA sign-in request` metrics are being reported.
    virtual void OnGaiaSignInRequested(GaiaView::GaiaLoginVariant variant) {}

    // Invoked when `GAIA sign-in complete` metrics are being reported.
    virtual void OnGaiaSignInCompleted(GaiaView::GaiaLoginVariant variant) {}

    // Invoked when `pre login OOBE flow resume` metrics are being reported.
    virtual void OnPreLoginOobeResumed(OobeScreenId screen) {}

    // Invoked when `onboarding resume` metrics are being reported.
    virtual void OnOnboardingResumed(OobeScreenId screen) {}

    // Invoked when `CHOOBE resume` metrics are being reported.
    virtual void OnChoobeResumed() {}
  };

  OobeMetricsHelper();
  ~OobeMetricsHelper();
  OobeMetricsHelper(const OobeMetricsHelper& other) = delete;
  OobeMetricsHelper& operator=(const OobeMetricsHelper&) = delete;

  // Called when the status of a screen during the flow is determined,
  // shown/skipped.
  void RecordScreenShownStatus(OobeScreenId screen, ScreenShownStatus status);

  // Called when the screen is exited, this should be preceded by a call to
  // `OnScreenShownStatusDetermined()`.
  void RecordScreenExit(OobeScreenId screen, const std::string& exit_reason);

  // Called the first time pre-login OOBE has started. This method will not be
  // called again if the device restarts into the pre-login flow.
  void RecordPreLoginOobeFirstStart();

  // Called upon marking pre-login OOBE as completed.
  void RecordPreLoginOobeComplete(CompletedPreLoginOobeFlowType screen);

  // Called after the log-in of a new user is completed and before the showing
  // of the first onboarding screen. If this is the first onboarding after OOBE
  // completion, the start time of OOBE should be passed to the method,
  // otherwise, the NULL time should be passed.
  void RecordOnboardingStart(base::Time oobe_start_time);

  // Called after the last screen of the onboarding flow is exited and before
  // the session starts.
  // A NULL time in either `oobe_start_time` or `onboarding_start_time` means
  // that the start time is not available.
  void RecordOnboadingComplete(base::Time oobe_start_time,
                               base::Time onboarding_start_time);

  // Called when `StartupUtils::MarkDeviceRegistered()` is called.
  void RecordDeviceRegistered();

  // Called after the user enters the password in GAIA flow and continues to the
  // authentication.
  void RecordGaiaSignInRequested(GaiaView::GaiaLoginVariant variant);

  // Called after GAIA authentication is completed successfully
  void RecordGaiaSignInCompleted(GaiaView::GaiaLoginVariant variant);

  // Called after the decision to resume prelogin OOBE from `screen`.
  void RecordPreLoginOobeResume(OobeScreenId screen);

  // Called after the decision to resume onboarding from `screen`.
  void RecordOnboardingResume(OobeScreenId screen);

  // Called after the decision to resume CHOOBE flow.
  void RecordChoobeResume();

  // Called when `ShowEnrollmentScreen()` is called.
  void RecordEnrollingUserType();

  void RecordChromeVersion();

  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

 private:
  void RecordUpdatedStepShownStatus(OobeScreenId screen,
                                    ScreenShownStatus status);
  void RecordUpdatedStepCompletionTime(OobeScreenId screen,
                                       base::TimeDelta step_time);

  // A callback triggered by StatsReportingController upon changes to the
  // enabled status of the metrics. This is to record whether
  // `StatsReportingController` ever reports the enabled status of metrics to be
  // `true` then `false` during OOBE.
  void OnStatsReportingSettingUpdated();

  // Maps screen names to last time of their shows.
  std::map<OobeScreenId, base::TimeTicks> screen_show_times_;

  base::CallbackListSubscription stats_reporting_subscription_;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_METRICS_HELPER_H_
