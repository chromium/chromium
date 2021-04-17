// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/util/values/values_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace {

using EventType = safe_browsing::SafeBrowsingMetricsCollector::EventType;
using UserState = safe_browsing::SafeBrowsingMetricsCollector::UserState;
using SafeBrowsingState = safe_browsing::SafeBrowsingState;

const int kMetricsLoggingIntervalDay = 1;

// The max length of event timestamps stored in pref.
const int kTimestampsMaxLength = 30;
// The quota for ESB disabled metrics. ESB disabled metrics should not be logged
// more than the quota in a week.
const int kEsbDisabledMetricsQuota = 3;
// Events that are older than 30 days are removed from pref.
const int kEventMaxDurationDay = 30;

std::string EventTypeToPrefKey(const EventType& type) {
  return base::NumberToString(static_cast<int>(type));
}

std::string UserStateToPrefKey(const UserState& user_state) {
  return base::NumberToString(static_cast<int>(user_state));
}

base::Value TimeToPrefValue(const base::Time& time) {
  return util::Int64ToValue(time.ToDeltaSinceWindowsEpoch().InSeconds());
}

base::Time PrefValueToTime(const base::Value& value) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromSeconds(util::ValueToInt64(value).value_or(0)));
}

}  // namespace

namespace safe_browsing {

SafeBrowsingMetricsCollector::SafeBrowsingMetricsCollector(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnhanced,
      base::BindRepeating(
          &SafeBrowsingMetricsCollector::OnEnhancedProtectionPrefChanged,
          base::Unretained(this)));
}

void SafeBrowsingMetricsCollector::Shutdown() {
  pref_change_registrar_.RemoveAll();
}

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
  LogDailyOptInMetrics();
  LogDailyEventMetrics();
  RemoveOldEventsFromPref();

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

void SafeBrowsingMetricsCollector::LogDailyOptInMetrics() {
  base::UmaHistogramEnumeration("SafeBrowsing.Pref.Daily.SafeBrowsingState",
                                GetSafeBrowsingState(*pref_service_));
  base::UmaHistogramBoolean("SafeBrowsing.Pref.Daily.Extended",
                            IsExtendedReportingEnabled(*pref_service_));
  base::UmaHistogramBoolean("SafeBrowsing.Pref.Daily.SafeBrowsingModeManaged",
                            IsSafeBrowsingPolicyManaged(*pref_service_));
}

void SafeBrowsingMetricsCollector::LogDailyEventMetrics() {
  SafeBrowsingState sb_state = GetSafeBrowsingState(*pref_service_);
  if (sb_state == SafeBrowsingState::NO_SAFE_BROWSING) {
    return;
  }
  UserState user_state = GetUserState();

  int total_bypass_count = 0;
  for (int event_type_int = 0; event_type_int <= EventType::kMaxValue;
       event_type_int += 1) {
    EventType event_type = static_cast<EventType>(event_type_int);
    if (!IsBypassEventType(event_type)) {
      continue;
    }
    int bypass_count =
        GetEventCountSince(user_state, event_type,
                           base::Time::Now() - base::TimeDelta::FromDays(28));
    base::UmaHistogramCounts100("SafeBrowsing.Daily.BypassCountLast28Days." +
                                    GetUserStateMetricSuffix(user_state) + "." +
                                    GetEventTypeMetricSuffix(event_type),
                                bypass_count);
    total_bypass_count += bypass_count;
  }
  base::UmaHistogramCounts100("SafeBrowsing.Daily.BypassCountLast28Days." +
                                  GetUserStateMetricSuffix(user_state) +
                                  ".AllEvents",
                              total_bypass_count);
}

void SafeBrowsingMetricsCollector::RemoveOldEventsFromPref() {
  DictionaryPrefUpdate update(pref_service_,
                              prefs::kSafeBrowsingEventTimestamps);
  base::DictionaryValue* mutable_state_dict = update.Get();
  for (auto state_map : mutable_state_dict->DictItems()) {
    for (auto event_map : state_map.second.DictItems()) {
      event_map.second.EraseListValueIf([&](const auto& timestamp) {
        return base::Time::Now() - PrefValueToTime(timestamp) >
               base::TimeDelta::FromDays(kEventMaxDurationDay);
      });
    }
  }
}

void SafeBrowsingMetricsCollector::AddSafeBrowsingEventToPref(
    EventType event_type) {
  SafeBrowsingState sb_state = GetSafeBrowsingState(*pref_service_);
  // Skip the event if Safe Browsing is disabled.
  if (sb_state == SafeBrowsingState::NO_SAFE_BROWSING) {
    return;
  }

  AddSafeBrowsingEventAndUserStateToPref(GetUserState(), event_type);
}

