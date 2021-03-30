// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_log.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace {

// The value is a list.
constexpr char kEventPrefKey[] = "sessions.event_log";
constexpr char kEventTypeKey[] = "type";
constexpr char kEventTimeKey[] = "time";
constexpr char kStartEventDidLastSessionCrashKey[] = "crashed";
constexpr char kRestoreEventWindowCountKey[] = "window_count";
constexpr char kRestoreEventTabCountKey[] = "tab_count";
constexpr char kRestoreEventErroredReadingKey[] = "errored_reading";
constexpr char kExitEventWindowCountKey[] = "window_count";
constexpr char kExitEventTabCountKey[] = "tab_count";
constexpr char kWriteErrorEventErrorCountKey[] = "error_count";
constexpr char kWriteErrorEventUnrecoverableErrorCountKey[] =
    "unrecoverable_error_count";

// This value is a balance between keeping too much in prefs, and the
// ability to see the last few restarts.
constexpr size_t kMaxEventCount = 12;

base::Value SerializeEvent(const SessionServiceEvent& event) {
  base::Value serialized_event(base::Value::Type::DICTIONARY);
  serialized_event.SetIntPath(kEventTypeKey, static_cast<int>(event.type));
  serialized_event.SetStringPath(
      kEventTimeKey,
      base::NumberToString(event.time.since_origin().InMicroseconds()));
  switch (event.type) {
    case SessionServiceEventLogType::kStart:
      serialized_event.SetBoolKey(kStartEventDidLastSessionCrashKey,
                                  event.data.start.did_last_session_crash);
      break;
    case SessionServiceEventLogType::kRestore:
      serialized_event.SetIntKey(kRestoreEventWindowCountKey,
                                 event.data.restore.window_count);
      serialized_event.SetIntKey(kRestoreEventTabCountKey,
                                 event.data.restore.tab_count);
      serialized_event.SetBoolKey(kRestoreEventErroredReadingKey,
                                  event.data.restore.encountered_error_reading);
      break;
    case SessionServiceEventLogType::kExit:
      serialized_event.SetIntKey(kExitEventWindowCountKey,
                                 event.data.exit.window_count);
      serialized_event.SetIntKey(kExitEventTabCountKey,
                                 event.data.exit.tab_count);
      break;
    case SessionServiceEventLogType::kWriteError:
      serialized_event.SetIntKey(kWriteErrorEventErrorCountKey,
                                 event.data.write_error.error_count);
      serialized_event.SetIntKey(
          kWriteErrorEventUnrecoverableErrorCountKey,
          event.data.write_error.unrecoverable_error_count);
      break;
  }
  return serialized_event;
}

bool DeserializeEvent(const base::Value& serialized_event,
                      SessionServiceEvent& event) {
  if (!serialized_event.is_dict())
    return false;
  auto type = serialized_event.FindIntKey(kEventTypeKey);
  if (!type)
    return false;
  if (*type < static_cast<int>(SessionServiceEventLogType::kMinValue) ||
      *type > static_cast<int>(SessionServiceEventLogType::kMaxValue)) {
    return false;
  }
  event.type = static_cast<SessionServiceEventLogType>(*type);

  const std::string* time_value = serialized_event.FindStringKey(kEventTimeKey);
  if (!time_value)
    return false;
  int64_t time_int;
  if (!base::StringToInt64(*time_value, &time_int))
    return false;
  event.time = base::Time() + base::TimeDelta::FromMicroseconds(time_int);

  switch (event.type) {
    case SessionServiceEventLogType::kStart: {
      auto crash_value =
          serialized_event.FindBoolKey(kStartEventDidLastSessionCrashKey);
      if (!crash_value)
        return false;
      event.data.start.did_last_session_crash = *crash_value;
      break;
    }
    case SessionServiceEventLogType::kRestore: {
      auto window_count =
          serialized_event.FindIntKey(kRestoreEventWindowCountKey);
      if (!window_count)
        return false;
      event.data.restore.window_count = *window_count;

      auto tab_count = serialized_event.FindIntKey(kRestoreEventTabCountKey);
      if (!tab_count)
        return false;
      event.data.restore.tab_count = *tab_count;

      auto error_reading =
          serialized_event.FindBoolKey(kRestoreEventErroredReadingKey);
      if (!error_reading)
        return false;
      event.data.restore.encountered_error_reading = *error_reading;
      break;
    }
    case SessionServiceEventLogType::kExit: {
      auto window_count = serialized_event.FindIntKey(kExitEventWindowCountKey);
      if (!window_count)
        return false;
      event.data.exit.window_count = *window_count;

      auto tab_count = serialized_event.FindIntKey(kExitEventTabCountKey);
      if (!tab_count)
        return false;
      event.data.exit.tab_count = *tab_count;
      break;
    }
    case SessionServiceEventLogType::kWriteError: {
      auto error_count =
          serialized_event.FindIntKey(kWriteErrorEventErrorCountKey);
      if (!error_count)
        return false;
      event.data.write_error.error_count = *error_count;
      event.data.write_error.unrecoverable_error_count = 0;
      // `kWriteErrorEventErrorCountKey` was added after initial code landed,
      // so don't fail if it isn't present.
      auto unrecoverable_error_count = serialized_event.FindIntKey(
          kWriteErrorEventUnrecoverableErrorCountKey);
      if (unrecoverable_error_count) {
        event.data.write_error.unrecoverable_error_count =
            *unrecoverable_error_count;
      }
      break;
    }
  }
  return true;
}

