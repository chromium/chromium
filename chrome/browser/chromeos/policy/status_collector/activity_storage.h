// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_ACTIVITY_STORAGE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_ACTIVITY_STORAGE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"

namespace base {
class DictionaryValue;
}

class PrefService;

namespace policy {

// Base class for storing activity time periods, needed for status reporting.
// Derived classes like ChildActivityStorage and EnterpriseActivityStorage
// handle specific use cases.
class ActivityStorage {
 public:
  // Stored activity period.
  struct ActivityPeriod {
    // Email can be empty.
    std::string user_email;

    // Timestamp dating the beginning of the captured activity.
    int64_t start_timestamp;

    // User's activity in milliseconds.
    int activity_milliseconds;
  };

  // Creates activity storage. Activity data will be stored in the given
  // |pref_service| under |pref_name| preference. Activity data are aggregated
  // by day. |day_start_offset| adds this offset to |GetBeginningOfDay|.
  ActivityStorage(PrefService* pref_service,
                  const std::string& pref_name,
                  base::TimeDelta day_start_offset);
  virtual ~ActivityStorage();

  // Returns when the day starts. An offset for this value can be provided
  // through the constructor.
  base::Time GetBeginningOfDay(base::Time timestamp);

  // Clears stored activity periods outside of storage range defined by
  // |max_past_activity_interval| and |max_future_activity_interval| from
  // |base_time|.
  void PruneActivityPeriods(base::Time base_time,
                            base::TimeDelta max_past_activity_interval,
                            base::TimeDelta max_future_activity_interval);

  // Trims the store activity periods to only retain data within the
  // [|min_day_key|, |max_day_key|). The record for |min_day_key| will be
  // adjusted by subtracting |min_day_trim_duration|.
  void TrimActivityPeriods(int64_t min_day_key,
                           int min_day_trim_duration,
                           int64_t max_day_key);

 protected:
  // Creates the key that will be used to store an ActivityPeriod in the prefs.
  // If |user_email| is empty, the key will be |start|. Otherwise it will
  // contain both values, which can retrieved using the
  // |ParseActivityPeriodPrefKey|.
  static std::string MakeActivityPeriodPrefKey(int64_t start,
                                               const std::string& user_email);

  // Tries to parse a pref key, returning true if succeeded (see
  // base::StringToInt64 and base::Base64Decode for conditions). Also
  // retrieves the values that are encoded in the pref key. |user_email| will
  // only be filled if it was provided when making the key (see
  // |MakeActivityPeriodPrefKey|).
  static bool ParseActivityPeriodPrefKey(const std::string& key,
                                         int64_t* start_timestamp,
                                         std::string* user_email);

  // Retrieves all activity periods that are in the pref keys that can be parsed
  // by |ParseActivityPeriodPrefKey|.
  static std::vector<ActivityPeriod> GetActivityPeriodsFromPref(
      const base::DictionaryValue& stored_activity_periods);

  // Determine the day key (milliseconds since epoch for corresponding
  // |GetBeginningOfDay()| in UTC) for a given |timestamp|.
  int64_t TimestampToDayKey(base::Time timestamp);

  PrefService* const pref_service_ = nullptr;
  const std::string pref_name_;

  // Distance from midnight. |GetBeginningOfDay| uses this, as some
  // implementations might have a different beginning of day from others.
  base::TimeDelta day_start_offset_;

  DISALLOW_COPY_AND_ASSIGN(ActivityStorage);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_ACTIVITY_STORAGE_H_