void SafeBrowsingMetricsCollector::AddSafeBrowsingEventAndUserStateToPref(
    UserState user_state,
    EventType event_type) {
  DictionaryPrefUpdate update(pref_service_,
                              prefs::kSafeBrowsingEventTimestamps);
  base::DictionaryValue* mutable_state_dict = update.Get();

  base::Value* event_dict =
      mutable_state_dict->FindDictKey(UserStateToPrefKey(user_state));
  if (!event_dict) {
    event_dict =
        mutable_state_dict->SetKey(UserStateToPrefKey(user_state),
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

void SafeBrowsingMetricsCollector::OnEnhancedProtectionPrefChanged() {
  // Pref changed by policy is not initiated by users, so this case is ignored.
  if (IsSafeBrowsingPolicyManaged(*pref_service_)) {
    return;
  }

  if (!pref_service_->GetBoolean(prefs::kSafeBrowsingEnhanced)) {
    AddSafeBrowsingEventAndUserStateToPref(UserState::ENHANCED_PROTECTION,
                                           EventType::USER_STATE_DISABLED);
    int disabled_times_last_week = GetEventCountSince(
        UserState::ENHANCED_PROTECTION, EventType::USER_STATE_DISABLED,
        base::Time::Now() - base::TimeDelta::FromDays(7));
    if (disabled_times_last_week <= kEsbDisabledMetricsQuota) {
      LogEnhancedProtectionDisabledMetrics();
    }
  } else {
    AddSafeBrowsingEventAndUserStateToPref(UserState::ENHANCED_PROTECTION,
                                           EventType::USER_STATE_ENABLED);
  }
}

const base::Value* SafeBrowsingMetricsCollector::GetSafeBrowsingEventDictionary(
    UserState user_state) {
  const base::DictionaryValue* state_dict =
      pref_service_->GetDictionary(prefs::kSafeBrowsingEventTimestamps);

  return state_dict->FindDictKey(UserStateToPrefKey(user_state));
}

base::Optional<SafeBrowsingMetricsCollector::Event>
SafeBrowsingMetricsCollector::GetLatestEventFromEventType(
    UserState user_state,
    EventType event_type) {
  const base::Value* event_dict = GetSafeBrowsingEventDictionary(user_state);

  if (!event_dict) {
    return base::nullopt;
  }

  const base::Value* timestamps =
      event_dict->FindListKey(EventTypeToPrefKey(event_type));

  if (timestamps && timestamps->GetList().size() > 0) {
    base::Time time = PrefValueToTime(timestamps->GetList().back());
    return Event(event_type, time);
  }

  return base::nullopt;
}

void SafeBrowsingMetricsCollector::LogEnhancedProtectionDisabledMetrics() {
  const base::Value* event_dict =
      GetSafeBrowsingEventDictionary(UserState::ENHANCED_PROTECTION);
  if (!event_dict) {
    return;
  }

  std::vector<Event> bypass_events;
  for (int event_type_int = 0; event_type_int <= EventType::kMaxValue;
       event_type_int += 1) {
    EventType event_type = static_cast<EventType>(event_type_int);
    if (!IsBypassEventType(event_type)) {
      continue;
    }
    base::UmaHistogramCounts100(
        "SafeBrowsing.EsbDisabled.BypassCountLast28Days." +
            GetEventTypeMetricSuffix(event_type),
        GetEventCountSince(UserState::ENHANCED_PROTECTION, event_type,
                           base::Time::Now() - base::TimeDelta::FromDays(28)));

    const base::Optional<Event> latest_event =
        GetLatestEventFromEventType(UserState::ENHANCED_PROTECTION, event_type);
    if (latest_event) {
      bypass_events.emplace_back(latest_event.value());
    }
  }

  const auto latest_event = std::max_element(
      bypass_events.begin(), bypass_events.end(),
      [](const Event& a, const Event& b) { return a.timestamp < b.timestamp; });

  if (latest_event != bypass_events.end()) {
    base::UmaHistogramEnumeration(
        "SafeBrowsing.EsbDisabled.LastBypassEventType", latest_event->type);
    base::UmaHistogramCustomTimes(
        "SafeBrowsing.EsbDisabled.LastBypassEventInterval." +
            GetEventTypeMetricSuffix(latest_event->type),
        /* sample */ base::Time::Now() - latest_event->timestamp,
        /* min */ base::TimeDelta::FromSeconds(1),
        /* max */ base::TimeDelta::FromDays(1), /* buckets */ 50);
  }

  const base::Optional<Event> latest_enabled_event =
      GetLatestEventFromEventType(UserState::ENHANCED_PROTECTION,
                                  EventType::USER_STATE_ENABLED);
  if (latest_enabled_event) {
    const auto days_since_enabled =
        (base::Time::Now() - latest_enabled_event.value().timestamp).InDays();
    base::UmaHistogramCounts100("SafeBrowsing.EsbDisabled.LastEnabledInterval",
                                /* sample */ days_since_enabled);
  }
}

int SafeBrowsingMetricsCollector::GetEventCountSince(UserState user_state,
                                                     EventType event_type,
                                                     base::Time since_time) {
  const base::Value* event_dict = GetSafeBrowsingEventDictionary(user_state);
  if (!event_dict) {
    return 0;
  }
  const base::Value* timestamps =
      event_dict->FindListKey(EventTypeToPrefKey(event_type));
  if (!timestamps) {
    return 0;
  }

  return std::count_if(timestamps->GetList().begin(),
                       timestamps->GetList().end(),
                       [&](const base::Value& timestamp) {
                         return PrefValueToTime(timestamp) > since_time;
                       });
}

UserState SafeBrowsingMetricsCollector::GetUserState() {
  if (IsSafeBrowsingPolicyManaged(*pref_service_)) {
    return UserState::MANAGED;
  }

  SafeBrowsingState sb_state = GetSafeBrowsingState(*pref_service_);
  switch (sb_state) {
    case SafeBrowsingState::ENHANCED_PROTECTION:
      return UserState::ENHANCED_PROTECTION;
    case SafeBrowsingState::STANDARD_PROTECTION:
      return UserState::STANDARD_PROTECTION;
    case SafeBrowsingState::NO_SAFE_BROWSING:
      NOTREACHED() << "Unexpected Safe Browsing state.";
      return UserState::STANDARD_PROTECTION;
  }
}

bool SafeBrowsingMetricsCollector::IsBypassEventType(const EventType& type) {
  switch (type) {
    case EventType::USER_STATE_DISABLED:
    case EventType::USER_STATE_ENABLED:
      return false;
    case EventType::DATABASE_INTERSTITIAL_BYPASS:
    case EventType::CSD_INTERSTITIAL_BYPASS:
    case EventType::REAL_TIME_INTERSTITIAL_BYPASS:
    case EventType::DANGEROUS_DOWNLOAD_BYPASS:
    case EventType::PASSWORD_REUSE_MODAL_BYPASS:
    case EventType::EXTENSION_ALLOWLIST_INSTALL_BYPASS:
    case EventType::NON_ALLOWLISTED_EXTENSION_RE_ENABLED:
      return true;
  }
}

std::string SafeBrowsingMetricsCollector::GetUserStateMetricSuffix(
    const UserState& user_state) {
  switch (user_state) {
    case UserState::STANDARD_PROTECTION:
      return "StandardProtection";
    case UserState::ENHANCED_PROTECTION:
      return "EnhancedProtection";
    case UserState::MANAGED:
      return "Managed";
  }
}

std::string SafeBrowsingMetricsCollector::GetEventTypeMetricSuffix(
    const EventType& event_type) {
  switch (event_type) {
    case EventType::USER_STATE_DISABLED:
      return "UserStateDisabled";
    case EventType::USER_STATE_ENABLED:
      return "UserStateEnabled";
    case EventType::DATABASE_INTERSTITIAL_BYPASS:
      return "DatabaseInterstitialBypass";
    case EventType::CSD_INTERSTITIAL_BYPASS:
      return "CsdInterstitialBypass";
    case EventType::REAL_TIME_INTERSTITIAL_BYPASS:
      return "RealTimeInterstitialBypass";
    case EventType::DANGEROUS_DOWNLOAD_BYPASS:
      return "DangerousDownloadBypass";
    case EventType::PASSWORD_REUSE_MODAL_BYPASS:
      return "PasswordReuseModalBypass";
    case EventType::EXTENSION_ALLOWLIST_INSTALL_BYPASS:
      return "ExtensionAllowlistInstallBypass";
    case EventType::NON_ALLOWLISTED_EXTENSION_RE_ENABLED:
      return "NonAllowlistedExtensionReEnabled";
  }
}

SafeBrowsingMetricsCollector::Event::Event(EventType type, base::Time timestamp)
    : type(type), timestamp(timestamp) {}

}  // namespace safe_browsing
