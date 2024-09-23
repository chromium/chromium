// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TRACE_EVENT_H_
#define BASE_TRACE_EVENT_TRACE_EVENT_H_

// This header file defines implementation details of how the trace macros in
// trace_event_common.h collect and store trace events. Anything not
// implementation-specific should go in trace_event_common.h instead of here.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/atomicops.h"
#include "base/base_export.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/trace_event/builtin_categories.h"
#include "base/trace_event/common/trace_event_common.h"  // IWYU pragma: export
#include "base/trace_event/trace_arguments.h"
#include "base/trace_event/trace_category.h"
#include "base/trace_event/trace_log.h"
#include "base/trace_event/traced_value_support.h"
#include "base/tracing_buildflags.h"

// Legacy TRACE_EVENT_API entrypoints. Do not use from new code.

// Add a trace event to the platform tracing system.
// base::trace_event::TraceEventHandle TRACE_EVENT_API_ADD_TRACE_EVENT(
//                    char phase,
//                    const unsigned char* category_group_enabled,
//                    const char* name,
//                    const char* scope,
//                    uint64_t id,
//                    base::trace_event::TraceArguments* args,
//                    unsigned int flags)
#define TRACE_EVENT_API_ADD_TRACE_EVENT trace_event_internal::AddTraceEvent

// Add a trace event to the platform tracing system.
// base::trace_event::TraceEventHandle
// TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_BIND_ID(
//                    char phase,
//                    const unsigned char* category_group_enabled,
//                    const char* name,
//                    const char* scope,
//                    uint64_t id,
//                    uint64_t bind_id,
//                    base::trace_event::TraceArguments* args,
//                    unsigned int flags)
#define TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_BIND_ID \
  trace_event_internal::AddTraceEventWithBindId

// Add a trace event to the platform tracing system overriding the pid.
// The resulting event will have tid = pid == (process_id passed here).
// base::trace_event::TraceEventHandle
// TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_PROCESS_ID(
//                    char phase,
//                    const unsigned char* category_group_enabled,
//                    const char* name,
//                    const char* scope,
//                    uint64_t id,
//                    base::ProcessId process_id,
//                    base::trace_event::TraceArguments* args,
//                    unsigned int flags)
#define TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_PROCESS_ID \
  trace_event_internal::AddTraceEventWithProcessId

// Add a trace event to the platform tracing system.
// base::trace_event::TraceEventHandle
// TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
//                    char phase,
//                    const unsigned char* category_group_enabled,
//                    const char* name,
//                    const char* scope,
//                    uint64_t id,
//                    uint64_t bind_id,
//                    base::PlatformThreadId thread_id,
//                    const TimeTicks& timestamp,
//                    base::trace_event::TraceArguments* args,
//                    unsigned int flags)
#define TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP \
  trace_event_internal::AddTraceEventWithThreadIdAndTimestamp

// Set the duration field of a COMPLETE trace event.
// void TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION(
//     const unsigned char* category_group_enabled,
//     const char* name,
//     base::trace_event::TraceEventHandle id)
#define TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION \
  trace_event_internal::UpdateTraceEventDuration

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
// Acquire/release barriers are important here: crbug.com/1330114#c8.
#define TRACE_EVENT_API_ATOMIC_WORD base::subtle::AtomicWord
#define TRACE_EVENT_API_ATOMIC_LOAD(var) base::subtle::Acquire_Load(&(var))
#define TRACE_EVENT_API_ATOMIC_STORE(var, value) \
  base::subtle::Release_Store(&(var), (value))

// Defines visibility for classes in trace_event.h
#define TRACE_EVENT_API_CLASS_EXPORT BASE_EXPORT

////////////////////////////////////////////////////////////////////////////////

namespace trace_event_internal {

// Specify these values when the corresponding argument of AddTraceEvent is not
// used.
const int kZeroNumArgs = 0;
const std::nullptr_t kGlobalScope = nullptr;
const uint64_t kNoId = 0;

// TraceID encapsulates an ID that can either be an integer or pointer.
class BASE_EXPORT TraceID {
 public:
  // Can be combined with WithScope.
  class LocalId {
   public:
    explicit LocalId(const void* raw_id)
        : raw_id_(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(raw_id))) {}
    explicit LocalId(uint64_t raw_id) : raw_id_(raw_id) {}
    uint64_t raw_id() const { return raw_id_; }

   private:
    uint64_t raw_id_;
  };

  // Can be combined with WithScope.
  class GlobalId {
   public:
    explicit GlobalId(uint64_t raw_id) : raw_id_(raw_id) {}
    uint64_t raw_id() const { return raw_id_; }

   private:
    uint64_t raw_id_;
  };

