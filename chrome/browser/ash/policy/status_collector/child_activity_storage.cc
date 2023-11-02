// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/status_collector/child_activity_storage.h"

#include <algorithm>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"

namespace policy {

ChildActivityStorage::ChildActivityStorage(PrefService* pref_service,
                                           const std::string& pref_name,
                                           base::TimeDelta day_start_offset)
    : ActivityStorage(pref_service, pref_name, day_start_offset) {
  DCHECK(user_manager::UserManager::Get()->IsLoggedInAsChildUser());
}

ChildActivityStorage::~ChildActivityStorage() = default;

void ChildActivityStorage::AddActivityPeriod(base::Time start,
                                             base::Time end,
                                             base::Time now) {
  DCHECK(start <= end);

  ScopedDictPrefUpdate update(pref_service_, pref_name_);
  base::Value::Dict& activity_times = update.Get();

  // Assign the period to day buckets in local time.
  base::Time day_start = GetBeginningOfDay(start);
  if (start < day_start)
    day_start -= base::Days(1);
  while (day_start < end) {
    day_start += base::Days(1);
    int64_t activity = (std::min(end, day_start) - start).InMilliseconds();
    const std::string key = MakeActivityPeriodPrefKey(
        LocalTimeToUtcDayStart(start), /*activity_id=*/"");
    int previous_activity = activity_times.FindInt(key).value_or(0);
    activity_times.Set(key, static_cast<int>(previous_activity + activity));

    StoreChildScreenTime(day_start - base::Days(1),
                         base::Milliseconds(activity), now);

    start = day_start;
  }
}

std::vector<enterprise_management::TimePeriod>
ChildActivityStorage::GetStoredActivityPeriods() {
  RemoveOverlappingActivityPeriods();
  return GetActivityPeriodsWithNoId();
}

void ChildActivityStorage::StoreChildScreenTime(base::Time activity_day_start,
                                                base::TimeDelta activity,
                                                base::Time now) {
  // Today's start time.
  base::Time today_start = GetBeginningOfDay(now);
  if (today_start > now)
    today_start -= base::Days(1);

  // The activity windows always start and end on the reset time of two
  // consecutive days, so it is not possible to have a window starting after
  // the current day's reset time.
  DCHECK(activity_day_start <= today_start);

  base::TimeDelta previous_activity = base::Milliseconds(
      pref_service_->GetInteger(prefs::kChildScreenTimeMilliseconds));

  // If this activity window belongs to the current day, the screen time pref
  // should be updated.
  if (activity_day_start == today_start) {
    pref_service_->SetInteger(prefs::kChildScreenTimeMilliseconds,
                              (previous_activity + activity).InMilliseconds());
    pref_service_->SetTime(prefs::kLastChildScreenTimeSaved, now);
    pref_service_->CommitPendingWrite();
  }
}

}  // namespace policy
