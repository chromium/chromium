// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_log.h"

#include <cmath>
#include <limits>
#include <memory>
#include <unordered_set>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/leak_annotations.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/process/process_metrics.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/current_thread.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/event_name_filter.h"
#include "base/trace_event/heap_profiler.h"
#include "base/trace_event/heap_profiler_allocation_context_tracker.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/thread_instruction_count.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/tracing/perfetto_platform.h"
#include "third_party/perfetto/include/perfetto/ext/trace_processor/export_json.h"  // nogncheck
#include "third_party/perfetto/include/perfetto/trace_processor/trace_processor_storage.h"  // nogncheck
#include "third_party/perfetto/protos/perfetto/config/interceptor_config.gen.h"  // nogncheck
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.gen.h"  // nogncheck
#include "third_party/perfetto/protos/perfetto/trace/track_event/thread_descriptor.gen.h"  // nogncheck
#endif

#if defined(OS_WIN)
#include "base/trace_event/trace_event_etw_export_win.h"
#endif

#if defined(OS_ANDROID)
#include "base/debug/elf_reader.h"

// The linker assigns the virtual address of the start of current library to
// this symbol.
extern char __executable_start;
#endif

namespace base {
namespace trace_event {

namespace {

// Controls the number of trace events we will buffer in-memory
// before throwing them away.
const size_t kTraceBufferChunkSize = TraceBufferChunk::kTraceBufferChunkSize;

const size_t kTraceEventVectorBigBufferChunks =
    512000000 / kTraceBufferChunkSize;
static_assert(
    kTraceEventVectorBigBufferChunks <= TraceBufferChunk::kMaxChunkIndex,
    "Too many big buffer chunks");
const size_t kTraceEventVectorBufferChunks = 256000 / kTraceBufferChunkSize;
static_assert(
    kTraceEventVectorBufferChunks <= TraceBufferChunk::kMaxChunkIndex,
    "Too many vector buffer chunks");
const size_t kTraceEventRingBufferChunks = kTraceEventVectorBufferChunks / 4;

// ECHO_TO_CONSOLE needs a small buffer to hold the unfinished COMPLETE events.
const size_t kEchoToConsoleTraceEventBufferChunks = 256;

const size_t kTraceEventBufferSizeInBytes = 100 * 1024;
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
const int kThreadFlushTimeoutMs = 3000;
#endif

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
static bool g_perfetto_initialized_by_tracelog;
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

TraceLog* g_trace_log_for_testing = nullptr;

#define MAX_TRACE_EVENT_FILTERS 32

// List of TraceEventFilter objects from the most recent tracing session.
std::vector<std::unique_ptr<TraceEventFilter>>& GetCategoryGroupFilters() {
  static auto* filters = new std::vector<std::unique_ptr<TraceEventFilter>>();
  return *filters;
}

ThreadTicks ThreadNow() {
  return ThreadTicks::IsSupported()
             ? base::subtle::ThreadTicksNowIgnoringOverride()
             : ThreadTicks();
}

ThreadInstructionCount ThreadInstructionNow() {
  return ThreadInstructionCount::IsSupported() ? ThreadInstructionCount::Now()
                                               : ThreadInstructionCount();
}

template <typename T>
void InitializeMetadataEvent(TraceEvent* trace_event,
                             int thread_id,
                             const char* metadata_name,
                             const char* arg_name,
                             const T& value) {
  if (!trace_event)
    return;

  TraceArguments args(arg_name, value);
  base::TimeTicks now = TRACE_TIME_TICKS_NOW();
  ThreadTicks thread_now;
  ThreadInstructionCount thread_instruction_count;
  trace_event->Reset(
      thread_id, now, thread_now, thread_instruction_count,
      TRACE_EVENT_PHASE_METADATA,
      TraceLog::GetInstance()->GetCategoryGroupEnabled("__metadata"),
      metadata_name,
      trace_event_internal::kGlobalScope,  // scope
      trace_event_internal::kNoId,         // id
      trace_event_internal::kNoId,         // bind_id
      &args, TRACE_EVENT_FLAG_NONE);
}

class AutoThreadLocalBoolean {
 public:
  explicit AutoThreadLocalBoolean(ThreadLocalBoolean* thread_local_boolean)
      : thread_local_boolean_(thread_local_boolean) {
    DCHECK(!thread_local_boolean_->Get());
    thread_local_boolean_->Set(true);
  }
  AutoThreadLocalBoolean(const AutoThreadLocalBoolean&) = delete;
  AutoThreadLocalBoolean& operator=(const AutoThreadLocalBoolean&) = delete;
  ~AutoThreadLocalBoolean() { thread_local_boolean_->Set(false); }

 private:
  ThreadLocalBoolean* thread_local_boolean_;
};

// Use this function instead of TraceEventHandle constructor to keep the
// overhead of ScopedTracer (trace_event.h) constructor minimum.
void MakeHandle(uint32_t chunk_seq,
                size_t chunk_index,
                size_t event_index,
                TraceEventHandle* handle) {
  DCHECK(chunk_seq);
  DCHECK(chunk_index <= TraceBufferChunk::kMaxChunkIndex);
  DCHECK(event_index < TraceBufferChunk::kTraceBufferChunkSize);
  DCHECK(chunk_index <= std::numeric_limits<uint16_t>::max());
  handle->chunk_seq = chunk_seq;
  handle->chunk_index = static_cast<uint16_t>(chunk_index);
  handle->event_index = static_cast<uint16_t>(event_index);
}

template <typename Function>
void ForEachCategoryFilter(const unsigned char* category_group_enabled,
                           Function filter_fn) {
  const TraceCategory* category =
      CategoryRegistry::GetCategoryByStatePtr(category_group_enabled);
  uint32_t filter_bitmap = category->enabled_filters();
  for (int index = 0; filter_bitmap != 0; filter_bitmap >>= 1, index++) {
    if (filter_bitmap & 1 && GetCategoryGroupFilters()[index])
      filter_fn(GetCategoryGroupFilters()[index].get());
  }
}

// The fallback arguments filtering function will filter away every argument.
bool DefaultIsTraceEventArgsAllowlisted(
    const char* category_group_name,
    const char* event_name,
    base::trace_event::ArgumentNameFilterPredicate* arg_name_filter) {
  return false;
}

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
class PerfettoProtoAppender
    : public base::trace_event::ConvertableToTraceFormat::ProtoAppender {
 public:
  explicit PerfettoProtoAppender(
      perfetto::protos::pbzero::DebugAnnotation* proto)
      : annotation_proto_(proto) {}
  ~PerfettoProtoAppender() override = default;

  // ProtoAppender implementation
  void AddBuffer(uint8_t* begin, uint8_t* end) override {
    ranges_.emplace_back();
    ranges_.back().begin = begin;
    ranges_.back().end = end;
  }

  size_t Finalize(uint32_t field_id) override {
    return annotation_proto_->AppendScatteredBytes(field_id, ranges_.data(),
                                                   ranges_.size());
  }

 private:
  std::vector<protozero::ContiguousMemoryRange> ranges_;
  perfetto::protos::pbzero::DebugAnnotation* annotation_proto_;
};

void AddConvertableToTraceFormat(
    base::trace_event::ConvertableToTraceFormat* value,
    perfetto::protos::pbzero::DebugAnnotation* annotation) {
  PerfettoProtoAppender proto_appender(annotation);
  if (value->AppendToProto(&proto_appender)) {
    return;
  }

  std::string json;
  value->AppendAsTraceFormat(&json);
  annotation->set_legacy_json_value(json.c_str());
}

void WriteDebugAnnotations(base::trace_event::TraceEvent* trace_event,
                           perfetto::protos::pbzero::TrackEvent* track_event) {
  for (size_t i = 0; i < trace_event->arg_size() && trace_event->arg_name(i);
       ++i) {
    auto type = trace_event->arg_type(i);
    auto* annotation = track_event->add_debug_annotations();

    annotation->set_name(trace_event->arg_name(i));

    if (type == TRACE_VALUE_TYPE_CONVERTABLE) {
      AddConvertableToTraceFormat(trace_event->arg_convertible_value(i),
                                  annotation);
      continue;
    }

    auto& value = trace_event->arg_value(i);
    switch (type) {
      case TRACE_VALUE_TYPE_BOOL:
        annotation->set_bool_value(value.as_bool);
        break;
      case TRACE_VALUE_TYPE_UINT:
        annotation->set_uint_value(value.as_uint);
        break;
      case TRACE_VALUE_TYPE_INT:
        annotation->set_int_value(value.as_int);
        break;
      case TRACE_VALUE_TYPE_DOUBLE:
        annotation->set_double_value(value.as_double);
        break;
      case TRACE_VALUE_TYPE_POINTER:
        annotation->set_pointer_value(static_cast<uint64_t>(
            reinterpret_cast<uintptr_t>(value.as_pointer)));
        break;
      case TRACE_VALUE_TYPE_STRING:
      case TRACE_VALUE_TYPE_COPY_STRING:
        annotation->set_string_value(value.as_string ? value.as_string
                                                     : "NULL");
        break;
      case TRACE_VALUE_TYPE_PROTO: {
        auto data = value.as_proto->SerializeAsArray();
        annotation->AppendRawProtoBytes(data.data(), data.size());
      } break;

      default:
        NOTREACHED() << "Don't know how to serialize this value";
        break;
    }
  }
}

void OnAddLegacyTraceEvent(TraceEvent* trace_event,
                           bool thread_will_flush,
                           base::trace_event::TraceEventHandle* handle) {
  perfetto::DynamicCategory category(
      TraceLog::GetInstance()->GetCategoryGroupName(
          trace_event->category_group_enabled()));
  auto write_args = [trace_event](perfetto::EventContext ctx) {
    WriteDebugAnnotations(trace_event, ctx.event());
    uint32_t id_flags = trace_event->flags() & (TRACE_EVENT_FLAG_HAS_ID |
                                                TRACE_EVENT_FLAG_HAS_LOCAL_ID |
                                                TRACE_EVENT_FLAG_HAS_GLOBAL_ID);
    if (!id_flags &&
        perfetto::internal::TrackEventLegacy::PhaseToType(
            trace_event->phase()) !=
            perfetto::protos::pbzero::TrackEvent::TYPE_UNSPECIFIED) {
      return;
    }
    auto* legacy_event = ctx.event()->set_legacy_event();
    legacy_event->set_phase(trace_event->phase());
    switch (id_flags) {
      case TRACE_EVENT_FLAG_HAS_ID:
        legacy_event->set_unscoped_id(trace_event->id());
        break;
      case TRACE_EVENT_FLAG_HAS_LOCAL_ID:
        legacy_event->set_local_id(trace_event->id());
        break;
      case TRACE_EVENT_FLAG_HAS_GLOBAL_ID:
        legacy_event->set_global_id(trace_event->id());
        break;
      default:
        break;
    }
  };

  auto phase = trace_event->phase();
  auto flags = trace_event->flags();
  base::TimeTicks timestamp = trace_event->timestamp().is_null()
                                  ? TRACE_TIME_TICKS_NOW()
                                  : trace_event->timestamp();
  if (phase == TRACE_EVENT_PHASE_COMPLETE) {
    phase = TRACE_EVENT_PHASE_BEGIN;
  } else if (phase == TRACE_EVENT_PHASE_INSTANT) {
    auto scope = flags & TRACE_EVENT_FLAG_SCOPE_MASK;
    switch (scope) {
      case TRACE_EVENT_SCOPE_GLOBAL:
        PERFETTO_INTERNAL_LEGACY_EVENT_ON_TRACK(
            phase, category, trace_event->name(), ::perfetto::Track::Global(0),
            timestamp, write_args);
        return;
      case TRACE_EVENT_SCOPE_PROCESS:
        PERFETTO_INTERNAL_LEGACY_EVENT_ON_TRACK(
            phase, category, trace_event->name(),
            ::perfetto::ProcessTrack::Current(), timestamp, write_args);
        return;
      default:
      case TRACE_EVENT_SCOPE_THREAD: /* Fallthrough. */
        break;
    }
  }
  if (trace_event->thread_id() &&
      trace_event->thread_id() != base::PlatformThread::CurrentId()) {
    PERFETTO_INTERNAL_LEGACY_EVENT_ON_TRACK(
        phase, category, trace_event->name(),
        perfetto::ThreadTrack::ForThread(trace_event->thread_id()), timestamp,
        write_args);
    return;
  }
  PERFETTO_INTERNAL_LEGACY_EVENT_ON_TRACK(
      phase, category, trace_event->name(),
      perfetto::internal::TrackEventInternal::kDefaultTrack, timestamp,
      write_args);
}

void OnUpdateLegacyTraceEventDuration(
    const unsigned char* category_group_enabled,
    const char* name,
    TraceEventHandle handle,
    int thread_id,
    bool explicit_timestamps,
    const TimeTicks& now,
    const ThreadTicks& thread_now,
    ThreadInstructionCount thread_instruction_now) {
  perfetto::DynamicCategory category(
      TraceLog::GetInstance()->GetCategoryGroupName(category_group_enabled));
  auto phase = TRACE_EVENT_PHASE_END;
  base::TimeTicks timestamp =
      explicit_timestamps ? now : TRACE_TIME_TICKS_NOW();
  if (thread_id && thread_id != base::PlatformThread::CurrentId()) {
    PERFETTO_INTERNAL_LEGACY_EVENT_ON_TRACK(
        phase, category, name, perfetto::ThreadTrack::ForThread(thread_id),
        timestamp);
    return;
  }
  PERFETTO_INTERNAL_LEGACY_EVENT_ON_TRACK(
      phase, category, name,
      perfetto::internal::TrackEventInternal::kDefaultTrack, timestamp);
}
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

}  // namespace

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY) && !defined(OS_NACL)
namespace {
// Perfetto provides us with a fully formed JSON trace file, while
// TraceResultBuffer wants individual JSON fragments without a containing
// object. We therefore need to strip away the outer object, including the
// metadata fields, from the JSON stream.
static constexpr char kJsonPrefix[] = "{\"traceEvents\":[\n";
static constexpr char kJsonJoiner[] = ",\n";
static constexpr char kJsonSuffix[] = "],\"metadata\":";
}  // namespace

class JsonStringOutputWriter
    : public perfetto::trace_processor::json::OutputWriter {
 public:
  JsonStringOutputWriter(scoped_refptr<SequencedTaskRunner> flush_task_runner,
                         TraceLog::OutputCallback flush_callback)
      : flush_task_runner_(flush_task_runner),
        flush_callback_(std::move(flush_callback)) {
    buffer_->data().reserve(kBufferReserveCapacity);
  }

  ~JsonStringOutputWriter() override { Flush(/*has_more=*/false); }

  perfetto::trace_processor::util::Status AppendString(
      const std::string& string) override {
    if (!did_strip_prefix_) {
      DCHECK_EQ(string, kJsonPrefix);
      did_strip_prefix_ = true;
      return perfetto::trace_processor::util::OkStatus();
    } else if (buffer_->data().empty() &&
               !strncmp(string.c_str(), kJsonJoiner, strlen(kJsonJoiner))) {
      // We only remove the leading joiner comma for the first chunk in a buffer
      // since the consumer is expected to insert commas between the buffers we
      // provide.
      buffer_->data() += string.substr(strlen(kJsonJoiner));
    } else if (!strncmp(string.c_str(), kJsonSuffix, strlen(kJsonSuffix))) {
      return perfetto::trace_processor::util::OkStatus();
    } else {
      buffer_->data() += string;
    }
    if (buffer_->data().size() > kBufferLimitInBytes) {
      Flush(/*has_more=*/true);
      // Reset the buffer_ after moving it above.
      buffer_ = new RefCountedString();
      buffer_->data().reserve(kBufferReserveCapacity);
    }
    return perfetto::trace_processor::util::OkStatus();
  }

 private:
  void Flush(bool has_more) {
    if (flush_task_runner_) {
      flush_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(flush_callback_, std::move(buffer_), has_more));
    } else {
      flush_callback_.Run(std::move(buffer_), has_more);
    }
  }