  class WithScope {
   public:
    WithScope(const char* scope, uint64_t raw_id)
        : scope_(scope), raw_id_(raw_id) {}
    WithScope(const char* scope, LocalId local_id)
        : scope_(scope), raw_id_(local_id.raw_id()) {
      id_flags_ = TRACE_EVENT_FLAG_HAS_LOCAL_ID;
    }
    WithScope(const char* scope, GlobalId global_id)
        : scope_(scope), raw_id_(global_id.raw_id()) {
      id_flags_ = TRACE_EVENT_FLAG_HAS_GLOBAL_ID;
    }
    uint64_t raw_id() const { return raw_id_; }
    const char* scope() const { return scope_; }
    unsigned int id_flags() const { return id_flags_; }

   private:
    const char* scope_ = nullptr;
    uint64_t raw_id_;
    unsigned int id_flags_ = TRACE_EVENT_FLAG_HAS_ID;
  };

  explicit TraceID(const void* raw_id)
      : raw_id_(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(raw_id))) {
    id_flags_ = TRACE_EVENT_FLAG_HAS_LOCAL_ID;
  }
  explicit TraceID(unsigned long long raw_id)
      : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(unsigned long raw_id)
      : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(unsigned int raw_id)
      : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(unsigned short raw_id)
      : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(unsigned char raw_id)
      : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(long long raw_id) : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(long raw_id) : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(int raw_id) : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(short raw_id) : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(signed char raw_id)
      : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(LocalId raw_id) : raw_id_(raw_id.raw_id()) {
    id_flags_ = TRACE_EVENT_FLAG_HAS_LOCAL_ID;
  }
  explicit TraceID(GlobalId raw_id) : raw_id_(raw_id.raw_id()) {
    id_flags_ = TRACE_EVENT_FLAG_HAS_GLOBAL_ID;
  }
  explicit TraceID(WithScope scoped_id)
      : scope_(scoped_id.scope()),
        raw_id_(scoped_id.raw_id()),
        id_flags_(scoped_id.id_flags()) {}

  uint64_t raw_id() const { return raw_id_; }
  const char* scope() const { return scope_; }
  unsigned int id_flags() const { return id_flags_; }

 private:
  const char* scope_ = nullptr;
  uint64_t raw_id_;
  unsigned int id_flags_ = TRACE_EVENT_FLAG_HAS_ID;
};

// These functions all internally call
// base::trace_event::TraceLog::GetInstance() then call the method with the same
// name on it. This is used to reduce the generated machine code at each
// TRACE_EVENTXXX macro call.

base::trace_event::TraceEventHandle BASE_EXPORT
AddTraceEvent(char phase,
              const unsigned char* category_group_enabled,
              const char* name,
              const char* scope,
              uint64_t id,
              base::trace_event::TraceArguments* args,
              unsigned int flags);

base::trace_event::TraceEventHandle BASE_EXPORT
AddTraceEventWithBindId(char phase,
                        const unsigned char* category_group_enabled,
                        const char* name,
                        const char* scope,
                        uint64_t id,
                        uint64_t bind_id,
                        base::trace_event::TraceArguments* args,
                        unsigned int flags);

base::trace_event::TraceEventHandle BASE_EXPORT
AddTraceEventWithProcessId(char phase,
                           const unsigned char* category_group_enabled,
                           const char* name,
                           const char* scope,
                           uint64_t id,
                           base::ProcessId process_id,
                           base::trace_event::TraceArguments* args,
                           unsigned int flags);

base::trace_event::TraceEventHandle BASE_EXPORT
AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    base::PlatformThreadId thread_id,
    const base::TimeTicks& timestamp,
    base::trace_event::TraceArguments* args,
    unsigned int flags);

base::trace_event::TraceEventHandle BASE_EXPORT
AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    uint64_t bind_id,
    base::PlatformThreadId thread_id,
    const base::TimeTicks& timestamp,
    base::trace_event::TraceArguments* args,
    unsigned int flags);

base::trace_event::TraceEventHandle BASE_EXPORT
AddTraceEventWithThreadIdAndTimestamps(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    base::PlatformThreadId thread_id,
    const base::TimeTicks& timestamp,
    const base::ThreadTicks& thread_timestamp,
    unsigned int flags);

void BASE_EXPORT AddMetadataEvent(const unsigned char* category_group_enabled,
                                  const char* name,
                                  base::trace_event::TraceArguments* args,
                                  unsigned int flags);

int BASE_EXPORT GetNumTracesRecorded();

void BASE_EXPORT
UpdateTraceEventDuration(const unsigned char* category_group_enabled,
                         const char* name,
                         base::trace_event::TraceEventHandle handle);

