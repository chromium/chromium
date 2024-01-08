// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_CHROME_ACTIVITY_METRICS_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_CHROME_ACTIVITY_METRICS_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/child_accounts/family_user_metrics_service.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_service_wrapper.h"
#include "chrome/browser/ash/child_accounts/usage_time_state_notifier.h"

namespace base {
class UnguessableToken;
}  // namespace base

class PrefRegistrySimple;
class PrefService;
class Profile;

namespace ash {
// This class records: FamilyUser.ChromeBrowserEngagement.Duration. It is the
// daily sum of user's active Chrome browser time in milliseconds. Recorded at
// the beginning of the first active Chrome OS session on a subsequent day.
class FamilyUserChromeActivityMetrics
    : public app_time::AppServiceWrapper::EventListener,
      public UsageTimeStateNotifier::Observer,
      public FamilyUserMetricsService::Observer {
 public:
  static const char kChromeBrowserEngagementDurationHistogramName[];

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit FamilyUserChromeActivityMetrics(Profile* profile);
  FamilyUserChromeActivityMetrics(const FamilyUserChromeActivityMetrics&) =
      delete;
  FamilyUserChromeActivityMetrics& operator=(
      const FamilyUserChromeActivityMetrics&) = delete;
  ~FamilyUserChromeActivityMetrics() override;

  // FamilyUserMetricsService::Observer:
  void OnNewDay() override;

  void SetActiveSessionStartForTesting(base::Time time);

 private:
  // AppServiceWrapper::EventListener:
  // When the screen goes from off to on or device goes idle
  // to active, this function does not get called. OnUsageTimeStateChange() gets
  // called instead.
  void OnAppActive(const app_time::AppId& app_id,
                   const base::UnguessableToken& instance_id,
                   base::Time timestamp) override;
  // When the screen goes from on to off or device goes active
  // to idle, this function does not get called. OnUsageTimeStateChange() gets
  // called instead.
  void OnAppInactive(const app_time::AppId& app_id,
                     const base::UnguessableToken& instance_id,
                     base::Time timestamp) override;

  // UsageTimeStateNotifier::Observer:
  // When the user signs out, this function doesn't get called and
  // UsageTimeStateNotifier::UsageTimeState doesn't change to inactive.
  // OnAppInactive will be called.
  void OnUsageTimeStateChange(
      UsageTimeStateNotifier::UsageTimeState state) override;

  // Called when user engagement of browser changes. Saves duration data to
  // prefs or report to UMA.
  void UpdateUserEngagement(bool is_user_active);

  const raw_ptr<PrefService> pref_service_;
  app_time::AppServiceWrapper app_service_wrapper_;

  // The time when the user becomes active.
  base::Time active_duration_start_;

  // A set of active browser instances. When a new Chrome browser instance gets
  // created or activated, OnAppActive adds that instance to the set. When the
  // Chrome browser instance gets destroyed or deactivated, OnAppInactive might
  // remove that instance from the set.
  std::set<base::UnguessableToken> active_browser_instances_;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_CHROME_ACTIVITY_METRICS_H_