  static constexpr size_t kBufferLimitInBytes = 100 * 1024;
  // Since we write each string before checking the limit, we'll always go
  // slightly over and hence we reserve some extra space to avoid most
  // reallocs.
  static constexpr size_t kBufferReserveCapacity = kBufferLimitInBytes * 5 / 4;

  scoped_refptr<SequencedTaskRunner> flush_task_runner_;
  TraceLog::OutputCallback flush_callback_;
  scoped_refptr<RefCountedString> buffer_ = new RefCountedString();
  bool did_strip_prefix_ = false;
};
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY) && !defined(OS_NACL)

// A helper class that allows the lock to be acquired in the middle of the scope
// and unlocks at the end of scope if locked.
class TraceLog::OptionalAutoLock {
 public:
  explicit OptionalAutoLock(Lock* lock) : lock_(lock) {}

  OptionalAutoLock(const OptionalAutoLock&) = delete;
  OptionalAutoLock& operator=(const OptionalAutoLock&) = delete;

  ~OptionalAutoLock() {
    if (locked_)
      lock_->Release();
  }

  void EnsureAcquired() EXCLUSIVE_LOCK_FUNCTION(lock_) {
    if (!locked_) {
      lock_->Acquire();
      locked_ = true;
    } else {
      lock_->AssertAcquired();
    }
  }

 private:
  Lock* lock_;
  bool locked_ = false;
};

class TraceLog::ThreadLocalEventBuffer
    : public CurrentThread::DestructionObserver,
      public MemoryDumpProvider {
 public:
  explicit ThreadLocalEventBuffer(TraceLog* trace_log);
  ThreadLocalEventBuffer(const ThreadLocalEventBuffer&) = delete;
  ThreadLocalEventBuffer& operator=(const ThreadLocalEventBuffer&) = delete;
  ~ThreadLocalEventBuffer() override;

  TraceEvent* AddTraceEvent(TraceEventHandle* handle);

  TraceEvent* GetEventByHandle(TraceEventHandle handle) {
    if (!chunk_ || handle.chunk_seq != chunk_->seq() ||
        handle.chunk_index != chunk_index_) {
      return nullptr;
    }

    return chunk_->GetEventAt(handle.event_index);
  }

  int generation() const { return generation_; }

 private:
  // CurrentThread::DestructionObserver
  void WillDestroyCurrentMessageLoop() override;

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(const MemoryDumpArgs& args,
                    ProcessMemoryDump* pmd) override;

  void FlushWhileLocked();

  void CheckThisIsCurrentBuffer() const {
    DCHECK(trace_log_->thread_local_event_buffer_.Get() == this);
  }

  // Since TraceLog is a leaky singleton, trace_log_ will always be valid
  // as long as the thread exists.
  TraceLog* trace_log_;
  std::unique_ptr<TraceBufferChunk> chunk_;
  size_t chunk_index_ = 0;
  int generation_;
};

TraceLog::ThreadLocalEventBuffer::ThreadLocalEventBuffer(TraceLog* trace_log)
    : trace_log_(trace_log),
      generation_(trace_log->generation()) {
  // ThreadLocalEventBuffer is created only if the thread has a message loop, so
  // the following message_loop won't be NULL.
  CurrentThread::Get()->AddDestructionObserver(this);

  // This is to report the local memory usage when memory-infra is enabled.
  MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "ThreadLocalEventBuffer", ThreadTaskRunnerHandle::Get());

  int thread_id = static_cast<int>(PlatformThread::CurrentId());

  AutoLock lock(trace_log->lock_);
  trace_log->thread_task_runners_[thread_id] = ThreadTaskRunnerHandle::Get();
}

TraceLog::ThreadLocalEventBuffer::~ThreadLocalEventBuffer() {
  CheckThisIsCurrentBuffer();
  CurrentThread::Get()->RemoveDestructionObserver(this);
  MemoryDumpManager::GetInstance()->UnregisterDumpProvider(this);

  {
    AutoLock lock(trace_log_->lock_);
    FlushWhileLocked();

    int thread_id = static_cast<int>(PlatformThread::CurrentId());
    trace_log_->thread_task_runners_.erase(thread_id);
  }
  trace_log_->thread_local_event_buffer_.Set(nullptr);
}

TraceEvent* TraceLog::ThreadLocalEventBuffer::AddTraceEvent(
    TraceEventHandle* handle) {
  CheckThisIsCurrentBuffer();

  if (chunk_ && chunk_->IsFull()) {
    AutoLock lock(trace_log_->lock_);
    FlushWhileLocked();
    chunk_.reset();
  }
  if (!chunk_) {
    AutoLock lock(trace_log_->lock_);
    chunk_ = trace_log_->logged_events_->GetChunk(&chunk_index_);
    trace_log_->CheckIfBufferIsFullWhileLocked();
  }
  if (!chunk_)
    return nullptr;

  size_t event_index;
  TraceEvent* trace_event = chunk_->AddTraceEvent(&event_index);
  if (trace_event && handle)
    MakeHandle(chunk_->seq(), chunk_index_, event_index, handle);

  return trace_event;
}

void TraceLog::ThreadLocalEventBuffer::WillDestroyCurrentMessageLoop() {
  delete this;
}

bool TraceLog::ThreadLocalEventBuffer::OnMemoryDump(const MemoryDumpArgs& args,
                                                    ProcessMemoryDump* pmd) {
  if (!chunk_)
    return true;
  std::string dump_base_name = StringPrintf(
      "tracing/thread_%d", static_cast<int>(PlatformThread::CurrentId()));
  TraceEventMemoryOverhead overhead;
  chunk_->EstimateTraceMemoryOverhead(&overhead);
  overhead.DumpInto(dump_base_name.c_str(), pmd);
  return true;
}

void TraceLog::ThreadLocalEventBuffer::FlushWhileLocked() {
  if (!chunk_)
    return;

  trace_log_->lock_.AssertAcquired();
  if (trace_log_->CheckGeneration(generation_)) {
    // Return the chunk to the buffer only if the generation matches.
    trace_log_->logged_events_->ReturnChunk(chunk_index_, std::move(chunk_));
  }
  // Otherwise this method may be called from the destructor, or TraceLog will
  // find the generation mismatch and delete this buffer soon.
}

