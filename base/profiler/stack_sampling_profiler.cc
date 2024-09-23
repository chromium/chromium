// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampling_profiler.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/atomicops.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/profiler/profiler_buildflags.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_sampler.h"
#include "base/profiler/stack_unwind_data.h"
#include "base/profiler/unwinder.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/static_constants.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "base/mac/mac_util.h"
#endif

namespace base {

// Allows StackSamplingProfiler to recall a thread which should already pretty
// much be dead (thus it should be a fast Join()).
class ScopedAllowThreadRecallForStackSamplingProfiler
    : public ScopedAllowBaseSyncPrimitivesOutsideBlockingScope {};

namespace {

// This value is used to initialize the WaitableEvent object. This MUST BE set
// to MANUAL for correct operation of the IsSignaled() call in Start(). See the
// comment there for why.
constexpr WaitableEvent::ResetPolicy kResetPolicy =
    WaitableEvent::ResetPolicy::MANUAL;

// This value is used when there is no collection in progress and thus no ID
// for referencing the active collection to the SamplingThread.
const int kNullProfilerId = -1;

TimeTicks GetNextSampleTimeImpl(TimeTicks scheduled_current_sample_time,
                                TimeDelta sampling_interval,
                                TimeTicks now) {
  // Schedule the next sample at the next sampling_interval-aligned time in
  // the future that's sufficiently far enough from the current sample. In the
  // general case this will be one sampling_interval from the current
  // sample. In cases where sample tasks were unable to be executed, such as
  // during system suspend or bad system-wide jank, we may have missed some
  // samples. The right thing to do for those cases is to skip the missed
  // samples since the rest of the systems also wasn't executing.

  // Ensure that the next sample time is at least half a sampling interval
  // away. This causes the second sample after resume to be taken between 0.5
  // and 1.5 samples after the first, or 1 sample interval on average. The delay
  // also serves to provide a grace period in the normal sampling case where the
  // current sample may be taken slightly later than its scheduled time.
  const TimeTicks earliest_next_sample_time = now + sampling_interval / 2;

  const TimeDelta minimum_time_delta_to_next_sample =
      earliest_next_sample_time - scheduled_current_sample_time;

  // The minimum number of sampling intervals required to get from the scheduled
  // current sample time to the earliest next sample time.
  const int64_t required_sampling_intervals = static_cast<int64_t>(
      std::ceil(minimum_time_delta_to_next_sample / sampling_interval));
  return scheduled_current_sample_time +
         required_sampling_intervals * sampling_interval;
}

}  // namespace

// StackSamplingProfiler::SamplingThread --------------------------------------

class StackSamplingProfiler::SamplingThread : public Thread {
 public:
  class TestPeer {
   public:
    // Reset the existing sampler. This will unfortunately create the object
    // unnecessarily if it doesn't already exist but there's no way around that.
    static void Reset();

    // Disables inherent idle-shutdown behavior.
    static void DisableIdleShutdown();

    // Begins an idle shutdown as if the idle-timer had expired and wait for
    // it to execute. Since the timer would have only been started at a time
    // when the sampling thread actually was idle, this must be called only
    // when it is known that there are no active sampling threads. If
    // |simulate_intervening_add| is true then, when executed, the shutdown
    // task will believe that a new collection has been added since it was
    // posted.
    static void ShutdownAssumingIdle(bool simulate_intervening_add);

   private:
    // Calls the sampling threads ShutdownTask and then signals an event.
    static void ShutdownTaskAndSignalEvent(SamplingThread* sampler,
                                           int add_events,
                                           WaitableEvent* event);
  };

  struct CollectionContext {
    CollectionContext(PlatformThreadId thread_id,
                      const SamplingParams& params,
                      WaitableEvent* finished,
                      std::unique_ptr<StackSampler> sampler)
        : collection_id(next_collection_id.GetNext()),
          thread_id(thread_id),
          params(params),
          finished(finished),
          sampler(std::move(sampler)) {}
    ~CollectionContext() = default;

    // An identifier for this collection, used to uniquely identify the
    // collection to outside interests.
    const int collection_id;
    const PlatformThreadId thread_id;  // Thread id of the sampled thread.

    const SamplingParams params;    // Information about how to sample.
    const raw_ptr<WaitableEvent>
        finished;  // Signaled when all sampling complete.

    // Platform-specific module that does the actual sampling.
    std::unique_ptr<StackSampler> sampler;

