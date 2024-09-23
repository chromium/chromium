// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/status_collector/activity_storage.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "base/base64.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace policy {

// Activity periods are keyed with day and activity key in format:
// '<day_timestamp>:<BASE64 encoded activity key>'
constexpr char kActivityKeySeparator = ':';

ActivityStorage::ActivityStorage(PrefService* pref_service,
                                 const std::string& pref_name,
                                 base::TimeDelta day_start_offset)
    : pref_service_(pref_service),
      pref_name_(pref_name),
      day_start_offset_(day_start_offset) {
  DCHECK(pref_service_);
  const PrefService::PrefInitializationStatus pref_service_status =
      pref_service_->GetInitializationStatus();
  DCHECK(pref_service_status != PrefService::INITIALIZATION_STATUS_WAITING &&
         pref_service_status != PrefService::INITIALIZATION_STATUS_ERROR);
}

ActivityStorage::~ActivityStorage() = default;

base::Time ActivityStorage::GetBeginningOfDay(base::Time timestamp) const {
  DCHECK(!timestamp.is_max());
  return timestamp.LocalMidnight() + day_start_offset_;
}

void ActivityStorage::PruneActivityPeriods(
    base::Time base_time,
    base::TimeDelta max_past_activity_interval,
    base::TimeDelta max_future_activity_interval) {
  base::Time min_time = base_time - max_past_activity_interval;
  base::Time max_time = max_past_activity_interval.is_max()
                            ? base::Time::Max()
                            : base_time + max_future_activity_interval;
  TrimActivityPeriods(LocalTimeToUtcDayStart(min_time),
                      LocalTimeToUtcDayStart(max_time));
}

void ActivityStorage::TrimActivityPeriods(int64_t min_day_key,
                                          int64_t max_day_key) {
  base::Value::Dict copy;

  ForEachActivityPeriodFromPref(base::BindRepeating(
      [](base::Value::Dict& copy, int64_t min_day_key, int64_t max_day_key,
         int64_t start, int64_t end, const std::string& activity_id) {
        int64_t day_key = start;
        // Remove data that is too old, or too far in the future.
        if (start >= max_day_key || end <= min_day_key)
          return;
        // Trim data that crosses beginning threshold.
        start = std::max(start, min_day_key);
        // Trim data that crosses ending threshold.
        end = std::min(end, max_day_key);
        // Skip intervals where there was no activity.
        int64_t duration = end - start;
        if (duration <= 0)
          return;
        const std::string key = MakeActivityPeriodPrefKey(day_key, activity_id);
        copy.SetByDottedPath(key, base::saturated_cast<int>(duration));
      },
      std::ref(copy), min_day_key, max_day_key));

  // Flush the activities into pref_service_
  pref_service_->SetDict(pref_name_, std::move(copy));
}

void ActivityStorage::RemoveOverlappingActivityPeriods() {
  std::map<int64_t, base::TimeDelta> day_capacities;
  std::map<std::string, ActivityStorage::Activities> periods_by_activity_id;
  ForEachActivityPeriodFromPref(base::BindRepeating(
      [](std::map<std::string, ActivityStorage::Activities>*
             periods_by_activity_id,
         std::map<int64_t, base::TimeDelta>* day_capacities,
         const int64_t start, const int64_t end,
         const std::string& activity_id) {
        if (day_capacities->count(start) == 0)
          day_capacities->emplace(start, base::Days(1));
        if (day_capacities->at(start).is_zero())
          return;
        base::TimeDelta duration = std::min(base::Milliseconds(end - start),
                                            day_capacities->at(start));
        day_capacities->at(start) -= duration;

        enterprise_management::TimePeriod period;
        period.set_start_timestamp(start);
        period.set_end_timestamp(start + duration.InMilliseconds());
        if (periods_by_activity_id->count(activity_id) == 0) {
          Activities activities;
          periods_by_activity_id->emplace(activity_id, activities);
        }
        Activities& activities = periods_by_activity_id->at(activity_id);
        activities.push_back(period);
      },
      &periods_by_activity_id, &day_capacities));

  SetActivityPeriods(periods_by_activity_id);
}

const ActivityStorage::Activities ActivityStorage::GetActivityPeriodsWithNoId(
    base::Time end_time) const {
  const auto& activity_periods = GetActivityPeriods(end_time);
  std::string no_id;
  if (activity_periods.count(no_id)) {
    return activity_periods.at(no_id);
  } else {
    return {};
  }
}

const std::map<std::string, ActivityStorage::Activities>
ActivityStorage::GetActivityPeriods(base::Time end_time) const {
  int64_t day_key = LocalTimeToUtcDayStart(end_time);

  std::map<std::string, ActivityStorage::Activities> periods_by_activity_id;
  ForEachActivityPeriodFromPref(base::BindRepeating(
      [](std::map<std::string, ActivityStorage::Activities>*
             periods_by_activity_id,
         int64_t day_key, const int64_t start, const int64_t end,
         const std::string& activity_id) {
        if (end > day_key) {
          return;
        }
        enterprise_management::TimePeriod period;
        period.set_start_timestamp(start);
        period.set_end_timestamp(end);
        if (periods_by_activity_id->count(activity_id) == 0) {
          Activities activities;
          periods_by_activity_id->emplace(activity_id, activities);
        }
        Activities& activities = periods_by_activity_id->at(activity_id);
        activities.push_back(period);
      },
      &periods_by_activity_id, day_key));
  return periods_by_activity_id;
}

