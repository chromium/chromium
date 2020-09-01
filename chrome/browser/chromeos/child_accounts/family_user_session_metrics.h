// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_SESSION_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_SESSION_METRICS_H_

#include "chrome/browser/chromeos/child_accounts/family_user_metrics_service.h"
#include "chrome/browser/chromeos/child_accounts/usage_time_state_notifier.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

// A class for recording session metrics. Calculates and reports the
// following metrics:
// - FamilyUser.SessionEngagement.Start: User action of session engagement
// begin. Recorded when UsageTimeNotifier::UsageTimeState changes to active.
// - FamilyUser.SessionEngagement.Weekday/Weekend/Total: Every hour of
// day when the user is active split by weekday/weekend and total of
// weekday/weekend. Recorded when UsageTimeNotifier::UsageTimeState changes to
// INACTIVE. Covers the time between ACTIVE and INACTIVE.
class FamilyUserSessionMetrics : public FamilyUserMetricsService::Observer,
                                 public UsageTimeStateNotifier::Observer {
 public:
  static const char kSessionEngagementStartActionName[];
  static const char kSessionEngagementWeekdayHistogramName[];
  static const char kSessionEngagementWeekendHistogramName[];
  static const char kSessionEngagementTotalHistogramName[];

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit FamilyUserSessionMetrics(PrefService* pref_service);
  FamilyUserSessionMetrics(const FamilyUserSessionMetrics&) = delete;
  FamilyUserSessionMetrics& operator=(const FamilyUserSessionMetrics&) = delete;
  ~FamilyUserSessionMetrics() override;

 private:
  // UsageTimeStateNotifier::Observer:
  // When the user signs out, this function doesn't get called and
  // |is_user_active_| doesn't change to false. Destructor will be called
  // instead.
  void OnUsageTimeStateChange(
      UsageTimeStateNotifier::UsageTimeState state) override;

  // Called when user engagement changes, save engagement data to pref
  // or report to UMA.
  void UpdateUserEngagement();

  PrefService* const pref_service_;

  bool is_user_active_ = false;
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_SESSION_METRICS_H_
