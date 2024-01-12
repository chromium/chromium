// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_ACTIVITY_STORAGE_H_
#define CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_ACTIVITY_STORAGE_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/policy/proto/device_management_backend.pb.h"

class PrefService;

namespace policy {

// Base class for storing activity time periods, needed for status reporting.
// Derived classes like ChildActivityStorage and EnterpriseActivityStorage
// handle specific use cases.
class ActivityStorage {
 public:
  using Activities = std::vector<enterprise_management::TimePeriod>;

  // Creates activity storage. Activity data will be stored in the given
  // |pref_service| under |pref_name| preference. Activity data are aggregated
  // by day. |day_start_offset| adds this offset to |GetBeginningOfDay|.
  ActivityStorage(PrefService* pref_service,
                  const std::string& pref_name,
                  base::TimeDelta day_start_offset);
  ActivityStorage(const ActivityStorage&) = delete;
  ActivityStorage& operator=(const ActivityStorage&) = delete;
  virtual ~ActivityStorage();

  // Returns when the day starts. An offset for this value can be provided
  // through the constructor.
  base::Time GetBeginningOfDay(base::Time timestamp) const;

  // Clears stored activity periods outside of storage range defined by
  // |max_past_activity_interval| and |max_future_activity_interval| from
  // |base_time|. If |max_past_activity_interval| and
  // |max_future_activity_interval| are not passed, all past data will be
  // removed.
  void PruneActivityPeriods(
      base::Time base_time,
      base::TimeDelta max_past_activity_interval = base::Days(0),
      base::TimeDelta max_future_activity_interval = base::TimeDelta::Max());

  // Trims the stored activity periods to only retain data within the
  // [|min_day_key|, |max_day_key|).
  void TrimActivityPeriods(int64_t min_day_key, int64_t max_day_key);

  // Trims the stored activity periods to ensure all intervals do not overlap.
  void RemoveOverlappingActivityPeriods();

  // Retrieves all activity periods that occurred up to |end_time| that are in
  // the pref keys that can be parsed by |ParseActivityPeriodPrefKey|. The map
  // is keyed by activity IDs.
  const std::map<std::string, Activities> GetActivityPeriods(
      base::Time end_time = base::Time::Max()) const;

  // Retrieves all activity periods that occurred up to |end_time| that are in
  // the pref keys that can be parsed by |ParseActivityPeriodPrefKey| where
  // there is no ID.
  const Activities GetActivityPeriodsWithNoId(
      base::Time end_time = base::Time::Max()) const;

  // Adds an activity period. Accepts empty |activity_id| if it should not
  // be stored.
  void AddActivityPeriod(base::Time start,
                         base::Time end,
                         const std::string& activity_id = "");

 protected:
  // Determine the day key (milliseconds since epoch for corresponding
  // |GetBeginningOfDay()| in UTC) for a given |timestamp|.
  int64_t LocalTimeToUtcDayStart(base::Time timestamp) const;

  // Creates the key that will be used to store an ActivityPeriod in the prefs.
  // If |activity_id| is empty, the key will be |start|. Otherwise it will
  // contain both values, which can retrieved using the
  // |ParseActivityPeriodPrefKey|.
  static std::string MakeActivityPeriodPrefKey(int64_t start,
                                               const std::string& activity_id);

  // Tries to parse a pref key, returning true if succeeded (see
  // base::StringToInt64 and base::Base64Decode for conditions). Also
  // retrieves the values that are encoded in the pref key. |activity_id| will
  // only be filled if it was provided when making the key (see
  // |MakeActivityPeriodPrefKey|).
  static bool ParseActivityPeriodPrefKey(const std::string& key,
                                         int64_t* start_timestamp,
                                         std::string* activity_id);

  void SetActivityPeriods(
      const std::map<std::string, Activities>& new_activity_periods);

  // Retrieves all activity periods that are in the pref keys that can be parsed
  // by |ParseActivityPeriodPrefKey|.
  void ForEachActivityPeriodFromPref(
      const base::RepeatingCallback<
          void(const int64_t, const int64_t, const std::string&)>& f) const;

  const raw_ptr<PrefService> pref_service_ = nullptr;
  const std::string pref_name_;

  // Distance from midnight. |GetBeginningOfDay| uses this, as some
  // implementations might have a different beginning of day from others.
  base::TimeDelta day_start_offset_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_ACTIVITY_STORAGE_H_
