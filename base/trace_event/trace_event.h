// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TRACE_EVENT_H_
#define BASE_TRACE_EVENT_TRACE_EVENT_H_

// This header file defines implementation details of how the trace macros in
// trace_event_common.h collect and store trace events. Anything not
// implementation-specific should go in trace_event_common.h instead of here.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/atomicops.h"
#include "base/debug/debugging_buildflags.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/heap_profiler.h"
#include "base/trace_event/trace_category.h"
#include "base/trace_event/trace_event_system_stats_monitor.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"

// By default, const char* argument values are assumed to have long-lived scope
// and will not be copied. Use this macro to force a const char* to be copied.
#define TRACE_STR_COPY(str) \
    trace_event_internal::TraceStringWithCopy(str)

// DEPRECATED: do not use: Consider using TRACE_ID_{GLOBAL, LOCAL} macros,
// instead. By default, uint64_t ID argument values are not mangled with the
// Process ID in TRACE_EVENT_ASYNC macros. Use this macro to force Process ID
// mangling.
#define TRACE_ID_MANGLE(id) \
    trace_event_internal::TraceID::ForceMangle(id)

// DEPRECATED: do not use: Consider using TRACE_ID_{GLOBAL, LOCAL} macros,
// instead. By default, pointers are mangled with the Process ID in
// TRACE_EVENT_ASYNC macros. Use this macro to prevent Process ID mangling.
#define TRACE_ID_DONT_MANGLE(id) \
    trace_event_internal::TraceID::DontMangle(id)