void BASE_EXPORT
UpdateTraceEventDurationExplicit(const unsigned char* category_group_enabled,
                                 const char* name,
                                 base::trace_event::TraceEventHandle handle,
                                 base::PlatformThreadId thread_id,
                                 bool explicit_timestamps,
                                 const base::TimeTicks& now,
                                 const base::ThreadTicks& thread_now);

// These AddTraceEvent and AddTraceEventWithThreadIdAndTimestamp template
// functions are defined here instead of in the macro, because the arg_values
// could be temporary objects, such as `std::string`. In order to store
// pointers to the internal c_str and pass through to the tracing API,
// the arg_values must live throughout these procedures.

template <class ARG1_TYPE>
inline base::trace_event::TraceEventHandle
AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    base::PlatformThreadId thread_id,
    const base::TimeTicks& timestamp,
    unsigned int flags,
    uint64_t bind_id,
    const char* arg1_name,
    ARG1_TYPE&& arg1_val) {
  base::trace_event::TraceArguments args(arg1_name,
                                         std::forward<ARG1_TYPE>(arg1_val));
  return TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
      phase, category_group_enabled, name, scope, id, bind_id, thread_id,
      timestamp, &args, flags);
}

template <class ARG1_TYPE, class ARG2_TYPE>
inline base::trace_event::TraceEventHandle
AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    base::PlatformThreadId thread_id,
    const base::TimeTicks& timestamp,
    unsigned int flags,
    uint64_t bind_id,
    const char* arg1_name,
    ARG1_TYPE&& arg1_val,
    const char* arg2_name,
    ARG2_TYPE&& arg2_val) {
  base::trace_event::TraceArguments args(
      arg1_name, std::forward<ARG1_TYPE>(arg1_val), arg2_name,
      std::forward<ARG2_TYPE>(arg2_val));
  return TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
      phase, category_group_enabled, name, scope, id, bind_id, thread_id,
      timestamp, &args, flags);
}

inline base::trace_event::TraceEventHandle
AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    base::PlatformThreadId thread_id,
    const base::TimeTicks& timestamp,
    unsigned int flags,
    uint64_t bind_id) {
  return TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
      phase, category_group_enabled, name, scope, id, bind_id, thread_id,
      timestamp, nullptr, flags);
}

inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    unsigned int flags,
    uint64_t bind_id) {
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id,
      TRACE_EVENT_API_CURRENT_THREAD_ID, TRACE_TIME_TICKS_NOW(), flags,
      bind_id);
}

template <class ARG1_TYPE>
inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    unsigned int flags,
    uint64_t bind_id,
    const char* arg1_name,
    ARG1_TYPE&& arg1_val) {
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id,
      TRACE_EVENT_API_CURRENT_THREAD_ID, TRACE_TIME_TICKS_NOW(), flags, bind_id,
      arg1_name, std::forward<ARG1_TYPE>(arg1_val));
}

template <class ARG1_TYPE, class ARG2_TYPE>
inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    unsigned int flags,
    uint64_t bind_id,
    const char* arg1_name,
    ARG1_TYPE&& arg1_val,
    const char* arg2_name,
    ARG2_TYPE&& arg2_val) {
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id,
      TRACE_EVENT_API_CURRENT_THREAD_ID, TRACE_TIME_TICKS_NOW(), flags, bind_id,
      arg1_name, std::forward<ARG1_TYPE>(arg1_val), arg2_name,
      std::forward<ARG2_TYPE>(arg2_val));
}

template <class ARG1_TYPE>
void AddMetadataEvent(const unsigned char* category_group_enabled,
                      const char* event_name,
                      const char* arg_name,
                      ARG1_TYPE&& arg_val) {
  base::trace_event::TraceArguments args(arg_name,
                                         std::forward<ARG1_TYPE>(arg_val));
  trace_event_internal::AddMetadataEvent(category_group_enabled, event_name,
                                         &args, TRACE_EVENT_FLAG_NONE);
}

}  // namespace trace_event_internal

namespace base {
namespace trace_event {

template <typename IDType, const char* category>
class TraceScopedTrackableObject {
 public:
  TraceScopedTrackableObject(const char* name, IDType id)
      : name_(name), id_(id) {
    TRACE_EVENT_OBJECT_CREATED_WITH_ID(category, name_, id_);
  }
  TraceScopedTrackableObject(const TraceScopedTrackableObject&) = delete;
  TraceScopedTrackableObject& operator=(const TraceScopedTrackableObject&) =
      delete;

  template <typename ArgType> void snapshot(ArgType snapshot) {
    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(category, name_, id_, snapshot);
  }

  ~TraceScopedTrackableObject() {
    TRACE_EVENT_OBJECT_DELETED_WITH_ID(category, name_, id_);
  }

 private:
  const char* name_;
  IDType id_;
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_TRACE_EVENT_H_
