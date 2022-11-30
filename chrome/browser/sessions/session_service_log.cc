// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_log.h"

#include <string>
#include <utility>

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
constexpr char kRestoreInitiatedEventRestoreBrowserKey[] = "restore_browser";
constexpr char kRestoreInitiatedEventSynchronousKey[] = "synchronous";
constexpr char kExitEventWindowCountKey[] = "window_count";
constexpr char kExitEventTabCountKey[] = "tab_count";
constexpr char kExitEventIsFirstSessionServiceKey[] = "first_session_service";
constexpr char kExitEventDidScheduleCommandKey[] = "did_schedule_command";
constexpr char kWriteErrorEventErrorCountKey[] = "error_count";
constexpr char kWriteErrorEventUnrecoverableErrorCountKey[] =
    "unrecoverable_error_count";

// This value is a balance between keeping too much in prefs, and the
// ability to see the last few restarts.
constexpr size_t kMaxEventCount = 20;

base::Value::Dict SerializeEvent(const SessionServiceEvent& event) {
  base::Value::Dict serialized_event;
  serialized_event.Set(kEventTypeKey, static_cast<int>(event.type));
  serialized_event.Set(
      kEventTimeKey,
      base::NumberToString(event.time.since_origin().InMicroseconds()));
  switch (event.type) {
    case SessionServiceEventLogType::kStart:
      serialized_event.Set(kStartEventDidLastSessionCrashKey,
                           event.data.start.did_last_session_crash);
      break;
    case SessionServiceEventLogType::kRestore:
      serialized_event.Set(kRestoreEventWindowCountKey,
                           event.data.restore.window_count);
      serialized_event.Set(kRestoreEventTabCountKey,
                           event.data.restore.tab_count);
      serialized_event.Set(kRestoreEventErroredReadingKey,
                           event.data.restore.encountered_error_reading);
      break;
    case SessionServiceEventLogType::kExit:
      serialized_event.Set(kExitEventWindowCountKey,
                           event.data.exit.window_count);
      serialized_event.Set(kExitEventTabCountKey, event.data.exit.tab_count);
      serialized_event.Set(kExitEventIsFirstSessionServiceKey,
                           event.data.exit.is_first_session_service);
      serialized_event.Set(kExitEventDidScheduleCommandKey,
                           event.data.exit.did_schedule_command);
      break;
    case SessionServiceEventLogType::kWriteError:
      serialized_event.Set(kWriteErrorEventErrorCountKey,
                           event.data.write_error.error_count);
      serialized_event.Set(kWriteErrorEventUnrecoverableErrorCountKey,
                           event.data.write_error.unrecoverable_error_count);
      break;
    case SessionServiceEventLogType::kRestoreCanceled:
      break;
    case SessionServiceEventLogType::kRestoreInitiated:
      serialized_event.Set(kRestoreInitiatedEventRestoreBrowserKey,
                           event.data.restore_initiated.restore_browser);
      serialized_event.Set(kRestoreInitiatedEventSynchronousKey,
                           event.data.restore_initiated.synchronous);
      break;
  }
  return serialized_event;
}

