// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_THREAD_GROUP_PROFILER_H_
#define BASE_PROFILER_THREAD_GROUP_PROFILER_H_

#include <memory>
#include <vector>

#include "base/base_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/profiler/periodic_sampling_scheduler.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

namespace base {
class SequencedTaskRunner;

namespace internal {
class WorkerThread;
}  // namespace internal

class ProfileBuilder;
class ThreadGroupProfilerClient;

// ThreadGroupProfiler manages sampling of active worker threads and
// schedules periodic sampling for a ThreadGroup.
//
// Once created, ThreadGroupProfiler will periodically profile active worker
// threads by creating a StackSamplingProfiler for each thread. At the beginning
// of a session, all active worker threads are sampled. During the session, if a
// worker thread becomes active (via MaybeAddWorkerThread) it will be sampled
// for the remainder of this session. Once the sampling starts for a thread it
// will continue until either the thread is exiting or the profile is completed.
// When a profile completes the associated StackSamplingProfiler is destroyed.
//
// All methods that access profiler state are expected to be externally
// synchronized by the owner (e.g. ThreadGroupImpl's CheckedLock).
class BASE_EXPORT ThreadGroupProfiler {
 public:
  // Interface for profiling stack samples from a specific thread.
  // This provides an abstraction over StackSamplingProfiler to enable testing
  // of ThreadGroupProfiler without depending on actual profiler implementation.
  class BASE_EXPORT Profiler : public RefCountedThreadSafe<Profiler> {
   public:
    virtual void Start() = 0;

   protected:
    friend class RefCountedThreadSafe<Profiler>;
    virtual ~Profiler() = default;
  };

  // Sets the instance of ThreadProfilerClient to provide embedder-specific
  // implementation logic. This instance must be set early, before
  // CreateThreadGroupProfiler() and IsProfilingEnabled() are called.
  static void SetClient(std::unique_ptr<ThreadGroupProfilerClient> client);

  // Must be called after SetClient().
  static bool IsProfilingEnabled();

  using ProfilerFactory = RepeatingCallback<scoped_refptr<Profiler>(
      int64_t thread_group_type,
      SamplingProfilerThreadToken thread_token,
      const StackSamplingProfiler::SamplingParams& params,
      std::unique_ptr<ProfileBuilder> profile_builder,
      StackSamplingProfiler::UnwindersFactory unwinder_factory)>;

  class BASE_EXPORT ActiveCollection;

  // Interface implemented by the owner of ThreadGroupProfiler (e.g.
  // ThreadGroupImpl) to receive start and end notifications for profiling
  // sessions.
  class BASE_EXPORT Delegate {
   public:
    virtual ~Delegate() = default;

    // Called on the service thread sequence when a profiling session starts.
    // The delegate is responsible for storing `active_collection` (under its
    // lock) and initiating profiling on active worker threads.
    virtual void OnStartProfilingSession(
        ActiveCollection active_collection) = 0;

    // Called on the service thread sequence when the profiling session ends.
    // The delegate is responsible for destroying its active collection.
    virtual void OnEndProfilingSession() = 0;
  };

  // ThreadGroupProfiler constructor. |thread_group_type| will be used to tag
  // metadata on all samples collected in this profiler. |profiler_factory| is a
  // repeating callback that will be used to make Profiler, intended to be used
  // for dependency injection for testing.
  explicit ThreadGroupProfiler(
      int64_t thread_group_type,
      Delegate* delegate,
      std::unique_ptr<PeriodicSamplingScheduler> periodic_sampling_scheduler =
          nullptr,
      ProfilerFactory profiler_factory = GetDefaultProfilerFactory());
  ThreadGroupProfiler(const ThreadGroupProfiler&) = delete;
  ThreadGroupProfiler& operator=(const ThreadGroupProfiler&) = delete;

  ~ThreadGroupProfiler();

  // Starts the periodic profiling loop on `service_thread_task_runner`.
  void Start(scoped_refptr<SequencedTaskRunner> service_thread_task_runner);

  // Represents an active sample collection phase and is responsible for
  // creating profilers for active threads both at the beginning as well as
  // during the sampling duration.
  //
  // All methods that access profiler state are expected to be externally
  // synchronized by the owner (e.g. ThreadGroupImpl's CheckedLock).
  class BASE_EXPORT ActiveCollection {
   public:
    explicit ActiveCollection(int64_t thread_group_type,
                              TimeDelta sampling_duration,
                              ProfilerFactory stack_sampling_profiler_factory);
    ~ActiveCollection();
    ActiveCollection(const ActiveCollection&) = delete;
    ActiveCollection& operator=(const ActiveCollection&) = delete;
    ActiveCollection(ActiveCollection&&);
    ActiveCollection& operator=(ActiveCollection&&);

    // Maybe create a new profiler for worker_thread depending on how close
    // the collection is to being complete. Returns the created profiler if any.
    scoped_refptr<Profiler> MaybeAddWorkerThread(
        internal::WorkerThread* worker_thread,
        SamplingProfilerThreadToken token);

    // Removes and returns the profiler for worker_thread if it exists, so that
    // it can be destroyed outside locks.
    scoped_refptr<Profiler> RemoveWorkerThread(
        internal::WorkerThread* worker_thread);

   private:
    // Helper function for creating the StackSamplingProfiler.
    scoped_refptr<Profiler> CreateSamplingProfilerForThread(
        internal::WorkerThread* worker_thread,
        const SamplingProfilerThreadToken& token,
        const StackSamplingProfiler::SamplingParams& sampling_params);

    int64_t thread_group_type_;

    // A map that stores the active `StackSamplingProfiler` instances
    // for each worker thread.
    flat_map<internal::WorkerThread*, scoped_refptr<Profiler>> profilers_;

    ProfilerFactory stack_sampling_profiler_factory_;

    TimeDelta sampling_duration_;

    // Tracks the end time (an estimate calculated at start of sampling by
    // adding the sampling duration) of the current sampling session.
    TimeTicks collection_end_time_;
  };

 private:
  class ProfilerImpl;

  // Creates a new ActiveCollection for a sampling session.
  ActiveCollection CreateActiveCollection();

  void ScheduleNextCollection();
  void OnStartProfilingSessionTask();
  void OnEndProfilingSessionTask();

  // Retrieve the ThreadGroupProfilerClient instance provided via SetClient().
  static ThreadGroupProfilerClient* GetClient();

  static ProfilerFactory GetDefaultProfilerFactory();

  // Retrieve the static sampling duration configured on the client.
  static TimeDelta GetSamplingDuration();

  SEQUENCE_CHECKER(sequence_checker_);

  // Value to use as metadata for specifying which type of thread group is being
  // profiled.
  const int64_t thread_group_type_;

  const raw_ptr<Delegate> delegate_;

  std::unique_ptr<PeriodicSamplingScheduler> periodic_sampling_scheduler_;

  ProfilerFactory stack_sampling_profiler_factory_;

  scoped_refptr<SequencedTaskRunner> service_thread_task_runner_;

  WeakPtrFactory<ThreadGroupProfiler> weak_ptr_factory_{this};
};

}  // namespace base

#endif  // BASE_PROFILER_THREAD_GROUP_PROFILER_H_
