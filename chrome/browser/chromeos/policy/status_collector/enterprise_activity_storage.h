// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_ENTERPRISE_ACTIVITY_STORAGE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_ENTERPRISE_ACTIVITY_STORAGE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/status_collector/activity_storage.h"

namespace base {
class DictionaryValue;
}

class PrefService;

namespace policy {

class EnterpriseActivityStorage : public ActivityStorage {
 public:
  // Forwards the arguments to ActivityStorage.
  EnterpriseActivityStorage(PrefService* pref_service,
                            const std::string& pref_name);
  ~EnterpriseActivityStorage() override;

  // Adds an activity period. Accepts empty |active_user_email| if it should not
  // be stored.
  void AddActivityPeriod(base::Time start,
                         base::Time end,
                         const std::string& active_user_email);

  // Returns the list of stored activity periods. Aggregated data is returned
  // without email addresses if |omit_emails| is set.
  std::vector<ActivityStorage::ActivityPeriod> GetFilteredActivityPeriods(
      bool omit_emails);

  // Updates stored activity period according to users' reporting preferences.
  // Removes user's email and aggregates the activity data if user's information
  // should no longer be reported.
  void FilterActivityPeriodsByUsers(
      const std::vector<std::string>& reporting_users);

 private:
  static void ProcessActivityPeriods(
      const base::DictionaryValue& activity_times,
      const std::vector<std::string>& reporting_users,
      base::DictionaryValue* const filtered_times);

  DISALLOW_COPY_AND_ASSIGN(EnterpriseActivityStorage);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_ENTERPRISE_ACTIVITY_STORAGE_H_
