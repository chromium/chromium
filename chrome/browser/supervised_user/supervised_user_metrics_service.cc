// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_metrics_service.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/parental_control_metrics.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace {

constexpr base::TimeDelta kTimerInterval = base::Minutes(10);

// Returns the number of days since the origin.
int GetDayId(base::Time time) {
  return time.LocalMidnight().since_origin().InDaysFloored();
}

}  // namespace

// static
void SupervisedUserMetricsService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kSupervisedUserMetricsDayId, 0);
}

// static
int SupervisedUserMetricsService::GetDayIdForTesting(base::Time time) {
  return GetDayId(time);
}

SupervisedUserMetricsService::SupervisedUserMetricsService(
    content::BrowserContext* context)
    : pref_service_(Profile::FromBrowserContext(context)->GetPrefs()) {
  DCHECK(pref_service_);

  // Reports parental control metrics for child user only.
  Profile* profile = Profile::FromBrowserContext(context);
  if (profile->IsChild()) {
    SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(profile);
    supervised_user_metrics_.push_back(
        std::make_unique<ParentalControlMetrics>(supervised_user_service));
  }

  for (auto& supervised_user_metric : supervised_user_metrics_)
    AddObserver(supervised_user_metric.get());

  CheckForNewDay();
  // Check for a new day every |kTimerInterval| as well.
  timer_.Start(FROM_HERE, kTimerInterval, this,
               &SupervisedUserMetricsService::CheckForNewDay);
}

SupervisedUserMetricsService::~SupervisedUserMetricsService() = default;

void SupervisedUserMetricsService::Shutdown() {
  CheckForNewDay();
  observers_.Clear();
  supervised_user_metrics_.clear();
  timer_.Stop();
}

void SupervisedUserMetricsService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SupervisedUserMetricsService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SupervisedUserMetricsService::CheckForNewDay() {
  int day_id = pref_service_->GetInteger(prefs::kSupervisedUserMetricsDayId);
  base::Time now = base::Time::Now();
  // The OnNewDay() event can fire sooner or later than 24 hours due to clock or
  // time zone changes.
  if (day_id < GetDayId(now)) {
    for (Observer& observer : observers_)
      observer.OnNewDay();
    pref_service_->SetInteger(prefs::kSupervisedUserMetricsDayId,
                              GetDayId(now));
  }
}