    // The absolute time for the next sample.
    TimeTicks next_sample_time;

    // The time that a profile was started, for calculating the total duration.
    TimeTicks profile_start_time;

    // Counter that indicates the current sample position along the acquisition.
    int sample_count = 0;

    // Whether stop has been requested.
    bool stopping = false;

    // Sequence number for generating new collection ids.
    static AtomicSequenceNumber next_collection_id;
  };

  // Gets the single instance of this class.
  static SamplingThread* GetInstance();

  SamplingThread(const SamplingThread&) = delete;
  SamplingThread& operator=(const SamplingThread&) = delete;

  // Adds a new CollectionContext to the thread. This can be called externally
  // from any thread. This returns a collection id that can later be used to
  // stop the sampling.
  int Add(std::unique_ptr<CollectionContext> collection);

  // Adds an auxiliary unwinder to be used for the collection, to handle
  // additional, non-native-code unwind scenarios.
  void AddAuxUnwinder(int collection_id, std::unique_ptr<Unwinder> unwinder);

  // Applies the metadata to already recorded samples in all collections.
  void ApplyMetadataToPastSamples(base::TimeTicks period_start,
                                  base::TimeTicks period_end,
                                  uint64_t name_hash,
                                  std::optional<int64_t> key,
                                  int64_t value,
                                  std::optional<PlatformThreadId> thread_id);

  // Adds the metadata as profile metadata. Profile metadata stores metadata
  // global to the profile.
  void AddProfileMetadata(uint64_t name_hash,
                          std::optional<int64_t> key,
                          int64_t value,
                          std::optional<PlatformThreadId> thread_id);

  // Removes an active collection based on its collection id, forcing it to run
  // its callback if any data has been collected. This can be called externally
  // from any thread.
  void Remove(int collection_id);

 private:
  friend struct DefaultSingletonTraits<SamplingThread>;

  // The different states in which the sampling-thread can be.
  enum ThreadExecutionState {
    // The thread is not running because it has never been started. It will be
    // started when a sampling request is received.
    NOT_STARTED,

    // The thread is running and processing tasks. This is the state when any
    // sampling requests are active and during the "idle" period afterward
    // before the thread is stopped.
    RUNNING,

    // Once all sampling requests have finished and the "idle" period has
    // expired, the thread will be set to this state and its shutdown
    // initiated. A call to Stop() must be made to ensure the previous thread
    // has completely exited before calling Start() and moving back to the
    // RUNNING state.
    EXITING,
  };

  SamplingThread();
  ~SamplingThread() override;

  // Get task runner that is usable from the outside.
  scoped_refptr<SingleThreadTaskRunner> GetOrCreateTaskRunnerForAdd();
  scoped_refptr<SingleThreadTaskRunner> GetTaskRunner(
      ThreadExecutionState* out_state);

  // Get task runner that is usable from the sampling thread itself.
  scoped_refptr<SingleThreadTaskRunner> GetTaskRunnerOnSamplingThread();

  // Finishes a collection. The collection's |finished| waitable event will be
  // signalled. The |collection| should already have been removed from
  // |active_collections_| by the caller, as this is needed to avoid flakiness
  // in unit tests.
  void FinishCollection(std::unique_ptr<CollectionContext> collection);

  // Check if the sampling thread is idle and begin a shutdown if it is.
  void ScheduleShutdownIfIdle();

  // These methods are tasks that get posted to the internal message queue.
  void AddCollectionTask(std::unique_ptr<CollectionContext> collection);
  void AddAuxUnwinderTask(int collection_id,
                          std::unique_ptr<Unwinder> unwinder);
  void ApplyMetadataToPastSamplesTask(
      base::TimeTicks period_start,
      base::TimeTicks period_end,
      uint64_t name_hash,
      std::optional<int64_t> key,
      int64_t value,
      std::optional<PlatformThreadId> thread_id);
  void AddProfileMetadataTask(uint64_t name_hash,
                              std::optional<int64_t> key,
                              int64_t value,
                              std::optional<PlatformThreadId> thread_id);
  void RemoveCollectionTask(int collection_id);
  void ScheduleCollectionStop(int collection_id);
  void RecordSampleTask(int collection_id);
  void ShutdownTask(int add_events);

  // Thread:
  void CleanUp() override;