void SaveEventsToPrefs(Profile* profile,
                       const std::list<SessionServiceEvent>& events) {
  base::Value serialized_events(base::Value::Type::LIST);
  for (const SessionServiceEvent& event : events)
    serialized_events.Append(SerializeEvent(event));
  profile->GetPrefs()->Set(kEventPrefKey, serialized_events);
}

}  // namespace

std::list<SessionServiceEvent> GetSessionServiceEvents(Profile* profile) {
  const base::ListValue* serialized_events =
      profile->GetPrefs()->GetList(kEventPrefKey);
  if (!serialized_events)
    return {};
  std::list<SessionServiceEvent> events;
  for (const auto& serialized_event : serialized_events->GetList()) {
    SessionServiceEvent event;
    if (DeserializeEvent(serialized_event, event))
      events.push_back(std::move(event));
  }
  return events;
}

void LogSessionServiceStartEvent(Profile* profile, bool after_crash) {
  SessionServiceEvent event;
  event.type = SessionServiceEventLogType::kStart;
  event.time = base::Time::Now();
  event.data.start.did_last_session_crash = after_crash;
  LogSessionServiceEvent(profile, event);
}

void LogSessionServiceExitEvent(Profile* profile,
                                int window_count,
                                int tab_count) {
  SessionServiceEvent event;
  event.type = SessionServiceEventLogType::kExit;
  event.time = base::Time::Now();
  event.data.exit.window_count = window_count;
  event.data.exit.tab_count = tab_count;
  LogSessionServiceEvent(profile, event);
}

void LogSessionServiceRestoreEvent(Profile* profile,
                                   int window_count,
                                   int tab_count,
                                   bool encountered_error_reading) {
  SessionServiceEvent event;
  event.type = SessionServiceEventLogType::kRestore;
  event.time = base::Time::Now();
  event.data.restore.window_count = window_count;
  event.data.restore.tab_count = tab_count;
  event.data.restore.encountered_error_reading = encountered_error_reading;
  LogSessionServiceEvent(profile, event);
}

void LogSessionServiceWriteErrorEvent(Profile* profile,
                                      bool unrecoverable_write_error) {
  SessionServiceEvent event;
  event.type = SessionServiceEventLogType::kWriteError;
  event.time = base::Time::Now();
  event.data.write_error.error_count = 1;
  event.data.write_error.unrecoverable_error_count =
      unrecoverable_write_error ? 1 : 0;
  LogSessionServiceEvent(profile, event);
}

void RemoveLastSessionServiceEventOfType(Profile* profile,
                                         SessionServiceEventLogType type) {
  std::list<SessionServiceEvent> events = GetSessionServiceEvents(profile);
  for (auto iter = events.rbegin(); iter != events.rend(); ++iter) {
    if (iter->type == type) {
      events.erase(std::next(iter).base());
      SaveEventsToPrefs(profile, events);
      return;
    }
  }
}

void RegisterSessionServiceLogProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kEventPrefKey);
}

void LogSessionServiceEvent(Profile* profile,
                            const SessionServiceEvent& event) {
  std::list<SessionServiceEvent> events = GetSessionServiceEvents(profile);
  if (event.type == SessionServiceEventLogType::kWriteError &&
      !events.empty() &&
      events.back().type == SessionServiceEventLogType::kWriteError) {
    events.back().data.write_error.error_count += 1;
    events.back().data.write_error.unrecoverable_error_count +=
        event.data.write_error.unrecoverable_error_count;
  } else {
    events.push_back(event);
    if (events.size() >= kMaxEventCount)
      events.erase(events.begin());
  }
  SaveEventsToPrefs(profile, events);
}
