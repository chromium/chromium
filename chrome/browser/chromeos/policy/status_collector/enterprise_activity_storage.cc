// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_collector/enterprise_activity_storage.h"

#include <stdint.h>

#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace policy {

EnterpriseActivityStorage::EnterpriseActivityStorage(
    PrefService* pref_service,
    const std::string& pref_name)
    : ActivityStorage(pref_service,
                      pref_name,
                      /*day_start_offset=*/base::TimeDelta::FromSeconds(0)) {}

EnterpriseActivityStorage::~EnterpriseActivityStorage() = default;

void EnterpriseActivityStorage::AddActivityPeriod(
    base::Time start,
    base::Time end,
    const std::string& active_user_email) {
  DCHECK(start <= end);

  DictionaryPrefUpdate update(pref_service_, pref_name_);
  base::DictionaryValue* activity_times = update.Get();

  // Assign the period to day buckets in local time.
  base::Time midnight = GetBeginningOfDay(start);
  while (midnight < end) {
    midnight += base::TimeDelta::FromDays(1);
    int64_t activity = (std::min(end, midnight) - start).InMilliseconds();

    const std::string key =
        MakeActivityPeriodPrefKey(TimestampToDayKey(start), active_user_email);
    int previous_activity = 0;
    activity_times->GetInteger(key, &previous_activity);
    activity_times->SetInteger(key, previous_activity + activity);
    start = midnight;
  }
}

std::vector<ActivityStorage::ActivityPeriod>
EnterpriseActivityStorage::GetFilteredActivityPeriods(bool omit_emails) {
  DictionaryPrefUpdate update(pref_service_, pref_name_);
  base::DictionaryValue* stored_activity_periods = update.Get();

  base::DictionaryValue filtered_activity_periods;
  if (omit_emails) {
    std::vector<std::string> empty_user_list;
    ProcessActivityPeriods(*stored_activity_periods, empty_user_list,
                           &filtered_activity_periods);
    stored_activity_periods = &filtered_activity_periods;
  }

  return GetActivityPeriodsFromPref(*stored_activity_periods);
}

void EnterpriseActivityStorage::FilterActivityPeriodsByUsers(
    const std::vector<std::string>& reporting_users) {
  const base::DictionaryValue* stored_activity_periods =
      pref_service_->GetDictionary(pref_name_);
  base::DictionaryValue filtered_activity_periods;
  ProcessActivityPeriods(*stored_activity_periods, reporting_users,
                         &filtered_activity_periods);
  pref_service_->Set(pref_name_, filtered_activity_periods);
}

// static
void EnterpriseActivityStorage::ProcessActivityPeriods(
    const base::DictionaryValue& activity_times,
    const std::vector<std::string>& reporting_users,
    base::DictionaryValue* const filtered_times) {
  std::set<std::string> reporting_users_set(reporting_users.begin(),
                                            reporting_users.end());
  const std::string empty;
  for (const auto& it : activity_times.DictItems()) {
    DCHECK(it.second.is_int());
    int64_t timestamp;
    std::string user_email;
    if (!ParseActivityPeriodPrefKey(it.first, &timestamp, &user_email))
      continue;
    if (!user_email.empty() && reporting_users_set.count(user_email) == 0) {
      int value = 0;
      const std::string timestamp_str =
          MakeActivityPeriodPrefKey(timestamp, empty);
      const base::Value* prev_value = filtered_times->FindKeyOfType(
          timestamp_str, base::Value::Type::INTEGER);
      if (prev_value)
        value = prev_value->GetInt();
      filtered_times->SetKey(timestamp_str,
                             base::Value(value + it.second.GetInt()));
    } else {
      filtered_times->SetKey(it.first, it.second.Clone());
    }
  }
}

}  // namespace policy
