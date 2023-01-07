// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_SERVICE_LOG_H_
#define CHROME_BROWSER_SESSIONS_SESSION_SERVICE_LOG_H_

#include <list>

#include "base/time/time.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// This file contains functionality used to track interesting session restore
// events. This is primarily aimed at helping understand whether session
// restore is failing.
//
// The appropriate code calls to the various log functions. This data is
// tracked in prefs, and only a limited amount of data is kept around.

// WARNING: these values are persisted to disk, do not change.
enum class SessionServiceEventLogType {
  // The profile was started.
  kStart = 0,

  // A restore was triggered. Restore may be triggered more than once after
  // a start.
  kRestore = 1,

  // The profile was shut down. It's still possible for a crash to happen
  // after this. This is not logged if a crash happens before exit is attempted.
  kExit = 2,

  // An error in writing the file occurred. Multiple calls to AddEvent()
  // when the last event is a error result in combining the event (this is
  // done to ensure lots of write error don't spam the event log).
  kWriteError = 3,

  // Restore stopped because the Browser restore was going to restore to closed.
  kRestoreCanceled = 4,

  // A restore was initiated, meaning the browser will ask SessionService for
  // the data to restore asynchronously.
  kRestoreInitiated = 5,

  kMinValue = kStart,
  kMaxValue = kRestoreInitiated,
};

struct StartData {
  // Whether the last run of chrome crashed.
  bool did_last_session_crash;
};

struct RestoreData {
  // The number of windows restored.
  int window_count;

  // The number of tabs restored.
  int tab_count;

  // Whether there was an error in reading the file contents.
  bool encountered_error_reading;
};

struct ExitData {
  // The number of windows open at the time of exit.
  int window_count;

  // The total number of tabs open at the time of exit.
  int tab_count;

  // True if this the first SessionService created for the Profile. False
  // means the first SessionService was destroyed and a new one created.
  bool is_first_session_service;

  // True if at least one command was scheduled
  bool did_schedule_command;
};

struct WriteErrorData {
  // Number of write errors that occurred.
  int error_count;
  // Number of write errors that were unrecoverable. See SessionService for
  // details on this.
  int unrecoverable_error_count;
};

struct RestoreInitiatedData {
  bool synchronous;
  bool restore_browser;
};

union EventData {
  StartData start;
  RestoreData restore;
  ExitData exit;
  WriteErrorData write_error;
  RestoreInitiatedData restore_initiated;
};

struct SessionServiceEvent {
  SessionServiceEventLogType type;
  base::Time time;
  EventData data;
};

// Returns the most recent events, ordered with oldest event first. In general
// the times shouldn't be compared, as it's possible for bad clocks and/or
// timezone changes to cause an earlier event to have a later time.
std::list<SessionServiceEvent> GetSessionServiceEvents(Profile* profile);

void LogSessionServiceStartEvent(Profile* profile, bool after_crash);
void LogSessionServiceExitEvent(Profile* profile,
                                int window_count,
                                int tab_count,
                                bool is_first_session_service,
                                bool did_schedule_command);
void LogSessionServiceRestoreInitiatedEvent(Profile* profile,
                                            bool synchronous,
                                            bool restore_browser);
void LogSessionServiceRestoreEvent(Profile* profile,
                                   int window_count,
                                   int tab_count,
                                   bool encountered_error_reading);
void LogSessionServiceRestoreCanceledEvent(Profile* profile);
void LogSessionServiceWriteErrorEvent(Profile* profile,
                                      bool unrecoverable_write_error);
void RemoveLastSessionServiceEventOfType(Profile* profile,
                                         SessionServiceEventLogType type);

void RegisterSessionServiceLogProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

// This function is used internally, and is generally only exposed for testing.
void LogSessionServiceEvent(Profile* profile, const SessionServiceEvent& event);

#endif  // CHROME_BROWSER_SESSIONS_SESSION_SERVICE_LOG_H_
