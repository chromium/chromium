// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/syslog_logging.h"

#if defined(OS_WIN)
#include <windows.h>

#include <sddl.h>

#include "base/debug/stack_trace.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
// <syslog.h> defines LOG_INFO, LOG_WARNING macros that could conflict with
// base::LOG_INFO, base::LOG_WARNING.
#include <syslog.h>
#undef LOG_INFO
#undef LOG_WARNING
#endif

#include <ostream>
#include <string>

namespace logging {

#if defined(OS_WIN)

namespace {

std::string* g_event_source_name = nullptr;
uint16_t g_category = 0;
uint32_t g_event_id = 0;
std::wstring* g_user_sid = nullptr;

class EventLogHandleTraits {
 public:
  using Handle = HANDLE;

  // Closes the handle.
  static bool CloseHandle(HANDLE handle) {
    return ::DeregisterEventSource(handle) != FALSE;
  }

  // Returns true if the handle value is valid.
  static bool IsHandleValid(HANDLE handle) { return handle != nullptr; }

  // Returns null handle value.
  static HANDLE NullHandle() { return nullptr; }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(EventLogHandleTraits);
};

using ScopedEventLogHandle =
    base::win::GenericScopedHandle<EventLogHandleTraits,
                                   base::win::DummyVerifierTraits>;

}  // namespace

void SetEventSource(const std::string& name,
                    uint16_t category,
                    uint32_t event_id) {
  DCHECK_EQ(nullptr, g_event_source_name);
  g_event_source_name = new std::string(name);
  g_category = category;
  g_event_id = event_id;
  DCHECK_EQ(nullptr, g_user_sid);
  g_user_sid = new std::wstring();
  base::win::GetUserSidString(g_user_sid);
}

void ResetEventSourceForTesting() {
  delete g_event_source_name;
  g_event_source_name = nullptr;
  delete g_user_sid;
  g_user_sid = nullptr;
}

#endif  // defined(OS_WIN)

EventLogMessage::EventLogMessage(const char* file,
                                 int line,
                                 LogSeverity severity)
    : log_message_(file, line, severity) {
}

EventLogMessage::~EventLogMessage() {
#if defined(OS_WIN)
  // If g_event_source_name is nullptr (which it is per default) SYSLOG will
  // degrade gracefully to regular LOG. If you see this happening most probably
  // you are using SYSLOG before you called SetEventSourceName.
  if (g_event_source_name == nullptr)
    return;

  ScopedEventLogHandle event_log_handle(
      RegisterEventSourceA(nullptr, g_event_source_name->c_str()));

  if (!event_log_handle.IsValid()) {
    stream() << " !!NOT ADDED TO EVENTLOG!!";
    return;
  }

  std::string message(log_message_.str());
  WORD log_type = EVENTLOG_ERROR_TYPE;
  switch (log_message_.severity()) {
    case LOGGING_INFO:
      log_type = EVENTLOG_INFORMATION_TYPE;
      break;
    case LOGGING_WARNING:
      log_type = EVENTLOG_WARNING_TYPE;
      break;
    case LOGGING_ERROR:
    case LOGGING_FATAL:
      // The price of getting the stack trace is not worth the hassle for
      // non-error conditions.
      base::debug::StackTrace trace;
      message.append(trace.ToString());
      log_type = EVENTLOG_ERROR_TYPE;
      break;
  }
  LPCSTR strings[1] = {message.data()};
  PSID user_sid = nullptr;
  if (!::ConvertStringSidToSid(g_user_sid->c_str(), &user_sid)) {
    stream() << " !!ERROR GETTING USER SID!!";
  }

  if (!ReportEventA(event_log_handle.Get(), log_type, g_category, g_event_id,
                    user_sid, 1, 0, strings, nullptr)) {
    stream() << " !!NOT ADDED TO EVENTLOG!!";
  }

  if (user_sid != nullptr)
    ::LocalFree(user_sid);
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
  const char kEventSource[] = "chrome";
  openlog(kEventSource, LOG_NOWAIT | LOG_PID, LOG_USER);
  // We can't use the defined names for the logging severity from syslog.h
  // because they collide with the names of our own severity levels. Therefore
  // we use the actual values which of course do not match ours.
  // See sys/syslog.h for reference.
  int priority = 3;
  switch (log_message_.severity()) {
    case LOGGING_INFO:
      priority = 6;
      break;
    case LOGGING_WARNING:
      priority = 4;
      break;
    case LOGGING_ERROR:
      priority = 3;
      break;
    case LOGGING_FATAL:
      priority = 2;
      break;
  }
  syslog(priority, "%s", log_message_.str().c_str());
  closelog();
#endif  // defined(OS_WIN)
}

}  // namespace logging
