// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_POLICY_DATA_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_POLICY_DATA_H_

#include <set>
#include <string>

#include "base/time/time.h"

namespace policy {

// Stores policy data that will be used to record metrics.
class ArcAppInstallPolicyData {
 public:
  ArcAppInstallPolicyData(base::TimeTicks time_created,
                          const std::set<std::string>& requested_apps_pending,
                          std::int64_t success_count);

  ~ArcAppInstallPolicyData();
  ArcAppInstallPolicyData(const ArcAppInstallPolicyData& other);
  ArcAppInstallPolicyData& operator=(const ArcAppInstallPolicyData&) = delete;

  // Use time_created_ for comparisons since it is unique across policies
  // for a single device/customer and will not change.
  bool operator<(ArcAppInstallPolicyData const& other) const;

  // Updates data with package install result. Returns true when success rate
  // metric was recorded.
  bool UpdatePackageInstallResult(const std::string& package,
                                  bool success) const;

  // Records success rate if there are no more pending packages or if the data
  // has reached the time out. Returns true when metric was recorded.
  bool MaybeRecordSuccessRate() const;

 private:
  // Time this data was created.
  base::TimeTicks time_created_;

  // Set of requested apps that have not been installed yet.
  mutable std::set<std::string> requested_apps_pending_;

  // Number of apps for this policy that have successfully been installed.
  mutable std::int64_t success_count_;

  // Total number of requested apps for this policy.
  std::int64_t requested_apps_total_;

  // Whether or not the success rate has been recorded yet.
  mutable bool is_success_rate_recorded_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_POLICY_DATA_H_
