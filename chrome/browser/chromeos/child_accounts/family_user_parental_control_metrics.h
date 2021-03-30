// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_PARENTAL_CONTROL_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_PARENTAL_CONTROL_METRICS_H_

#include "chrome/browser/chromeos/child_accounts/family_user_metrics_service.h"

class Profile;

namespace chromeos {

// A class for recording time limit metrics and web filter metrics for Family
// Link users on Chrome OS. These metrics will be recorded at the beginning of
// the first active session daily.
class FamilyUserParentalControlMetrics
    : public FamilyUserMetricsService::Observer {
 public:
  // These enum values represent the current Family Link user's time limit
  // policy type for the Family Experiences team's metrics. Multiple time limit
  // policy types might be enabled at the same time. These values are
  // logged to UMA. Entries should not be renumbered and numeric values should
  // never be reused. Please keep in sync with "TimeLimitPolicyType" in
  // src/tools/metrics/histograms/enums.xml.
  enum class TimeLimitPolicyType {
    kNoTimeLimit = 0,
    kOverrideTimeLimit = 1,
    kBedTimeLimit = 2,
    kScreenTimeLimit = 3,
    kWebTimeLimit = 4,
    kAppTimeLimit = 5,

    // Used for UMA. Update kMaxValue to the last value. Add future entries
    // above this comment. Sync with enums.xml.
    kMaxValue = kAppTimeLimit
  };

  explicit FamilyUserParentalControlMetrics(Profile* profile);
  FamilyUserParentalControlMetrics(const FamilyUserParentalControlMetrics&) =
      delete;
  FamilyUserParentalControlMetrics& operator=(
      const FamilyUserParentalControlMetrics&) = delete;
  ~FamilyUserParentalControlMetrics() override;

  static const char* GetTimeLimitPolicyTypesHistogramNameForTest();
  static const char* GetWebFilterTypeHistogramNameForTest();
  static const char* GetManagedSiteListHistogramNameForTest();

  // TODO(crbug/1152622): listen to the policy by using PrefChangeRegistrar to
  // observe pref. Report when policy change in addition to OnNewDay().
  // FamilyUserMetricsService::Observer:
  void OnNewDay() override;

 private:
  void ReportTimeLimitPolicy() const;
  void ReportWebFilterPolicy() const;

 private:
  Profile* const profile_;
  bool first_report_on_current_device_ = false;
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_PARENTAL_CONTROL_METRICS_H_
