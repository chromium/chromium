// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/family_user_metrics_service.h"

#include "base/check.h"
#include "base/time/time.h"
#include "chrome/browser/ash/child_accounts/family_user_app_metrics.h"
#include "chrome/browser/ash/child_accounts/family_user_chrome_activity_metrics.h"
#include "chrome/browser/ash/child_accounts/family_user_device_metrics.h"
#include "chrome/browser/ash/child_accounts/family_user_parental_control_metrics.h"
#include "chrome/browser/ash/child_accounts/family_user_session_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {

namespace {

constexpr base::TimeDelta kTimerInterval = base::Minutes(10);

// Returns the number of days since the origin.
int GetDayId(base::Time time) {
  return time.LocalMidnight().since_origin().InDaysFloored();
}

}  // namespace

// static
void FamilyUserMetricsService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kFamilyUserMetricsDayId, 0);
}

// static
int FamilyUserMetricsService::GetDayIdForTesting(base::Time time) {
  return GetDayId(time);
}

FamilyUserMetricsService::FamilyUserMetricsService(
    content::BrowserContext* context)
    : pref_service_(Profile::FromBrowserContext(context)->GetPrefs()) {
  DCHECK(pref_service_);
  family_user_metrics_.push_back(
      std::make_unique<FamilyUserSessionMetrics>(pref_service_));
  Profile* profile = Profile::FromBrowserContext(context);
  family_user_metrics_.push_back(FamilyUserAppMetrics::Create(profile));
  family_user_metrics_.push_back(
      std::make_unique<FamilyUserChromeActivityMetrics>(profile));
  family_user_metrics_.push_back(std::make_unique<FamilyUserDeviceMetrics>());

  // Reports parental control metrics for child user only.
  if (profile->IsChild()) {
    family_user_metrics_.push_back(
        std::make_unique<FamilyUserParentalControlMetrics>(profile));
  }

  for (auto& family_user_metric : family_user_metrics_)
    AddObserver(family_user_metric.get());

  CheckForNewDay();
  // Check for a new day every |kTimerInterval| as well.
  timer_.Start(FROM_HERE, kTimerInterval, this,
               &FamilyUserMetricsService::CheckForNewDay);
}

FamilyUserMetricsService::~FamilyUserMetricsService() = default;

void FamilyUserMetricsService::Shutdown() {
  CheckForNewDay();
  observers_.Clear();
  family_user_metrics_.clear();
  timer_.Stop();
}

void FamilyUserMetricsService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FamilyUserMetricsService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FamilyUserMetricsService::CheckForNewDay() {
  int day_id = pref_service_->GetInteger(prefs::kFamilyUserMetricsDayId);
  base::Time now = base::Time::Now();
  // The OnNewDay() event can fire sooner or later than 24 hours due to clock or
  // time zone changes.
  if (day_id < GetDayId(now)) {
    for (Observer& observer : observers_)
      observer.OnNewDay();
    pref_service_->SetInteger(prefs::kFamilyUserMetricsDayId, GetDayId(now));
  }
}

}  // namespace ash