  // A stack-buffer used by the sampler for its work. This buffer is re-used
  // across multiple sampler objects since their execution is serialized on the
  // sampling thread.
  std::unique_ptr<StackBuffer> stack_buffer_;

  // A map of collection ids to collection contexts. Because this class is a
  // singleton that is never destroyed, context objects will never be destructed
  // except by explicit action. Thus, it's acceptable to pass unretained
  // pointers to these objects when posting tasks.
  std::map<int, std::unique_ptr<CollectionContext>> active_collections_;

  // State maintained about the current execution (or non-execution) of
  // the thread. This state must always be accessed while holding the
  // lock. A copy of the task-runner is maintained here for use by any
  // calling thread; this is necessary because Thread's accessor for it is
  // not itself thread-safe. The lock is also used to order calls to the
  // Thread API (Start, Stop, StopSoon, & DetachFromSequence) so that
  // multiple threads may make those calls.
  Lock thread_execution_state_lock_;  // Protects all thread_execution_state_*
  ThreadExecutionState thread_execution_state_
      GUARDED_BY(thread_execution_state_lock_) = NOT_STARTED;
  scoped_refptr<SingleThreadTaskRunner> thread_execution_state_task_runner_
      GUARDED_BY(thread_execution_state_lock_);
  bool thread_execution_state_disable_idle_shutdown_for_testing_
      GUARDED_BY(thread_execution_state_lock_) = false;

