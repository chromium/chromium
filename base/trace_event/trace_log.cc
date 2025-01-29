// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/trace_event/trace_log.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/debug/leak_annotations.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/stack_allocated.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "base/process/process_metrics.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/perfetto_proto_appender.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/perfetto/include/perfetto/ext/trace_processor/export_json.h"  // nogncheck
#include "third_party/perfetto/include/perfetto/trace_processor/trace_processor_storage.h"  // nogncheck
#include "third_party/perfetto/include/perfetto/tracing/console_interceptor.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/chrome_config.gen.h"  // nogncheck
#include "third_party/perfetto/protos/perfetto/config/interceptor_config.gen.h"  // nogncheck
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.gen.h"  // nogncheck
#include "third_party/perfetto/protos/perfetto/trace/track_event/thread_descriptor.gen.h"  // nogncheck

#if BUILDFLAG(IS_ANDROID)
#include "base/debug/elf_reader.h"

// The linker assigns the virtual address of the start of current library to
// this symbol.
extern char __executable_start;
#endif

namespace base::trace_event {

namespace {

bool g_perfetto_initialized_by_tracelog = false;

TraceLog* g_trace_log_for_testing = nullptr;

ThreadTicks ThreadNow() {
  return ThreadTicks::IsSupported()
             ? base::subtle::ThreadTicksNowIgnoringOverride()
             : ThreadTicks();
}

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
    }
  }
}