// By default, trace IDs are eventually converted to a single 64-bit number. Use
// this macro to add a scope string. For example,
//
// TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
//     "network", "ResourceLoad",
//     TRACE_ID_WITH_SCOPE("BlinkResourceID", resourceID));
//
// Also, it is possible to prepend the ID with another number, like the process
// ID. This is useful in creating IDs that are unique among all processes. To do
// that, pass two numbers after the scope string instead of one. For example,
//
// TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
//     "network", "ResourceLoad",
//     TRACE_ID_WITH_SCOPE("BlinkResourceID", pid, resourceID));
#define TRACE_ID_WITH_SCOPE(scope, ...) \
  trace_event_internal::TraceID::WithScope(scope, ##__VA_ARGS__)

#define TRACE_ID_GLOBAL(id) trace_event_internal::TraceID::GlobalId(id)
#define TRACE_ID_LOCAL(id) trace_event_internal::TraceID::LocalId(id)

#define TRACE_EVENT_API_CURRENT_THREAD_ID \
  static_cast<int>(base::PlatformThread::CurrentId())

#define INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED_FOR_RECORDING_MODE() \
  UNLIKELY(*INTERNAL_TRACE_EVENT_UID(category_group_enabled) &           \
           (base::trace_event::TraceCategory::ENABLED_FOR_RECORDING |    \
            base::trace_event::TraceCategory::ENABLED_FOR_ETW_EXPORT))

#define INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()                  \
  UNLIKELY(*INTERNAL_TRACE_EVENT_UID(category_group_enabled) &         \
           (base::trace_event::TraceCategory::ENABLED_FOR_RECORDING |  \
            base::trace_event::TraceCategory::ENABLED_FOR_ETW_EXPORT | \
            base::trace_event::TraceCategory::ENABLED_FOR_FILTERING))

////////////////////////////////////////////////////////////////////////////////
// Implementation specific tracing API definitions.

// Get a pointer to the enabled state of the given trace category. Only
// long-lived literal strings should be given as the category group. The
// returned pointer can be held permanently in a local static for example. If
// the unsigned char is non-zero, tracing is enabled. If tracing is enabled,
// TRACE_EVENT_API_ADD_TRACE_EVENT can be called. It's OK if tracing is disabled
// between the load of the tracing state and the call to
// TRACE_EVENT_API_ADD_TRACE_EVENT, because this flag only provides an early out
// for best performance when tracing is disabled.
// const unsigned char*
//     TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(const char* category_group)
#define TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED \
    base::trace_event::TraceLog::GetCategoryGroupEnabled

// Get the number of times traces have been recorded. This is used to implement
// the TRACE_EVENT_IS_NEW_TRACE facility.
// unsigned int TRACE_EVENT_API_GET_NUM_TRACES_RECORDED()
#define TRACE_EVENT_API_GET_NUM_TRACES_RECORDED \
    base::trace_event::TraceLog::GetInstance()->GetNumTracesRecorded

// Add a trace event to the platform tracing system.
// base::trace_event::TraceEventHandle TRACE_EVENT_API_ADD_TRACE_EVENT(
//                    char phase,
//                    const unsigned char* category_group_enabled,
//                    const char* name,
//                    const char* scope,
//                    unsigned long long id,
//                    int num_args,
//                    const char** arg_names,
//                    const unsigned char* arg_types,
//                    const unsigned long long* arg_values,
//                    std::unique_ptr<ConvertableToTraceFormat>*
//                    convertable_values,
//                    unsigned int flags)
#define TRACE_EVENT_API_ADD_TRACE_EVENT \
    base::trace_event::TraceLog::GetInstance()->AddTraceEvent

// Add a trace event to the platform tracing system.
// base::trace_event::TraceEventHandle
// TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_BIND_ID(
//                    char phase,
//                    const unsigned char* category_group_enabled,
//                    const char* name,
//                    const char* scope,
//                    unsigned long long id,
//                    unsigned long long bind_id,
//                    int num_args,
//                    const char** arg_names,
//                    const unsigned char* arg_types,
//                    const unsigned long long* arg_values,
//                    std::unique_ptr<ConvertableToTraceFormat>*
//                    convertable_values,
//                    unsigned int flags)
#define TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_BIND_ID \
  base::trace_event::TraceLog::GetInstance()->AddTraceEventWithBindId

// Add a trace event to the platform tracing system overriding the pid.
// The resulting event will have tid = pid == (process_id passed here).
// base::trace_event::TraceEventHandle
// TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_PROCESS_ID(
//                    char phase,
//                    const unsigned char* category_group_enabled,
//                    const char* name,
//                    const char* scope,
//                    unsigned long long id,
//                    int process_id,
//                    int num_args,
//                    const char** arg_names,
//                    const unsigned char* arg_types,
//                    const unsigned long long* arg_values,
//                    std::unique_ptr<ConvertableToTraceFormat>*
//                    convertable_values,
//                    unsigned int flags)
#define TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_PROCESS_ID \
  base::trace_event::TraceLog::GetInstance()->AddTraceEventWithProcessId

// Add a trace event to the platform tracing system.
// base::trace_event::TraceEventHandle
// TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_TIMESTAMP(
//                    char phase,
//                    const unsigned char* category_group_enabled,
//                    const char* name,
//                    const char* scope,
//                    unsigned long long id,
//                    int thread_id,
//                    const TimeTicks& timestamp,
//                    int num_args,
//                    const char** arg_names,
//                    const unsigned char* arg_types,
//                    const unsigned long long* arg_values,
//                    std::unique_ptr<ConvertableToTraceFormat>*
//                    convertable_values,
//                    unsigned int flags)
#define TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP \
    base::trace_event::TraceLog::GetInstance() \
      ->AddTraceEventWithThreadIdAndTimestamp

// Set the duration field of a COMPLETE trace event.
// void TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION(
//     const unsigned char* category_group_enabled,
//     const char* name,
//     base::trace_event::TraceEventHandle id)
#define TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION \
    base::trace_event::TraceLog::GetInstance()->UpdateTraceEventDuration

// Set the duration field of a COMPLETE trace event.
// void TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION_EXPLICIT(
//     const unsigned char* category_group_enabled,
//     const char* name,
//     base::trace_event::TraceEventHandle id,
//     const TimeTicks& now,
//     const ThreadTicks* thread_now)
#define TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION_EXPLICIT \
  base::trace_event::TraceLog::GetInstance()->UpdateTraceEventDurationExplicit

// Adds a metadata event to the trace log. The |AppendValueAsTraceFormat| method
// on the convertable value will be called at flush time.
// TRACE_EVENT_API_ADD_METADATA_EVENT(
//     const unsigned char* category_group_enabled,
//     const char* event_name,
//     const char* arg_name,
//     std::unique_ptr<ConvertableToTraceFormat> arg_value)
#define TRACE_EVENT_API_ADD_METADATA_EVENT \
    trace_event_internal::AddMetadataEvent

// Defines atomic operations used internally by the tracing system.
#define TRACE_EVENT_API_ATOMIC_WORD base::subtle::AtomicWord
#define TRACE_EVENT_API_ATOMIC_LOAD(var) base::subtle::NoBarrier_Load(&(var))
#define TRACE_EVENT_API_ATOMIC_STORE(var, value) \
    base::subtle::NoBarrier_Store(&(var), (value))

// Defines visibility for classes in trace_event.h
#define TRACE_EVENT_API_CLASS_EXPORT BASE_EXPORT

////////////////////////////////////////////////////////////////////////////////

// Implementation detail: trace event macros create temporary variables
// to keep instrumentation overhead low. These macros give each temporary
// variable a unique name based on the line number to prevent name collisions.
#define INTERNAL_TRACE_EVENT_UID3(a,b) \
    trace_event_unique_##a##b
#define INTERNAL_TRACE_EVENT_UID2(a,b) \
    INTERNAL_TRACE_EVENT_UID3(a,b)
#define INTERNAL_TRACE_EVENT_UID(name_prefix) \
    INTERNAL_TRACE_EVENT_UID2(name_prefix, __LINE__)

// Implementation detail: internal macro to create static category.
// No barriers are needed, because this code is designed to operate safely
// even when the unsigned char* points to garbage data (which may be the case
// on processors without cache coherency).
#define INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO_CUSTOM_VARIABLES( \
    category_group, atomic, category_group_enabled) \
    category_group_enabled = \
        reinterpret_cast<const unsigned char*>(TRACE_EVENT_API_ATOMIC_LOAD( \
            atomic)); \
    if (UNLIKELY(!category_group_enabled)) { \
      category_group_enabled = \
          TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(category_group); \
      TRACE_EVENT_API_ATOMIC_STORE(atomic, \
          reinterpret_cast<TRACE_EVENT_API_ATOMIC_WORD>( \
              category_group_enabled)); \
    }

#define INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group) \
    static TRACE_EVENT_API_ATOMIC_WORD INTERNAL_TRACE_EVENT_UID(atomic) = 0; \
    const unsigned char* INTERNAL_TRACE_EVENT_UID(category_group_enabled); \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO_CUSTOM_VARIABLES(category_group, \
        INTERNAL_TRACE_EVENT_UID(atomic), \
        INTERNAL_TRACE_EVENT_UID(category_group_enabled));

// Implementation detail: internal macro to return unoverridden
// base::TimeTicks::Now(). This is important because in headless VirtualTime can
// override base:TimeTicks::Now().
#define INTERNAL_TRACE_TIME_TICKS_NOW() \
  base::subtle::TimeTicksNowIgnoringOverride()

// Implementation detail: internal macro to return unoverridden
// base::Time::Now(). This is important because in headless VirtualTime can
// override base:TimeTicks::Now().
#define INTERNAL_TRACE_TIME_NOW() base::subtle::TimeNowIgnoringOverride()

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD(phase, category_group, name, flags, ...)  \
  do {                                                                     \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);                \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                   \
      trace_event_internal::AddTraceEvent(                                 \
          phase, INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,   \
          trace_event_internal::kGlobalScope, trace_event_internal::kNoId, \
          flags, trace_event_internal::kNoId, ##__VA_ARGS__);              \
    }                                                                      \
  } while (0)