  // A counter that notes adds of new collection requests. It is incremented
  // when changes occur so that delayed shutdown tasks are able to detect if
  // something new has happened while it was waiting. Like all "execution_state"
  // vars, this must be accessed while holding |thread_execution_state_lock_|.
  int thread_execution_state_add_events_
      GUARDED_BY(thread_execution_state_lock_) = 0;
};

// static
void StackSamplingProfiler::SamplingThread::TestPeer::Reset() {
  SamplingThread* sampler = SamplingThread::GetInstance();

  ThreadExecutionState state;
  {
    AutoLock lock(sampler->thread_execution_state_lock_);
    state = sampler->thread_execution_state_;
    DCHECK(sampler->active_collections_.empty());
  }

  // Stop the thread and wait for it to exit. This has to be done through by
  // the thread itself because it has taken ownership of its own lifetime.
  if (state == RUNNING) {
    ShutdownAssumingIdle(false);
    state = EXITING;
  }
  // Make sure thread is cleaned up since state will be reset to NOT_STARTED.
  if (state == EXITING)
    sampler->Stop();

  // Reset internal variables to the just-initialized state.
  {
    AutoLock lock(sampler->thread_execution_state_lock_);
    sampler->thread_execution_state_ = NOT_STARTED;
    sampler->thread_execution_state_task_runner_ = nullptr;
    sampler->thread_execution_state_disable_idle_shutdown_for_testing_ = false;
    sampler->thread_execution_state_add_events_ = 0;
  }
}

// static
void StackSamplingProfiler::SamplingThread::TestPeer::DisableIdleShutdown() {
  SamplingThread* sampler = SamplingThread::GetInstance();

  {
    AutoLock lock(sampler->thread_execution_state_lock_);
    sampler->thread_execution_state_disable_idle_shutdown_for_testing_ = true;
  }
}

// static
void StackSamplingProfiler::SamplingThread::TestPeer::ShutdownAssumingIdle(
    bool simulate_intervening_add) {
  SamplingThread* sampler = SamplingThread::GetInstance();

  ThreadExecutionState state;
  scoped_refptr<SingleThreadTaskRunner> task_runner =
      sampler->GetTaskRunner(&state);
  DCHECK_EQ(RUNNING, state);
  DCHECK(task_runner);

  int add_events;
  {
    AutoLock lock(sampler->thread_execution_state_lock_);
    add_events = sampler->thread_execution_state_add_events_;
    if (simulate_intervening_add)
      ++sampler->thread_execution_state_add_events_;
  }

  WaitableEvent executed(WaitableEvent::ResetPolicy::MANUAL,
                         WaitableEvent::InitialState::NOT_SIGNALED);
  // PostTaskAndReply won't work because thread and associated message-loop may
  // be shut down.
  task_runner->PostTask(
      FROM_HERE, BindOnce(&ShutdownTaskAndSignalEvent, Unretained(sampler),
                          add_events, Unretained(&executed)));
  executed.Wait();
}

// static
void StackSamplingProfiler::SamplingThread::TestPeer::
    ShutdownTaskAndSignalEvent(SamplingThread* sampler,
                               int add_events,
                               WaitableEvent* event) {
  sampler->ShutdownTask(add_events);
  event->Signal();
}

AtomicSequenceNumber StackSamplingProfiler::SamplingThread::CollectionContext::
    next_collection_id;

StackSamplingProfiler::SamplingThread::SamplingThread()
    : Thread("StackSamplingProfiler") {}

StackSamplingProfiler::SamplingThread::~SamplingThread() = default;

StackSamplingProfiler::SamplingThread*
StackSamplingProfiler::SamplingThread::GetInstance() {
  return Singleton<SamplingThread, LeakySingletonTraits<SamplingThread>>::get();
}

int StackSamplingProfiler::SamplingThread::Add(
    std::unique_ptr<CollectionContext> collection) {
  // This is not to be run on the sampling thread.

  int collection_id = collection->collection_id;
  scoped_refptr<SingleThreadTaskRunner> task_runner =
      GetOrCreateTaskRunnerForAdd();

  task_runner->PostTask(
      FROM_HERE, BindOnce(&SamplingThread::AddCollectionTask, Unretained(this),
                          std::move(collection)));

  return collection_id;
}

void StackSamplingProfiler::SamplingThread::AddAuxUnwinder(
    int collection_id,
    std::unique_ptr<Unwinder> unwinder) {
  ThreadExecutionState state;
  scoped_refptr<SingleThreadTaskRunner> task_runner = GetTaskRunner(&state);
  if (state != RUNNING)
    return;
  DCHECK(task_runner);
  task_runner->PostTask(
      FROM_HERE, BindOnce(&SamplingThread::AddAuxUnwinderTask, Unretained(this),
                          collection_id, std::move(unwinder)));
}

void StackSamplingProfiler::SamplingThread::ApplyMetadataToPastSamples(
    base::TimeTicks period_start,
    base::TimeTicks period_end,
    uint64_t name_hash,
    std::optional<int64_t> key,
    int64_t value,
    std::optional<PlatformThreadId> thread_id) {
  ThreadExecutionState state;
  scoped_refptr<SingleThreadTaskRunner> task_runner = GetTaskRunner(&state);
  if (state != RUNNING)
    return;
  DCHECK(task_runner);
  task_runner->PostTask(
      FROM_HERE, BindOnce(&SamplingThread::ApplyMetadataToPastSamplesTask,
                          Unretained(this), period_start, period_end, name_hash,
                          key, value, thread_id));
}

void StackSamplingProfiler::SamplingThread::AddProfileMetadata(
    uint64_t name_hash,
    std::optional<int64_t> key,
    int64_t value,
    std::optional<PlatformThreadId> thread_id) {
  ThreadExecutionState state;
  scoped_refptr<SingleThreadTaskRunner> task_runner = GetTaskRunner(&state);
  if (state != RUNNING) {
    return;
  }
  DCHECK(task_runner);
  task_runner->PostTask(
      FROM_HERE, BindOnce(&SamplingThread::AddProfileMetadataTask,
                          Unretained(this), name_hash, key, value, thread_id));
}

void StackSamplingProfiler::SamplingThread::Remove(int collection_id) {
  // This is not to be run on the sampling thread.

  ThreadExecutionState state;
  scoped_refptr<SingleThreadTaskRunner> task_runner = GetTaskRunner(&state);
  if (state != RUNNING)
    return;
  DCHECK(task_runner);

  // This can fail if the thread were to exit between acquisition of the task
  // runner above and the call below. In that case, however, everything has
  // stopped so there's no need to try to stop it.
  task_runner->PostTask(FROM_HERE,
                        BindOnce(&SamplingThread::ScheduleCollectionStop,
                                 Unretained(this), collection_id));
}

scoped_refptr<SingleThreadTaskRunner>
StackSamplingProfiler::SamplingThread::GetOrCreateTaskRunnerForAdd() {
  AutoLock lock(thread_execution_state_lock_);

  // The increment of the "add events" count is why this method is to be only
  // called from "add".
  ++thread_execution_state_add_events_;

  if (thread_execution_state_ == RUNNING) {
    DCHECK(thread_execution_state_task_runner_);
    // This shouldn't be called from the sampling thread as it's inefficient.
    // Use GetTaskRunnerOnSamplingThread() instead.
    DCHECK_NE(GetThreadId(), PlatformThread::CurrentId());
    return thread_execution_state_task_runner_;
  }

  if (thread_execution_state_ == EXITING) {
    // StopSoon() was previously called to shut down the thread
    // asynchonously. Stop() must now be called before calling Start() again to
    // reset the thread state.
    //
    // We must allow blocking here to satisfy the Thread implementation, but in
    // practice the Stop() call is unlikely to actually block. For this to
    // happen a new profiling request would have to be made within the narrow
    // window between StopSoon() and thread exit following the end of the 60
    // second idle period.
    ScopedAllowThreadRecallForStackSamplingProfiler allow_thread_join;
    Stop();
  }

  DCHECK(!stack_buffer_);
  stack_buffer_ = StackSampler::CreateStackBuffer();

  // The thread is not running. Start it and get associated runner. The task-
  // runner has to be saved for future use because though it can be used from
  // any thread, it can be acquired via task_runner() only on the created
  // thread and the thread that creates it (i.e. this thread) for thread-safety
  // reasons which are alleviated in SamplingThread by gating access to it with
  // the |thread_execution_state_lock_|.
  Start();
  thread_execution_state_ = RUNNING;
  thread_execution_state_task_runner_ = Thread::task_runner();

  // Detach the sampling thread from the "sequence" (i.e. thread) that
  // started it so that it can be self-managed or stopped by another thread.
  DetachFromSequence();

  return thread_execution_state_task_runner_;
}

scoped_refptr<SingleThreadTaskRunner>
StackSamplingProfiler::SamplingThread::GetTaskRunner(
    ThreadExecutionState* out_state) {
  AutoLock lock(thread_execution_state_lock_);
  if (out_state)
    *out_state = thread_execution_state_;
  if (thread_execution_state_ == RUNNING) {
    // This shouldn't be called from the sampling thread as it's inefficient.
    // Use GetTaskRunnerOnSamplingThread() instead.
    DCHECK_NE(GetThreadId(), PlatformThread::CurrentId());
    DCHECK(thread_execution_state_task_runner_);
  } else {
    DCHECK(!thread_execution_state_task_runner_);
  }

  return thread_execution_state_task_runner_;
}

scoped_refptr<SingleThreadTaskRunner>
StackSamplingProfiler::SamplingThread::GetTaskRunnerOnSamplingThread() {
  // This should be called only from the sampling thread as it has limited
  // accessibility.
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());