// TRACE_EVENT macros will bypass TraceLog entirely. However, trace event
// embedders which haven't been ported to Perfetto yet will still be using
// TRACE_EVENT_API_ADD_TRACE_EVENT, so we need to route these events to Perfetto
// using an override here.
// TODO(crbug.com/343404899): Remove when all embedders migrate to Perfetto.
void OnAddLegacyTraceEvent(TraceEvent* trace_event) {
  perfetto::DynamicCategory category(
      TraceLog::GetInstance()->GetCategoryGroupName(
          trace_event->category_group_enabled()));

  auto phase = trace_event->phase();
  if (phase == TRACE_EVENT_PHASE_COMPLETE) {
    phase = TRACE_EVENT_PHASE_BEGIN;
  }

  auto write_args = [trace_event, phase](perfetto::EventContext ctx) {
    WriteDebugAnnotations(trace_event, ctx.event());
    uint32_t id_flags = trace_event->flags() & (TRACE_EVENT_FLAG_HAS_ID |
                                                TRACE_EVENT_FLAG_HAS_LOCAL_ID |
                                                TRACE_EVENT_FLAG_HAS_GLOBAL_ID);
    if (!id_flags &&
        perfetto::internal::TrackEventLegacy::PhaseToType(phase) !=
            perfetto::protos::pbzero::TrackEvent::TYPE_UNSPECIFIED) {
      return;
    }
    auto* legacy_event = ctx.event()->set_legacy_event();
    legacy_event->set_phase(phase);
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

  auto flags = trace_event->flags();
  base::TimeTicks timestamp = trace_event->timestamp().is_null()
                                  ? TRACE_TIME_TICKS_NOW()
                                  : trace_event->timestamp();
  if (phase == TRACE_EVENT_PHASE_INSTANT) {
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
    PlatformThreadId thread_id,
    bool explicit_timestamps,
    const TimeTicks& now,
    const ThreadTicks& thread_now) {
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

base::trace_event::TraceEventHandle AddTraceEventWithThreadIdAndTimestamps(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    base::PlatformThreadId thread_id,
    const base::TimeTicks& timestamp,
    const base::ThreadTicks& thread_timestamp,
    base::trace_event::TraceArguments* args,
    unsigned int flags) {
  base::trace_event::TraceEventHandle handle = {};
  if (!*category_group_enabled) {
    return handle;
  }
  DCHECK(!timestamp.is_null());

  base::trace_event::TraceEvent new_trace_event(
      thread_id, timestamp, thread_timestamp, phase, category_group_enabled,
      name, scope, id, args, flags);

  base::trace_event::OnAddLegacyTraceEvent(&new_trace_event);
  return handle;
}

}  // namespace

#if BUILDFLAG(USE_PERFETTO_TRACE_PROCESSOR)
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
    buffer_->as_string().reserve(kBufferReserveCapacity);
  }

  ~JsonStringOutputWriter() override { Flush(/*has_more=*/false); }

  perfetto::trace_processor::util::Status AppendString(
      const std::string& string) override {
    if (!did_strip_prefix_) {
      DCHECK_EQ(string, kJsonPrefix);
      did_strip_prefix_ = true;
      return perfetto::trace_processor::util::OkStatus();
    } else if (buffer_->as_string().empty() &&
               !strncmp(string.c_str(), kJsonJoiner, strlen(kJsonJoiner))) {
      // We only remove the leading joiner comma for the first chunk in a buffer
      // since the consumer is expected to insert commas between the buffers we
      // provide.
      buffer_->as_string() += string.substr(strlen(kJsonJoiner));
    } else if (!strncmp(string.c_str(), kJsonSuffix, strlen(kJsonSuffix))) {
      return perfetto::trace_processor::util::OkStatus();
    } else {
      buffer_->as_string() += string;
    }
    if (buffer_->as_string().size() > kBufferLimitInBytes) {
      Flush(/*has_more=*/true);
      // Reset the buffer_ after moving it above.
      buffer_ = new RefCountedString();
      buffer_->as_string().reserve(kBufferReserveCapacity);
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
#endif  // BUILDFLAG(USE_PERFETTO_TRACE_PROCESSOR)

struct TraceLog::RegisteredAsyncObserver {
  explicit RegisteredAsyncObserver(WeakPtr<AsyncEnabledStateObserver> observer)
      : observer(observer),
        task_runner(SequencedTaskRunner::GetCurrentDefault()) {}
  ~RegisteredAsyncObserver() = default;

  WeakPtr<AsyncEnabledStateObserver> observer;
  scoped_refptr<SequencedTaskRunner> task_runner;
};

// static
TraceLog* TraceLog::GetInstance() {
  static base::NoDestructor<TraceLog> instance(0);
  return instance.get();
}

// static
void TraceLog::ResetForTesting() {
  auto* self = GetInstance();
  AutoLock lock(self->observers_lock_);
  self->enabled_state_observers_.clear();
  self->owned_enabled_state_observer_copy_.clear();
  self->async_observers_.clear();
  self->InitializePerfettoIfNeeded();
}

TraceLog::TraceLog(int generation) : process_id_(base::kNullProcessId) {
#if BUILDFLAG(IS_NACL)  // NaCl shouldn't expose the process id.
  SetProcessID(0);
#else
  SetProcessID(GetCurrentProcId());
#endif
  TrackEvent::AddSessionObserver(this);
  g_trace_log_for_testing = this;
}

TraceLog::~TraceLog() {
  TrackEvent::RemoveSessionObserver(this);
}

const unsigned char* TraceLog::GetCategoryGroupEnabled(
    const char* category_group) {
  return TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(category_group);
}

const char* TraceLog::GetCategoryGroupName(
    const unsigned char* category_group_enabled) {
  return TRACE_EVENT_API_GET_CATEGORY_GROUP_NAME(category_group_enabled);
}

void TraceLog::SetEnabled(const TraceConfig& trace_config) {
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

  DCHECK(!trace_config.IsArgumentFilterEnabled());

  // TODO(khokhlov): Avoid duplication between this code and
  // services/tracing/public/cpp/perfetto/perfetto_config.cc.
  perfetto::TraceConfig perfetto_config;
  size_t size_limit = trace_config.GetTraceBufferSizeInKb();
  if (size_limit == 0) {
    size_limit = 200 * 1024;
  }
  auto* buffer_config = perfetto_config.add_buffers();
  buffer_config->set_size_kb(checked_cast<uint32_t>(size_limit));
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
  auto* source_chrome_config = source_config->mutable_chrome_config();
  source_chrome_config->set_trace_config(trace_config.ToString());
  source_chrome_config->set_convert_to_legacy_json(true);

  if (trace_config.GetTraceRecordMode() == base::trace_event::ECHO_TO_CONSOLE) {
    perfetto::ConsoleInterceptor::Register();
    source_config->mutable_interceptor_config()->set_name("console");
  }

  source_config->set_track_event_config_raw(
      trace_config.ToPerfettoTrackEventConfigRaw(
          /*privacy_filtering_enabled = */ false));

  if (trace_config.IsCategoryGroupEnabled("disabled-by-default-memory-infra")) {
    data_source = perfetto_config.add_data_sources();
    source_config = data_source->mutable_config();
    source_config->set_name("org.chromium.memory_instrumentation");
    source_config->set_target_buffer(0);
    source_chrome_config = source_config->mutable_chrome_config();
    source_chrome_config->set_trace_config(trace_config.ToString());
    source_chrome_config->set_convert_to_legacy_json(true);
  }

  // Clear incremental state every 0.5 seconds, so that we lose at most the
  // first 0.5 seconds of the trace (if we wrap around Perfetto's central
  // buffer).
  // This value strikes balance between minimizing interned data overhead, and
  // reducing the risk of data loss in ring buffer mode.
  perfetto_config.mutable_incremental_state_config()->set_clear_period_ms(500);

  SetEnabledImpl(trace_config, perfetto_config);
}

std::vector<TraceLog::TrackEventSession> TraceLog::GetTrackEventSessions()
    const {
  AutoLock lock(track_event_lock_);
  return track_event_sessions_;
}

perfetto::DataSourceConfig TraceLog::GetCurrentTrackEventDataSourceConfig()
    const {
  AutoLock lock(track_event_lock_);
  if (track_event_sessions_.empty()) {
    return perfetto::DataSourceConfig();
  }
  return track_event_sessions_[0].config;
}

void TraceLog::InitializePerfettoIfNeeded() {
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

  if (perfetto::Tracing::IsInitialized()) {
    return;
  }
  g_perfetto_initialized_by_tracelog = true;
  perfetto::TracingInitArgs init_args;
  init_args.backends = perfetto::BackendType::kInProcessBackend;
  init_args.shmem_batch_commits_duration_ms = 1000;
  init_args.shmem_size_hint_kb = 4 * 1024;
  init_args.shmem_direct_patching_enabled = true;
  init_args.disallow_merging_with_system_tracks = true;
  perfetto::Tracing::Initialize(init_args);
  TrackEvent::Register();
}

bool TraceLog::IsPerfettoInitializedByTraceLog() const {
  return g_perfetto_initialized_by_tracelog;
}

void TraceLog::SetEnabled(const TraceConfig& trace_config,
                          const perfetto::TraceConfig& perfetto_config) {
  AutoLock lock(lock_);
  SetEnabledImpl(trace_config, perfetto_config);
}

void TraceLog::SetEnabledImpl(const TraceConfig& trace_config,
                              const perfetto::TraceConfig& perfetto_config) {
  DCHECK(!TrackEvent::IsEnabled());
  lock_.AssertAcquired();
  InitializePerfettoIfNeeded();
  perfetto_config_ = perfetto_config;
  tracing_session_ = perfetto::Tracing::NewTrace();

  AutoUnlock unlock(lock_);
  tracing_session_->Setup(perfetto_config);
  tracing_session_->StartBlocking();
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

TraceConfig TraceLog::GetCurrentTraceConfig() const {
  const auto chrome_config =
      GetCurrentTrackEventDataSourceConfig().chrome_config();
  return TraceConfig(chrome_config.trace_config());
}

void TraceLog::SetDisabled() {
  AutoLock lock(lock_);
  SetDisabledWhileLocked();
}

void TraceLog::SetDisabledWhileLocked() {
  if (!tracing_session_) {
    return;
  }

  TrackEvent::Flush();
  // If the current thread has an active task runner, allow nested tasks to run
  // while stopping the session. This is needed by some tests, e.g., to allow
  // data sources to properly flush themselves.
  if (SingleThreadTaskRunner::HasCurrentDefault()) {
    RunLoop stop_loop(RunLoop::Type::kNestableTasksAllowed);
    auto quit_closure = stop_loop.QuitClosure();
    tracing_session_->SetOnStopCallback(
        [&quit_closure] { quit_closure.Run(); });
    tracing_session_->Stop();
    AutoUnlock unlock(lock_);
    stop_loop.Run();
  } else {
    tracing_session_->StopBlocking();
  }
}

void TraceLog::AddEnabledStateObserver(EnabledStateObserver* listener) {
  AutoLock lock(observers_lock_);
  enabled_state_observers_.push_back(listener);
}

void TraceLog::RemoveEnabledStateObserver(EnabledStateObserver* listener) {
  AutoLock lock(observers_lock_);
  auto removed = std::ranges::remove(enabled_state_observers_, listener);
  enabled_state_observers_.erase(removed.begin(), removed.end());
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
#if BUILDFLAG(USE_PERFETTO_TRACE_PROCESSOR)
  TrackEvent::Flush();

  if (!tracing_session_ || discard_events) {
    tracing_session_.reset();
    scoped_refptr<RefCountedString> empty_result = new RefCountedString;
    cb.Run(empty_result, /*has_more_events=*/false);
    return;
  }

  bool convert_to_json = true;
  for (const auto& data_source : perfetto_config_.data_sources()) {
    if (data_source.config().has_chrome_config() &&
        data_source.config().chrome_config().has_convert_to_legacy_json()) {
      convert_to_json =
          data_source.config().chrome_config().convert_to_legacy_json();
      break;
    }
  }

  if (convert_to_json) {
    perfetto::trace_processor::Config processor_config;
    trace_processor_ =
        perfetto::trace_processor::TraceProcessorStorage::CreateInstance(
            processor_config);
    json_output_writer_ = std::make_unique<JsonStringOutputWriter>(
        use_worker_thread ? SingleThreadTaskRunner::GetCurrentDefault()
                          : nullptr,
        cb);
  } else {
    proto_output_callback_ = std::move(cb);
  }

  if (use_worker_thread) {
    tracing_session_->ReadTrace(
        [this](perfetto::TracingSession::ReadTraceCallbackArgs args) {
          OnTraceData(args.data, args.size, args.has_more);
        });
  } else {
    auto data = tracing_session_->ReadTraceBlocking();
    OnTraceData(data.data(), data.size(), /*has_more=*/false);
  }
#else
  // Trace processor isn't enabled so we can't convert the resulting trace into
  // JSON.
  NOTREACHED() << "JSON tracing isn't supported";
#endif  // BUILDFLAG(USE_PERFETTO_TRACE_PROCESSOR)
}

#if BUILDFLAG(USE_PERFETTO_TRACE_PROCESSOR)
void TraceLog::OnTraceData(const char* data, size_t size, bool has_more) {
  if (proto_output_callback_) {
    scoped_refptr<RefCountedString> chunk = new RefCountedString();
    if (size) {
      chunk->as_string().assign(data, size);
    }
    proto_output_callback_.Run(std::move(chunk), has_more);
    if (!has_more) {
      proto_output_callback_.Reset();
      tracing_session_.reset();
    }
    return;
  }
  if (size) {
    std::unique_ptr<uint8_t[]> data_copy(new uint8_t[size]);
    memcpy(&data_copy[0], data, size);
    auto status = trace_processor_->Parse(std::move(data_copy), size);
    DCHECK(status.ok()) << status.message();
  }
  if (has_more) {
    return;
  }

  auto status = trace_processor_->NotifyEndOfFile();
  DCHECK(status.ok()) << status.message();

  status = perfetto::trace_processor::json::ExportJson(
      trace_processor_.get(), json_output_writer_.get());
  DCHECK(status.ok()) << status.message();
  trace_processor_.reset();
  tracing_session_.reset();
  json_output_writer_.reset();
}
#endif  // BUILDFLAG(USE_PERFETTO_TRACE_PROCESSOR)

void TraceLog::SetProcessID(ProcessId process_id) {
  process_id_ = process_id;
}

int TraceLog::GetNewProcessLabelId() {
  AutoLock lock(lock_);
  return next_process_label_id_++;
}

void TraceLog::UpdateProcessLabel(int label_id,
                                  const std::string& current_label) {
  if (!current_label.length()) {
    return RemoveProcessLabel(label_id);
  }

  AutoLock lock(lock_);
  process_labels_[label_id] = current_label;
}

void TraceLog::RemoveProcessLabel(int label_id) {
  AutoLock lock(lock_);
  process_labels_.erase(label_id);
}

size_t TraceLog::GetObserverCountForTest() const {
  AutoLock lock(observers_lock_);
  return enabled_state_observers_.size();
}

void TraceLog::OnSetup(const perfetto::DataSourceBase::SetupArgs& args) {
  AutoLock lock(track_event_lock_);
  track_event_sessions_.emplace_back(args.internal_instance_index, *args.config,
                                     args.backend_type);
}

void TraceLog::OnStart(const perfetto::DataSourceBase::StartArgs&) {
  {
    AutoLock lock(track_event_lock_);
    ++active_track_event_sessions_;
    // Legacy observers don't support multiple tracing sessions. So we only
    // notify them about the first one.
    if (active_track_event_sessions_ > 1) {
      return;
    }
  }

  AutoLock lock(observers_lock_);
  for (EnabledStateObserver* observer : enabled_state_observers_) {
    observer->OnTraceLogEnabled();
  }
  for (const auto& it : async_observers_) {
    it.second.task_runner->PostTask(
        FROM_HERE, BindOnce(&AsyncEnabledStateObserver::OnTraceLogEnabled,
                            it.second.observer));
  }
}

void TraceLog::OnStop(const perfetto::DataSourceBase::StopArgs& args) {
  {
    // We can't use |lock_| because OnStop() can be called from within
    // SetDisabled(). We also can't use |observers_lock_|, because observers
    // below can call into IsEnabled(), which needs to access
    // |track_event_sessions_|. So we use a separate lock.
    AutoLock track_event_lock(track_event_lock_);
    std::erase_if(track_event_sessions_, [&args](
                                             const TrackEventSession& session) {
      return session.internal_instance_index == args.internal_instance_index;
    });
  }

  {
    AutoLock lock(track_event_lock_);
    --active_track_event_sessions_;
    // Legacy observers don't support multiple tracing sessions. So we only
    // notify them when the last one stopped.
    if (active_track_event_sessions_ > 0) {
      return;
    }
  }

  AutoLock lock(observers_lock_);
  for (base::trace_event::TraceLog::EnabledStateObserver* it :
       enabled_state_observers_) {
    it->OnTraceLogDisabled();
  }
  for (const auto& it : async_observers_) {
    it.second.task_runner->PostTask(
        FROM_HERE, BindOnce(&AsyncEnabledStateObserver::OnTraceLogDisabled,
                            it.second.observer));
  }
}

}  // namespace base::trace_event

namespace trace_event_internal {

base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    base::trace_event::TraceArguments* args,
    unsigned int flags) {
  auto thread_id = base::PlatformThread::CurrentId();
  base::TimeTicks now = TRACE_TIME_TICKS_NOW();
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id,
      trace_event_internal::kNoId,  // bind_id
      thread_id, now, args, flags);
}

base::trace_event::TraceEventHandle AddTraceEventWithProcessId(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    base::ProcessId process_id,
    base::trace_event::TraceArguments* args,
    unsigned int flags) {
  base::TimeTicks now = TRACE_TIME_TICKS_NOW();
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id,
      trace_event_internal::kNoId,  // bind_id
      static_cast<base::PlatformThreadId>(process_id), now, args,
      flags | TRACE_EVENT_FLAG_HAS_PROCESS_ID);
}

base::trace_event::TraceEventHandle AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    uint64_t bind_id,
    base::PlatformThreadId thread_id,
    const base::TimeTicks& timestamp,
    base::trace_event::TraceArguments* args,
    unsigned int flags) {
  base::ThreadTicks thread_now;
  // If timestamp is provided explicitly, don't record thread time as it would
  // be for the wrong timestamp. Similarly, if we record an event for another
  // process or thread, we shouldn't report the current thread's thread time.
  if (!(flags & TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP ||
        flags & TRACE_EVENT_FLAG_HAS_PROCESS_ID ||
        thread_id != base::PlatformThread::CurrentId())) {
    thread_now = base::trace_event::ThreadNow();
  }
  return base::trace_event::AddTraceEventWithThreadIdAndTimestamps(
      phase, category_group_enabled, name, scope, id, thread_id, timestamp,
      thread_now, args, flags);
}

base::trace_event::TraceEventHandle AddTraceEventWithThreadIdAndTimestamps(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    base::PlatformThreadId thread_id,
    const base::TimeTicks& timestamp,
    const base::ThreadTicks& thread_timestamp,
    unsigned int flags) {
  return base::trace_event::AddTraceEventWithThreadIdAndTimestamps(
      phase, category_group_enabled, name, scope, id, thread_id, timestamp,
      thread_timestamp, nullptr, flags);
}

void UpdateTraceEventDuration(const unsigned char* category_group_enabled,
                              const char* name,
                              base::trace_event::TraceEventHandle handle) {
  if (!*category_group_enabled) {
    return;
  }

  base::trace_event::OnUpdateLegacyTraceEventDuration(
      category_group_enabled, name, base::PlatformThread::CurrentId(),
      /*explicit_timestamps=*/false,
      base::subtle::TimeTicksNowIgnoringOverride(),
      base::trace_event::ThreadNow());
}

}  // namespace trace_event_internal
