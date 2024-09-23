// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_metrics_helper.h"

#include <map>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/auto_enrollment_check_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/consumer_update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/demo_preferences_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/demo_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enrollment_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_info_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/packaged_license_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/startup_metric_utils/common/startup_metric_utils.h"
#include "components/version_info/version_info.h"

namespace ash {

namespace {

// Legacy histogram, use legacy screen names.
constexpr char kUmaScreenShownStatusName[] = "OOBE.StepShownStatus.";
// Legacy histogram, use legacy screen names.
constexpr char kUmaScreenCompletionTimeName[] = "OOBE.StepCompletionTime.";

// Updated histograms to replace legacy ones.
constexpr char kUmaScreenShownStatusName2[] = "OOBE.StepShownStatus2.";
constexpr char kUmaScreenCompletionTimeName2[] = "OOBE.StepCompletionTime2.";

constexpr char kUmaStepCompletionTimeByExitReasonName[] =
    "OOBE.StepCompletionTimeByExitReason.";
constexpr char kUmaBootToOobeCompleted[] = "OOBE.BootToOOBECompleted.";

constexpr char kUmaOobeFlowStatus[] = "OOBE.OobeFlowStatus";
constexpr char kUmaOobeFlowDuration[] = "OOBE.OobeFlowDuration";
constexpr char kUmaOnboardingFlowStatus[] = "OOBE.OnboardingFlowStatus.";
constexpr char kUmaOnboardingFlowDuration[] = "OOBE.OnboardingFlowDuration.";
constexpr char kUmaOobeStartToOnboardingStart[] =
    "OOBE.OobeStartToOnboardingStartTime";

constexpr char kUmaFirstOnboardingSuffix[] = "FirstOnboarding";
constexpr char kUmaSubsequentOnboardingSuffix[] = "SubsequentOnboarding";

constexpr char kUmaOobeMetricsClientIdReset[] = "OOBE.MetricsClientIdReset";
constexpr char kUmaOobeMetricsClientIdReset2[] = "OOBE.MetricsClientIdReset2";
constexpr char kUmaOobeStatsReportingControllerReportedReset[] =
    "OOBE.StatsReportingControllerReportedReset";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This should be kept in sync with
// OobeMetricsClientIdResetState in enums.xml.
enum class OobeMetricsClientIdResetState {
  kNoMetricsClientIDReset = 0,
  kMetricsClientIDReset = 1,
  kInitialIDMissing = 2,
  kCurrentIDMissing = 3,

