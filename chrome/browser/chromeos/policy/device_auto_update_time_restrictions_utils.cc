// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/policy/device_auto_update_time_restrictions_utils.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/device_auto_update_time_restrictions_decoder.h"
#include "chromeos/policy/weekly_time/time_utils.h"
#include "chromeos/policy/weekly_time/weekly_time.h"
#include "chromeos/policy/weekly_time/weekly_time_interval.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/settings/cros_settings_names.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

using base::Clock;
using base::ListValue;
using std::vector;

namespace policy {

namespace {

// Expects timezone agnostic |WeeklyTimeInterval|s in |intervals|.
// Transforms |intervals| into |*intervals_out|, converting them to the local
// timezone. The local timezone is determined using |clock|. If this returns
// false, an error occurred and the output vector should not be used. This can
// happen if the local timezone offset can not be determined, or if at least one
// of the passed intervals was already bound to a timezone.
bool MaterializeIntervalsToLocalTimezone(
    const vector<WeeklyTimeInterval>& intervals,
    Clock* clock,
    vector<WeeklyTimeInterval>* intervals_out) {
  DCHECK(intervals_out);
  int local_to_gmt_offset;
  auto local_time_zone = base::WrapUnique(icu::TimeZone::createDefault());
  if (!weekly_time_utils::GetOffsetFromTimezoneToGmt(*local_time_zone, clock,
                                                     &local_to_gmt_offset)) {
    LOG(ERROR) << "Unable to get local timezone.";
    return false;
  }
  int gmt_to_local_offset = -local_to_gmt_offset;
  intervals_out->clear();
  for (const auto& interval : intervals) {
    if (interval.start().timezone_offset()) {
      LOG(ERROR) << "Intervals are not timezone-agnostic.";
      return false;
    }
    intervals_out->push_back(WeeklyTimeInterval(
        interval.start().ConvertToCustomTimezone(gmt_to_local_offset),
        interval.end().ConvertToCustomTimezone(gmt_to_local_offset)));
  }
  return true;
}

}  // namespace

bool GetDeviceAutoUpdateTimeRestrictionsIntervalsInLocalTimezone(
    Clock* clock,
    vector<WeeklyTimeInterval>* intervals_out) {
  const ListValue* intervals_list;
  if (!chromeos::CrosSettings::Get()->GetList(
          chromeos::kDeviceAutoUpdateTimeRestrictions, &intervals_list)) {
    return false;
  }

  vector<WeeklyTimeInterval> decoded_intervals;
  if (!WeeklyTimeIntervalsFromListValue(*intervals_list, &decoded_intervals)) {
    return false;
  }

  return MaterializeIntervalsToLocalTimezone(decoded_intervals, clock,
                                             intervals_out);
}

}  // namespace policy
