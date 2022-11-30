// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/family_user_session_metrics.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

constexpr int kEngagementHourBuckets = base::Time::kHoursPerDay;
constexpr base::TimeDelta kOneHour = base::Hours(1);
constexpr base::TimeDelta kMinSessionDuration = base::Seconds(1);
constexpr base::TimeDelta kMaxSessionDuration = base::Days(1);
constexpr int kSessionDurationBuckets = 100;

// Returns the hour (0-23) within the day for given local time.
int HourOfDay(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return exploded.hour;
}

// Returns 0-based day of week (0 = Sunday, etc.)
int DayOfWeek(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return exploded.day_of_week;
}

// Reports every active hour between |start| and |end| to UMA.
void ReportUserEngagementHourToUma(base::Time start, base::Time end) {
  if (start.is_null() || end.is_null() || end < start)
    return;
  base::Time time = start;
  while (time <= end) {
    int day_of_week = DayOfWeek(time);
    int hour_of_day = HourOfDay(time);
    if (day_of_week == 0 || day_of_week == 6) {
      base::UmaHistogramExactLinear(
          FamilyUserSessionMetrics::kSessionEngagementWeekendHistogramName,
          hour_of_day, kEngagementHourBuckets);
    } else {
      base::UmaHistogramExactLinear(
          FamilyUserSessionMetrics::kSessionEngagementWeekdayHistogramName,
          hour_of_day, kEngagementHourBuckets);
    }

    base::UmaHistogramExactLinear(
        FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName,
        hour_of_day, kEngagementHourBuckets);

    // When the difference between end and time less than 1 hour and their hours
    // of day are different, i.e. time = 10:55 and end = 11:05, we need to
    // report both 10 and 11. To ensure we don't omit reporting 11, set |time|
    // equal to |end|.
    if (end - time < kOneHour && hour_of_day != HourOfDay(end)) {
      time = end;
    } else {
      time += kOneHour;
    }
  }
}

}  // namespace

// static
const char FamilyUserSessionMetrics::kSessionEngagementStartActionName[] =
    "FamilyUser.SessionEngagement.Start";
const char FamilyUserSessionMetrics::kSessionEngagementWeekdayHistogramName[] =
    "FamilyUser.SessionEngagement.Weekday";
const char FamilyUserSessionMetrics::kSessionEngagementWeekendHistogramName[] =
    "FamilyUser.SessionEngagement.Weekend";
const char FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName[] =
    "FamilyUser.SessionEngagement.Total";
const char FamilyUserSessionMetrics::kSessionEngagementDurationHistogramName[] =
    "FamilyUser.SessionEngagement.Duration";

// static
void FamilyUserSessionMetrics::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimeDeltaPref(
      prefs::kFamilyUserMetricsSessionEngagementDuration, base::TimeDelta());
}

FamilyUserSessionMetrics::FamilyUserSessionMetrics(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
  UsageTimeStateNotifier::GetInstance()->AddObserver(this);
}

FamilyUserSessionMetrics::~FamilyUserSessionMetrics() {
  // |active_session_start_| will be reset in UpdateUserEngagement() after user
  // becomes inactive. |active_session_start_| equals to base::Time() indicates
  // that UpdateUserEngagement(false) has already been called.
  if (active_session_start_ != base::Time()) {
    UpdateUserEngagement(/*is_user_active=*/false);
  }

  UsageTimeStateNotifier::GetInstance()->RemoveObserver(this);
}

void FamilyUserSessionMetrics::OnNewDay() {
  base::TimeDelta unreported_duration = pref_service_->GetTimeDelta(
      prefs::kFamilyUserMetricsSessionEngagementDuration);
  if (unreported_duration <= base::TimeDelta())
    return;
  base::UmaHistogramCustomTimes(kSessionEngagementDurationHistogramName,
                                unreported_duration, kMinSessionDuration,
                                kMaxSessionDuration, kSessionDurationBuckets);
  pref_service_->ClearPref(prefs::kFamilyUserMetricsSessionEngagementDuration);
}

void FamilyUserSessionMetrics::SetActiveSessionStartForTesting(
    base::Time time) {
  active_session_start_ = time;
}

void FamilyUserSessionMetrics::OnUsageTimeStateChange(
    UsageTimeStateNotifier::UsageTimeState state) {
  UpdateUserEngagement(/*is_user_active=*/state ==
                       UsageTimeStateNotifier::UsageTimeState::ACTIVE);
}

void FamilyUserSessionMetrics::UpdateUserEngagement(bool is_user_active) {
  base::Time now = base::Time::Now();
  if (is_user_active) {
    base::RecordAction(
        base::UserMetricsAction(kSessionEngagementStartActionName));
    active_session_start_ = now;
  } else {
    ReportUserEngagementHourToUma(
        /*start=*/active_session_start_,
        /*end=*/now);
    if (now > active_session_start_) {
      base::TimeDelta unreported_duration = pref_service_->GetTimeDelta(
          prefs::kFamilyUserMetricsSessionEngagementDuration);
      pref_service_->SetTimeDelta(
          prefs::kFamilyUserMetricsSessionEngagementDuration,
          unreported_duration + now - active_session_start_);
    }

    active_session_start_ = base::Time();
  }
}

}  // namespace ash
