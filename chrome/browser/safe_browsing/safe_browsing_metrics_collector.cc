// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/util/values/values_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace {

using EventType = safe_browsing::SafeBrowsingMetricsCollector::EventType;
using UserState = safe_browsing::SafeBrowsingMetricsCollector::UserState;
using SafeBrowsingState = safe_browsing::SafeBrowsingState;

const int kMetricsLoggingIntervalDay = 1;

const int kTimestampsMaxLength = 30;

std::string EventTypeToPrefKey(const EventType type) {
  return base::NumberToString(static_cast<int>(type));
}

std::string SafeBrowsingStateToPrefKey(const SafeBrowsingState state) {
  switch (state) {
    case SafeBrowsingState::ENHANCED_PROTECTION:
      return base::NumberToString(
          static_cast<int>(UserState::ENHANCED_PROTECTION));
    case SafeBrowsingState::STANDARD_PROTECTION:
      return base::NumberToString(
          static_cast<int>(UserState::STANDARD_PROTECTION));
    case SafeBrowsingState::NO_SAFE_BROWSING:
      NOTREACHED() << "Unexpected Safe Browsing state.";
      return base::NumberToString(
          static_cast<int>(UserState::STANDARD_PROTECTION));
  }
}

base::Value TimeToPrefValue(const base::Time& time) {
  return util::Int64ToValue(time.ToDeltaSinceWindowsEpoch().InSeconds());
}

}  // namespace

namespace safe_browsing {

SafeBrowsingMetricsCollector::SafeBrowsingMetricsCollector(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

void SafeBrowsingMetricsCollector::StartLogging() {
  base::TimeDelta log_interval =
      base::TimeDelta::FromDays(kMetricsLoggingIntervalDay);
  base::Time last_log_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromSeconds(
          pref_service_->GetInt64(prefs::kSafeBrowsingMetricsLastLogTime)));
  base::TimeDelta delay = base::Time::Now() - last_log_time;
  if (delay >= log_interval) {
    LogMetricsAndScheduleNextLogging();
  } else {
    ScheduleNextLoggingAfterInterval(log_interval - delay);
  }
}

void SafeBrowsingMetricsCollector::LogMetricsAndScheduleNextLogging() {
  base::UmaHistogramEnumeration("SafeBrowsing.Pref.Daily.SafeBrowsingState",
                                GetSafeBrowsingState(*pref_service_));
  base::UmaHistogramBoolean("SafeBrowsing.Pref.Daily.Extended",
                            IsExtendedReportingEnabled(*pref_service_));
  base::UmaHistogramBoolean("SafeBrowsing.Pref.Daily.SafeBrowsingModeManaged",
                            IsSafeBrowsingPolicyManaged(*pref_service_));
  pref_service_->SetInt64(
      prefs::kSafeBrowsingMetricsLastLogTime,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
  ScheduleNextLoggingAfterInterval(
      base::TimeDelta::FromDays(kMetricsLoggingIntervalDay));
}

void SafeBrowsingMetricsCollector::ScheduleNextLoggingAfterInterval(
    base::TimeDelta interval) {
  metrics_collector_timer_.Stop();
  metrics_collector_timer_.Start(
      FROM_HERE, interval, this,
      &SafeBrowsingMetricsCollector::LogMetricsAndScheduleNextLogging);
}

void SafeBrowsingMetricsCollector::AddSafeBrowsingEventToPref(
    EventType event_type) {
  DictionaryPrefUpdate update(pref_service_,
                              prefs::kSafeBrowsingEventTimestamps);
  base::DictionaryValue* mutable_state_dict = update.Get();

  SafeBrowsingState sb_state = GetSafeBrowsingState(*pref_service_);
  // Safe Browsing events should not be triggered when Safe Browsing is
  // disabled.
  DCHECK(sb_state != SafeBrowsingState::NO_SAFE_BROWSING);
  base::Value* event_dict =
      mutable_state_dict->FindDictKey(SafeBrowsingStateToPrefKey(sb_state));

  if (!event_dict) {
    event_dict =
        mutable_state_dict->SetKey(SafeBrowsingStateToPrefKey(sb_state),
                                   base::Value(base::Value::Type::DICTIONARY));
  }

  base::Value* timestamps =
      event_dict->FindListKey(EventTypeToPrefKey(event_type));
  if (!timestamps) {
    timestamps = event_dict->SetKey(EventTypeToPrefKey(event_type),
                                    base::Value(base::Value::Type::LIST));
  }

  // Remove the oldest timestamp if the length of the timestamps hits the limit.
  while (timestamps->GetList().size() >= kTimestampsMaxLength) {
    timestamps->EraseListIter(timestamps->GetList().begin());
  }

  timestamps->Append(TimeToPrefValue(base::Time::Now()));
}

}  // namespace safe_browsing