  return Thread::task_runner();
}

void StackSamplingProfiler::SamplingThread::FinishCollection(
    std::unique_ptr<CollectionContext> collection) {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());
  DCHECK_EQ(0u, active_collections_.count(collection->collection_id));

  TimeDelta profile_duration = TimeTicks::Now() -
                               collection->profile_start_time +
                               collection->params.sampling_interval;

  collection->sampler->GetStackUnwindData()
      ->profile_builder()
      ->OnProfileCompleted(profile_duration,
                           collection->params.sampling_interval);

  // Signal that this collection is finished.
  WaitableEvent* collection_finished = collection->finished;
  // Ensure the collection is destroyed before signaling, so that it may
  // not outlive StackSamplingProfiler.
  collection.reset();
  collection_finished->Signal();

  ScheduleShutdownIfIdle();
}

void StackSamplingProfiler::SamplingThread::ScheduleShutdownIfIdle() {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());

  if (!active_collections_.empty())
    return;

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
               "StackSamplingProfiler::SamplingThread::ScheduleShutdownIfIdle");

  int add_events;
  {
    AutoLock lock(thread_execution_state_lock_);
    if (thread_execution_state_disable_idle_shutdown_for_testing_)
      return;
    add_events = thread_execution_state_add_events_;
  }

  GetTaskRunnerOnSamplingThread()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&SamplingThread::ShutdownTask, Unretained(this), add_events),
      Seconds(60));
}

void StackSamplingProfiler::SamplingThread::AddAuxUnwinderTask(
    int collection_id,
    std::unique_ptr<Unwinder> unwinder) {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());

  auto loc = active_collections_.find(collection_id);
  if (loc == active_collections_.end())
    return;

  loc->second->sampler->AddAuxUnwinder(std::move(unwinder));
}