bool DeserializeEvent(const base::Value::Dict& serialized_event,
                      SessionServiceEvent& event) {
  auto type = serialized_event.FindInt(kEventTypeKey);
  if (!type)
    return false;
  if (*type < static_cast<int>(SessionServiceEventLogType::kMinValue) ||
      *type > static_cast<int>(SessionServiceEventLogType::kMaxValue)) {
    return false;
  }
  event.type = static_cast<SessionServiceEventLogType>(*type);

  const std::string* time_value = serialized_event.FindString(kEventTimeKey);
  if (!time_value)
    return false;
  int64_t time_int;
  if (!base::StringToInt64(*time_value, &time_int))
    return false;
  event.time = base::Time() + base::Microseconds(time_int);

  switch (event.type) {
    case SessionServiceEventLogType::kStart: {
      auto crash_value =
          serialized_event.FindBool(kStartEventDidLastSessionCrashKey);
      if (!crash_value)
        return false;
      event.data.start.did_last_session_crash = *crash_value;
      break;
    }
    case SessionServiceEventLogType::kRestore: {
      auto window_count = serialized_event.FindInt(kRestoreEventWindowCountKey);
      if (!window_count)
        return false;
      event.data.restore.window_count = *window_count;

      auto tab_count = serialized_event.FindInt(kRestoreEventTabCountKey);
      if (!tab_count)
        return false;
      event.data.restore.tab_count = *tab_count;

      auto error_reading =
          serialized_event.FindBool(kRestoreEventErroredReadingKey);
      if (!error_reading)
        return false;
      event.data.restore.encountered_error_reading = *error_reading;
      break;
    }
    case SessionServiceEventLogType::kExit: {
      auto window_count = serialized_event.FindInt(kExitEventWindowCountKey);
      if (!window_count)
        return false;
      event.data.exit.window_count = *window_count;

      auto tab_count = serialized_event.FindInt(kExitEventTabCountKey);
      if (!tab_count)
        return false;
      event.data.exit.tab_count = *tab_count;

      // The remaining values were added later on. Don't error if not found.
      auto is_first_session_service =
          serialized_event.FindBool(kExitEventIsFirstSessionServiceKey);
      event.data.exit.is_first_session_service =
          !is_first_session_service || *is_first_session_service;

      auto did_schedule_command =
          serialized_event.FindBool(kExitEventDidScheduleCommandKey);
      event.data.exit.did_schedule_command =
          !did_schedule_command || *did_schedule_command;
      break;
    }
    case SessionServiceEventLogType::kWriteError: {
      auto error_count =
          serialized_event.FindInt(kWriteErrorEventErrorCountKey);
      if (!error_count)
        return false;
      event.data.write_error.error_count = *error_count;
      event.data.write_error.unrecoverable_error_count = 0;
      // `kWriteErrorEventErrorCountKey` was added after initial code landed,
      // so don't fail if it isn't present.
      auto unrecoverable_error_count =
          serialized_event.FindInt(kWriteErrorEventUnrecoverableErrorCountKey);
      if (unrecoverable_error_count) {
        event.data.write_error.unrecoverable_error_count =
            *unrecoverable_error_count;
      }
      break;
    }
    case SessionServiceEventLogType::kRestoreCanceled:
      break;
    case SessionServiceEventLogType::kRestoreInitiated: {
      auto synchronous =
          serialized_event.FindBool(kRestoreInitiatedEventSynchronousKey);
      event.data.restore_initiated.synchronous = synchronous && *synchronous;
      auto restore_browser =
          serialized_event.FindBool(kRestoreInitiatedEventRestoreBrowserKey);
      event.data.restore_initiated.restore_browser =
          restore_browser && *restore_browser;
      break;
    }
  }
  return true;
}

void SaveEventsToPrefs(Profile* profile,
                       const std::list<SessionServiceEvent>& events) {
  base::Value serialized_events(base::Value::Type::LIST);
  for (const SessionServiceEvent& event : events)
    serialized_events.GetList().Append(SerializeEvent(event));
  profile->GetPrefs()->Set(kEventPrefKey, serialized_events);
}

}  // namespace

std::list<SessionServiceEvent> GetSessionServiceEvents(Profile* profile) {
  const base::Value::List& serialized_events =
      profile->GetPrefs()->GetList(kEventPrefKey);
  std::list<SessionServiceEvent> events;
  for (const auto& serialized_event : serialized_events) {
    SessionServiceEvent event;
    if (!serialized_event.is_dict())
      continue;
    if (DeserializeEvent(serialized_event.GetDict(), event))
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
                                int tab_count,
                                bool is_first_session_service,
                                bool did_schedule_command) {
  SessionServiceEvent event;
  event.type = SessionServiceEventLogType::kExit;
  event.time = base::Time::Now();
  event.data.exit.window_count = window_count;
  event.data.exit.tab_count = tab_count;
  event.data.exit.is_first_session_service = is_first_session_service;
  event.data.exit.did_schedule_command = did_schedule_command;
  LogSessionServiceEvent(profile, event);
}

void LogSessionServiceRestoreInitiatedEvent(Profile* profile,
                                            bool synchronous,
                                            bool restore_browser) {
  SessionServiceEvent event;
  event.type = SessionServiceEventLogType::kRestoreInitiated;
  event.time = base::Time::Now();
  event.data.restore_initiated.synchronous = synchronous;
  event.data.restore_initiated.restore_browser = restore_browser;
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

void LogSessionServiceRestoreCanceledEvent(Profile* profile) {
  SessionServiceEvent event;
  event.type = SessionServiceEventLogType::kRestoreCanceled;
  event.time = base::Time::Now();
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
