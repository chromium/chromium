// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_ENTERPRISE_ACTIVITY_STORAGE_H_
#define CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_ENTERPRISE_ACTIVITY_STORAGE_H_

#include <string>
#include <vector>

#include "chrome/browser/ash/policy/status_collector/activity_storage.h"

class PrefService;

namespace policy {

class EnterpriseActivityStorage : public ActivityStorage {
 public:
  // Forwards the arguments to ActivityStorage.
  EnterpriseActivityStorage(PrefService* pref_service,
                            const std::string& pref_name);
  EnterpriseActivityStorage(const EnterpriseActivityStorage&) = delete;
  EnterpriseActivityStorage& operator=(const EnterpriseActivityStorage&) =
      delete;
  ~EnterpriseActivityStorage() override;

  // Returns the list of stored activity periods. Aggregated data is returned
  // without email addresses if |omit_emails| is set.
  const std::map<std::string, Activities> GetFilteredActivityPeriods(
      bool omit_emails) const;

  // Updates stored activity period according to users' reporting preferences.
  // Removes user's email and aggregates the activity data if user's information
  // should no longer be reported.
  void FilterActivityPeriodsByUsers(
      const std::vector<std::string>& reporting_users);

 private:
  const std::map<std::string, ActivityStorage::Activities>
  GetRedactedActivityPeriods(
      const std::vector<std::string>& reporting_users) const;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_ENTERPRISE_ACTIVITY_STORAGE_H_
