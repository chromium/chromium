// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/family_user_metrics_service.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/child_accounts/family_user_session_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

namespace {

constexpr base::TimeDelta kTimerInterval = base::TimeDelta::FromMinutes(10);

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

  for (auto& family_user_metric : family_user_metrics_)
    AddObserver(family_user_metric.get());

  // Check for a new day every |kTimerInterval|.
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

}  // namespace chromeos
