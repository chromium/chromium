// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_CHROME_ACTIVITY_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_CHROME_ACTIVITY_METRICS_H_

#include <set>

#include "base/time/time.h"
#include "chrome/browser/chromeos/child_accounts/family_user_metrics_service.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_service_wrapper.h"

class PrefRegistrySimple;
class PrefService;
class Profile;

namespace aura {
class Window;
}

namespace chromeos {
// This class records: FamilyUser.ChromeBrowserEngagement.Duration. It is the
// daily sum of user's active Chrome browser time in milliseconds. Recorded at
// the beginning of the first active Chrome OS session on a subsequent day.
class FamilyUserChromeActivityMetrics
    : public app_time::AppServiceWrapper::EventListener,
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
  void OnAppActive(const app_time::AppId& app_id,
                   aura::Window* window,
                   base::Time timestamp) override;
  void OnAppInactive(const app_time::AppId& app_id,
                     aura::Window* window,
                     base::Time timestamp) override;

  // Called when user engagement of browser changes. Saves duration data to
  // prefs or report to UMA.
  void UpdateUserEngagement(bool is_user_active);

  PrefService* const pref_service_;
  app_time::AppServiceWrapper app_service_wrapper_;

  // The time when the user becomes active.
  base::Time active_duration_start_;

  // A set of active browser window instances. When the new Chrome browser
  // window get created or activated, OnAppActive adds that window to the set.
  // When the Chrome browser window get destroyed or deactivated, OnAppInactive
  // might remove that window from the set.
  std::set<aura::Window*> active_browser_windows_;
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_CHROME_ACTIVITY_METRICS_H_