// Implementation detail: internal macro to create static category and add begin
// event if the category is enabled. Also adds the end event when the scope
// ends.
#define INTERNAL_TRACE_EVENT_ADD_SCOPED(category_group, name, ...)           \
  INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);                    \
  trace_event_internal::ScopedTracer INTERNAL_TRACE_EVENT_UID(tracer);       \
  if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                       \
    base::trace_event::TraceEventHandle h =                                  \
        trace_event_internal::AddTraceEvent(                                 \
            TRACE_EVENT_PHASE_COMPLETE,                                      \
            INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,          \
            trace_event_internal::kGlobalScope, trace_event_internal::kNoId, \
            TRACE_EVENT_FLAG_NONE, trace_event_internal::kNoId,              \
            ##__VA_ARGS__);                                                  \
    INTERNAL_TRACE_EVENT_UID(tracer).Initialize(                             \
        INTERNAL_TRACE_EVENT_UID(category_group_enabled), name, h);          \
  }

#define INTERNAL_TRACE_EVENT_ADD_SCOPED_WITH_FLOW(category_group, name,      \
                                                  bind_id, flow_flags, ...)  \
  INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);                    \
  trace_event_internal::ScopedTracer INTERNAL_TRACE_EVENT_UID(tracer);       \
  if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                       \
    trace_event_internal::TraceID trace_event_bind_id((bind_id));            \
    unsigned int trace_event_flags =                                         \
        flow_flags | trace_event_bind_id.id_flags();                         \
    base::trace_event::TraceEventHandle h =                                  \
        trace_event_internal::AddTraceEvent(                                 \
            TRACE_EVENT_PHASE_COMPLETE,                                      \
            INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,          \
            trace_event_internal::kGlobalScope, trace_event_internal::kNoId, \
            trace_event_flags, trace_event_bind_id.raw_id(), ##__VA_ARGS__); \
    INTERNAL_TRACE_EVENT_UID(tracer).Initialize(                             \
        INTERNAL_TRACE_EVENT_UID(category_group_enabled), name, h);          \
  }

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD_WITH_ID(phase, category_group, name, id, \
                                         flags, ...)                      \
  do {                                                                    \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);               \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                  \
      trace_event_internal::TraceID trace_event_trace_id((id));           \
      unsigned int trace_event_flags =                                    \
          flags | trace_event_trace_id.id_flags();                        \
      trace_event_internal::AddTraceEvent(                                \
          phase, INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,  \
          trace_event_trace_id.scope(), trace_event_trace_id.raw_id(),    \
          trace_event_flags, trace_event_internal::kNoId, ##__VA_ARGS__); \
    }                                                                     \
  } while (0)

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD_WITH_TIMESTAMP(phase, category_group, name, \
                                                timestamp, flags, ...)       \
  do {                                                                       \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);                  \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                     \
      trace_event_internal::AddTraceEventWithThreadIdAndTimestamp(           \
          phase, INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,     \
          trace_event_internal::kGlobalScope, trace_event_internal::kNoId,   \
          TRACE_EVENT_API_CURRENT_THREAD_ID, timestamp,                      \
          flags | TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP,                       \
          trace_event_internal::kNoId, ##__VA_ARGS__);                       \
    }                                                                        \
  } while (0)

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(              \
    phase, category_group, name, id, thread_id, timestamp, flags, ...)   \
  do {                                                                   \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);              \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                 \
      trace_event_internal::TraceID trace_event_trace_id((id));          \
      unsigned int trace_event_flags =                                   \
          flags | trace_event_trace_id.id_flags();                       \
      trace_event_internal::AddTraceEventWithThreadIdAndTimestamp(       \
          phase, INTERNAL_TRACE_EVENT_UID(category_group_enabled), name, \
          trace_event_trace_id.scope(), trace_event_trace_id.raw_id(),   \
          thread_id, timestamp,                                          \
          trace_event_flags | TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP,       \
          trace_event_internal::kNoId, ##__VA_ARGS__);                   \
    }                                                                    \
  } while (0)

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMPS(                \
    category_group, name, id, thread_id, begin_timestamp, end_timestamp,    \
    thread_end_timestamp, flags, ...)                                       \
  do {                                                                      \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);                 \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                    \
      trace_event_internal::TraceID trace_event_trace_id((id));             \
      unsigned int trace_event_flags =                                      \
          flags | trace_event_trace_id.id_flags();                          \
      const unsigned char* uid_category_group_enabled =                     \
          INTERNAL_TRACE_EVENT_UID(category_group_enabled);                 \
      auto handle =                                                         \
          trace_event_internal::AddTraceEventWithThreadIdAndTimestamp(      \
              TRACE_EVENT_PHASE_COMPLETE, uid_category_group_enabled, name, \
              trace_event_trace_id.scope(), trace_event_trace_id.raw_id(),  \
              thread_id, begin_timestamp,                                   \
              trace_event_flags | TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP,      \
              trace_event_internal::kNoId, ##__VA_ARGS__);                  \
      TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION_EXPLICIT(                 \
          uid_category_group_enabled, name, handle, end_timestamp,          \
          thread_end_timestamp);                                            \
    }                                                                       \
  } while (0)

// The linked ID will not be mangled.
#define INTERNAL_TRACE_EVENT_ADD_LINK_IDS(category_group, name, id1, id2) \
  do {                                                                    \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);               \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                  \
      trace_event_internal::TraceID source_id((id1));                     \
      unsigned int source_flags = source_id.id_flags();                   \
      trace_event_internal::TraceID target_id((id2));                     \
      trace_event_internal::AddTraceEvent(                                \
          TRACE_EVENT_PHASE_LINK_IDS,                                     \
          INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,         \
          source_id.scope(), source_id.raw_id(), source_flags,            \
          trace_event_internal::kNoId, "linked_id",                       \
          target_id.AsConvertableToTraceFormat());                        \
    }                                                                     \
  } while (0)