void StackSamplingProfiler::SamplingThread::ApplyMetadataToPastSamplesTask(
    base::TimeTicks period_start,
    base::TimeTicks period_end,
    uint64_t name_hash,
    std::optional<int64_t> key,
    int64_t value,
    std::optional<PlatformThreadId> thread_id) {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());
  MetadataRecorder::Item item(name_hash, key, thread_id, value);
  for (auto& id_collection_pair : active_collections_) {
    if (thread_id && id_collection_pair.second->thread_id != thread_id)
      continue;
    id_collection_pair.second->sampler->GetStackUnwindData()
        ->profile_builder()
        ->ApplyMetadataRetrospectively(period_start, period_end, item);
  }
}

void StackSamplingProfiler::SamplingThread::AddProfileMetadataTask(
    uint64_t name_hash,
    std::optional<int64_t> key,
    int64_t value,
    std::optional<PlatformThreadId> thread_id) {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());
  MetadataRecorder::Item item(name_hash, key, thread_id, value);
  for (auto& id_collection_pair : active_collections_) {
    if (thread_id && id_collection_pair.second->thread_id != thread_id) {
      continue;
    }
    id_collection_pair.second->sampler->GetStackUnwindData()
        ->profile_builder()
        ->AddProfileMetadata(item);
  }
}

void StackSamplingProfiler::SamplingThread::AddCollectionTask(
    std::unique_ptr<CollectionContext> collection) {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());

  const int collection_id = collection->collection_id;
  const TimeDelta initial_delay = collection->params.initial_delay;

  collection->sampler->Initialize();

  active_collections_.insert(
      std::make_pair(collection_id, std::move(collection)));

  GetTaskRunnerOnSamplingThread()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&SamplingThread::RecordSampleTask, Unretained(this),
               collection_id),
      initial_delay);

  // Another increment of "add events" serves to invalidate any pending
  // shutdown tasks that may have been initiated between the Add() and this
  // task running.
  {
    AutoLock lock(thread_execution_state_lock_);
    ++thread_execution_state_add_events_;
  }
}

void StackSamplingProfiler::SamplingThread::RemoveCollectionTask(
    int collection_id) {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());

  auto found = active_collections_.find(collection_id);
  if (found == active_collections_.end())
    return;

  // Remove |collection| from |active_collections_|.
  std::unique_ptr<CollectionContext> collection = std::move(found->second);
  size_t count = active_collections_.erase(collection_id);
  DCHECK_EQ(1U, count);

  FinishCollection(std::move(collection));
}

void StackSamplingProfiler::SamplingThread::ScheduleCollectionStop(
    int collection_id) {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());

  auto found = active_collections_.find(collection_id);
  if (found == active_collections_.end()) {
    return;
  }

  CollectionContext* collection = found->second.get();
  collection->stopping = true;
  collection->sampler->Stop(BindOnce(&SamplingThread::RemoveCollectionTask,
                                     Unretained(this), collection_id));
}

void StackSamplingProfiler::SamplingThread::RecordSampleTask(
    int collection_id) {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());

  auto found = active_collections_.find(collection_id);

  // The task won't be found if it has been stopped.
  if (found == active_collections_.end()) {
    return;
  }

  CollectionContext* collection = found->second.get();

  // If we are in the process of stopping just don't collect a stack
  // trace as that would cause further jobs to be scheduled.
  if (collection->stopping) {
    return;
  }

  // If this is the first sample, the collection params need to be filled.
  if (collection->sample_count == 0) {
    collection->profile_start_time = TimeTicks::Now();
    collection->next_sample_time = TimeTicks::Now();
  }

  bool more_collections_remaining =
      ++collection->sample_count < collection->params.samples_per_profile;
  // Record a single sample.
  collection->sampler->RecordStackFrames(
      stack_buffer_.get(), collection->thread_id,
      more_collections_remaining
          ? DoNothing()
          : BindOnce(&SamplingThread::RemoveCollectionTask, Unretained(this),
                     collection_id));
  // Schedule the next sample recording if there is one.
  if (more_collections_remaining) {
    collection->next_sample_time = GetNextSampleTimeImpl(
        collection->next_sample_time, collection->params.sampling_interval,
        TimeTicks::Now());
    bool success = GetTaskRunnerOnSamplingThread()->PostDelayedTask(
        FROM_HERE,
        BindOnce(&SamplingThread::RecordSampleTask, Unretained(this),
                 collection_id),
        std::max(collection->next_sample_time - TimeTicks::Now(), TimeDelta()));
    DCHECK(success);
    return;
  }
}

