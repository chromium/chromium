// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_SESSION_METRICS_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_SESSION_METRICS_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/child_accounts/family_user_metrics_service.h"
#include "chrome/browser/ash/child_accounts/usage_time_state_notifier.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

// A class for recording session metrics. Calculates and reports the
// following metrics:
// - FamilyUser.SessionEngagement.Start: User action of session engagement
// begin. Recorded when UsageTimeNotifier::UsageTimeState changes to active.
// - FamilyUser.SessionEngagement.Weekday/Weekend/Total: Every hour of
// day when the user is active split by weekday/weekend and total of
// weekday/weekend. Recorded when UsageTimeNotifier::UsageTimeState changes to
// INACTIVE. Covers the time between ACTIVE and INACTIVE.
// - FamilyUser.SessionEngagement.Duration: Daily sum of user's active time in
// milliseconds. Recorded at the beginning of the first active session on a
// subsequent day.
class FamilyUserSessionMetrics : public FamilyUserMetricsService::Observer,
                                 public UsageTimeStateNotifier::Observer {
 public:
  static const char kSessionEngagementStartActionName[];
  static const char kSessionEngagementWeekdayHistogramName[];
  static const char kSessionEngagementWeekendHistogramName[];
  static const char kSessionEngagementTotalHistogramName[];
  static const char kSessionEngagementDurationHistogramName[];

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit FamilyUserSessionMetrics(PrefService* pref_service);
  FamilyUserSessionMetrics(const FamilyUserSessionMetrics&) = delete;
  FamilyUserSessionMetrics& operator=(const FamilyUserSessionMetrics&) = delete;
  ~FamilyUserSessionMetrics() override;

  // FamilyUserMetricsService::Observer:
  void OnNewDay() override;

  void SetActiveSessionStartForTesting(base::Time time);

 private:
  // UsageTimeStateNotifier::Observer:
  // When the user signs out, this function doesn't get called and
  // UsageTimeStateNotifier::UsageTimeState doesn't change to inactive.
  // Destructor will be called instead.
  void OnUsageTimeStateChange(
      UsageTimeStateNotifier::UsageTimeState state) override;

  // Called when user engagement changes.Saves engagement hour and session
  // duration data to prefs or report to UMA.
  void UpdateUserEngagement(bool is_user_active);

  const raw_ptr<PrefService> pref_service_;

  // The time when the user becomes active. It will be reset to base::Time()
  // when the user becomes inactive.
  base::Time active_session_start_;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_SESSION_METRICS_H_