void TraceLog::SetAddTraceEventOverrides(
    const AddTraceEventOverrideFunction& add_event_override,
    const OnFlushFunction& on_flush_override,
    const UpdateDurationFunction& update_duration_override) {
  add_trace_event_override_.store(add_event_override);
  on_flush_override_.store(on_flush_override);
  update_duration_override_.store(update_duration_override);
}

struct TraceLog::RegisteredAsyncObserver {
  explicit RegisteredAsyncObserver(WeakPtr<AsyncEnabledStateObserver> observer)
      : observer(observer), task_runner(ThreadTaskRunnerHandle::Get()) {}
  ~RegisteredAsyncObserver() = default;

  WeakPtr<AsyncEnabledStateObserver> observer;
  scoped_refptr<SequencedTaskRunner> task_runner;
};

TraceLogStatus::TraceLogStatus() : event_capacity(0), event_count(0) {}

TraceLogStatus::~TraceLogStatus() = default;

// static
TraceLog* TraceLog::GetInstance() {
  static base::NoDestructor<TraceLog> instance(0);
  return instance.get();
}

// static
void TraceLog::ResetForTesting() {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  auto* self = GetInstance();
  AutoLock lock(self->observers_lock_);
  self->enabled_state_observers_.clear();
  self->owned_enabled_state_observer_copy_.clear();
  self->async_observers_.clear();
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  if (!g_trace_log_for_testing)
    return;
  {
    AutoLock lock(g_trace_log_for_testing->lock_);
    CategoryRegistry::ResetForTesting();
  }
  // Don't reset the generation value back to 0. TraceLog is normally
  // supposed to be a singleton and the value of generation is never
  // supposed to decrease.
  const int generation = g_trace_log_for_testing->generation() + 1;
  g_trace_log_for_testing->~TraceLog();
  new (g_trace_log_for_testing) TraceLog(generation);
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
}

TraceLog::TraceLog(int generation)
    : enabled_modes_(0),
      num_traces_recorded_(0),
      process_sort_index_(0),
      process_id_hash_(0),
      process_id_(base::kNullProcessId),
      trace_options_(kInternalRecordUntilFull),
      trace_config_(TraceConfig()),
      thread_shared_chunk_index_(0),
      generation_(generation),
      use_worker_thread_(false) {
  CategoryRegistry::Initialize();

#if defined(OS_NACL)  // NaCl shouldn't expose the process id.
  SetProcessID(0);
#else
  SetProcessID(static_cast<int>(GetCurrentProcId()));
#endif

  logged_events_.reset(CreateTraceBuffer());

  MemoryDumpManager::GetInstance()->RegisterDumpProvider(this, "TraceLog",
                                                         nullptr);
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  perfetto::TrackEvent::AddSessionObserver(this);
  // When using the Perfetto client library, TRACE_EVENT macros will bypass
  // TraceLog entirely. However, trace event embedders which haven't been ported
  // to Perfetto yet will still be using TRACE_EVENT_API_ADD_TRACE_EVENT, so we
  // need to route these events to Perfetto using an override here. This
  // override is also used to capture internal metadata events.
  SetAddTraceEventOverrides(&OnAddLegacyTraceEvent, nullptr,
                            &OnUpdateLegacyTraceEventDuration);
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  g_trace_log_for_testing = this;
}

TraceLog::~TraceLog() {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  perfetto::TrackEvent::RemoveSessionObserver(this);
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
}

void TraceLog::InitializeThreadLocalEventBufferIfSupported() {
  // A ThreadLocalEventBuffer needs the message loop with a task runner
  // - to know when the thread exits;
  // - to handle the final flush.
  // For a thread without a message loop or if the message loop may be blocked,
  // the trace events will be added into the main buffer directly.
  if (thread_blocks_message_loop_.Get() || !CurrentThread::IsSet() ||
      !ThreadTaskRunnerHandle::IsSet()) {
    return;
  }
  HEAP_PROFILER_SCOPED_IGNORE;
  auto* thread_local_event_buffer = thread_local_event_buffer_.Get();
  if (thread_local_event_buffer &&
      !CheckGeneration(thread_local_event_buffer->generation())) {
    delete thread_local_event_buffer;
    thread_local_event_buffer = nullptr;
  }
  if (!thread_local_event_buffer) {
    thread_local_event_buffer = new ThreadLocalEventBuffer(this);
    thread_local_event_buffer_.Set(thread_local_event_buffer);
  }
}

bool TraceLog::OnMemoryDump(const MemoryDumpArgs& args,
                            ProcessMemoryDump* pmd) {
  // TODO(ssid): Use MemoryDumpArgs to create light dumps when requested
  // (crbug.com/499731).
  TraceEventMemoryOverhead overhead;
  overhead.Add(TraceEventMemoryOverhead::kOther, sizeof(*this));
  {
    AutoLock lock(lock_);
    if (logged_events_)
      logged_events_->EstimateTraceMemoryOverhead(&overhead);

    for (auto& metadata_event : metadata_events_)
      metadata_event->EstimateTraceMemoryOverhead(&overhead);
  }
  overhead.AddSelf();
  overhead.DumpInto("tracing/main_trace_log", pmd);
  return true;
}

const unsigned char* TraceLog::GetCategoryGroupEnabled(
    const char* category_group) {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  return TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(category_group);
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  TraceLog* tracelog = GetInstance();
  if (!tracelog) {
    DCHECK(!CategoryRegistry::kCategoryAlreadyShutdown->is_enabled());
    return CategoryRegistry::kCategoryAlreadyShutdown->state_ptr();
  }
  TraceCategory* category = CategoryRegistry::GetCategoryByName(category_group);
  if (!category) {
    // Slow path: in the case of a new category we have to repeat the check
    // holding the lock, as multiple threads might have reached this point
    // at the same time.
    auto category_initializer = [](TraceCategory* category) {
      TraceLog::GetInstance()->UpdateCategoryState(category);
    };
    AutoLock lock(tracelog->lock_);
    CategoryRegistry::GetOrCreateCategoryLocked(
        category_group, category_initializer, &category);
  }
  DCHECK(category->state_ptr());
  return category->state_ptr();
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
}

const char* TraceLog::GetCategoryGroupName(
    const unsigned char* category_group_enabled) {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  return TRACE_EVENT_API_GET_CATEGORY_GROUP_NAME(category_group_enabled);
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  return CategoryRegistry::GetCategoryByStatePtr(category_group_enabled)
      ->name();
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
}

void TraceLog::UpdateCategoryState(TraceCategory* category) {
  lock_.AssertAcquired();
  DCHECK(category->is_valid());
  unsigned char state_flags = 0;
  if (enabled_modes_ & RECORDING_MODE &&
      trace_config_.IsCategoryGroupEnabled(category->name())) {
    state_flags |= TraceCategory::ENABLED_FOR_RECORDING;
  }

  // TODO(primiano): this is a temporary workaround for catapult:#2341,
  // to guarantee that metadata events are always added even if the category
  // filter is "-*". See crbug.com/618054 for more details and long-term fix.
  if (enabled_modes_ & RECORDING_MODE &&
      category == CategoryRegistry::kCategoryMetadata) {
    state_flags |= TraceCategory::ENABLED_FOR_RECORDING;
  }

#if defined(OS_WIN)
  if (base::trace_event::TraceEventETWExport::IsCategoryGroupEnabled(
          category->name())) {
    state_flags |= TraceCategory::ENABLED_FOR_ETW_EXPORT;
  }
#endif

  uint32_t enabled_filters_bitmap = 0;
  int index = 0;
  for (const auto& event_filter : enabled_event_filters_) {
    if (event_filter.IsCategoryGroupEnabled(category->name())) {
      state_flags |= TraceCategory::ENABLED_FOR_FILTERING;
      DCHECK(GetCategoryGroupFilters()[index]);
      enabled_filters_bitmap |= 1 << index;
    }
    if (index++ >= MAX_TRACE_EVENT_FILTERS) {
      NOTREACHED();
      break;
    }
  }
  category->set_enabled_filters(enabled_filters_bitmap);
  category->set_state(state_flags);
}

void TraceLog::UpdateCategoryRegistry() {
  lock_.AssertAcquired();
  CreateFiltersForTraceConfig();
  for (TraceCategory& category : CategoryRegistry::GetAllCategories()) {
    UpdateCategoryState(&category);
  }
}

void TraceLog::CreateFiltersForTraceConfig() {
  if (!(enabled_modes_ & FILTERING_MODE))
    return;

  // Filters were already added and tracing could be enabled. Filters list
  // cannot be changed when trace events are using them.
  if (GetCategoryGroupFilters().size())
    return;

  for (auto& filter_config : enabled_event_filters_) {
    if (GetCategoryGroupFilters().size() >= MAX_TRACE_EVENT_FILTERS) {
      NOTREACHED()
          << "Too many trace event filters installed in the current session";
      break;
    }

    std::unique_ptr<TraceEventFilter> new_filter;
    const std::string& predicate_name = filter_config.predicate_name();
    if (predicate_name == EventNameFilter::kName) {
      auto whitelist = std::make_unique<std::unordered_set<std::string>>();
      CHECK(filter_config.GetArgAsSet("event_name_allowlist", &*whitelist));
      new_filter = std::make_unique<EventNameFilter>(std::move(whitelist));
    } else {
      if (filter_factory_for_testing_)
        new_filter = filter_factory_for_testing_(predicate_name);
      CHECK(new_filter) << "Unknown trace filter " << predicate_name;
    }
    GetCategoryGroupFilters().push_back(std::move(new_filter));
  }
}