// Implementation detail: internal macro to create static category and add
// metadata event if the category is enabled.
#define INTERNAL_TRACE_EVENT_METADATA_ADD(category_group, name, ...) \
  do {                                                               \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);          \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {             \
      TRACE_EVENT_API_ADD_METADATA_EVENT(                            \
          INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,    \
          ##__VA_ARGS__);                                            \
    }                                                                \
  } while (0)

// Implementation detail: internal macro to enter and leave a
// context based on the current scope.
#define INTERNAL_TRACE_EVENT_SCOPED_CONTEXT(category_group, name, context) \
  struct INTERNAL_TRACE_EVENT_UID(ScopedContext) {                         \
   public:                                                                 \
    INTERNAL_TRACE_EVENT_UID(ScopedContext)(uint64_t cid) : cid_(cid) {    \
      TRACE_EVENT_ENTER_CONTEXT(category_group, name, cid_);               \
    }                                                                      \
    ~INTERNAL_TRACE_EVENT_UID(ScopedContext)() {                           \
      TRACE_EVENT_LEAVE_CONTEXT(category_group, name, cid_);               \
    }                                                                      \
                                                                           \
   private:                                                                \
    uint64_t cid_;                                                         \
    /* Local class friendly DISALLOW_COPY_AND_ASSIGN */                    \
    INTERNAL_TRACE_EVENT_UID(ScopedContext)                                \
    (const INTERNAL_TRACE_EVENT_UID(ScopedContext)&) {};                   \
    void operator=(const INTERNAL_TRACE_EVENT_UID(ScopedContext)&) {};     \
  };                                                                       \
  INTERNAL_TRACE_EVENT_UID(ScopedContext)                                  \
  INTERNAL_TRACE_EVENT_UID(scoped_context)(context);

#if BUILDFLAG(ENABLE_LOCATION_SOURCE)

// Implementation detail: internal macro to trace a task execution with the
// location where it was posted from.
//
// This implementation is for when location sources are available.
// TODO(ssid): The program counter of the current task should be added here.
#define INTERNAL_TRACE_TASK_EXECUTION(run_function, task)                 \
  TRACE_EVENT2("toplevel", run_function, "src_file",                      \
               (task).posted_from.file_name(), "src_func",                \
               (task).posted_from.function_name());                       \
  TRACE_HEAP_PROFILER_API_SCOPED_TASK_EXECUTION INTERNAL_TRACE_EVENT_UID( \
      task_event)((task).posted_from.file_name());                        \
  TRACE_HEAP_PROFILER_API_SCOPED_WITH_PROGRAM_COUNTER                     \
  INTERNAL_TRACE_EVENT_UID(task_pc_event)((task).posted_from.program_counter());

#else

// TODO(http://crbug.com760702) remove file name and just pass the program
// counter to the heap profiler macro.
// TODO(ssid): The program counter of the current task should be added here.
#define INTERNAL_TRACE_TASK_EXECUTION(run_function, task)                      \
  TRACE_EVENT1("toplevel", run_function, "src", (task).posted_from.ToString()) \
  TRACE_HEAP_PROFILER_API_SCOPED_TASK_EXECUTION INTERNAL_TRACE_EVENT_UID(      \
      task_event)((task).posted_from.file_name());                             \
  TRACE_HEAP_PROFILER_API_SCOPED_WITH_PROGRAM_COUNTER                          \
  INTERNAL_TRACE_EVENT_UID(task_pc_event)((task).posted_from.program_counter());

#endif

namespace trace_event_internal {

// Specify these values when the corresponding argument of AddTraceEvent is not
// used.
const int kZeroNumArgs = 0;
const std::nullptr_t kGlobalScope = nullptr;
const unsigned long long kNoId = 0;

// TraceID encapsulates an ID that can either be an integer or pointer. Pointers
// are by default mangled with the Process ID so that they are unlikely to
// collide when the same pointer is used on different processes.
class BASE_EXPORT TraceID {
 public:
  // Can be combined with WithScope.
  class LocalId {
   public:
    explicit LocalId(const void* raw_id)
        : raw_id_(static_cast<unsigned long long>(
              reinterpret_cast<uintptr_t>(raw_id))) {}
    explicit LocalId(unsigned long long raw_id) : raw_id_(raw_id) {}
    unsigned long long raw_id() const { return raw_id_; }
   private:
    unsigned long long raw_id_;
  };

  // Can be combined with WithScope.
  class GlobalId {
   public:
    explicit GlobalId(unsigned long long raw_id) : raw_id_(raw_id) {}
    unsigned long long raw_id() const { return raw_id_; }
   private:
    unsigned long long raw_id_;
  };

  class WithScope {
   public:
    WithScope(const char* scope, unsigned long long raw_id)
        : scope_(scope), raw_id_(raw_id) {}
    WithScope(const char* scope, LocalId local_id)
        : scope_(scope), raw_id_(local_id.raw_id()) {
      id_flags_ = TRACE_EVENT_FLAG_HAS_LOCAL_ID;
    }
    WithScope(const char* scope, GlobalId global_id)
        : scope_(scope), raw_id_(global_id.raw_id()) {
      id_flags_ = TRACE_EVENT_FLAG_HAS_GLOBAL_ID;
    }
    WithScope(const char* scope,
              unsigned long long prefix,
              unsigned long long raw_id)
        : scope_(scope), has_prefix_(true), prefix_(prefix), raw_id_(raw_id) {}
    WithScope(const char* scope, unsigned long long prefix, GlobalId global_id)
        : scope_(scope),
          has_prefix_(true),
          prefix_(prefix),
          raw_id_(global_id.raw_id()) {
      id_flags_ = TRACE_EVENT_FLAG_HAS_GLOBAL_ID;
    }
    unsigned long long raw_id() const { return raw_id_; }
    const char* scope() const { return scope_; }
    bool has_prefix() const { return has_prefix_; }
    unsigned long long prefix() const { return prefix_; }
    unsigned int id_flags() const { return id_flags_; }

