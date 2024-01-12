// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_PARENTAL_CONTROL_METRICS_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_PARENTAL_CONTROL_METRICS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/child_accounts/family_user_metrics_service.h"

class Profile;

namespace ash {

// A class for recording time limit metrics and web filter metrics for Family
// Link users on Chrome OS at the beginning of the first active session daily.
class FamilyUserParentalControlMetrics
    : public FamilyUserMetricsService::Observer {
 public:
  explicit FamilyUserParentalControlMetrics(Profile* profile);
  FamilyUserParentalControlMetrics(const FamilyUserParentalControlMetrics&) =
      delete;
  FamilyUserParentalControlMetrics& operator=(
      const FamilyUserParentalControlMetrics&) = delete;
  ~FamilyUserParentalControlMetrics() override;

  // FamilyUserMetricsService::Observer:
  void OnNewDay() override;

 private:
  const raw_ptr<Profile> profile_;
  bool first_report_on_current_device_ = false;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_PARENTAL_CONTROL_METRICS_H_
