// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TASK_EXECUTION_MACROS_H_
#define BASE_TRACE_EVENT_TASK_EXECUTION_MACROS_H_

#include "base/location.h"
#include "base/trace_event/heap_profiler.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_interned_data_index.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/log_message.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/source_location.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/task_execution.pbzero.h"

namespace {

// TrackEventInternedDataIndex expects the same data structure to be used for
// all interned fields with the same field number. We can't use base::Location
// for log event's location since base::Location uses program counter based
// location.
struct TraceSourceLocation {
  const char* function_name = nullptr;
  const char* file_name = nullptr;
  size_t line_number = 0;

  bool operator==(const TraceSourceLocation& other) const {
    return file_name == other.file_name &&
           function_name == other.function_name &&
           line_number == other.line_number;
  }
};

}  // namespace

namespace std {

template <>
struct ::std::hash<TraceSourceLocation> {
  std::size_t operator()(const TraceSourceLocation& loc) const {
    static_assert(
        sizeof(TraceSourceLocation) == 2 * sizeof(const char*) + sizeof(size_t),
        "Padding will cause uninitialized memory to be hashed.");
    return base::FastHash(base::as_bytes(base::make_span(&loc, 1)));
  }
};

}  // namespace std

namespace {

struct InternedSourceLocation
    : public perfetto::TrackEventInternedDataIndex<
          InternedSourceLocation,
          perfetto::protos::pbzero::InternedData::kSourceLocationsFieldNumber,
          TraceSourceLocation> {
  static void Add(perfetto::protos::pbzero::InternedData* interned_data,
                  size_t iid,
                  const TraceSourceLocation& location) {
    auto* msg = interned_data->add_source_locations();
    msg->set_iid(iid);
    if (location.file_name != nullptr)
      msg->set_file_name(location.file_name);
    if (location.function_name != nullptr)
      msg->set_function_name(location.function_name);
    // TODO(ssid): Add line number once it is whitelisted in internal proto.
    // TODO(ssid): Add program counter to the proto fields when
    // !BUILDFLAG(ENABLE_LOCATION_SOURCE).
    // TODO(http://crbug.com760702) remove file name and just pass the program
    // counter to the heap profiler macro.
    // TODO(ssid): Consider writing the program counter of the current task
    // (from the callback function pointer) instead of location that posted the
    // task.
  }
};

struct InternedLogMessage
    : public perfetto::TrackEventInternedDataIndex<
          InternedLogMessage,
          perfetto::protos::pbzero::InternedData::kLogMessageBodyFieldNumber,
          std::string> {
  static void Add(perfetto::protos::pbzero::InternedData* interned_data,
                  size_t iid,
                  const std::string& log_message) {
    auto* msg = interned_data->add_log_message_body();
    msg->set_iid(iid);
    msg->set_body(log_message);
  }
};

}  // namespace

// Implementation detail: internal macro to trace a task execution with the
// location where it was posted from.
#define INTERNAL_TRACE_TASK_EXECUTION(run_function, task)                  \
  TRACE_EVENT("toplevel", run_function, [&](perfetto::EventContext ctx) {  \
    ctx.event()->set_task_execution()->set_posted_from_iid(                \
        InternedSourceLocation::Get(                                       \
            &ctx, TraceSourceLocation{(task).posted_from.function_name(),  \
                                      (task).posted_from.file_name(),      \
                                      (task).posted_from.line_number()})); \
  });                                                                      \
  TRACE_HEAP_PROFILER_API_SCOPED_TASK_EXECUTION INTERNAL_TRACE_EVENT_UID(  \
      task_event)((task).posted_from.file_name());                         \
  TRACE_HEAP_PROFILER_API_SCOPED_WITH_PROGRAM_COUNTER                      \
  INTERNAL_TRACE_EVENT_UID(task_pc_event)                                  \
  ((task).posted_from.program_counter());

// Implementation detail: internal macro to trace a log message, with the source
// location of the log statement.
#define INTERNAL_TRACE_LOG_MESSAGE(file, message, line)                        \
  TRACE_EVENT_INSTANT(                                                         \
      "log", "LogMessage", TRACE_EVENT_SCOPE_THREAD,                           \
      [&](perfetto::EventContext ctx) {                                        \
        perfetto::protos::pbzero::LogMessage* log =                            \
            ctx.event()->set_log_message();                                    \
        log->set_source_location_iid(InternedSourceLocation::Get(              \
            &ctx,                                                              \
            TraceSourceLocation{/*function_name=*/nullptr, file, line}));      \
        log->set_body_iid(InternedLogMessage::Get(&ctx, message.as_string())); \
      });

#endif  // BASE_TRACE_EVENT_TASK_EXECUTION_MACROS_H_
