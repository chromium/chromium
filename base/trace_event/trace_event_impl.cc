// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_event_impl.h"

#include "base/process/process_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "base/trace_event/traced_value.h"

// Define static storage for trace event categories (see
// PERFETTO_DEFINE_CATEGORIES).
PERFETTO_TRACK_EVENT_STATIC_STORAGE_IN_NAMESPACE_WITH_ATTRS(base, BASE_EXPORT);

namespace perfetto {
namespace legacy {

template <>
perfetto::ThreadTrack ConvertThreadId(const ::base::PlatformThreadId& thread) {
  return perfetto::ThreadTrack::ForThread(thread.raw());
}

}  // namespace legacy

TraceTimestamp
TraceTimestampTraits<::base::TimeTicks>::ConvertTimestampToTraceTimeNs(
    const ::base::TimeTicks& ticks) {
  return {static_cast<uint32_t>(::base::TrackEvent::GetTraceClockId()),
          static_cast<uint64_t>(ticks.since_origin().InNanoseconds())};
}

namespace internal {

void WriteDebugAnnotation(protos::pbzero::DebugAnnotation* annotation,
                          ::base::TimeTicks ticks) {
  annotation->set_int_value(ticks.since_origin().InMilliseconds());
}

void WriteDebugAnnotation(protos::pbzero::DebugAnnotation* annotation,
                          ::base::Time time) {
  annotation->set_int_value(time.since_origin().InMilliseconds());
}

}  // namespace internal
}  // namespace perfetto

namespace base::trace_event {
namespace {
bool g_perfetto_initialized_for_testing = false;
}

bool ConvertableToTraceFormat::AppendToProto(ProtoAppender* appender) const {
  return false;
}

TraceEvent::TraceEvent(PlatformThreadId thread_id,
                       TimeTicks timestamp,
                       char phase,
                       const unsigned char* category_group_enabled,
                       const char* name,
                       unsigned long long id,
                       TraceArguments* args,
                       unsigned int flags)
    : timestamp_(timestamp),
      id_(id),
      category_group_enabled_(category_group_enabled),
      name_(name),
      thread_id_(thread_id),
      flags_(flags),
      phase_(phase) {
  InitArgs(args);
}

TraceEvent::~TraceEvent() = default;

TraceEvent::TraceEvent(TraceEvent&& other) noexcept = default;
TraceEvent& TraceEvent::operator=(TraceEvent&& other) noexcept = default;

void TraceEvent::Reset() {
  // Only reset fields that won't be initialized in Reset(int, ...), or that may
  // hold references to other objects.
  args_.Reset();
  parameter_copy_storage_.Reset();
}

void TraceEvent::InitArgs(TraceArguments* args) {
  if (args) {
    args_ = std::move(*args);
  }
  args_.CopyStringsTo(&parameter_copy_storage_,
                      !!(flags_ & TRACE_EVENT_FLAG_COPY), &name_);
}

void SetPerfettoInitializedForTesting() {
  g_perfetto_initialized_for_testing = true;
}

bool IsPerfettoInitializedForTesting() {
  return g_perfetto_initialized_for_testing;
}

void InitializeInProcessPerfettoBackend() {
  perfetto::TracingInitArgs init_args;
  init_args.backends = perfetto::BackendType::kInProcessBackend;
  init_args.shmem_batch_commits_duration_ms = 1000;
  init_args.shmem_size_hint_kb = 4 * 1024;
  init_args.shmem_direct_patching_enabled = true;
  init_args.disallow_merging_with_system_tracks = true;
  perfetto::Tracing::Initialize(init_args);
  TrackEvent::Register();
}

}  // namespace base::trace_event