void TraceLog::SetEnabled(const TraceConfig& trace_config,
                          uint8_t modes_to_enable) {
  DCHECK(trace_config.process_filter_config().IsEnabled(process_id_));

  AutoLock lock(lock_);

  // Perfetto only supports basic wildcard filtering, so check that we're not
  // trying to use more complex filters.
  for (const auto& excluded :
       trace_config.category_filter().excluded_categories()) {
    DCHECK(excluded.find("?") == std::string::npos);
    DCHECK(excluded.find("*") == std::string::npos ||
           excluded.find("*") == excluded.size() - 1);
  }
  for (const auto& included :
       trace_config.category_filter().included_categories()) {
    DCHECK(included.find("?") == std::string::npos);
    DCHECK(included.find("*") == std::string::npos ||
           included.find("*") == included.size() - 1);
  }
  for (const auto& disabled :
       trace_config.category_filter().disabled_categories()) {
    DCHECK(disabled.find("?") == std::string::npos);
    DCHECK(disabled.find("*") == std::string::npos ||
           disabled.find("*") == disabled.size() - 1);
  }

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  DCHECK(modes_to_enable == RECORDING_MODE);
  DCHECK(!trace_config.IsArgumentFilterEnabled());
  DCHECK(!perfetto::TrackEvent::IsEnabled());

  // When we're using the Perfetto client library, only tests should be
  // recording traces directly through TraceLog. Production code should instead
  // use perfetto::Tracing::NewTrace(). Let's make sure the tracing service
  // didn't already initialize Perfetto in this process, because it's not safe
  // to consume trace data from arbitrary processes through TraceLog as the JSON
  // conversion here isn't sandboxed like with the real tracing service.
  //
  // Note that initializing Perfetto here requires the thread pool to be ready.
  CHECK(!perfetto::Tracing::IsInitialized() ||
        g_perfetto_initialized_by_tracelog)
      << "Don't use TraceLog for recording traces from non-test code. Use "
         "perfetto::Tracing::NewTrace() instead.";

  if (!perfetto::Tracing::IsInitialized()) {
    g_perfetto_initialized_by_tracelog = true;
    auto* perfetto_platform = GetOrCreatePerfettoPlatform();
    perfetto::TracingInitArgs init_args;
    init_args.backends = perfetto::BackendType::kInProcessBackend;
    init_args.platform = perfetto_platform;
    perfetto::Tracing::Initialize(init_args);
    perfetto::TrackEvent::Register();
  }

  perfetto::TraceConfig perfetto_config;
  size_t size_limit = trace_config.GetTraceBufferSizeInKb();
  if (size_limit == 0)
    size_limit = 200 * 1024;
  auto* buffer_config = perfetto_config.add_buffers();
  buffer_config->set_size_kb(size_limit);
  switch (trace_config.GetTraceRecordMode()) {
    case base::trace_event::RECORD_UNTIL_FULL:
    case base::trace_event::RECORD_AS_MUCH_AS_POSSIBLE:
      buffer_config->set_fill_policy(
          perfetto::TraceConfig::BufferConfig::DISCARD);
      break;
    case base::trace_event::RECORD_CONTINUOUSLY:
      buffer_config->set_fill_policy(
          perfetto::TraceConfig::BufferConfig::RING_BUFFER);
      break;
    case base::trace_event::ECHO_TO_CONSOLE:
      // Handled below.
      break;
  }

  // Add the track event data source.
  // TODO(skyostil): Configure kTraceClockId as the primary trace clock.
  auto* data_source = perfetto_config.add_data_sources();
  auto* source_config = data_source->mutable_config();
  source_config->set_name("track_event");
  source_config->set_target_buffer(0);

  if (trace_config.GetTraceRecordMode() == base::trace_event::ECHO_TO_CONSOLE) {
    perfetto::ConsoleInterceptor::Register();
    source_config->mutable_interceptor_config()->set_name("console");
  }

  // Translate the category filter into included and excluded categories.
  perfetto::protos::gen::TrackEventConfig te_cfg;
  // If no categories are explicitly enabled, enable the default ones. Otherwise
  // only matching categories are enabled.
  if (!trace_config.category_filter().included_categories().empty())
    te_cfg.add_disabled_categories("*");
  // Metadata is always enabled.
  te_cfg.add_enabled_categories("__metadata");
  for (const auto& excluded :
       trace_config.category_filter().excluded_categories()) {
    te_cfg.add_disabled_categories(excluded);
  }
  for (const auto& included :
       trace_config.category_filter().included_categories()) {
    te_cfg.add_enabled_categories(included);
  }
  for (const auto& disabled :
       trace_config.category_filter().disabled_categories()) {
    te_cfg.add_enabled_categories(disabled);
  }
  source_config->set_track_event_config_raw(te_cfg.SerializeAsString());

  // Clear incremental state every 5 seconds, so that we lose at most the first
  // 5 seconds of the trace (if we wrap around Perfetto's central buffer).
  perfetto_config.mutable_incremental_state_config()->set_clear_period_ms(5000);

  tracing_session_ = perfetto::Tracing::NewTrace();
  tracing_session_->Setup(perfetto_config);
  tracing_session_->StartBlocking();
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  // Can't enable tracing when Flush() is in progress.
  DCHECK(!flush_task_runner_);

  InternalTraceOptions new_options =
      GetInternalOptionsFromTraceConfig(trace_config);

  InternalTraceOptions old_options = trace_options();

  if (dispatching_to_observers_) {
    // TODO(ssid): Change to NOTREACHED after fixing crbug.com/625170.
    DLOG(ERROR)
        << "Cannot manipulate TraceLog::Enabled state from an observer.";
    return;
  }

  // Clear all filters from previous tracing session. These filters are not
  // cleared at the end of tracing because some threads which hit trace event
  // when disabling, could try to use the filters.
  if (!enabled_modes_)
    GetCategoryGroupFilters().clear();

  // Update trace config for recording.
  const bool already_recording = enabled_modes_ & RECORDING_MODE;
  if (modes_to_enable & RECORDING_MODE) {
    if (already_recording) {
      trace_config_.Merge(trace_config);
    } else {
      trace_config_ = trace_config;
    }
  }

  // Update event filters only if filtering was not enabled.
  if (modes_to_enable & FILTERING_MODE && enabled_event_filters_.empty()) {
    DCHECK(!trace_config.event_filters().empty());
    enabled_event_filters_ = trace_config.event_filters();
  }
  // Keep the |trace_config_| updated with only enabled filters in case anyone
  // tries to read it using |GetCurrentTraceConfig| (even if filters are
  // empty).
  trace_config_.SetEventFilters(enabled_event_filters_);

  enabled_modes_ |= modes_to_enable;
  UpdateCategoryRegistry();

  // Do not notify observers or create trace buffer if only enabled for
  // filtering or if recording was already enabled.
  if (!(modes_to_enable & RECORDING_MODE) || already_recording)
    return;

  // Discard events if new trace options are different. Reducing trace buffer
  // size is not supported while already recording, so only replace trace
  // buffer if we were not already recording.
  if (new_options != old_options ||
      (trace_config_.GetTraceBufferSizeInEvents() && !already_recording)) {
    trace_options_.store(new_options, std::memory_order_relaxed);
    UseNextTraceBuffer();
  }

  num_traces_recorded_++;

  UpdateCategoryRegistry();

  dispatching_to_observers_ = true;
  {
    // Notify observers outside of the thread events lock, so they can trigger
    // trace events.
    AutoUnlock unlock(lock_);
    AutoLock lock2(observers_lock_);
    for (EnabledStateObserver* observer : enabled_state_observers_)
      observer->OnTraceLogEnabled();
    for (const auto& it : async_observers_) {
      it.second.task_runner->PostTask(
          FROM_HERE, BindOnce(&AsyncEnabledStateObserver::OnTraceLogEnabled,
                              it.second.observer));
    }
  }
  dispatching_to_observers_ = false;
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
}

void TraceLog::SetArgumentFilterPredicate(
    const ArgumentFilterPredicate& argument_filter_predicate) {
  AutoLock lock(lock_);
  DCHECK(!argument_filter_predicate.is_null());
  // Replace the existing argument filter.
  argument_filter_predicate_ = argument_filter_predicate;
}

ArgumentFilterPredicate TraceLog::GetArgumentFilterPredicate() const {
  AutoLock lock(lock_);
  return argument_filter_predicate_;
}

void TraceLog::SetMetadataFilterPredicate(
    const MetadataFilterPredicate& metadata_filter_predicate) {
  AutoLock lock(lock_);
  DCHECK(!metadata_filter_predicate.is_null());
  // Replace the existing argument filter.
  metadata_filter_predicate_ = metadata_filter_predicate;
}

MetadataFilterPredicate TraceLog::GetMetadataFilterPredicate() const {
  AutoLock lock(lock_);
  return metadata_filter_predicate_;
}

void TraceLog::SetRecordHostAppPackageName(bool record_host_app_package_name) {
  record_host_app_package_name_ = record_host_app_package_name;
}

bool TraceLog::ShouldRecordHostAppPackageName() const {
  return record_host_app_package_name_;
}

TraceLog::InternalTraceOptions TraceLog::GetInternalOptionsFromTraceConfig(
    const TraceConfig& config) {
  InternalTraceOptions ret = config.IsArgumentFilterEnabled()
                                 ? kInternalEnableArgumentFilter
                                 : kInternalNone;
  switch (config.GetTraceRecordMode()) {
    case RECORD_UNTIL_FULL:
      return ret | kInternalRecordUntilFull;
    case RECORD_CONTINUOUSLY:
      return ret | kInternalRecordContinuously;
    case ECHO_TO_CONSOLE:
      return ret | kInternalEchoToConsole;
    case RECORD_AS_MUCH_AS_POSSIBLE:
      return ret | kInternalRecordAsMuchAsPossible;
  }
  NOTREACHED();
  return kInternalNone;
}

TraceConfig TraceLog::GetCurrentTraceConfig() const {
  AutoLock lock(lock_);
  return trace_config_;
}

void TraceLog::SetDisabled() {
  AutoLock lock(lock_);
  SetDisabledWhileLocked(RECORDING_MODE);
}

void TraceLog::SetDisabled(uint8_t modes_to_disable) {
  AutoLock lock(lock_);
  SetDisabledWhileLocked(modes_to_disable);
}

void TraceLog::SetDisabledWhileLocked(uint8_t modes_to_disable) {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  if (!tracing_session_)
    return;

  AddMetadataEventsWhileLocked();
  // Remove metadata events so they will not get added to a subsequent trace.
  metadata_events_.clear();

  perfetto::TrackEvent::Flush();
  tracing_session_->StopBlocking();
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  if (!(enabled_modes_ & modes_to_disable))
    return;

  if (dispatching_to_observers_) {
    // TODO(ssid): Change to NOTREACHED after fixing crbug.com/625170.
    DLOG(ERROR)
        << "Cannot manipulate TraceLog::Enabled state from an observer.";
    return;
  }

  bool is_recording_mode_disabled =
      (enabled_modes_ & RECORDING_MODE) && (modes_to_disable & RECORDING_MODE);
  enabled_modes_ &= ~modes_to_disable;

  if (modes_to_disable & FILTERING_MODE)
    enabled_event_filters_.clear();

  if (modes_to_disable & RECORDING_MODE)
    trace_config_.Clear();

  UpdateCategoryRegistry();

  // Add metadata events and notify observers only if recording mode was
  // disabled now.
  if (!is_recording_mode_disabled)
    return;

  AddMetadataEventsWhileLocked();

  // Remove metadata events so they will not get added to a subsequent trace.
  metadata_events_.clear();

  dispatching_to_observers_ = true;
  {
    // Release trace events lock, so observers can trigger trace events.
    AutoUnlock unlock(lock_);
    AutoLock lock2(observers_lock_);
    for (auto* it : enabled_state_observers_)
      it->OnTraceLogDisabled();
    for (const auto& it : async_observers_) {
      it.second.task_runner->PostTask(
          FROM_HERE, BindOnce(&AsyncEnabledStateObserver::OnTraceLogDisabled,
                              it.second.observer));
    }
  }
  dispatching_to_observers_ = false;
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
}

int TraceLog::GetNumTracesRecorded() {
  AutoLock lock(lock_);
  return (enabled_modes_ & RECORDING_MODE) ? num_traces_recorded_ : -1;
}

void TraceLog::AddEnabledStateObserver(EnabledStateObserver* listener) {
  AutoLock lock(observers_lock_);
  enabled_state_observers_.push_back(listener);
}