void StackSamplingProfiler::SamplingThread::ShutdownTask(int add_events) {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());

  // Holding this lock ensures that any attempt to start another job will
  // get postponed until |thread_execution_state_| is updated, thus eliminating
  // the race in starting a new thread while the previous one is exiting.
  AutoLock lock(thread_execution_state_lock_);

  // If the current count of creation requests doesn't match the passed count
  // then other tasks have been created since this was posted. Abort shutdown.
  if (thread_execution_state_add_events_ != add_events)
    return;

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
               "StackSamplingProfiler::SamplingThread::ShutdownTask");

  // There can be no new AddCollectionTasks at this point because creating
  // those always increments "add events". There may be other requests, like
  // Remove, but it's okay to schedule the thread to stop once they've been
  // executed (i.e. "soon").
  DCHECK(active_collections_.empty());
  StopSoon();

  // StopSoon will have set the owning sequence (again) so it must be detached
  // (again) in order for Stop/Start to be called (again) should more work
  // come in. Holding the |thread_execution_state_lock_| ensures the necessary
  // happens-after with regard to this detach and future Thread API calls.
  DetachFromSequence();

  // Set the thread_state variable so the thread will be restarted when new
  // work comes in. Remove the |thread_execution_state_task_runner_| to avoid
  // confusion.
  thread_execution_state_ = EXITING;
  thread_execution_state_task_runner_ = nullptr;
  stack_buffer_.reset();
}

void StackSamplingProfiler::SamplingThread::CleanUp() {
  DCHECK_EQ(GetThreadId(), PlatformThread::CurrentId());

  // There should be no collections remaining when the thread stops.
  DCHECK(active_collections_.empty());

  // Let the parent clean up.
  Thread::CleanUp();
}

// StackSamplingProfiler ------------------------------------------------------

// static
void StackSamplingProfiler::TestPeer::Reset() {
  SamplingThread::TestPeer::Reset();
}

// static
bool StackSamplingProfiler::TestPeer::IsSamplingThreadRunning() {
  return SamplingThread::GetInstance()->IsRunning();
}

// static
void StackSamplingProfiler::TestPeer::DisableIdleShutdown() {
  SamplingThread::TestPeer::DisableIdleShutdown();
}

// static
void StackSamplingProfiler::TestPeer::PerformSamplingThreadIdleShutdown(
    bool simulate_intervening_start) {
  SamplingThread::TestPeer::ShutdownAssumingIdle(simulate_intervening_start);
}

// static
TimeTicks StackSamplingProfiler::TestPeer::GetNextSampleTime(
    TimeTicks scheduled_current_sample_time,
    TimeDelta sampling_interval,
    TimeTicks now) {
  return GetNextSampleTimeImpl(scheduled_current_sample_time, sampling_interval,
                               now);
}

// static
// The profiler is currently supported for Windows x64, macOS, iOS 64-bit,
// Android ARM32 and ARM64, and ChromeOS x64 and ARM64.
bool StackSamplingProfiler::IsSupportedForCurrentPlatform() {
#if (BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86_64)) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_IOS) && defined(ARCH_CPU_64_BITS)) ||                     \
    (BUILDFLAG(IS_ANDROID) &&                                               \
     ((defined(ARCH_CPU_ARMEL) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)) ||       \
      (defined(ARCH_CPU_ARM64)))) ||                                        \
    (BUILDFLAG(IS_CHROMEOS) &&                                              \
     (defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64)))
#if BUILDFLAG(IS_WIN)
  // Do not start the profiler when Application Verifier is in use; running them
  // simultaneously can cause crashes and has no known use case.
  if (GetModuleHandleA(base::win::kApplicationVerifierDllName))
    return false;
  // Checks if Trend Micro DLLs are loaded in process, so we can disable the
  // profiler to avoid hitting their performance bug. See
  // https://crbug.com/1018291 and https://crbug.com/1113832.
  if (GetModuleHandleA("tmmon64.dll") || GetModuleHandleA("tmmonmgr64.dll"))
    return false;
#endif  // BUILDFLAG(IS_WIN)
  return true;
#else
  return false;
#endif
}

