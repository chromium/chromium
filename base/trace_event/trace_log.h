// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TRACE_LOG_H_
#define BASE_TRACE_EVENT_TRACE_LOG_H_

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/base_export.h"
#include "base/containers/stack.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/time/time_override.h"
#include "base/trace_event/category_registry.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_event_impl.h"
#include "build/build_config.h"

#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

namespace perfetto {
namespace trace_processor {
class TraceProcessorStorage;
}  // namespace trace_processor
}  // namespace perfetto

namespace base {
class RefCountedString;

namespace trace_event {

struct TraceCategory;
class TraceBuffer;
class TraceBufferChunk;
class TraceEvent;
class TraceEventMemoryOverhead;
class JsonStringOutputWriter;

struct BASE_EXPORT TraceLogStatus {
  TraceLogStatus();
  ~TraceLogStatus();
  uint32_t event_capacity;
  uint32_t event_count;
};

class BASE_EXPORT TraceLog :
    public perfetto::TrackEventSessionObserver,
    public MemoryDumpProvider {
 public:
  class ThreadLocalEventBuffer;

  // Argument passed to TraceLog::SetEnabled.
  enum Mode : uint8_t {
    // Enables normal tracing (recording trace events in the trace buffer).
    // This is the only tracing mode supported now.
    // TODO(khokhlov): Clean up all uses of tracing mode and remove this enum
    // completely.
    RECORDING_MODE = 1 << 0,
  };

  static TraceLog* GetInstance();

  TraceLog(const TraceLog&) = delete;
  TraceLog& operator=(const TraceLog&) = delete;

  // Retrieves a copy (for thread-safety) of the current TraceConfig.
  TraceConfig GetCurrentTraceConfig() const;

  // Initializes the thread-local event buffer, if not already initialized and
  // if the current thread supports that (has a message loop).
  void InitializeThreadLocalEventBufferIfSupported();

  // See TraceConfig comments for details on how to control which categories
  // will be traced. Only RECORDING_MODE is supported.
  void SetEnabled(const TraceConfig& trace_config, uint8_t modes_to_enable);

  // Enable tracing using a customized Perfetto trace config. This allows, for
  // example, enabling additional data sources and enabling protobuf output
  // instead of the legacy JSON trace format.
  void SetEnabled(const TraceConfig& trace_config,
                  const perfetto::TraceConfig& perfetto_config);

  // Disables tracing for all categories. Only RECORDING_MODE is supported.
  void SetDisabled();
  void SetDisabled(uint8_t modes_to_disable);

  // Returns true if TraceLog is enabled on recording mode.
  // Note: Returns false even if FILTERING_MODE is enabled.
  bool IsEnabled() {
    // In SDK build we return true as soon as the datasource has been set up and
    // we know the config. This doesn't necessarily mean that the tracing has
    // already started.
    // Note that TrackEvent::IsEnabled() can be true even earlier, before the
    // OnSetup call, so we can't guarantee that we know the config by the time
    // TrackEvent::IsEnabled() is true.
    AutoLock lock(track_event_lock_);
    return track_event_sessions_.size() > 0;
  }

  // The number of times we have begun recording traces. If tracing is off,
  // returns -1. If tracing is on, then it returns the number of times we have
  // recorded a trace. By watching for this number to increment, you can
  // passively discover when a new trace has begun. This is then used to
  // implement the TRACE_EVENT_IS_NEW_TRACE() primitive.
  int GetNumTracesRecorded();

  // Enabled state listeners give a callback when tracing is enabled or
  // disabled. This can be used to tie into other library's tracing systems
  // on-demand.
  class BASE_EXPORT EnabledStateObserver {
   public:
    virtual ~EnabledStateObserver() = default;

    // Called just after the tracing system becomes enabled, outside of the
    // |lock_|. TraceLog::IsEnabled() is true at this point.
    virtual void OnTraceLogEnabled() = 0;

    // Called just after the tracing system disables, outside of the |lock_|.
    // TraceLog::IsEnabled() is false at this point.
    virtual void OnTraceLogDisabled() = 0;
  };
  // Adds an observer. Cannot be called from within the observer callback.
  void AddEnabledStateObserver(EnabledStateObserver* listener);
  // Removes an observer. Cannot be called from within the observer callback.
  void RemoveEnabledStateObserver(EnabledStateObserver* listener);
  // Adds an observer that is owned by TraceLog. This is useful for agents that
  // implement tracing feature that needs to stay alive as long as TraceLog
  // does.
  void AddOwnedEnabledStateObserver(
      std::unique_ptr<EnabledStateObserver> listener);
  bool HasEnabledStateObserver(EnabledStateObserver* listener) const;

  // Asynchronous enabled state listeners. When tracing is enabled or disabled,
  // for each observer, a task for invoking its appropriate callback is posted
  // to the `SequencedTaskRunner` from which AddAsyncEnabledStateObserver() was
  // called. This allows the observer to be safely destroyed, provided that it
  // happens on the same `SequencedTaskRunner` that invoked
  // AddAsyncEnabledStateObserver().
  class BASE_EXPORT AsyncEnabledStateObserver {
   public:
    virtual ~AsyncEnabledStateObserver() = default;

    // Posted just after the tracing system becomes enabled, outside |lock_|.
    // TraceLog::IsEnabled() is true at this point.
    virtual void OnTraceLogEnabled() = 0;

    // Posted just after the tracing system becomes disabled, outside |lock_|.
    // TraceLog::IsEnabled() is false at this point.
    virtual void OnTraceLogDisabled() = 0;
  };
  // TODO(oysteine): This API originally needed to use WeakPtrs as the observer
  // list was copied under the global trace lock, but iterated over outside of
  // that lock so that observers could add tracing. The list is now protected by
  // its own lock, so this can be changed to a raw ptr.
  void AddAsyncEnabledStateObserver(
      WeakPtr<AsyncEnabledStateObserver> listener);
  void RemoveAsyncEnabledStateObserver(AsyncEnabledStateObserver* listener);
  bool HasAsyncEnabledStateObserver(AsyncEnabledStateObserver* listener) const;

  // Observers that are notified when incremental state is cleared. This only
  // happens when tracing using the perfetto backend.
  class BASE_EXPORT IncrementalStateObserver {
   public:
    virtual ~IncrementalStateObserver() = default;

    // Called just after the tracing system has cleared incremental state, while
    // a tracing session is active.
    virtual void OnIncrementalStateCleared() = 0;
  };
  // Adds an observer. Cannot be called from within the observer callback.
  void AddIncrementalStateObserver(IncrementalStateObserver* listener);
  // Removes an observer. Cannot be called from within the observer callback.
  void RemoveIncrementalStateObserver(IncrementalStateObserver* listener);

  TraceLogStatus GetStatus() const;

  // Computes an estimate of the size of the TraceLog including all the retained
  // objects.
  void EstimateTraceMemoryOverhead(TraceEventMemoryOverhead* overhead);

  void SetArgumentFilterPredicate(
      const ArgumentFilterPredicate& argument_filter_predicate);
  ArgumentFilterPredicate GetArgumentFilterPredicate() const;

  void SetMetadataFilterPredicate(
      const MetadataFilterPredicate& metadata_filter_predicate);
  MetadataFilterPredicate GetMetadataFilterPredicate() const;

  void SetRecordHostAppPackageName(bool record_host_app_package_name);
  bool ShouldRecordHostAppPackageName() const;

  // Flush all collected events to the given output callback. The callback will
  // be called one or more times either synchronously or asynchronously from
  // the current thread with IPC-bite-size chunks. The string format is
  // undefined. Use TraceResultBuffer to convert one or more trace strings to
  // JSON. The callback can be null if the caller doesn't want any data.
  // Due to the implementation of thread-local buffers, flush can't be
  // done when tracing is enabled. If called when tracing is enabled, the
  // callback will be called directly with (empty_string, false) to indicate
  // the end of this unsuccessful flush. Flush does the serialization
  // on the same thread if the caller doesn't set use_worker_thread explicitly.
  using OutputCallback =
      base::RepeatingCallback<void(const scoped_refptr<base::RefCountedString>&,
                                   bool has_more_events)>;
  void Flush(const OutputCallback& cb, bool use_worker_thread = false);

  // Cancels tracing and discards collected data.
  void CancelTracing(const OutputCallback& cb);

  using AddTraceEventOverrideFunction = void (*)(TraceEvent*,
                                                 bool thread_will_flush,
                                                 TraceEventHandle* handle);
  using OnFlushFunction = void (*)();
  using UpdateDurationFunction =
      void (*)(const unsigned char* category_group_enabled,
               const char* name,
               TraceEventHandle handle,
               PlatformThreadId thread_id,
               bool explicit_timestamps,
               const TimeTicks& now,
               const ThreadTicks& thread_now);
  // The callbacks will be called up until the point where the flush is
  // finished, i.e. must be callable until OutputCallback is called with
  // has_more_events==false.
  void SetAddTraceEventOverrides(
      const AddTraceEventOverrideFunction& add_event_override,
      const OnFlushFunction& on_flush_callback,
      const UpdateDurationFunction& update_duration_callback);

  // Called by TRACE_EVENT* macros, don't call this directly.
  // The name parameter is a category group for example:
  // TRACE_EVENT0("renderer,webkit", "WebViewImpl::HandleInputEvent")
  static const unsigned char* GetCategoryGroupEnabled(const char* name);
  static const char* GetCategoryGroupName(
      const unsigned char* category_group_enabled);
  static constexpr const unsigned char* GetBuiltinCategoryEnabled(
      const char* name) {
    TraceCategory* builtin_category =
        CategoryRegistry::GetBuiltinCategoryByName(name);
    if (builtin_category)
      return builtin_category->state_ptr();
    return nullptr;
  }

  // Called by TRACE_EVENT* macros, don't call this directly.
  // If |copy| is set, |name|, |arg_name1| and |arg_name2| will be deep copied
  // into the event; see "Memory scoping note" and TRACE_EVENT_COPY_XXX above.
  bool ShouldAddAfterUpdatingState(char phase,
                                   const unsigned char* category_group_enabled,
                                   const char* name,
                                   uint64_t id,
                                   PlatformThreadId thread_id,
                                   const TimeTicks timestamp,
                                   TraceArguments* args);
  TraceEventHandle AddTraceEvent(char phase,
                                 const unsigned char* category_group_enabled,
                                 const char* name,
                                 const char* scope,
                                 uint64_t id,
                                 TraceArguments* args,
                                 unsigned int flags);
  TraceEventHandle AddTraceEventWithBindId(
      char phase,
      const unsigned char* category_group_enabled,
      const char* name,
      const char* scope,
      uint64_t id,
      uint64_t bind_id,
      TraceArguments* args,
      unsigned int flags);
  TraceEventHandle AddTraceEventWithProcessId(
      char phase,
      const unsigned char* category_group_enabled,
      const char* name,
      const char* scope,
      uint64_t id,
      ProcessId process_id,
      TraceArguments* args,
      unsigned int flags);
  TraceEventHandle AddTraceEventWithThreadIdAndTimestamp(
      char phase,
      const unsigned char* category_group_enabled,
      const char* name,
      const char* scope,
      uint64_t id,
      PlatformThreadId thread_id,
      const TimeTicks& timestamp,
      TraceArguments* args,
      unsigned int flags);
  TraceEventHandle AddTraceEventWithThreadIdAndTimestamp(
      char phase,
      const unsigned char* category_group_enabled,
      const char* name,
      const char* scope,
      uint64_t id,
      uint64_t bind_id,
      PlatformThreadId thread_id,
      const TimeTicks& timestamp,
      TraceArguments* args,
      unsigned int flags);
  TraceEventHandle AddTraceEventWithThreadIdAndTimestamps(
      char phase,
      const unsigned char* category_group_enabled,
      const char* name,
      const char* scope,
      uint64_t id,
      uint64_t bind_id,
      PlatformThreadId thread_id,
      const TimeTicks& timestamp,
      const ThreadTicks& thread_timestamp,
      TraceArguments* args,
      unsigned int flags);

  // Adds a metadata event that will be written when the trace log is flushed.
  void AddMetadataEvent(const unsigned char* category_group_enabled,
                        const char* name,
                        TraceArguments* args,
                        unsigned int flags);

  void UpdateTraceEventDuration(const unsigned char* category_group_enabled,
                                const char* name,
                                TraceEventHandle handle);

  void UpdateTraceEventDurationExplicit(
      const unsigned char* category_group_enabled,
      const char* name,
      TraceEventHandle handle,
      PlatformThreadId thread_id,
      bool explicit_timestamps,
      const TimeTicks& now,
      const ThreadTicks& thread_now);

  ProcessId process_id() const { return process_id_; }

  std::unordered_map<int, std::string> process_labels() const {
    AutoLock lock(lock_);
    return process_labels_;
  }

  uint64_t MangleEventId(uint64_t id);

  // Exposed for unittesting:
  // Allows clearing up our singleton instance.
  static void ResetForTesting();

  // Allow tests to inspect TraceEvents.
  TraceEvent* GetEventByHandle(TraceEventHandle handle);

  void SetProcessID(ProcessId process_id);

  // Process sort indices, if set, override the order of a process will appear
  // relative to other processes in the trace viewer. Processes are sorted first
  // on their sort index, ascending, then by their name, and then tid.
  void SetProcessSortIndex(int sort_index);

  // Helper function to set process_name in base::CurrentProcess.
  void OnSetProcessName(const std::string& process_name);

  // Processes can have labels in addition to their names. Use labels, for
  // instance, to list out the web page titles that a process is handling.
  int GetNewProcessLabelId();
  void UpdateProcessLabel(int label_id, const std::string& current_label);
  void RemoveProcessLabel(int label_id);

  // Thread sort indices, if set, override the order of a thread will appear
  // within its process in the trace viewer. Threads are sorted first on their
  // sort index, ascending, then by their name, and then tid.
  void SetThreadSortIndex(PlatformThreadId thread_id, int sort_index);

  size_t GetObserverCountForTest() const;

  // Call this method if the current thread may block the message loop to
  // prevent the thread from using the thread-local buffer because the thread
  // may not handle the flush request in time causing lost of unflushed events.
  void SetCurrentThreadBlocksMessageLoop();

#if BUILDFLAG(IS_WIN)
  // This function is called by the ETW exporting module whenever the ETW
  // keyword (flags) changes. This keyword indicates which categories should be
  // exported, so whenever it changes, we adjust accordingly.
  void UpdateETWCategoryGroupEnabledFlags();
#endif

  // Replaces |logged_events_| with a new TraceBuffer for testing.
  void SetTraceBufferForTesting(std::unique_ptr<TraceBuffer> trace_buffer);

  struct TrackEventSession {
    uint32_t internal_instance_index;
    perfetto::DataSourceConfig config;
    perfetto::BackendType backend_type = perfetto::kUnspecifiedBackend;
  };
  std::vector<TrackEventSession> GetTrackEventSessions() const;

  // DEPRECATED. In the presence of multiple simultaneous sessions, this method
  // returns only the first session's config. When no tracing sessions are
  // active, returns an empty config for compatibility with legacy code.
  // TODO(khokhlov): Remove this method and migrate all its uses to
  // GetTrackEventSessions().
  perfetto::DataSourceConfig GetCurrentTrackEventDataSourceConfig() const;
  void InitializePerfettoIfNeeded();
  bool IsPerfettoInitializedByTraceLog() const;
  void SetEnabledImpl(const TraceConfig& trace_config,
                      const perfetto::TraceConfig& perfetto_config);

  // perfetto::TrackEventSessionObserver implementation.
  void OnSetup(const perfetto::DataSourceBase::SetupArgs&) override;
  void OnStart(const perfetto::DataSourceBase::StartArgs&) override;
  void OnStop(const perfetto::DataSourceBase::StopArgs&) override;

  // Called by the perfetto backend just after incremental state was cleared.
  void OnIncrementalStateCleared();

 private:
  typedef unsigned int InternalTraceOptions;

  FRIEND_TEST_ALL_PREFIXES(TraceEventTestFixture,
                           TraceBufferRingBufferGetReturnChunk);
  FRIEND_TEST_ALL_PREFIXES(TraceEventTestFixture,
                           TraceBufferRingBufferHalfIteration);
  FRIEND_TEST_ALL_PREFIXES(TraceEventTestFixture,
                           TraceBufferRingBufferFullIteration);
  FRIEND_TEST_ALL_PREFIXES(TraceEventTestFixture, TraceBufferVectorReportFull);
  FRIEND_TEST_ALL_PREFIXES(TraceEventTestFixture,
                           ConvertTraceConfigToInternalOptions);
  FRIEND_TEST_ALL_PREFIXES(TraceEventTestFixture,
                           TraceRecordAsMuchAsPossibleMode);
  FRIEND_TEST_ALL_PREFIXES(TraceEventTestFixture, ConfigTraceBufferLimit);

  friend class base::NoDestructor<TraceLog>;

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(const MemoryDumpArgs& args,
                    ProcessMemoryDump* pmd) override;

  // Enable/disable each category group based on the current mode_,
  // category_filter_ and event_filters_enabled_.
  // Enable the category group in the recording mode if category_filter_ matches
  // the category group, is not null.
  void UpdateCategoryRegistry();
  void UpdateCategoryState(TraceCategory* category);

  InternalTraceOptions GetInternalOptionsFromTraceConfig(
      const TraceConfig& config);

  class OptionalAutoLock;
  struct RegisteredAsyncObserver;

  explicit TraceLog(int generation);
  ~TraceLog() override;
  void AddMetadataEventsWhileLocked() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  template <typename T>
  void AddMetadataEventWhileLocked(PlatformThreadId thread_id,
                                   const char* metadata_name,
                                   const char* arg_name,
                                   const T& value)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  InternalTraceOptions trace_options() const {
    return trace_options_.load(std::memory_order_relaxed);
  }

  TraceBuffer* trace_buffer() const { return logged_events_.get(); }
  TraceBuffer* CreateTraceBuffer();

  std::string EventToConsoleMessage(char phase,
                                    const TimeTicks& timestamp,
                                    TraceEvent* trace_event);

  TraceEvent* AddEventToThreadSharedChunkWhileLocked(TraceEventHandle* handle,
                                                     bool check_buffer_is_full)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void CheckIfBufferIsFullWhileLocked() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void SetDisabledWhileLocked(uint8_t modes) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  TraceEvent* GetEventByHandleInternal(TraceEventHandle handle,
                                       OptionalAutoLock* lock);

  void FlushInternal(const OutputCallback& cb,
                     bool use_worker_thread,
                     bool discard_events);

  void OnTraceData(const char* data, size_t size, bool has_more);

  // |generation| is used in the following callbacks to check if the callback
  // is called for the flush of the current |logged_events_|.
  void FlushCurrentThread(int generation, bool discard_events);
  // Usually it runs on a different thread.
  static void ConvertTraceEventsToTraceFormat(
      std::unique_ptr<TraceBuffer> logged_events,
      const TraceLog::OutputCallback& flush_output_callback,
      const ArgumentFilterPredicate& argument_filter_predicate);
  void FinishFlush(int generation, bool discard_events);
  void OnFlushTimeout(int generation, bool discard_events);

  int generation() const {
    return generation_.load(std::memory_order_relaxed);
  }
  bool CheckGeneration(int generation) const {
    return generation == this->generation();
  }
  void UseNextTraceBuffer();

  TimeTicks OffsetNow() const {
    // This should be TRACE_TIME_TICKS_NOW but include order makes that hard.
    return OffsetTimestamp(base::subtle::TimeTicksNowIgnoringOverride());
  }
  TimeTicks OffsetTimestamp(const TimeTicks& timestamp) const {
    return timestamp - time_offset_;
  }

  // Internal representation of trace options since we store the currently used
  // trace option as an AtomicWord.
  static const InternalTraceOptions kInternalNone;
  static const InternalTraceOptions kInternalRecordUntilFull;
  static const InternalTraceOptions kInternalRecordContinuously;
  static const InternalTraceOptions kInternalEchoToConsole;
  static const InternalTraceOptions kInternalRecordAsMuchAsPossible;
  static const InternalTraceOptions kInternalEnableArgumentFilter;

  // This lock protects TraceLog member accesses (except for members protected
  // by thread_info_lock_) from arbitrary threads.
  mutable Lock lock_;
  Lock thread_info_lock_;
  bool enabled_{false};
  int num_traces_recorded_{0};
  std::unique_ptr<TraceBuffer> logged_events_;
  std::vector<std::unique_ptr<TraceEvent>> metadata_events_;

  // The lock protects observers access.
  mutable Lock observers_lock_;
  bool dispatching_to_observers_ = false;
  std::vector<raw_ptr<EnabledStateObserver, VectorExperimental>>
      enabled_state_observers_ GUARDED_BY(observers_lock_);
  std::map<AsyncEnabledStateObserver*, RegisteredAsyncObserver> async_observers_
      GUARDED_BY(observers_lock_);
  // Manages ownership of the owned observers. The owned observers will also be
  // added to |enabled_state_observers_|.
  std::vector<std::unique_ptr<EnabledStateObserver>>
      owned_enabled_state_observer_copy_ GUARDED_BY(observers_lock_);
  std::vector<raw_ptr<IncrementalStateObserver, VectorExperimental>>
      incremental_state_observers_ GUARDED_BY(observers_lock_);

  int next_process_label_id_ GUARDED_BY(lock_) = 0;
  std::unordered_map<int, std::string> process_labels_;
  int process_sort_index_;
  std::unordered_map<PlatformThreadId, int> thread_sort_indices_;
  std::unordered_map<PlatformThreadId, std::string> thread_names_
      GUARDED_BY(thread_info_lock_);

  // The following two maps are used only when ECHO_TO_CONSOLE.
  std::unordered_map<PlatformThreadId, base::stack<TimeTicks>>
      thread_event_start_times_ GUARDED_BY(thread_info_lock_);
  std::unordered_map<std::string, size_t> thread_colors_
      GUARDED_BY(thread_info_lock_);

  TimeTicks buffer_limit_reached_timestamp_;

  // XORed with TraceID to make it unlikely to collide with other processes.
  uint64_t process_id_hash_;

  ProcessId process_id_;

  TimeDelta time_offset_;

  std::atomic<InternalTraceOptions> trace_options_;

  TraceConfig trace_config_;

  // Contains task runners for the threads that have had at least one event
  // added into the local event buffer.
  std::unordered_map<PlatformThreadId, scoped_refptr<SingleThreadTaskRunner>>
      thread_task_runners_;

  // For events which can't be added into the thread local buffer, e.g. events
  // from threads without a message loop.
  std::unique_ptr<TraceBufferChunk> thread_shared_chunk_;
  size_t thread_shared_chunk_index_;

  // Set when asynchronous Flush is in progress.
  OutputCallback flush_output_callback_;
  scoped_refptr<SequencedTaskRunner> flush_task_runner_;
  ArgumentFilterPredicate argument_filter_predicate_;
  MetadataFilterPredicate metadata_filter_predicate_;
  bool record_host_app_package_name_{false};
  std::atomic<int> generation_;
  bool use_worker_thread_;
  std::atomic<AddTraceEventOverrideFunction> add_trace_event_override_{nullptr};
  std::atomic<OnFlushFunction> on_flush_override_{nullptr};
  std::atomic<UpdateDurationFunction> update_duration_override_{nullptr};

  std::unique_ptr<perfetto::TracingSession> tracing_session_;
  perfetto::TraceConfig perfetto_config_;
  std::vector<TrackEventSession> track_event_sessions_
      GUARDED_BY(track_event_lock_);
  int active_track_event_sessions_ = 0;
  mutable Lock track_event_lock_;
#if BUILDFLAG(USE_PERFETTO_TRACE_PROCESSOR)
  std::unique_ptr<perfetto::trace_processor::TraceProcessorStorage>
      trace_processor_;
  std::unique_ptr<JsonStringOutputWriter> json_output_writer_;
  OutputCallback proto_output_callback_;
#endif  // BUILDFLAG(USE_PERFETTO_TRACE_PROCESSOR)

#if BUILDFLAG(IS_ANDROID)
  std::optional<TraceConfig> atrace_startup_config_;
#endif
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_TRACE_LOG_H_