void TraceLog::RemoveEnabledStateObserver(EnabledStateObserver* listener) {
  AutoLock lock(observers_lock_);
  enabled_state_observers_.erase(
      ranges::remove(enabled_state_observers_, listener),
      enabled_state_observers_.end());
}

void TraceLog::AddOwnedEnabledStateObserver(
    std::unique_ptr<EnabledStateObserver> listener) {
  AutoLock lock(observers_lock_);
  enabled_state_observers_.push_back(listener.get());
  owned_enabled_state_observer_copy_.push_back(std::move(listener));
}

bool TraceLog::HasEnabledStateObserver(EnabledStateObserver* listener) const {
  AutoLock lock(observers_lock_);
  return Contains(enabled_state_observers_, listener);
}

void TraceLog::AddAsyncEnabledStateObserver(
    WeakPtr<AsyncEnabledStateObserver> listener) {
  AutoLock lock(observers_lock_);
  async_observers_.emplace(listener.get(), RegisteredAsyncObserver(listener));
}

void TraceLog::RemoveAsyncEnabledStateObserver(
    AsyncEnabledStateObserver* listener) {
  AutoLock lock(observers_lock_);
  async_observers_.erase(listener);
}

bool TraceLog::HasAsyncEnabledStateObserver(
    AsyncEnabledStateObserver* listener) const {
  AutoLock lock(observers_lock_);
  return Contains(async_observers_, listener);
}

TraceLogStatus TraceLog::GetStatus() const {
  AutoLock lock(lock_);
  TraceLogStatus result;
  result.event_capacity = static_cast<uint32_t>(logged_events_->Capacity());
  result.event_count = static_cast<uint32_t>(logged_events_->Size());
  return result;
}

bool TraceLog::BufferIsFull() const {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  // TODO(skyostil): Remove this method since there are no non-test usages.
  DCHECK(false);
  return false;
#else
  AutoLock lock(lock_);
  return logged_events_->IsFull();
#endif
}

TraceEvent* TraceLog::AddEventToThreadSharedChunkWhileLocked(
    TraceEventHandle* handle,
    bool check_buffer_is_full) {
  if (thread_shared_chunk_ && thread_shared_chunk_->IsFull()) {
    logged_events_->ReturnChunk(thread_shared_chunk_index_,
                                std::move(thread_shared_chunk_));
  }

  if (!thread_shared_chunk_) {
    thread_shared_chunk_ =
        logged_events_->GetChunk(&thread_shared_chunk_index_);
    if (check_buffer_is_full)
      CheckIfBufferIsFullWhileLocked();
  }
  if (!thread_shared_chunk_)
    return nullptr;

  size_t event_index;
  TraceEvent* trace_event = thread_shared_chunk_->AddTraceEvent(&event_index);
  if (trace_event && handle) {
    MakeHandle(thread_shared_chunk_->seq(), thread_shared_chunk_index_,
               event_index, handle);
  }
  return trace_event;
}

void TraceLog::CheckIfBufferIsFullWhileLocked() {
  if (logged_events_->IsFull()) {
    if (buffer_limit_reached_timestamp_.is_null()) {
      buffer_limit_reached_timestamp_ = OffsetNow();
    }
    SetDisabledWhileLocked(RECORDING_MODE);
  }
}

// Flush() works as the following:
// 1. Flush() is called in thread A whose task runner is saved in
//    flush_task_runner_;
// 2. If thread_message_loops_ is not empty, thread A posts task to each message
//    loop to flush the thread local buffers; otherwise finish the flush;
// 3. FlushCurrentThread() deletes the thread local event buffer:
//    - The last batch of events of the thread are flushed into the main buffer;
//    - The message loop will be removed from thread_message_loops_;
//    If this is the last message loop, finish the flush;
// 4. If any thread hasn't finish its flush in time, finish the flush.
void TraceLog::Flush(const TraceLog::OutputCallback& cb,
                     bool use_worker_thread) {
  FlushInternal(cb, use_worker_thread, false);
}

void TraceLog::CancelTracing(const OutputCallback& cb) {
  SetDisabled();
  FlushInternal(cb, false, true);
}

void TraceLog::FlushInternal(const TraceLog::OutputCallback& cb,
                             bool use_worker_thread,
                             bool discard_events) {
  use_worker_thread_ = use_worker_thread;

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY) && !defined(OS_NACL)
  perfetto::TrackEvent::Flush();

  if (discard_events) {
    tracing_session_.reset();
    scoped_refptr<RefCountedString> empty_result = new RefCountedString;
    cb.Run(empty_result, /*has_more_events=*/false);
    return;
  }

  perfetto::trace_processor::Config processor_config;
  trace_processor_ =
      perfetto::trace_processor::TraceProcessorStorage::CreateInstance(
          processor_config);
  json_output_writer_.reset(new JsonStringOutputWriter(
      use_worker_thread ? ThreadTaskRunnerHandle::Get() : nullptr, cb));

  if (use_worker_thread) {
    tracing_session_->ReadTrace(
        [this](perfetto::TracingSession::ReadTraceCallbackArgs args) {
          OnTraceData(args.data, args.size, args.has_more);
        });
  } else {
    auto data = tracing_session_->ReadTraceBlocking();
    OnTraceData(data.data(), data.size(), /*has_more=*/false);
  }
#elif BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY) && defined(OS_NACL)
  // Trace processor isn't built on NaCL, so we can't convert the resulting
  // trace into JSON.
  CHECK(false) << "JSON tracing isn't supported on NaCL";
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  if (IsEnabled()) {
    // Can't flush when tracing is enabled because otherwise PostTask would
    // - generate more trace events;
    // - deschedule the calling thread on some platforms causing inaccurate
    //   timing of the trace events.
    scoped_refptr<RefCountedString> empty_result = new RefCountedString;
    if (!cb.is_null())
      cb.Run(empty_result, false);
    LOG(WARNING) << "Ignored TraceLog::Flush called when tracing is enabled";
    return;
  }

  int gen = generation();
  // Copy of thread_task_runners_ to be used without locking.
  std::vector<scoped_refptr<SingleThreadTaskRunner>> task_runners;
  {
    AutoLock lock(lock_);
    DCHECK(!flush_task_runner_);
    flush_task_runner_ = SequencedTaskRunnerHandle::IsSet()
                             ? SequencedTaskRunnerHandle::Get()
                             : nullptr;
    DCHECK(thread_task_runners_.empty() || flush_task_runner_);
    flush_output_callback_ = cb;

    if (thread_shared_chunk_) {
      logged_events_->ReturnChunk(thread_shared_chunk_index_,
                                  std::move(thread_shared_chunk_));
    }

    for (const auto& it : thread_task_runners_)
      task_runners.push_back(it.second);
  }

  if (!task_runners.empty()) {
    for (auto& task_runner : task_runners) {
      task_runner->PostTask(
          FROM_HERE, BindOnce(&TraceLog::FlushCurrentThread, Unretained(this),
                              gen, discard_events));
    }
    flush_task_runner_->PostDelayedTask(
        FROM_HERE,
        BindOnce(&TraceLog::OnFlushTimeout, Unretained(this), gen,
                 discard_events),
        TimeDelta::FromMilliseconds(kThreadFlushTimeoutMs));
    return;
  }

  FinishFlush(gen, discard_events);
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
}

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY) && !defined(OS_NACL)
void TraceLog::OnTraceData(const char* data, size_t size, bool has_more) {
  if (size) {
    std::unique_ptr<uint8_t[]> data_copy(new uint8_t[size]);
    memcpy(&data_copy[0], data, size);
    auto status = trace_processor_->Parse(std::move(data_copy), size);
    DCHECK(status.ok()) << status.message();
  }
  if (has_more)
    return;
  trace_processor_->NotifyEndOfFile();

  auto status = perfetto::trace_processor::json::ExportJson(
      trace_processor_.get(), json_output_writer_.get());
  DCHECK(status.ok()) << status.message();
  trace_processor_.reset();
  tracing_session_.reset();
  json_output_writer_.reset();
}
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY) && !defined(OS_NACL)

// Usually it runs on a different thread.
void TraceLog::ConvertTraceEventsToTraceFormat(
    std::unique_ptr<TraceBuffer> logged_events,
    const OutputCallback& flush_output_callback,
    const ArgumentFilterPredicate& argument_filter_predicate) {
  if (flush_output_callback.is_null())
    return;

  HEAP_PROFILER_SCOPED_IGNORE;
  // The callback need to be called at least once even if there is no events
  // to let the caller know the completion of flush.
  scoped_refptr<RefCountedString> json_events_str_ptr = new RefCountedString();
  const size_t kReserveCapacity = kTraceEventBufferSizeInBytes * 5 / 4;
  json_events_str_ptr->data().reserve(kReserveCapacity);
  while (const TraceBufferChunk* chunk = logged_events->NextChunk()) {
    for (size_t j = 0; j < chunk->size(); ++j) {
      size_t size = json_events_str_ptr->size();
      if (size > kTraceEventBufferSizeInBytes) {
        flush_output_callback.Run(json_events_str_ptr, true);
        json_events_str_ptr = new RefCountedString();
        json_events_str_ptr->data().reserve(kReserveCapacity);
      } else if (size) {
        json_events_str_ptr->data().append(",\n");
      }
      chunk->GetEventAt(j)->AppendAsJSON(&(json_events_str_ptr->data()),
                                         argument_filter_predicate);
    }
  }
  flush_output_callback.Run(json_events_str_ptr, false);
}

void TraceLog::FinishFlush(int generation, bool discard_events) {
  std::unique_ptr<TraceBuffer> previous_logged_events;
  OutputCallback flush_output_callback;
  ArgumentFilterPredicate argument_filter_predicate;

  if (!CheckGeneration(generation))
    return;

  {
    AutoLock lock(lock_);

    previous_logged_events.swap(logged_events_);
    UseNextTraceBuffer();
    thread_task_runners_.clear();

    flush_task_runner_ = nullptr;
    flush_output_callback = flush_output_callback_;
    flush_output_callback_.Reset();

    if (trace_options() & kInternalEnableArgumentFilter) {
      // If argument filtering is activated and there is no filtering predicate,
      // use the safe default filtering predicate.
      if (argument_filter_predicate_.is_null()) {
        argument_filter_predicate =
            base::BindRepeating(&DefaultIsTraceEventArgsAllowlisted);
      } else {
        argument_filter_predicate = argument_filter_predicate_;
      }
    }
  }

  if (discard_events) {
    if (!flush_output_callback.is_null()) {
      scoped_refptr<RefCountedString> empty_result = new RefCountedString;
      flush_output_callback.Run(empty_result, false);
    }
    return;
  }

  if (use_worker_thread_) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {MayBlock(), TaskPriority::BEST_EFFORT,
         TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        BindOnce(&TraceLog::ConvertTraceEventsToTraceFormat,
                 std::move(previous_logged_events), flush_output_callback,
                 argument_filter_predicate));
    return;
  }

  ConvertTraceEventsToTraceFormat(std::move(previous_logged_events),
                                  flush_output_callback,
                                  argument_filter_predicate);
}