StackSamplingProfiler::StackSamplingProfiler(
    SamplingProfilerThreadToken thread_token,
    const SamplingParams& params,
    std::unique_ptr<ProfileBuilder> profile_builder,
    UnwindersFactory core_unwinders_factory,
    RepeatingClosure record_sample_callback,
    StackSamplerTestDelegate* test_delegate)
    : thread_token_(thread_token),
      params_(params),
      sampler_(StackSampler::Create(
          thread_token,
          std::make_unique<StackUnwindData>(std::move(profile_builder)),
          std::move(core_unwinders_factory),
          std::move(record_sample_callback),
          test_delegate)),
      // The event starts "signaled" so code knows it's safe to start thread
      // and "manual" so that it can be waited in multiple places.
      profiling_inactive_(kResetPolicy, WaitableEvent::InitialState::SIGNALED),
      profiler_id_(kNullProfilerId) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
               "StackSamplingProfiler::StackSamplingProfiler");
}

StackSamplingProfiler::~StackSamplingProfiler() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
               "StackSamplingProfiler::~StackSamplingProfiler");

  // Stop returns immediately but the shutdown runs asynchronously. There is a
  // non-zero probability that one more sample will be taken after this call
  // returns.
  Stop();

  // The behavior of sampling a thread that has exited is undefined and could
  // cause Bad Things(tm) to occur. The safety model provided by this class is
  // that an instance of this object is expected to live at least as long as
  // the thread it is sampling. However, because the sampling is performed
  // asynchronously by the SamplingThread, there is no way to guarantee this
  // is true without waiting for it to signal that it has finished.
  //
  // The wait time should, at most, be only as long as it takes to collect one
  // sample (~200us) or none at all if sampling has already completed.
  ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  profiling_inactive_.Wait();
}

void StackSamplingProfiler::Start() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
               "StackSamplingProfiler::Start");

  // Multiple calls to Start() for a single StackSamplingProfiler object is not
  // allowed. If `sampler_` is nullptr, then Start() has been called already or
  // sampling isn't supported on the current platform.
  if (!sampler_) {
    return;
  }

  // The IsSignaled() check below requires that the WaitableEvent be manually
  // reset, to avoid signaling the event in IsSignaled() itself.
  static_assert(kResetPolicy == WaitableEvent::ResetPolicy::MANUAL,
                "The reset policy must be set to MANUAL");

  // If a previous profiling phase is still winding down, wait for it to
  // complete. We can't use task posting for this coordination because the
  // thread owning the profiler may not have a message loop.
  if (!profiling_inactive_.IsSignaled())
    profiling_inactive_.Wait();
  profiling_inactive_.Reset();

  DCHECK_EQ(kNullProfilerId, profiler_id_);
  profiler_id_ = SamplingThread::GetInstance()->Add(
      std::make_unique<SamplingThread::CollectionContext>(
          thread_token_.id, params_, &profiling_inactive_,
          std::move(sampler_)));
  DCHECK_NE(kNullProfilerId, profiler_id_);

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
               "StackSamplingProfiler::Started", "profiler_id", profiler_id_);
}

void StackSamplingProfiler::Stop() {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
               "StackSamplingProfiler::Stop", "profiler_id", profiler_id_);

  SamplingThread::GetInstance()->Remove(profiler_id_);
  profiler_id_ = kNullProfilerId;
}

void StackSamplingProfiler::AddAuxUnwinder(std::unique_ptr<Unwinder> unwinder) {
  if (profiler_id_ == kNullProfilerId) {
    // We haven't started sampling, and so we can add |unwinder| to the sampler
    // directly
    if (sampler_)
      sampler_->AddAuxUnwinder(std::move(unwinder));
    return;
  }

  SamplingThread::GetInstance()->AddAuxUnwinder(profiler_id_,
                                                std::move(unwinder));
}

// static
void StackSamplingProfiler::ApplyMetadataToPastSamples(
    base::TimeTicks period_start,
    base::TimeTicks period_end,
    uint64_t name_hash,
    std::optional<int64_t> key,
    int64_t value,
    std::optional<PlatformThreadId> thread_id) {
  SamplingThread::GetInstance()->ApplyMetadataToPastSamples(
      period_start, period_end, name_hash, key, value, thread_id);
}

// static
void StackSamplingProfiler::AddProfileMetadata(
    uint64_t name_hash,
    int64_t key,
    int64_t value,
    std::optional<PlatformThreadId> thread_id) {
  SamplingThread::GetInstance()->AddProfileMetadata(name_hash, key, value,
                                                    thread_id);
}

}  // namespace base
