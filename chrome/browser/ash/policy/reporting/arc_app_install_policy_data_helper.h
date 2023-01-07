// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_POLICY_DATA_HELPER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_POLICY_DATA_HELPER_H_

#include <stdint.h>
#include <set>

#include "base/timer/timer.h"
#include "chrome/browser/ash/policy/reporting/arc_app_install_policy_data.h"

namespace policy {

// Handles operations to track policy metrics.
class ArcAppInstallPolicyDataHelper {
 public:
  ArcAppInstallPolicyDataHelper();
  ~ArcAppInstallPolicyDataHelper();

  // Adds data for a new policy.
  void AddPolicyData(const std::set<std::string>& current_pending,
                     std::int64_t num_apps_previously_installed);

  // Check if any policy data has timed out.
  void CheckForPolicyDataTimeout();

  // Updates policy data with new package info.
  void UpdatePolicySuccessRate(const std::string& package, bool success);

  // Updates policy data for a set of packages.
  void UpdatePolicySuccessRateForPackages(const std::set<std::string>& packages,
                                          bool success);

  // For testing
  std::set<ArcAppInstallPolicyData>* policy_data() { return &policy_data_; }
  base::RepeatingTimer* policy_data_timer() { return &policy_data_timer_; }

 private:
  // Data used to track metrics for single policies.
  std::set<ArcAppInstallPolicyData> policy_data_;

  // Repeating timer to check if policy data has timed out.
  base::RepeatingTimer policy_data_timer_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_POLICY_DATA_HELPER_H_
