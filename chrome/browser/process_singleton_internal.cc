// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_singleton_internal.h"

#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/trace_event/base_tracing.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "build/build_config.h"

namespace internal {

namespace {

#define CASE(enum_value)             \
  case ProcessSingleton::enum_value: \
    return perfetto::protos::pbzero::ProcessSingleton::enum_value

perfetto::protos::pbzero::ProcessSingleton::RemoteProcessInteractionResult
ToProtoEnum(ProcessSingleton::RemoteProcessInteractionResult result) {
  switch (result) {
    CASE(TERMINATE_SUCCEEDED);
    CASE(TERMINATE_FAILED);
    CASE(REMOTE_PROCESS_NOT_FOUND);
#if BUILDFLAG(IS_WIN)
    CASE(TERMINATE_WAIT_TIMEOUT);
    CASE(RUNNING_PROCESS_NOTIFY_ERROR);
#elif BUILDFLAG(IS_POSIX)
    CASE(TERMINATE_NOT_ENOUGH_PERMISSIONS);
    CASE(REMOTE_PROCESS_SHUTTING_DOWN);
    CASE(PROFILE_UNLOCKED);
    CASE(PROFILE_UNLOCKED_BEFORE_KILL);
    CASE(SAME_BROWSER_INSTANCE);
    CASE(SAME_BROWSER_INSTANCE_BEFORE_KILL);
    CASE(FAILED_TO_EXTRACT_PID);
    CASE(INVALID_LOCK_FILE);
    CASE(ORPHANED_LOCK_FILE);
#endif
    CASE(USER_REFUSED_TERMINATION);
    case ProcessSingleton::REMOTE_PROCESS_INTERACTION_RESULT_COUNT:
      NOTREACHED_IN_MIGRATION();
      return perfetto::protos::pbzero::ProcessSingleton::
          INTERACTION_RESULT_UNSPECIFIED;
  }
}

perfetto::protos::pbzero::ProcessSingleton::RemoteHungProcessTerminateReason
ToProtoEnum(ProcessSingleton::RemoteHungProcessTerminateReason reason) {
  switch (reason) {
#if BUILDFLAG(IS_WIN)
    CASE(USER_ACCEPTED_TERMINATION);
    CASE(NO_VISIBLE_WINDOW_FOUND);
#elif BUILDFLAG(IS_POSIX)
    CASE(NOTIFY_ATTEMPTS_EXCEEDED);
    CASE(SOCKET_WRITE_FAILED);
    CASE(SOCKET_READ_FAILED);
#endif
    case ProcessSingleton::REMOTE_HUNG_PROCESS_TERMINATE_REASON_COUNT:
      NOTREACHED_IN_MIGRATION();
      return perfetto::protos::pbzero::ProcessSingleton::
          TERMINATE_REASON_UNSPECIFIED;
  }
}

}  // namespace

void SendRemoteProcessInteractionResultHistogram(
    ProcessSingleton::RemoteProcessInteractionResult result) {
  TRACE_EVENT_INSTANT(
      "startup", "ProcessSingleton:SendRemoteProcessInteractionResultHistogram",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* process_singleton = event->set_process_singleton();
        process_singleton->set_remote_process_interaction_result(
            ToProtoEnum(result));
      });

  UMA_HISTOGRAM_ENUMERATION(
      "Chrome.ProcessSingleton.RemoteProcessInteractionResult", result,
      ProcessSingleton::REMOTE_PROCESS_INTERACTION_RESULT_COUNT);
}

void SendRemoteHungProcessTerminateReasonHistogram(
    ProcessSingleton::RemoteHungProcessTerminateReason reason) {
  TRACE_EVENT_INSTANT(
      "startup",
      "ProcessSingleton:SendRemoteHungProcessTerminateReasonHistogram",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* process_singleton = event->set_process_singleton();
        process_singleton->set_remote_process_terminate_reason(
            ToProtoEnum(reason));
      });

  UMA_HISTOGRAM_ENUMERATION(
      "Chrome.ProcessSingleton.RemoteHungProcessTerminateReason", reason,
      ProcessSingleton::REMOTE_HUNG_PROCESS_TERMINATE_REASON_COUNT);
}

}  // namespace internal
