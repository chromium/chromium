// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/family_user_chrome_activity_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"

namespace chromeos {

namespace {

constexpr base::TimeDelta kMinChromeDuration = base::TimeDelta::FromSeconds(1);
constexpr base::TimeDelta kMaxChromeDuration = base::TimeDelta::FromDays(1);
constexpr int kChromeDurationBuckets = 100;

}  // namespace

// static
const char FamilyUserChromeActivityMetrics::
    kChromeBrowserEngagementDurationHistogramName[] =
        "FamilyUser.ChromeBrowserEngagement.Duration";

// static
void FamilyUserChromeActivityMetrics::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimeDeltaPref(
      prefs::kFamilyUserMetricsChromeBrowserEngagementDuration,
      base::TimeDelta());
}

FamilyUserChromeActivityMetrics::FamilyUserChromeActivityMetrics(
    Profile* profile)
    : pref_service_(profile->GetPrefs()), app_service_wrapper_(profile) {
  DCHECK(pref_service_);
  app_service_wrapper_.AddObserver(this);
}

FamilyUserChromeActivityMetrics::~FamilyUserChromeActivityMetrics() {
  DCHECK_EQ(base::Time(), active_duration_start_);
  app_service_wrapper_.RemoveObserver(this);
}

void FamilyUserChromeActivityMetrics::OnNewDay() {
  base::TimeDelta unreported_duration = pref_service_->GetTimeDelta(
      prefs::kFamilyUserMetricsChromeBrowserEngagementDuration);
  if (unreported_duration <= base::TimeDelta())
    return;
  base::UmaHistogramCustomTimes(kChromeBrowserEngagementDurationHistogramName,
                                unreported_duration, kMinChromeDuration,
                                kMaxChromeDuration, kChromeDurationBuckets);
  pref_service_->ClearPref(
      prefs::kFamilyUserMetricsChromeBrowserEngagementDuration);
}

void FamilyUserChromeActivityMetrics::SetActiveSessionStartForTesting(
    base::Time time) {
  active_duration_start_ = time;
}

void FamilyUserChromeActivityMetrics::OnAppActive(const app_time::AppId& app_id,
                                                  aura::Window* window,
                                                  base::Time timestamp) {
  if (app_id != app_time::GetChromeAppId())
    return;

  if (active_browser_windows_.empty())
    UpdateUserEngagement(/*is_user_active=*/true);

  active_browser_windows_.insert(window);
}

void FamilyUserChromeActivityMetrics::OnAppInactive(
    const app_time::AppId& app_id,
    aura::Window* window,
    base::Time timestamp) {
  if (app_id != app_time::GetChromeAppId())
    return;

  // OnAppInactive might get called for the same window multiple times. The
  // |window| might have already been removed from |active_browser_windows_|.
  if (!base::Contains(active_browser_windows_, window))
    return;

  active_browser_windows_.erase(window);
  if (active_browser_windows_.empty())
    UpdateUserEngagement(/*is_user_active=*/false);
}

void FamilyUserChromeActivityMetrics::UpdateUserEngagement(
    bool is_user_active) {
  base::Time now = base::Time::Now();
  if (is_user_active) {
    active_duration_start_ = now;
  } else {
    if (now > active_duration_start_) {
      base::TimeDelta unreported_duration = pref_service_->GetTimeDelta(
          prefs::kFamilyUserMetricsChromeBrowserEngagementDuration);
      pref_service_->SetTimeDelta(
          prefs::kFamilyUserMetricsChromeBrowserEngagementDuration,
          unreported_duration + now - active_duration_start_);
    }

    active_duration_start_ = base::Time();
  }
}

}  // namespace chromeos