   private:
    const char* scope_ = nullptr;
    bool has_prefix_ = false;
    unsigned long long prefix_;
    unsigned long long raw_id_;
    unsigned int id_flags_ = TRACE_EVENT_FLAG_HAS_ID;
  };

  // DEPRECATED: consider using LocalId or GlobalId, instead.
  class DontMangle {
   public:
    explicit DontMangle(const void* raw_id)
        : raw_id_(static_cast<unsigned long long>(
              reinterpret_cast<uintptr_t>(raw_id))) {}
    explicit DontMangle(unsigned long long raw_id) : raw_id_(raw_id) {}
    explicit DontMangle(unsigned long raw_id) : raw_id_(raw_id) {}
    explicit DontMangle(unsigned int raw_id) : raw_id_(raw_id) {}
    explicit DontMangle(unsigned short raw_id) : raw_id_(raw_id) {}
    explicit DontMangle(unsigned char raw_id) : raw_id_(raw_id) {}
    explicit DontMangle(long long raw_id)
        : raw_id_(static_cast<unsigned long long>(raw_id)) {}
    explicit DontMangle(long raw_id)
        : raw_id_(static_cast<unsigned long long>(raw_id)) {}
    explicit DontMangle(int raw_id)
        : raw_id_(static_cast<unsigned long long>(raw_id)) {}
    explicit DontMangle(short raw_id)
        : raw_id_(static_cast<unsigned long long>(raw_id)) {}
    explicit DontMangle(signed char raw_id)
        : raw_id_(static_cast<unsigned long long>(raw_id)) {}
    unsigned long long raw_id() const { return raw_id_; }
   private:
    unsigned long long raw_id_;
  };

  // DEPRECATED: consider using LocalId or GlobalId, instead.
  class ForceMangle {
   public:
    explicit ForceMangle(unsigned long long raw_id) : raw_id_(raw_id) {}
    explicit ForceMangle(unsigned long raw_id) : raw_id_(raw_id) {}
    explicit ForceMangle(unsigned int raw_id) : raw_id_(raw_id) {}
    explicit ForceMangle(unsigned short raw_id) : raw_id_(raw_id) {}
    explicit ForceMangle(unsigned char raw_id) : raw_id_(raw_id) {}
    explicit ForceMangle(long long raw_id)
        : raw_id_(static_cast<unsigned long long>(raw_id)) {}
    explicit ForceMangle(long raw_id)
        : raw_id_(static_cast<unsigned long long>(raw_id)) {}
    explicit ForceMangle(int raw_id)
        : raw_id_(static_cast<unsigned long long>(raw_id)) {}
    explicit ForceMangle(short raw_id)
        : raw_id_(static_cast<unsigned long long>(raw_id)) {}
    explicit ForceMangle(signed char raw_id)
        : raw_id_(static_cast<unsigned long long>(raw_id)) {}
    unsigned long long raw_id() const { return raw_id_; }
   private:
    unsigned long long raw_id_;
  };

  TraceID(const void* raw_id) : raw_id_(static_cast<unsigned long long>(
                                        reinterpret_cast<uintptr_t>(raw_id))) {
    id_flags_ = TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_MANGLE_ID;
  }
  TraceID(ForceMangle raw_id) : raw_id_(raw_id.raw_id()) {
    id_flags_ = TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_MANGLE_ID;
  }
  TraceID(DontMangle raw_id) : raw_id_(raw_id.raw_id()) {}
  TraceID(unsigned long long raw_id) : raw_id_(raw_id) {}
  TraceID(unsigned long raw_id) : raw_id_(raw_id) {}
  TraceID(unsigned int raw_id) : raw_id_(raw_id) {}
  TraceID(unsigned short raw_id) : raw_id_(raw_id) {}
  TraceID(unsigned char raw_id) : raw_id_(raw_id) {}
  TraceID(long long raw_id)
      : raw_id_(static_cast<unsigned long long>(raw_id)) {}
  TraceID(long raw_id)
      : raw_id_(static_cast<unsigned long long>(raw_id)) {}
  TraceID(int raw_id)
      : raw_id_(static_cast<unsigned long long>(raw_id)) {}
  TraceID(short raw_id)
      : raw_id_(static_cast<unsigned long long>(raw_id)) {}
  TraceID(signed char raw_id)
      : raw_id_(static_cast<unsigned long long>(raw_id)) {}
  TraceID(LocalId raw_id) : raw_id_(raw_id.raw_id()) {
    id_flags_ = TRACE_EVENT_FLAG_HAS_LOCAL_ID;
  }
  TraceID(GlobalId raw_id) : raw_id_(raw_id.raw_id()) {
    id_flags_ = TRACE_EVENT_FLAG_HAS_GLOBAL_ID;
  }
  TraceID(WithScope scoped_id)
      : scope_(scoped_id.scope()),
        has_prefix_(scoped_id.has_prefix()),
        prefix_(scoped_id.prefix()),
        raw_id_(scoped_id.raw_id()),
        id_flags_(scoped_id.id_flags()) {}

  unsigned long long raw_id() const { return raw_id_; }
  const char* scope() const { return scope_; }
  bool has_prefix() const { return has_prefix_; }
  unsigned long long prefix() const { return prefix_; }
  unsigned int id_flags() const { return id_flags_; }

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
  AsConvertableToTraceFormat() const;