// Run in each thread holding a local event buffer.
void TraceLog::FlushCurrentThread(int generation, bool discard_events) {
  {
    AutoLock lock(lock_);
    if (!CheckGeneration(generation) || !flush_task_runner_) {
      // This is late. The corresponding flush has finished.
      return;
    }
  }

  // This will flush the thread local buffer.
  delete thread_local_event_buffer_.Get();

  auto on_flush_override = on_flush_override_.load(std::memory_order_relaxed);
  if (on_flush_override) {
    on_flush_override();
  }

  // Scheduler uses TRACE_EVENT macros when posting a task, which can lead
  // to acquiring a tracing lock. Given that posting a task requires grabbing
  // a scheduler lock, we need to post this task outside tracing lock to avoid
  // deadlocks.
  scoped_refptr<SequencedTaskRunner> cached_flush_task_runner;
  {
    AutoLock lock(lock_);
    cached_flush_task_runner = flush_task_runner_;
    if (!CheckGeneration(generation) || !flush_task_runner_ ||
        !thread_task_runners_.empty())
      return;
  }
  cached_flush_task_runner->PostTask(
      FROM_HERE, BindOnce(&TraceLog::FinishFlush, Unretained(this), generation,
                          discard_events));
}

void TraceLog::OnFlushTimeout(int generation, bool discard_events) {
  {
    AutoLock lock(lock_);
    if (!CheckGeneration(generation) || !flush_task_runner_) {
      // Flush has finished before timeout.
      return;
    }

    LOG(WARNING)
        << "The following threads haven't finished flush in time. "
           "If this happens stably for some thread, please call "
           "TraceLog::GetInstance()->SetCurrentThreadBlocksMessageLoop() from "
           "the thread to avoid its trace events from being lost.";
    for (const auto& it : thread_task_runners_) {
      LOG(WARNING) << "Thread: "
                   << ThreadIdNameManager::GetInstance()->GetName(it.first);
    }
  }
  FinishFlush(generation, discard_events);
}

void TraceLog::UseNextTraceBuffer() {
  logged_events_.reset(CreateTraceBuffer());
  subtle::NoBarrier_AtomicIncrement(&generation_, 1);
  thread_shared_chunk_.reset();
  thread_shared_chunk_index_ = 0;
}

bool TraceLog::ShouldAddAfterUpdatingState(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    unsigned long long id,
    int thread_id,
    TraceArguments* args) {
  if (!*category_group_enabled)
    return false;

  // Avoid re-entrance of AddTraceEvent. This may happen in GPU process when
  // ECHO_TO_CONSOLE is enabled: AddTraceEvent -> LOG(ERROR) ->
  // GpuProcessLogMessageHandler -> PostPendingTask -> TRACE_EVENT ...
  if (thread_is_in_trace_event_.Get())
    return false;

  DCHECK(name);

  // Check and update the current thread name only if the event is for the
  // current thread to avoid locks in most cases.
  if (thread_id == static_cast<int>(PlatformThread::CurrentId())) {
    const char* new_name =
        ThreadIdNameManager::GetInstance()->GetNameForCurrentThread();
    // Check if the thread name has been set or changed since the previous
    // call (if any), but don't bother if the new name is empty. Note this will
    // not detect a thread name change within the same char* buffer address: we
    // favor common case performance over corner case correctness.
    static auto* current_thread_name = new ThreadLocalPointer<const char>();
    if (new_name != current_thread_name->Get() && new_name && *new_name) {
      current_thread_name->Set(new_name);

      AutoLock thread_info_lock(thread_info_lock_);

      auto existing_name = thread_names_.find(thread_id);
      if (existing_name == thread_names_.end()) {
        // This is a new thread id, and a new name.
        thread_names_[thread_id] = new_name;
      } else {
        // This is a thread id that we've seen before, but potentially with a
        // new name.
        std::vector<StringPiece> existing_names = base::SplitStringPiece(
            existing_name->second, ",", base::KEEP_WHITESPACE,
            base::SPLIT_WANT_NONEMPTY);
        if (!Contains(existing_names, new_name)) {
          if (!existing_names.empty())
            existing_name->second.push_back(',');
          existing_name->second.append(new_name);
        }
      }
    }
  }

#if defined(OS_WIN)
  // This is done sooner rather than later, to avoid creating the event and
  // acquiring the lock, which is not needed for ETW as it's already threadsafe.
  if (*category_group_enabled & TraceCategory::ENABLED_FOR_ETW_EXPORT) {
    TraceEventETWExport::AddEvent(phase, category_group_enabled, name, id,
                                  args);
  }
#endif  // OS_WIN
  return true;
}

TraceEventHandle TraceLog::AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    TraceArguments* args,
    unsigned int flags) {
  int thread_id = static_cast<int>(base::PlatformThread::CurrentId());
  base::TimeTicks now = TRACE_TIME_TICKS_NOW();
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id,
      trace_event_internal::kNoId,  // bind_id
      thread_id, now, args, flags);
}

TraceEventHandle TraceLog::AddTraceEventWithBindId(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned long long bind_id,
    TraceArguments* args,
    unsigned int flags) {
  int thread_id = static_cast<int>(base::PlatformThread::CurrentId());
  base::TimeTicks now = TRACE_TIME_TICKS_NOW();
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id, bind_id, thread_id, now,
      args, flags | TRACE_EVENT_FLAG_HAS_CONTEXT_ID);
}

TraceEventHandle TraceLog::AddTraceEventWithProcessId(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    int process_id,
    TraceArguments* args,
    unsigned int flags) {
  base::TimeTicks now = TRACE_TIME_TICKS_NOW();
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id,
      trace_event_internal::kNoId,  // bind_id
      process_id, now, args, flags | TRACE_EVENT_FLAG_HAS_PROCESS_ID);
}

// Handle legacy calls to AddTraceEventWithThreadIdAndTimestamp
// with kNoId as bind_id
TraceEventHandle TraceLog::AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    int thread_id,
    const TimeTicks& timestamp,
    TraceArguments* args,
    unsigned int flags) {
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id,
      trace_event_internal::kNoId,  // bind_id
      thread_id, timestamp, args, flags);
}

TraceEventHandle TraceLog::AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned long long bind_id,
    int thread_id,
    const TimeTicks& timestamp,
    TraceArguments* args,
    unsigned int flags) {
  ThreadTicks thread_now;
  // If timestamp is provided explicitly, don't record thread time as it would
  // be for the wrong timestamp. Similarly, if we record an event for another
  // process or thread, we shouldn't report the current thread's thread time.
  if (!(flags & TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP ||
        flags & TRACE_EVENT_FLAG_HAS_PROCESS_ID ||
        thread_id != static_cast<int>(PlatformThread::CurrentId()))) {
    thread_now = ThreadNow();
  }
  return AddTraceEventWithThreadIdAndTimestamps(
      phase, category_group_enabled, name, scope, id, bind_id, thread_id,
      timestamp, thread_now, args, flags);
}

TraceEventHandle TraceLog::AddTraceEventWithThreadIdAndTimestamps(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned long long bind_id,
    int thread_id,
    const TimeTicks& timestamp,
    const ThreadTicks& thread_timestamp,
    TraceArguments* args,
    unsigned int flags) NO_THREAD_SAFETY_ANALYSIS {
  TraceEventHandle handle = {0, 0, 0};
  if (!ShouldAddAfterUpdatingState(phase, category_group_enabled, name, id,
                                   thread_id, args)) {
    return handle;
  }
  DCHECK(!timestamp.is_null());

  AutoThreadLocalBoolean thread_is_in_trace_event(&thread_is_in_trace_event_);

  // Flow bind_ids don't have scopes, so we need to mangle in-process ones to
  // avoid collisions.
  bool has_flow =
      flags & (TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN);
  if (has_flow && (flags & TRACE_EVENT_FLAG_HAS_LOCAL_ID))
    bind_id = MangleEventId(bind_id);

  TimeTicks offset_event_timestamp = OffsetTimestamp(timestamp);
  ThreadInstructionCount thread_instruction_now;
  // If timestamp is provided explicitly, don't record thread instruction count
  // as it would be for the wrong timestamp. Similarly, if we record an event
  // for another process or thread, we shouldn't report the current thread's
  // thread time.
  if (!(flags & TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP ||
        flags & TRACE_EVENT_FLAG_HAS_PROCESS_ID ||
        thread_id != static_cast<int>(PlatformThread::CurrentId()))) {
    thread_instruction_now = ThreadInstructionNow();
  }

  ThreadLocalEventBuffer* thread_local_event_buffer = nullptr;
  if (*category_group_enabled & RECORDING_MODE) {
    // |thread_local_event_buffer_| can be null if the current thread doesn't
    // have a message loop or the message loop is blocked.
    InitializeThreadLocalEventBufferIfSupported();
    thread_local_event_buffer = thread_local_event_buffer_.Get();
  }

  if (*category_group_enabled & RECORDING_MODE) {
    auto trace_event_override =
        add_trace_event_override_.load(std::memory_order_relaxed);
    if (trace_event_override) {
      TraceEvent new_trace_event(thread_id, offset_event_timestamp,
                                 thread_timestamp, thread_instruction_now,
                                 phase, category_group_enabled, name, scope, id,
                                 bind_id, args, flags);

      trace_event_override(
          &new_trace_event,
          /*thread_will_flush=*/thread_local_event_buffer != nullptr, &handle);
      return handle;
    }
  }

  std::string console_message;
  std::unique_ptr<TraceEvent> filtered_trace_event;
  bool disabled_by_filters = false;
  if (*category_group_enabled & TraceCategory::ENABLED_FOR_FILTERING) {
    auto new_trace_event = std::make_unique<TraceEvent>(
        thread_id, offset_event_timestamp, thread_timestamp,
        thread_instruction_now, phase, category_group_enabled, name, scope, id,
        bind_id, args, flags);

    disabled_by_filters = true;
    ForEachCategoryFilter(
        category_group_enabled, [&new_trace_event, &disabled_by_filters](
                                    TraceEventFilter* trace_event_filter) {
          if (trace_event_filter->FilterTraceEvent(*new_trace_event))
            disabled_by_filters = false;
        });
    if (!disabled_by_filters)
      filtered_trace_event = std::move(new_trace_event);
  }

  // If enabled for recording, the event should be added only if one of the
  // filters indicates or category is not enabled for filtering.
  if ((*category_group_enabled & TraceCategory::ENABLED_FOR_RECORDING) &&
      !disabled_by_filters) {
    OptionalAutoLock lock(&lock_);

    TraceEvent* trace_event = nullptr;
    if (thread_local_event_buffer) {
      trace_event = thread_local_event_buffer->AddTraceEvent(&handle);
    } else {
      lock.EnsureAcquired();
      trace_event = AddEventToThreadSharedChunkWhileLocked(&handle, true);
    }

    // NO_THREAD_SAFETY_ANALYSIS: Conditional locking above.
    if (trace_event) {
      if (filtered_trace_event) {
        *trace_event = std::move(*filtered_trace_event);
      } else {
        trace_event->Reset(thread_id, offset_event_timestamp, thread_timestamp,
                           thread_instruction_now, phase,
                           category_group_enabled, name, scope, id, bind_id,
                           args, flags);
      }

#if defined(OS_ANDROID)
      trace_event->SendToATrace();
#endif
    }

    if (trace_options() & kInternalEchoToConsole) {
      console_message = EventToConsoleMessage(
          phase == TRACE_EVENT_PHASE_COMPLETE ? TRACE_EVENT_PHASE_BEGIN : phase,
          timestamp, trace_event);
    }
  }

  if (!console_message.empty())
    LOG(ERROR) << console_message;

  return handle;
}