void ActivityStorage::AddActivityPeriod(base::Time start,
                                        base::Time end,
                                        const std::string& activity_id) {
  DCHECK(start <= end);
  DCHECK(!start.is_max());
  DCHECK(!end.is_max());

  ScopedDictPrefUpdate update(pref_service_, pref_name_);
  base::Value::Dict& activity_times = update.Get();

  // Assign the period to day buckets in local time.
  base::Time midnight = GetBeginningOfDay(start);
  while (midnight < end) {
    midnight += base::Days(1);
    int64_t activity = (std::min(end, midnight) - start).InMilliseconds();

    const int64_t day_key = LocalTimeToUtcDayStart(start);
    const std::string key = MakeActivityPeriodPrefKey(day_key, activity_id);
    VLOG(1) << "Add Activity: "
            << base::Time::FromMillisecondsSinceUnixEpoch(day_key) << " to "
            << base::Time::FromMillisecondsSinceUnixEpoch(day_key + activity);
    const auto previous_activity = activity_times.FindIntByDottedPath(key);
    if (previous_activity.has_value()) {
      activity += previous_activity.value();
    }
    activity_times.Set(key, static_cast<int>(activity));
    start = midnight;
  }
}

void ActivityStorage::SetActivityPeriods(
    const std::map<std::string, Activities>& new_activity_periods) {
  base::Value::Dict copy;
  for (const auto& activity_pair : new_activity_periods) {
    const std::string& activity_id = activity_pair.first;
    const Activities& activities = activity_pair.second;
    for (const auto& activity : activities) {
      const std::string& key =
          MakeActivityPeriodPrefKey(activity.start_timestamp(), activity_id);
      copy.Set(key, base::saturated_cast<int>(activity.end_timestamp() -
                                              activity.start_timestamp()));
    }
  }

  pref_service_->SetDict(pref_name_, std::move(copy));
}

int64_t ActivityStorage::LocalTimeToUtcDayStart(base::Time timestamp) const {
  if (timestamp.is_max()) {
    // If timestamp is base::Time::Max(), trying to calculate day start
    // is not needed, just keep it as is. timestamp like this cannot be part
    // of an actual activity interval, it only happens as a threshold for
    // activities report.
    return timestamp.InMillisecondsSinceUnixEpoch();
  }

  base::Time::Exploded exploded;
  base::Time day_start = GetBeginningOfDay(timestamp);
  // TODO(crbug.com/40569404): directly test this time change. Currently it is
  // tested through ScreenTimeControllerBrowsertest.
  if (timestamp < day_start)
    day_start -= base::Days(1);
  day_start.LocalExplode(&exploded);
  base::Time out_time;
  bool conversion_success = base::Time::FromUTCExploded(exploded, &out_time);
  DCHECK(conversion_success);
  return out_time.InMillisecondsSinceUnixEpoch();
}

// static
std::string ActivityStorage::MakeActivityPeriodPrefKey(
    int64_t start,
    const std::string& activity_id) {
  const std::string day_key = base::NumberToString(start);
  if (activity_id.empty())
    return day_key;

  return day_key + kActivityKeySeparator + base::Base64Encode(activity_id);
}

// static
bool ActivityStorage::ParseActivityPeriodPrefKey(const std::string& key,
                                                 int64_t* start_timestamp,
                                                 std::string* activity_id) {
  auto separator_pos = key.find(kActivityKeySeparator);
  if (separator_pos == std::string::npos) {
    activity_id->clear();
    return base::StringToInt64(key, start_timestamp);
  }
  return base::StringToInt64(key.substr(0, separator_pos), start_timestamp) &&
         base::Base64Decode(key.substr(separator_pos + 1), activity_id);
}

void ActivityStorage::ForEachActivityPeriodFromPref(
    const base::RepeatingCallback<
        void(const int64_t, const int64_t, const std::string&)>& f) const {
  const base::Value::Dict& stored_activity_periods =
      pref_service_->GetDict(pref_name_);
  for (const auto item : stored_activity_periods) {
    int64_t timestamp;
    std::string activity_id;
    if (!ParseActivityPeriodPrefKey(item.first, &timestamp, &activity_id)) {
      LOG(WARNING) << "Cannot parse recorded activity key: '" << item.first
                   << "'";
      continue;
    }
    if (!item.second.is_int()) {
      LOG(WARNING) << "Cannot parse recorded activity duration: '"
                   << item.second << "'";
      continue;
    }
    if (item.second.GetInt() > 0) {
      f.Run(timestamp, timestamp + item.second.GetInt(), activity_id);
    }
  }
}

}  // namespace policy
