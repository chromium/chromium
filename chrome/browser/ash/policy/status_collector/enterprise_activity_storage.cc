// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/status_collector/enterprise_activity_storage.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <set>

#include "base/functional/bind.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace policy {

EnterpriseActivityStorage::EnterpriseActivityStorage(
    PrefService* pref_service,
    const std::string& pref_name)
    : ActivityStorage(pref_service,
                      pref_name,
                      /*day_start_offset=*/base::Seconds(0)) {}

EnterpriseActivityStorage::~EnterpriseActivityStorage() = default;

const std::map<std::string, ActivityStorage::Activities>
EnterpriseActivityStorage::GetFilteredActivityPeriods(bool omit_emails) const {
  if (omit_emails) {
    std::vector<std::string> empty_user_list;
    return GetRedactedActivityPeriods(empty_user_list);
  }
  return GetActivityPeriods();
}

void EnterpriseActivityStorage::FilterActivityPeriodsByUsers(
    const std::vector<std::string>& reporting_users) {
  const auto& filter_activity_periods =
      GetRedactedActivityPeriods(reporting_users);
  SetActivityPeriods(filter_activity_periods);
}

const std::map<std::string, ActivityStorage::Activities>
EnterpriseActivityStorage::GetRedactedActivityPeriods(
    const std::vector<std::string>& reporting_users) const {
  std::set<std::string> reporting_users_set(reporting_users.begin(),
                                            reporting_users.end());

  std::map<std::string, ActivityStorage::Activities> filtered_activity_periods;
  std::map<int64_t, enterprise_management::TimePeriod> unreported_activities;
  const std::string empty;
  for (const auto& activity_pair : GetActivityPeriods()) {
    const std::string& user_email = activity_pair.first;
    const Activities& activity_periods = activity_pair.second;

    if (user_email.empty() || reporting_users_set.count(user_email) == 0) {
      for (const auto& activity : activity_periods) {
        const auto& day_key = activity.start_timestamp();
        if (unreported_activities.count(day_key) == 0) {
          unreported_activities[day_key] = activity;
        } else {
          long duration = activity.end_timestamp() - activity.start_timestamp();
          unreported_activities[day_key].set_end_timestamp(
              unreported_activities[day_key].end_timestamp() + duration);
        }
      }
    } else {
      filtered_activity_periods[user_email] = activity_periods;
    }
  }

  std::vector<enterprise_management::TimePeriod> unreported;
  for (const auto& activity_pair : unreported_activities) {
    unreported.push_back(activity_pair.second);
  }
  std::string no_id;
  filtered_activity_periods[no_id] = unreported;

  return filtered_activity_periods;
}

}  // namespace policy