void TraceLog::AddMetadataEvent(const unsigned char* category_group_enabled,
                                const char* name,
                                TraceArguments* args,
                                unsigned int flags) {
  HEAP_PROFILER_SCOPED_IGNORE;
  int thread_id = static_cast<int>(base::PlatformThread::CurrentId());
  ThreadTicks thread_now = ThreadNow();
  TimeTicks now = OffsetNow();
  ThreadInstructionCount thread_instruction_now = ThreadInstructionNow();
  AutoLock lock(lock_);
  auto trace_event = std::make_unique<TraceEvent>(
      thread_id, now, thread_now, thread_instruction_now,
      TRACE_EVENT_PHASE_METADATA, category_group_enabled, name,
      trace_event_internal::kGlobalScope,  // scope
      trace_event_internal::kNoId,         // id
      trace_event_internal::kNoId,         // bind_id
      args, flags);
  metadata_events_.push_back(std::move(trace_event));
}

// May be called when a COMPELETE event ends and the unfinished event has been
// recycled (phase == TRACE_EVENT_PHASE_END and trace_event == NULL).
std::string TraceLog::EventToConsoleMessage(unsigned char phase,
                                            const TimeTicks& timestamp,
                                            TraceEvent* trace_event) {
  HEAP_PROFILER_SCOPED_IGNORE;
  AutoLock thread_info_lock(thread_info_lock_);

  // The caller should translate TRACE_EVENT_PHASE_COMPLETE to
  // TRACE_EVENT_PHASE_BEGIN or TRACE_EVENT_END.
  DCHECK(phase != TRACE_EVENT_PHASE_COMPLETE);

  TimeDelta duration;
  int thread_id =
      trace_event ? trace_event->thread_id() : PlatformThread::CurrentId();
  if (phase == TRACE_EVENT_PHASE_END) {
    duration = timestamp - thread_event_start_times_[thread_id].top();
    thread_event_start_times_[thread_id].pop();
  }

  std::string thread_name = thread_names_[thread_id];
  if (thread_colors_.find(thread_name) == thread_colors_.end()) {
    size_t next_color = (thread_colors_.size() % 6) + 1;
    thread_colors_[thread_name] = next_color;
  }

  std::ostringstream log;
  log << base::StringPrintf("%s: \x1b[0;3%dm", thread_name.c_str(),
                            thread_colors_[thread_name]);

  size_t depth = 0;
  auto it = thread_event_start_times_.find(thread_id);
  if (it != thread_event_start_times_.end())
    depth = it->second.size();

  for (size_t i = 0; i < depth; ++i)
    log << "| ";

  if (trace_event)
    trace_event->AppendPrettyPrinted(&log);
  if (phase == TRACE_EVENT_PHASE_END)
    log << base::StringPrintf(" (%.3f ms)", duration.InMillisecondsF());

  log << "\x1b[0;m";

  if (phase == TRACE_EVENT_PHASE_BEGIN)
    thread_event_start_times_[thread_id].push(timestamp);

  return log.str();
}

void TraceLog::EndFilteredEvent(const unsigned char* category_group_enabled,
                                const char* name,
                                TraceEventHandle handle) {
  const char* category_name = GetCategoryGroupName(category_group_enabled);
  ForEachCategoryFilter(
      category_group_enabled,
      [name, category_name](TraceEventFilter* trace_event_filter) {
        trace_event_filter->EndEvent(category_name, name);
      });
}

void TraceLog::UpdateTraceEventDuration(
    const unsigned char* category_group_enabled,
    const char* name,
    TraceEventHandle handle) {
  char category_group_enabled_local = *category_group_enabled;
  if (!category_group_enabled_local)
    return;

  UpdateTraceEventDurationExplicit(
      category_group_enabled, name, handle,
      static_cast<int>(base::PlatformThread::CurrentId()),
      /*explicit_timestamps=*/false, OffsetNow(), ThreadNow(),
      ThreadInstructionNow());
}

void TraceLog::UpdateTraceEventDurationExplicit(
    const unsigned char* category_group_enabled,
    const char* name,
    TraceEventHandle handle,
    int thread_id,
    bool explicit_timestamps,
    const TimeTicks& now,
    const ThreadTicks& thread_now,
    ThreadInstructionCount thread_instruction_now) {
  char category_group_enabled_local = *category_group_enabled;
  if (!category_group_enabled_local)
    return;

  // Avoid re-entrance of AddTraceEvent. This may happen in GPU process when
  // ECHO_TO_CONSOLE is enabled: AddTraceEvent -> LOG(ERROR) ->
  // GpuProcessLogMessageHandler -> PostPendingTask -> TRACE_EVENT ...
  if (thread_is_in_trace_event_.Get())
    return;
  AutoThreadLocalBoolean thread_is_in_trace_event(&thread_is_in_trace_event_);

#if defined(OS_WIN)
  // Generate an ETW event that marks the end of a complete event.
  if (category_group_enabled_local & TraceCategory::ENABLED_FOR_ETW_EXPORT)
    TraceEventETWExport::AddCompleteEndEvent(category_group_enabled, name);
#endif  // OS_WIN

  if (category_group_enabled_local & TraceCategory::ENABLED_FOR_RECORDING) {
    auto update_duration_override =
        update_duration_override_.load(std::memory_order_relaxed);
    if (update_duration_override) {
      update_duration_override(category_group_enabled, name, handle, thread_id,
                               explicit_timestamps, now, thread_now,
                               thread_instruction_now);
      return;
    }
  }

  std::string console_message;
  if (category_group_enabled_local & TraceCategory::ENABLED_FOR_RECORDING) {
    OptionalAutoLock lock(&lock_);

    TraceEvent* trace_event = GetEventByHandleInternal(handle, &lock);
    if (trace_event) {
      DCHECK(trace_event->phase() == TRACE_EVENT_PHASE_COMPLETE);

      trace_event->UpdateDuration(now, thread_now, thread_instruction_now);
#if defined(OS_ANDROID)
      trace_event->SendToATrace();
#endif
    }

    if (trace_options() & kInternalEchoToConsole) {
      console_message =
          EventToConsoleMessage(TRACE_EVENT_PHASE_END, now, trace_event);
    }
  }

  if (!console_message.empty())
    LOG(ERROR) << console_message;

  if (category_group_enabled_local & TraceCategory::ENABLED_FOR_FILTERING)
    EndFilteredEvent(category_group_enabled, name, handle);
}

uint64_t TraceLog::MangleEventId(uint64_t id) {
  return id ^ process_id_hash_;
}

template <typename T>
void TraceLog::AddMetadataEventWhileLocked(int thread_id,
                                           const char* metadata_name,
                                           const char* arg_name,
                                           const T& value) {
  auto trace_event_override =
      add_trace_event_override_.load(std::memory_order_relaxed);
  if (trace_event_override) {
    TraceEvent trace_event;
    InitializeMetadataEvent(&trace_event, thread_id, metadata_name, arg_name,
                            value);
    trace_event_override(&trace_event, /*thread_will_flush=*/true, nullptr);
  } else {
    InitializeMetadataEvent(
        AddEventToThreadSharedChunkWhileLocked(nullptr, false), thread_id,
        metadata_name, arg_name, value);
  }
}

void TraceLog::AddMetadataEventsWhileLocked() {
  auto trace_event_override =
      add_trace_event_override_.load(std::memory_order_relaxed);

  // Move metadata added by |AddMetadataEvent| into the trace log.
  if (trace_event_override) {
    while (!metadata_events_.empty()) {
      trace_event_override(metadata_events_.back().get(),
                           /*thread_will_flush=*/true, nullptr);
      metadata_events_.pop_back();
    }
  } else {
    while (!metadata_events_.empty()) {
      TraceEvent* event =
          AddEventToThreadSharedChunkWhileLocked(nullptr, false);
      *event = std::move(*metadata_events_.back());
      metadata_events_.pop_back();
    }
  }

#if !defined(OS_NACL)  // NaCl shouldn't expose the process id.
  AddMetadataEventWhileLocked(0, "num_cpus", "number",
                              base::SysInfo::NumberOfProcessors());
#endif

  int current_thread_id = static_cast<int>(base::PlatformThread::CurrentId());
  if (process_sort_index_ != 0) {
    AddMetadataEventWhileLocked(current_thread_id, "process_sort_index",
                                "sort_index", process_sort_index_);
  }

#if defined(OS_ANDROID)
  AddMetadataEventWhileLocked(current_thread_id, "chrome_library_address",
                              "start_address",
                              base::StringPrintf("%p", &__executable_start));
  base::debug::ElfBuildIdBuffer build_id;
  size_t build_id_length =
      base::debug::ReadElfBuildId(&__executable_start, true, build_id);
  if (build_id_length > 0) {
    AddMetadataEventWhileLocked(current_thread_id, "chrome_library_module",
                                "id", std::string(build_id));
  }
#endif

  if (!process_labels_.empty()) {
    std::vector<base::StringPiece> labels;
    for (const auto& it : process_labels_)
      labels.push_back(it.second);
    AddMetadataEventWhileLocked(current_thread_id, "process_labels", "labels",
                                base::JoinString(labels, ","));
  }

  // Thread sort indices.
  for (const auto& it : thread_sort_indices_) {
    if (it.second == 0)
      continue;
    AddMetadataEventWhileLocked(it.first, "thread_sort_index", "sort_index",
                                it.second);
  }

  // If buffer is full, add a metadata record to report this.
  if (!buffer_limit_reached_timestamp_.is_null()) {
    AddMetadataEventWhileLocked(current_thread_id, "trace_buffer_overflowed",
                                "overflowed_at_ts",
                                buffer_limit_reached_timestamp_);
  }
}

TraceEvent* TraceLog::GetEventByHandle(TraceEventHandle handle) {
  return GetEventByHandleInternal(handle, nullptr);
}

TraceEvent* TraceLog::GetEventByHandleInternal(TraceEventHandle handle,
                                               OptionalAutoLock* lock)
    NO_THREAD_SAFETY_ANALYSIS {
  if (!handle.chunk_seq)
    return nullptr;

  DCHECK(handle.chunk_seq);
  DCHECK(handle.chunk_index <= TraceBufferChunk::kMaxChunkIndex);
  DCHECK(handle.event_index <= TraceBufferChunk::kTraceBufferChunkSize - 1);

  if (thread_local_event_buffer_.Get()) {
    TraceEvent* trace_event =
        thread_local_event_buffer_.Get()->GetEventByHandle(handle);
    if (trace_event)
      return trace_event;
  }

  // The event has been out-of-control of the thread local buffer.
  // Try to get the event from the main buffer with a lock.
  // NO_THREAD_SAFETY_ANALYSIS: runtime-dependent locking here.
  if (lock)
    lock->EnsureAcquired();

  if (thread_shared_chunk_ &&
      handle.chunk_index == thread_shared_chunk_index_) {
    return handle.chunk_seq == thread_shared_chunk_->seq()
               ? thread_shared_chunk_->GetEventAt(handle.event_index)
               : nullptr;
  }

  return logged_events_->GetEventByHandle(handle);
}

