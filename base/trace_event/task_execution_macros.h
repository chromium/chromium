// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TASK_EXECUTION_MACROS_H_
#define BASE_TRACE_EVENT_TASK_EXECUTION_MACROS_H_

#include "base/location.h"
#include "base/trace_event/heap_profiler.h"
#include "base/trace_event/interned_args_helper.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/log_message.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/task_execution.pbzero.h"

// Implementation detail: internal macro to trace a task execution with the
// location where it was posted from.
#define INTERNAL_TRACE_TASK_EXECUTION(run_function, task)                 \
  TRACE_EVENT("toplevel", run_function, [&](perfetto::EventContext ctx) { \
    ctx.event()->set_task_execution()->set_posted_from_iid(               \
        base::trace_event::InternedSourceLocation::Get(                   \
            &ctx,                                                         \
            base::trace_event::TraceSourceLocation((task).posted_from))); \
  });                                                                     \
  TRACE_HEAP_PROFILER_API_SCOPED_TASK_EXECUTION INTERNAL_TRACE_EVENT_UID( \
      task_event)((task).posted_from.file_name());

// Implementation detail: internal macro to trace a log message, with the source
// location of the log statement.
#define INTERNAL_TRACE_LOG_MESSAGE(file, message, line)                      \
  TRACE_EVENT_INSTANT("log", "LogMessage", [&](perfetto::EventContext ctx) { \
    perfetto::protos::pbzero::LogMessage* log =                              \
        ctx.event()->set_log_message();                                      \
    log->set_source_location_iid(                                            \
        base::trace_event::InternedSourceLocation::Get(                      \
            &ctx, base::trace_event::TraceSourceLocation(                    \
                      /*function_name=*/nullptr, file, line)));              \
    log->set_body_iid(base::trace_event::InternedLogMessage::Get(            \
        &ctx, std::string(message)));                                        \
  });

#endif  // BASE_TRACE_EVENT_TASK_EXECUTION_MACROS_H_