 private:
  const char* scope_ = nullptr;
  bool has_prefix_ = false;
  unsigned long long prefix_;
  unsigned long long raw_id_;
  unsigned int id_flags_ = TRACE_EVENT_FLAG_HAS_ID;
};

// Simple union to store various types as unsigned long long.
union TraceValueUnion {
  bool as_bool;
  unsigned long long as_uint;
  long long as_int;
  double as_double;
  const void* as_pointer;
  const char* as_string;
};

// Simple container for const char* that should be copied instead of retained.
class TraceStringWithCopy {
 public:
  explicit TraceStringWithCopy(const char* str) : str_(str) {}
  const char* str() const { return str_; }
 private:
  const char* str_;
};

// Define SetTraceValue for each allowed type. It stores the type and
// value in the return arguments. This allows this API to avoid declaring any
// structures so that it is portable to third_party libraries.
#define INTERNAL_DECLARE_SET_TRACE_VALUE(actual_type, \
                                         arg_expression, \
                                         union_member, \
                                         value_type_id) \
    static inline void SetTraceValue( \
        actual_type arg, \
        unsigned char* type, \
        unsigned long long* value) { \
      TraceValueUnion type_value; \
      type_value.union_member = arg_expression; \
      *type = value_type_id; \
      *value = type_value.as_uint; \
    }
// Simpler form for int types that can be safely casted.
#define INTERNAL_DECLARE_SET_TRACE_VALUE_INT(actual_type, \
                                             value_type_id) \
    static inline void SetTraceValue( \
        actual_type arg, \
        unsigned char* type, \
        unsigned long long* value) { \
      *type = value_type_id; \
      *value = static_cast<unsigned long long>(arg); \
    }

INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned long long, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned long, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned int, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned short, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned char, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(long long, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(long, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(int, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(short, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(signed char, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE(bool, arg, as_bool, TRACE_VALUE_TYPE_BOOL)
INTERNAL_DECLARE_SET_TRACE_VALUE(double, arg, as_double,
                                 TRACE_VALUE_TYPE_DOUBLE)
INTERNAL_DECLARE_SET_TRACE_VALUE(const void*, arg, as_pointer,
                                 TRACE_VALUE_TYPE_POINTER)
INTERNAL_DECLARE_SET_TRACE_VALUE(const char*, arg, as_string,
                                 TRACE_VALUE_TYPE_STRING)
INTERNAL_DECLARE_SET_TRACE_VALUE(const TraceStringWithCopy&, arg.str(),
                                 as_string, TRACE_VALUE_TYPE_COPY_STRING)

#undef INTERNAL_DECLARE_SET_TRACE_VALUE
#undef INTERNAL_DECLARE_SET_TRACE_VALUE_INT

// std::string version of SetTraceValue so that trace arguments can be strings.
static inline void SetTraceValue(const std::string& arg,
                                 unsigned char* type,
                                 unsigned long long* value) {
  TraceValueUnion type_value;
  type_value.as_string = arg.c_str();
  *type = TRACE_VALUE_TYPE_COPY_STRING;
  *value = type_value.as_uint;
}

// base::Time, base::TimeTicks, etc. versions of SetTraceValue to make it easier
// to trace these types.
static inline void SetTraceValue(const base::Time arg,
                                 unsigned char* type,
                                 unsigned long long* value) {
  *type = TRACE_VALUE_TYPE_INT;
  *value = arg.ToInternalValue();
}

static inline void SetTraceValue(const base::TimeTicks arg,
                                 unsigned char* type,
                                 unsigned long long* value) {
  *type = TRACE_VALUE_TYPE_INT;
  *value = arg.ToInternalValue();
}

static inline void SetTraceValue(const base::ThreadTicks arg,
                                 unsigned char* type,
                                 unsigned long long* value) {
  *type = TRACE_VALUE_TYPE_INT;
  *value = arg.ToInternalValue();
}

// These AddTraceEvent and AddTraceEventWithThreadIdAndTimestamp template
// functions are defined here instead of in the macro, because the arg_values
// could be temporary objects, such as std::string. In order to store
// pointers to the internal c_str and pass through to the tracing API,
// the arg_values must live throughout these procedures.

template <class ARG1_CONVERTABLE_TYPE>
static inline base::trace_event::TraceEventHandle
AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    int thread_id,
    const base::TimeTicks& timestamp,
    unsigned int flags,
    unsigned long long bind_id,
    const char* arg1_name,
    std::unique_ptr<ARG1_CONVERTABLE_TYPE> arg1_val) {
  const int num_args = 1;
  unsigned char arg_types[1] = { TRACE_VALUE_TYPE_CONVERTABLE };
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
      convertable_values[1] = {std::move(arg1_val)};
  return TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
      phase, category_group_enabled, name, scope, id, bind_id, thread_id,
      timestamp, num_args, &arg1_name, arg_types, NULL, convertable_values,
      flags);
}

template <class ARG1_TYPE, class ARG2_CONVERTABLE_TYPE>
static inline base::trace_event::TraceEventHandle
AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    int thread_id,
    const base::TimeTicks& timestamp,
    unsigned int flags,
    unsigned long long bind_id,
    const char* arg1_name,
    const ARG1_TYPE& arg1_val,
    const char* arg2_name,
    std::unique_ptr<ARG2_CONVERTABLE_TYPE> arg2_val) {
  const int num_args = 2;
  const char* arg_names[2] = { arg1_name, arg2_name };

  unsigned char arg_types[2];
  unsigned long long arg_values[2];
  SetTraceValue(arg1_val, &arg_types[0], &arg_values[0]);
  arg_types[1] = TRACE_VALUE_TYPE_CONVERTABLE;
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
      convertable_values[2] = {nullptr, std::move(arg2_val)};
  return TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
      phase, category_group_enabled, name, scope, id, bind_id, thread_id,
      timestamp, num_args, arg_names, arg_types, arg_values, convertable_values,
      flags);
}

template <class ARG1_CONVERTABLE_TYPE, class ARG2_TYPE>
static inline base::trace_event::TraceEventHandle
AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    int thread_id,
    const base::TimeTicks& timestamp,
    unsigned int flags,
    unsigned long long bind_id,
    const char* arg1_name,
    std::unique_ptr<ARG1_CONVERTABLE_TYPE> arg1_val,
    const char* arg2_name,
    const ARG2_TYPE& arg2_val) {
  const int num_args = 2;
  const char* arg_names[2] = { arg1_name, arg2_name };

  unsigned char arg_types[2];
  unsigned long long arg_values[2];
  arg_types[0] = TRACE_VALUE_TYPE_CONVERTABLE;
  arg_values[0] = 0;
  SetTraceValue(arg2_val, &arg_types[1], &arg_values[1]);
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
      convertable_values[2] = {std::move(arg1_val), nullptr};
  return TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
      phase, category_group_enabled, name, scope, id, bind_id, thread_id,
      timestamp, num_args, arg_names, arg_types, arg_values, convertable_values,
      flags);
}

