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
#include "chrome/browser/ui/webui/ash/login/enrollment_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "components/prefs/pref_service.h"
#include "components/startup_metric_utils/common/startup_metric_utils.h"
#include "components/version_info/version_info.h"

namespace ash {

namespace {

// Legacy histogram, use legacy screen names.
constexpr char kUmaScreenShownStatusName[] = "OOBE.StepShownStatus.";
// Legacy histogram, use legacy screen names.
constexpr char kUmaScreenCompletionTimeName[] = "OOBE.StepCompletionTime.";
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

}  // namespace

OobeMetricsHelper::OobeMetricsHelper() = default;

OobeMetricsHelper::~OobeMetricsHelper() = default;

void OobeMetricsHelper::OnScreenShownStatusDetermined(
    OobeScreenId screen,
    ScreenShownStatus status) {
  if (status == ScreenShownStatus::kShown) {
    screen_show_times_[screen] = base::TimeTicks::Now();
  }

  // Legacy histogram, requires old screen names.
  std::string screen_name = GetUmaLegacyScreenName(screen);
  std::string histogram_name = kUmaScreenShownStatusName + screen_name;
  base::UmaHistogramEnumeration(histogram_name, status);
}

void OobeMetricsHelper::OnScreenExited(OobeScreenId screen,
                                       const std::string& exit_reason) {
  // Legacy histogram, requires old screen names.
  std::string legacy_screen_name = GetUmaLegacyScreenName(screen);
  std::string histogram_name =
      kUmaScreenCompletionTimeName + legacy_screen_name;

  base::TimeDelta step_time =
      base::TimeTicks::Now() - screen_show_times_[screen];
  base::UmaHistogramMediumTimes(histogram_name, step_time);

  // Use for this histogram real screen names.
  std::string screen_name = screen.name;
  screen_name[0] = base::ToUpperASCII(screen_name[0]);
  std::string histogram_name_with_reason =
      kUmaStepCompletionTimeByExitReasonName + screen_name + "." + exit_reason;

  base::UmaHistogramCustomTimes(histogram_name_with_reason, step_time,
                                base::Milliseconds(10), base::Minutes(10), 100);
}

void OobeMetricsHelper::OnPreLoginOobeFirstStart() {
  // Record `False` to report the `Started` bucket.
  base::UmaHistogramBoolean(kUmaOobeFlowStatus, false);
}

void OobeMetricsHelper::OnPreLoginOobeCompleted(
    CompletedPreLoginOobeFlowType flow_type) {
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

void OobeMetricsHelper::OnOnboardingFlowStarted(base::Time oobe_start_time) {
  std::string onboarding_type;
  if (oobe_start_time.is_null()) {
    onboarding_type = kUmaSubsequentOnboardingSuffix;
  } else {
    base::UmaHistogramCustomTimes(
        kUmaOobeStartToOnboardingStart, base::Time::Now() - oobe_start_time,
        base::Milliseconds(10), base::Minutes(30), 100);
    onboarding_type = kUmaFirstOnboardingSuffix;
  }

  // Record `False` to report the `Started` bucket.
  base::UmaHistogramBoolean(kUmaOnboardingFlowStatus + onboarding_type, false);
}

void OobeMetricsHelper::OnOnboadingFlowCompleted(
    base::Time oobe_start_time,
    base::Time onboarding_start_time) {
  if (!oobe_start_time.is_null()) {
    // Record `True` to report the `Completed` bucket.
    base::UmaHistogramBoolean(kUmaOobeFlowStatus, true);
    base::UmaHistogramLongTimes(kUmaOobeFlowDuration,
                                base::Time::Now() - oobe_start_time);
  }

  if (!onboarding_start_time.is_null()) {
    std::string type = oobe_start_time.is_null()
                           ? kUmaSubsequentOnboardingSuffix
                           : kUmaFirstOnboardingSuffix;

    // Record `True` to report the `Completed` bucket.
    base::UmaHistogramBoolean(kUmaOnboardingFlowStatus + type, true);
    base::UmaHistogramCustomTimes(kUmaOnboardingFlowDuration + type,
                                  base::Time::Now() - onboarding_start_time,
                                  base::Milliseconds(1), base::Minutes(30),
                                  100);
  }
}

void OobeMetricsHelper::OnEnrollmentScreenShown() {
  bool is_consumer = g_browser_process->local_state()->GetBoolean(
      prefs::kOobeIsConsumerSegment);
  base::UmaHistogramBoolean("OOBE.Enrollment.IsUserEnrollingAConsumer",
                            is_consumer);
}

void OobeMetricsHelper::RecordChromeVersion() {
  base::UmaHistogramSparse("OOBE.ChromeVersionBeforeUpdate",
                           version_info::GetMajorVersionNumberAsInt());
}

}  // namespace ash