  kMaxValue = kCurrentIDMissing
};

struct LegacyScreenNameEntry {
  StaticOobeScreenId screen;
  const char* uma_name;
};

// Some screens had multiple different names in the past (they have since been
// unified). We need to always use the same name for UMA stats, though.
constexpr const LegacyScreenNameEntry kUmaLegacyScreenName[] = {
    {EnrollmentScreenView::kScreenId, "enroll"},
    {WelcomeView::kScreenId, "network"},
    {TermsOfServiceScreenView::kScreenId, "tos"}};

// This list must be kept in sync with `OOBEOnlyScreenName` variants in
// src/tools/metrics/histograms/metadata/oobe/histograms.xml file.
const StaticOobeScreenId kOobeOnlyScreenNames[] = {
    AutoEnrollmentCheckScreenView::kScreenId,
    WelcomeView::kScreenId,
    ConsumerUpdateScreenView::kScreenId,
    EnableDebuggingScreenView::kScreenId,
    DemoPreferencesScreenView::kScreenId,
    DemoPreferencesScreenView::kScreenId,
    EnrollmentScreenView::kScreenId,
    GaiaInfoScreenView::kScreenId,
    HIDDetectionView::kScreenId,
    NetworkScreenView::kScreenId,
    PackagedLicenseView::kScreenId,
    QuickStartView::kScreenId,
    UpdateView::kScreenId,
};

std::string GetUmaLegacyScreenName(const OobeScreenId& screen_id) {
  // Make sure to use initial UMA name if the name has changed.
  std::string uma_name = screen_id.name;
  for (const auto& entry : kUmaLegacyScreenName) {
    if (entry.screen.AsId() == screen_id) {
      uma_name = entry.uma_name;
      break;
    }
  }
  uma_name[0] = base::ToUpperASCII(uma_name[0]);
  return uma_name;
}

std::string GetCaptializedScreenName(const OobeScreenId& screen_id) {
  std::string id = screen_id.name;
  id[0] = base::ToUpperASCII(id[0]);
  return id;
}

bool IsOobeOnlyScreen(const OobeScreenId& screen_id) {
  for (const auto& oobe_screen : kOobeOnlyScreenNames) {
    if (screen_id == oobe_screen) {
      return true;
    }
  }
  return false;
}

std::string GetOnboardingTypeSuffix() {
  base::Time oobe_time =
      g_browser_process->local_state()->GetTime(prefs::kOobeStartTime);
  return oobe_time.is_null() ? kUmaSubsequentOnboardingSuffix
                             : kUmaFirstOnboardingSuffix;
}

std::string GetMetricsClientID() {
  std::string client_id;
  if (g_browser_process->metrics_service()) {
    client_id = g_browser_process->metrics_service()->GetClientId();
  }

  // Early in OOBE `metrics_service()->GetClientId()` will return an empty
  // string. If that's the case look for the client ID in the preference
  // `kMetricsProvisionalClientID`.
  if (client_id.empty()) {
    client_id = g_browser_process->local_state()->GetString(
        metrics::prefs::kMetricsProvisionalClientID);
  }

  return client_id;
}

}  // namespace

OobeMetricsHelper::OobeMetricsHelper() {
  if (StatsReportingController::IsInitialized()) {
    stats_reporting_subscription_ =
        StatsReportingController::Get()->AddObserver(base::BindRepeating(
            &OobeMetricsHelper::OnStatsReportingSettingUpdated,
            base::Unretained(this)));
  }
}

OobeMetricsHelper::~OobeMetricsHelper() = default;

void OobeMetricsHelper::RecordScreenShownStatus(OobeScreenId screen,
                                                ScreenShownStatus status) {
  // Notify registered observers.
  for (auto& observer : observers_) {
    observer.OnScreenShownStatusChanged(screen, status);
  }

  if (status == ScreenShownStatus::kShown) {
    screen_show_times_[screen] = base::TimeTicks::Now();
  }

  // Legacy histogram, requires old screen names.
  std::string screen_name = GetUmaLegacyScreenName(screen);
  std::string histogram_name = kUmaScreenShownStatusName + screen_name;
  base::UmaHistogramEnumeration(histogram_name, status);

  RecordUpdatedStepShownStatus(screen, status);
}

void OobeMetricsHelper::RecordUpdatedStepShownStatus(OobeScreenId screen,
                                                     ScreenShownStatus status) {
  // New histogram, don't use legacy screen names.
  std::string screen_name = GetCaptializedScreenName(screen);
  std::string histogram_name = kUmaScreenShownStatusName2 + screen_name;
  if (!IsOobeOnlyScreen(screen)) {
    histogram_name += "." + GetOnboardingTypeSuffix();
  }
  base::UmaHistogramEnumeration(histogram_name, status);
}

void OobeMetricsHelper::RecordScreenExit(OobeScreenId screen,
                                         const std::string& exit_reason) {
  // Notify registered observers.
  for (auto& observer : observers_) {
    observer.OnScreenExited(screen, exit_reason);
  }

  // Legacy histogram, requires old screen names.
  std::string legacy_screen_name = GetUmaLegacyScreenName(screen);
  std::string histogram_name =
      kUmaScreenCompletionTimeName + legacy_screen_name;

  base::TimeDelta step_time =
      base::TimeTicks::Now() - screen_show_times_[screen];
  base::UmaHistogramMediumTimes(histogram_name, step_time);

  RecordUpdatedStepCompletionTime(screen, step_time);

  // Use for this histogram real screen names.
  std::string screen_name = GetCaptializedScreenName(screen);
  std::string histogram_name_with_reason =
      kUmaStepCompletionTimeByExitReasonName + screen_name + "." + exit_reason;

  base::UmaHistogramCustomTimes(histogram_name_with_reason, step_time,
                                base::Milliseconds(10), base::Minutes(10), 100);
}

void OobeMetricsHelper::RecordUpdatedStepCompletionTime(
    OobeScreenId screen,
    base::TimeDelta step_time) {
  // New histogram, don't use legacy screen names.
  std::string screen_name = GetCaptializedScreenName(screen);
  std::string histogram_name = kUmaScreenCompletionTimeName2 + screen_name;
  if (!IsOobeOnlyScreen(screen)) {
    histogram_name += "." + GetOnboardingTypeSuffix();
  }
  base::UmaHistogramCustomTimes(histogram_name, step_time,
                                base::Milliseconds(10), base::Minutes(10), 100);
}

void OobeMetricsHelper::RecordPreLoginOobeFirstStart() {
  // Notify registered observers.
  for (auto& observer : observers_) {
    observer.OnPreLoginOobeFirstStarted();
  }

  // Record `False` to report the `Started` bucket.
  base::UmaHistogramBoolean(kUmaOobeFlowStatus, false);

  // Store the metrics client ID to be later compared to the metrics
  // client ID at the end of OOBE.
  g_browser_process->local_state()->SetString(
      prefs::kOobeMetricsClientIdAtOobeStart, GetMetricsClientID());
}

void OobeMetricsHelper::RecordPreLoginOobeComplete(
    CompletedPreLoginOobeFlowType flow_type) {
  // Notify registered observers.
  for (auto& observer : observers_) {
    observer.OnPreLoginOobeCompleted(flow_type);
  }

  base::TimeTicks startup_time =
      startup_metric_utils::GetCommon().MainEntryPointTicks();
  if (startup_time.is_null()) {
    return;
  }
  base::TimeDelta delta = base::TimeTicks::Now() - startup_time;

  std::string type_string;
  switch (flow_type) {
    case CompletedPreLoginOobeFlowType::kAutoEnrollment:
      type_string = "AutoEnrollment";
      break;
    case CompletedPreLoginOobeFlowType::kDemo:
      type_string = "Demo";
      break;
    case CompletedPreLoginOobeFlowType::kRegular:
      type_string = "Regular";
      break;
  }
  std::string histogram_name = kUmaBootToOobeCompleted + type_string;
  base::UmaHistogramCustomTimes(histogram_name, delta, base::Milliseconds(10),
                                base::Minutes(10), 100);
}

void OobeMetricsHelper::RecordOnboardingStart(base::Time oobe_start_time) {
  // Notify registered observers.
  for (auto& observer : observers_) {
    observer.OnOnboardingStarted();
  }

  if (!oobe_start_time.is_null()) {
    base::UmaHistogramCustomTimes(
        kUmaOobeStartToOnboardingStart, base::Time::Now() - oobe_start_time,
        base::Milliseconds(10), base::Minutes(30), 100);
  }

  // Record `False` to report the `Started` bucket.
  base::UmaHistogramBoolean(
      kUmaOnboardingFlowStatus + GetOnboardingTypeSuffix(), false);
}

void OobeMetricsHelper::RecordOnboadingComplete(
    base::Time oobe_start_time,
    base::Time onboarding_start_time) {
  for (auto& observer : observers_) {
    observer.OnOnboardingCompleted();
  }

  if (!oobe_start_time.is_null()) {
    // Record `True` to report the `Completed` bucket.
    base::UmaHistogramBoolean(kUmaOobeFlowStatus, true);
    base::UmaHistogramLongTimes(kUmaOobeFlowDuration,
                                base::Time::Now() - oobe_start_time);

    // Record `Metrics Client ID Reset` histograms.
    std::string initial_id = g_browser_process->local_state()->GetString(
        prefs::kOobeMetricsClientIdAtOobeStart);

    if (initial_id.empty()) {
      base::UmaHistogramEnumeration(
          kUmaOobeMetricsClientIdReset2,
          OobeMetricsClientIdResetState::kInitialIDMissing);
    } else {
      std::string current_id = GetMetricsClientID();
      if (current_id.empty()) {
        base::UmaHistogramEnumeration(
            kUmaOobeMetricsClientIdReset2,
            OobeMetricsClientIdResetState::kCurrentIDMissing);

      } else {
        if (initial_id == current_id) {
          base::UmaHistogramBoolean(kUmaOobeMetricsClientIdReset, false);
          base::UmaHistogramEnumeration(
              kUmaOobeMetricsClientIdReset2,
              OobeMetricsClientIdResetState::kNoMetricsClientIDReset);
        } else {
          base::UmaHistogramBoolean(kUmaOobeMetricsClientIdReset, true);
          base::UmaHistogramEnumeration(
              kUmaOobeMetricsClientIdReset2,
              OobeMetricsClientIdResetState::kMetricsClientIDReset);
        }
      }
    }

    // Record whether `StatsReportingController` reported a reset during OOBE.
    base::UmaHistogramBoolean(
        kUmaOobeStatsReportingControllerReportedReset,
        g_browser_process->local_state()->GetBoolean(
            prefs::kOobeStatsReportingControllerReportedReset));
  }

  if (!onboarding_start_time.is_null()) {
    std::string type = GetOnboardingTypeSuffix();

    // Record `True` to report the `Completed` bucket.
    base::UmaHistogramBoolean(kUmaOnboardingFlowStatus + type, true);
    base::UmaHistogramCustomTimes(kUmaOnboardingFlowDuration + type,
                                  base::Time::Now() - onboarding_start_time,
                                  base::Milliseconds(1), base::Minutes(30),
                                  100);
  }
}

void OobeMetricsHelper::RecordGaiaSignInRequested(
    GaiaView::GaiaLoginVariant variant) {
  for (auto& observer : observers_) {
    observer.OnGaiaSignInRequested(variant);
  }

  base::UmaHistogramEnumeration("OOBE.GaiaScreen.LoginRequests", variant);
}

void OobeMetricsHelper::RecordGaiaSignInCompleted(
    GaiaView::GaiaLoginVariant variant) {
  for (auto& observer : observers_) {
    observer.OnGaiaSignInCompleted(variant);
  }

  base::UmaHistogramEnumeration("OOBE.GaiaScreen.SuccessLoginRequests",
                                variant);
}

void OobeMetricsHelper::RecordPreLoginOobeResume(OobeScreenId screen) {
  for (auto& observer : observers_) {
    observer.OnPreLoginOobeResumed(screen);
  }
}

void OobeMetricsHelper::RecordOnboardingResume(OobeScreenId screen) {
  for (auto& observer : observers_) {
    observer.OnOnboardingResumed(screen);
  }
}

void OobeMetricsHelper::RecordChoobeResume() {
  for (auto& observer : observers_) {
    observer.OnChoobeResumed();
  }
}

void OobeMetricsHelper::RecordEnrollingUserType() {
  bool is_consumer = g_browser_process->local_state()->GetBoolean(
      prefs::kOobeIsConsumerSegment);
  base::UmaHistogramBoolean("OOBE.Enrollment.IsUserEnrollingAConsumer",
                            is_consumer);
}

void OobeMetricsHelper::RecordDeviceRegistered() {
  for (auto& observer : observers_) {
    observer.OnDeviceRegistered();
  }
}

void OobeMetricsHelper::RecordChromeVersion() {
  base::UmaHistogramSparse("OOBE.ChromeVersionBeforeUpdate",
                           version_info::GetMajorVersionNumberAsInt());
}

void OobeMetricsHelper::OnStatsReportingSettingUpdated() {
  // Return if the method was called outside of OOBE or the first onboarding
  // experience.
  if (g_browser_process->local_state()
          ->GetTime(prefs::kOobeStartTime)
          .is_null()) {
    return;
  }

  bool enabled = StatsReportingController::Get()->IsEnabled();
  if (enabled) {
    g_browser_process->local_state()->SetBoolean(
        prefs::kOobeMetricsReportedAsEnabled, true);
  } else if (g_browser_process->local_state()->GetBoolean(
                 prefs::kOobeMetricsReportedAsEnabled)) {
    base::debug::DumpWithoutCrashing();

    // It's possible that the `dump` will never be uploaded due to the client
    // ID reset, therefore, store that metrics was reset in OOBE to report it
    // at at later time.
    g_browser_process->local_state()->SetBoolean(
        prefs::kOobeStatsReportingControllerReportedReset, true);
  }
}

void OobeMetricsHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OobeMetricsHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