template <class ARG1_CONVERTABLE_TYPE, class ARG2_CONVERTABLE_TYPE>
static inline base::trace_event::TraceEventHandle
AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    int thread_id,
    const base::TimeTicks& timestamp,
    unsigned int flags,
    unsigned long long bind_id,
    const char* arg1_name,
    std::unique_ptr<ARG1_CONVERTABLE_TYPE> arg1_val,
    const char* arg2_name,
    std::unique_ptr<ARG2_CONVERTABLE_TYPE> arg2_val) {
  const int num_args = 2;
  const char* arg_names[2] = { arg1_name, arg2_name };
  unsigned char arg_types[2] =
      { TRACE_VALUE_TYPE_CONVERTABLE, TRACE_VALUE_TYPE_CONVERTABLE };
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
      convertable_values[2] = {std::move(arg1_val), std::move(arg2_val)};
  return TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
      phase, category_group_enabled, name, scope, id, bind_id, thread_id,
      timestamp, num_args, arg_names, arg_types, NULL, convertable_values,
      flags);
}

static inline base::trace_event::TraceEventHandle
AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    int thread_id,
    const base::TimeTicks& timestamp,
    unsigned int flags,
    unsigned long long bind_id) {
  return TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
      phase, category_group_enabled, name, scope, id, bind_id, thread_id,
      timestamp, kZeroNumArgs, NULL, NULL, NULL, NULL, flags);
}

static inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned int flags,
    unsigned long long bind_id) {
  const int thread_id = static_cast<int>(base::PlatformThread::CurrentId());
  const base::TimeTicks now = TRACE_TIME_TICKS_NOW();
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id, thread_id, now, flags,
      bind_id);
}

template<class ARG1_TYPE>
static inline base::trace_event::TraceEventHandle
AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    int thread_id,
    const base::TimeTicks& timestamp,
    unsigned int flags,
    unsigned long long bind_id,
    const char* arg1_name,
    const ARG1_TYPE& arg1_val) {
  const int num_args = 1;
  unsigned char arg_types[1];
  unsigned long long arg_values[1];
  SetTraceValue(arg1_val, &arg_types[0], &arg_values[0]);
  return TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
      phase, category_group_enabled, name, scope, id, bind_id, thread_id,
      timestamp, num_args, &arg1_name, arg_types, arg_values, NULL, flags);
}

template<class ARG1_TYPE>
static inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned int flags,
    unsigned long long bind_id,
    const char* arg1_name,
    const ARG1_TYPE& arg1_val) {
  int thread_id = static_cast<int>(base::PlatformThread::CurrentId());
  base::TimeTicks now = TRACE_TIME_TICKS_NOW();
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id, thread_id, now, flags,
      bind_id, arg1_name, arg1_val);
}

template <class ARG1_CONVERTABLE_TYPE>
static inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned int flags,
    unsigned long long bind_id,
    const char* arg1_name,
    std::unique_ptr<ARG1_CONVERTABLE_TYPE> arg1_val) {
  int thread_id = static_cast<int>(base::PlatformThread::CurrentId());
  base::TimeTicks now = TRACE_TIME_TICKS_NOW();
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id, thread_id, now, flags,
      bind_id, arg1_name, std::move(arg1_val));
}

template<class ARG1_TYPE, class ARG2_TYPE>
static inline base::trace_event::TraceEventHandle
AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    int thread_id,
    const base::TimeTicks& timestamp,
    unsigned int flags,
    unsigned long long bind_id,
    const char* arg1_name,
    const ARG1_TYPE& arg1_val,
    const char* arg2_name,
    const ARG2_TYPE& arg2_val) {
  const int num_args = 2;
  const char* arg_names[2] = { arg1_name, arg2_name };
  unsigned char arg_types[2];
  unsigned long long arg_values[2];
  SetTraceValue(arg1_val, &arg_types[0], &arg_values[0]);
  SetTraceValue(arg2_val, &arg_types[1], &arg_values[1]);
  return TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
      phase, category_group_enabled, name, scope, id, bind_id, thread_id,
      timestamp, num_args, arg_names, arg_types, arg_values, NULL, flags);
}

template <class ARG1_CONVERTABLE_TYPE, class ARG2_TYPE>
static inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned int flags,
    unsigned long long bind_id,
    const char* arg1_name,
    std::unique_ptr<ARG1_CONVERTABLE_TYPE> arg1_val,
    const char* arg2_name,
    const ARG2_TYPE& arg2_val) {
  int thread_id = static_cast<int>(base::PlatformThread::CurrentId());
  base::TimeTicks now = TRACE_TIME_TICKS_NOW();
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id, thread_id, now, flags,
      bind_id, arg1_name, std::move(arg1_val), arg2_name, arg2_val);
}