void TraceLog::SetProcessID(int process_id) {
  process_id_ = process_id;
  // Create a FNV hash from the process ID for XORing.
  // See http://isthe.com/chongo/tech/comp/fnv/ for algorithm details.
  const unsigned long long kOffsetBasis = 14695981039346656037ull;
  const unsigned long long kFnvPrime = 1099511628211ull;
  const unsigned long long pid = static_cast<unsigned long long>(process_id_);
  process_id_hash_ = (kOffsetBasis ^ pid) * kFnvPrime;
}

void TraceLog::SetProcessSortIndex(int sort_index) {
  AutoLock lock(lock_);
  process_sort_index_ = sort_index;
}

void TraceLog::set_process_name(const std::string& process_name) {
  {
    AutoLock lock(lock_);
    process_name_ = process_name;
  }
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  auto track = perfetto::ProcessTrack::Current();
  auto desc = track.Serialize();
  desc.mutable_process()->set_process_name(process_name);
  perfetto::TrackEvent::SetTrackDescriptor(track, std::move(desc));
#endif
}

void TraceLog::UpdateProcessLabel(int label_id,
                                  const std::string& current_label) {
  if (!current_label.length())
    return RemoveProcessLabel(label_id);

  AutoLock lock(lock_);
  process_labels_[label_id] = current_label;
}

void TraceLog::RemoveProcessLabel(int label_id) {
  AutoLock lock(lock_);
  process_labels_.erase(label_id);
}

void TraceLog::SetThreadSortIndex(PlatformThreadId thread_id, int sort_index) {
  AutoLock lock(lock_);
  thread_sort_indices_[static_cast<int>(thread_id)] = sort_index;
}

#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
void TraceLog::SetTimeOffset(TimeDelta offset) {
  time_offset_ = offset;
}
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

size_t TraceLog::GetObserverCountForTest() const {
  AutoLock lock(observers_lock_);
  return enabled_state_observers_.size();
}

void TraceLog::SetCurrentThreadBlocksMessageLoop() {
  thread_blocks_message_loop_.Set(true);
  // This will flush the thread local buffer.
  delete thread_local_event_buffer_.Get();
}

TraceBuffer* TraceLog::CreateTraceBuffer() {
  HEAP_PROFILER_SCOPED_IGNORE;
  InternalTraceOptions options = trace_options();
  const size_t config_buffer_chunks =
      trace_config_.GetTraceBufferSizeInEvents() / kTraceBufferChunkSize;
  if (options & kInternalRecordContinuously) {
    return TraceBuffer::CreateTraceBufferRingBuffer(
        config_buffer_chunks > 0 ? config_buffer_chunks
                                 : kTraceEventRingBufferChunks);
  }
  if (options & kInternalEchoToConsole) {
    return TraceBuffer::CreateTraceBufferRingBuffer(
        config_buffer_chunks > 0 ? config_buffer_chunks
                                 : kEchoToConsoleTraceEventBufferChunks);
  }
  if (options & kInternalRecordAsMuchAsPossible) {
    return TraceBuffer::CreateTraceBufferVectorOfSize(
        config_buffer_chunks > 0 ? config_buffer_chunks
                                 : kTraceEventVectorBigBufferChunks);
  }
  return TraceBuffer::CreateTraceBufferVectorOfSize(
      config_buffer_chunks > 0 ? config_buffer_chunks
                               : kTraceEventVectorBufferChunks);
}

#if defined(OS_WIN)
void TraceLog::UpdateETWCategoryGroupEnabledFlags() {
  // Go through each category and set/clear the ETW bit depending on whether the
  // category is enabled.
  for (TraceCategory& category : CategoryRegistry::GetAllCategories()) {
    if (base::trace_event::TraceEventETWExport::IsCategoryGroupEnabled(
            category.name())) {
      category.set_state_flag(TraceCategory::ENABLED_FOR_ETW_EXPORT);
    } else {
      category.clear_state_flag(TraceCategory::ENABLED_FOR_ETW_EXPORT);
    }
  }
}
#endif  // defined(OS_WIN)

void TraceLog::SetTraceBufferForTesting(
    std::unique_ptr<TraceBuffer> trace_buffer) {
  AutoLock lock(lock_);
  logged_events_ = std::move(trace_buffer);
}

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
tracing::PerfettoPlatform* TraceLog::GetOrCreatePerfettoPlatform() {
  if (!perfetto_platform_) {
    perfetto_platform_.reset(new tracing::PerfettoPlatform(
        tracing::PerfettoPlatform::TaskRunnerType::kBuiltin));
  }
  return perfetto_platform_.get();
}

void TraceLog::OnSetup(const perfetto::DataSourceBase::SetupArgs&) {}

void TraceLog::OnStart(const perfetto::DataSourceBase::StartArgs&) {
  AutoLock lock(observers_lock_);
  for (EnabledStateObserver* observer : enabled_state_observers_)
    observer->OnTraceLogEnabled();
  for (const auto& it : async_observers_) {
    it.second.task_runner->PostTask(
        FROM_HERE, BindOnce(&AsyncEnabledStateObserver::OnTraceLogEnabled,
                            it.second.observer));
  }
}

void TraceLog::OnStop(const perfetto::DataSourceBase::StopArgs&) {
  AutoLock lock(observers_lock_);
  for (auto* it : enabled_state_observers_)
    it->OnTraceLogDisabled();
  for (const auto& it : async_observers_) {
    it.second.task_runner->PostTask(
        FROM_HERE, BindOnce(&AsyncEnabledStateObserver::OnTraceLogDisabled,
                            it.second.observer));
  }
}
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

void ConvertableToTraceFormat::EstimateTraceMemoryOverhead(
    TraceEventMemoryOverhead* overhead) {
  overhead->Add(TraceEventMemoryOverhead::kConvertableToTraceFormat,
                sizeof(*this));
}

}  // namespace trace_event
}  // namespace base

namespace trace_event_internal {

base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    base::trace_event::TraceArguments* args,
    unsigned int flags) {
  return base::trace_event::TraceLog::GetInstance()->AddTraceEvent(
      phase, category_group_enabled, name, scope, id, args, flags);
}

base::trace_event::TraceEventHandle AddTraceEventWithBindId(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned long long bind_id,
    base::trace_event::TraceArguments* args,
    unsigned int flags) {
  return base::trace_event::TraceLog::GetInstance()->AddTraceEventWithBindId(
      phase, category_group_enabled, name, scope, id, bind_id, args, flags);
}

base::trace_event::TraceEventHandle AddTraceEventWithProcessId(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    int process_id,
    base::trace_event::TraceArguments* args,
    unsigned int flags) {
  return base::trace_event::TraceLog::GetInstance()->AddTraceEventWithProcessId(
      phase, category_group_enabled, name, scope, id, process_id, args, flags);
}

base::trace_event::TraceEventHandle AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    int thread_id,
    const base::TimeTicks& timestamp,
    base::trace_event::TraceArguments* args,
    unsigned int flags) {
  return base::trace_event::TraceLog::GetInstance()
      ->AddTraceEventWithThreadIdAndTimestamp(phase, category_group_enabled,
                                              name, scope, id, thread_id,
                                              timestamp, args, flags);
}

base::trace_event::TraceEventHandle AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned long long bind_id,
    int thread_id,
    const base::TimeTicks& timestamp,
    base::trace_event::TraceArguments* args,
    unsigned int flags) {
  return base::trace_event::TraceLog::GetInstance()
      ->AddTraceEventWithThreadIdAndTimestamp(
          phase, category_group_enabled, name, scope, id, bind_id, thread_id,
          timestamp, args, flags);
}

base::trace_event::TraceEventHandle AddTraceEventWithThreadIdAndTimestamps(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    int thread_id,
    const base::TimeTicks& timestamp,
    const base::ThreadTicks& thread_timestamp,
    unsigned int flags) {
  return base::trace_event::TraceLog::GetInstance()
      ->AddTraceEventWithThreadIdAndTimestamps(
          phase, category_group_enabled, name, scope, id,
          /*bind_id=*/trace_event_internal::kNoId, thread_id, timestamp,
          thread_timestamp, nullptr, flags);
}

void AddMetadataEvent(const unsigned char* category_group_enabled,
                      const char* name,
                      base::trace_event::TraceArguments* args,
                      unsigned int flags) {
  return base::trace_event::TraceLog::GetInstance()->AddMetadataEvent(
      category_group_enabled, name, args, flags);
}

int GetNumTracesRecorded() {
  return base::trace_event::TraceLog::GetInstance()->GetNumTracesRecorded();
}

void UpdateTraceEventDuration(const unsigned char* category_group_enabled,
                              const char* name,
                              base::trace_event::TraceEventHandle handle) {
  return base::trace_event::TraceLog::GetInstance()->UpdateTraceEventDuration(
      category_group_enabled, name, handle);
}

void UpdateTraceEventDurationExplicit(
    const unsigned char* category_group_enabled,
    const char* name,
    base::trace_event::TraceEventHandle handle,
    int thread_id,
    bool explicit_timestamps,
    const base::TimeTicks& now,
    const base::ThreadTicks& thread_now,
    base::trace_event::ThreadInstructionCount thread_instruction_now) {
  return base::trace_event::TraceLog::GetInstance()
      ->UpdateTraceEventDurationExplicit(category_group_enabled, name, handle,
                                         thread_id, explicit_timestamps, now,
                                         thread_now, thread_instruction_now);
}

#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
ScopedTraceBinaryEfficient::ScopedTraceBinaryEfficient(
    const char* category_group,
    const char* name) {
  // The single atom works because for now the category_group can only be "gpu".
  DCHECK_EQ(strcmp(category_group, "gpu"), 0);
  static TRACE_EVENT_API_ATOMIC_WORD atomic = 0;
  INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO_CUSTOM_VARIABLES(
      category_group, atomic, category_group_enabled_);
  name_ = name;
  if (*category_group_enabled_) {
    event_handle_ =
        TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
            TRACE_EVENT_PHASE_COMPLETE, category_group_enabled_, name,
            trace_event_internal::kGlobalScope,                   // scope
            trace_event_internal::kNoId,                          // id
            static_cast<int>(base::PlatformThread::CurrentId()),  // thread_id
            TRACE_TIME_TICKS_NOW(), nullptr, TRACE_EVENT_FLAG_NONE);
  }
}

ScopedTraceBinaryEfficient::~ScopedTraceBinaryEfficient() {
  if (*category_group_enabled_) {
    TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION(category_group_enabled_, name_,
                                                event_handle_);
  }
}
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

}  // namespace trace_event_internal