template <class ARG1_TYPE, class ARG2_CONVERTABLE_TYPE>
static inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned int flags,
    unsigned long long bind_id,
    const char* arg1_name,
    const ARG1_TYPE& arg1_val,
    const char* arg2_name,
    std::unique_ptr<ARG2_CONVERTABLE_TYPE> arg2_val) {
  int thread_id = static_cast<int>(base::PlatformThread::CurrentId());
  base::TimeTicks now = TRACE_TIME_TICKS_NOW();
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id, thread_id, now, flags,
      bind_id, arg1_name, arg1_val, arg2_name, std::move(arg2_val));
}

template <class ARG1_CONVERTABLE_TYPE, class ARG2_CONVERTABLE_TYPE>
static inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned int flags,
    unsigned long long bind_id,
    const char* arg1_name,
    std::unique_ptr<ARG1_CONVERTABLE_TYPE> arg1_val,
    const char* arg2_name,
    std::unique_ptr<ARG2_CONVERTABLE_TYPE> arg2_val) {
  int thread_id = static_cast<int>(base::PlatformThread::CurrentId());
  base::TimeTicks now = TRACE_TIME_TICKS_NOW();
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id, thread_id, now, flags,
      bind_id, arg1_name, std::move(arg1_val), arg2_name, std::move(arg2_val));
}

template<class ARG1_TYPE, class ARG2_TYPE>
static inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned int flags,
    unsigned long long bind_id,
    const char* arg1_name,
    const ARG1_TYPE& arg1_val,
    const char* arg2_name,
    const ARG2_TYPE& arg2_val) {
  int thread_id = static_cast<int>(base::PlatformThread::CurrentId());
  base::TimeTicks now = TRACE_TIME_TICKS_NOW();
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id, thread_id, now, flags,
      bind_id, arg1_name, arg1_val, arg2_name, arg2_val);
}

template <class ARG1_CONVERTABLE_TYPE>
static inline void AddMetadataEvent(
    const unsigned char* category_group_enabled,
    const char* event_name,
    const char* arg_name,
    std::unique_ptr<ARG1_CONVERTABLE_TYPE> arg_value) {
  const char* arg_names[1] = {arg_name};
  unsigned char arg_types[1] = {TRACE_VALUE_TYPE_CONVERTABLE};
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
      convertable_values[1] = {std::move(arg_value)};
  base::trace_event::TraceLog::GetInstance()->AddMetadataEvent(
      category_group_enabled, event_name,
      1,  // num_args
      arg_names, arg_types,
      nullptr,  // arg_values
      convertable_values, TRACE_EVENT_FLAG_NONE);
}

template <class ARG1_TYPE>
static void AddMetadataEvent(const unsigned char* category_group_enabled,
                             const char* event_name,
                             const char* arg_name,
                             const ARG1_TYPE& arg_val) {
  const int num_args = 1;
  const char* arg_names[1] = {arg_name};
  unsigned char arg_types[1];
  unsigned long long arg_values[1];
  SetTraceValue(arg_val, &arg_types[0], &arg_values[0]);

  base::trace_event::TraceLog::GetInstance()->AddMetadataEvent(
      category_group_enabled, event_name, num_args, arg_names, arg_types,
      arg_values, nullptr, TRACE_EVENT_FLAG_NONE);
}

// Used by TRACE_EVENTx macros. Do not use directly.
class TRACE_EVENT_API_CLASS_EXPORT ScopedTracer {
 public:
  ScopedTracer() = default;

  ~ScopedTracer() {
    if (category_group_enabled_ && *category_group_enabled_) {
      TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION(category_group_enabled_,
                                                  name_, event_handle_);
    }
  }

  void Initialize(const unsigned char* category_group_enabled,
                  const char* name,
                  base::trace_event::TraceEventHandle event_handle) {
    category_group_enabled_ = category_group_enabled;
    name_ = name;
    event_handle_ = event_handle;
  }

 private:
  // NOTE: Only initialize the first member to reduce generated code size,
  // since there is no point in initializing the other members if Initialize()
  // is never called.
  const unsigned char* category_group_enabled_ = nullptr;
  const char* name_;
  base::trace_event::TraceEventHandle event_handle_;
};

// Used by TRACE_EVENT_BINARY_EFFICIENTx macro. Do not use directly.
class TRACE_EVENT_API_CLASS_EXPORT ScopedTraceBinaryEfficient {
 public:
  ScopedTraceBinaryEfficient(const char* category_group, const char* name);
  ~ScopedTraceBinaryEfficient();

 private:
  const unsigned char* category_group_enabled_;
  const char* name_;
  base::trace_event::TraceEventHandle event_handle_;
};

// This macro generates less code then TRACE_EVENT0 but is also
// slower to execute when tracing is off. It should generally only be
// used with code that is seldom executed or conditionally executed
// when debugging.
// For now the category_group must be "gpu".
#define TRACE_EVENT_BINARY_EFFICIENT0(category_group, name) \
    trace_event_internal::ScopedTraceBinaryEfficient \
        INTERNAL_TRACE_EVENT_UID(scoped_trace)(category_group, name);

}  // namespace trace_event_internal

namespace base {
namespace trace_event {

template<typename IDType> class TraceScopedTrackableObject {
 public:
  TraceScopedTrackableObject(const char* category_group, const char* name,
      IDType id)
    : category_group_(category_group),
      name_(name),
      id_(id) {
    TRACE_EVENT_OBJECT_CREATED_WITH_ID(category_group_, name_, id_);
  }

  template <typename ArgType> void snapshot(ArgType snapshot) {
    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(category_group_, name_, id_, snapshot);
  }

  ~TraceScopedTrackableObject() {
    TRACE_EVENT_OBJECT_DELETED_WITH_ID(category_group_, name_, id_);
  }

 private:
  const char* category_group_;
  const char* name_;
  IDType id_;

  DISALLOW_COPY_AND_ASSIGN(TraceScopedTrackableObject);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_TRACE_EVENT_H_
