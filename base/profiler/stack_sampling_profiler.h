// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_STACK_SAMPLING_PROFILER_H_
#define BASE_PROFILER_STACK_SAMPLING_PROFILER_H_

#include <memory>
#include <vector>

#include "base/base_export.h"
#include "base/macros.h"
#include "base/sampling_heap_profiler/module_cache.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"

namespace base {

class NativeStackSampler;
class NativeStackSamplerTestDelegate;

// StackSamplingProfiler periodically stops a thread to sample its stack, for
// the purpose of collecting information about which code paths are
// executing. This information is used in aggregate by UMA to identify hot
// and/or janky code paths.
//
// Sample StackSamplingProfiler usage:
//
//   // Create and customize params as desired.
//   base::StackStackSamplingProfiler::SamplingParams params;
//
//   // To process the profiles, use a custom ProfileBuilder subclass:
//   class SubProfileBuilder :
//       public base::StackSamplingProfiler::ProfileBuilder{...}
//
//   // On Android the |sampler| is not implemented in base. So, client can pass
//   // in |sampler| to use while profiling.
//   base::StackSamplingProfiler profiler(base::PlatformThread::CurrentId()),
//       params, std::make_unique<SubProfileBuilder>(...), <optional> sampler);
//
//   profiler.Start();
//   // ... work being done on the target thread here ...
//   profiler.Stop();  // optional, stops collection before complete per params
//
// The default SamplingParams causes stacks to be recorded in a single profile
// at a 10Hz interval for a total of 30 seconds. All of these parameters may be
// altered as desired.
//
// When a call stack profile is complete, or the profiler is stopped,
// ProfileBuilder's OnProfileCompleted function is called from a thread created
// by the profiler.
class BASE_EXPORT StackSamplingProfiler {
 public:
  // Frame represents an individual sampled stack frame with full module
  // information.
  //
  // This struct is only used for sampling data transfer from NativeStackSampler
  // to ProfileBuilder.
  struct BASE_EXPORT Frame {
    Frame(uintptr_t instruction_pointer, ModuleCache::Module module);
    ~Frame();

    // The sampled instruction pointer within the function.
    uintptr_t instruction_pointer;

    // The module information.
    ModuleCache::Module module;
  };

  // Represents parameters that configure the sampling.
  struct BASE_EXPORT SamplingParams {
    // Time to delay before first samples are taken.
    TimeDelta initial_delay = TimeDelta::FromMilliseconds(0);

    // Number of samples to record per profile.
    int samples_per_profile = 300;

    // Interval between samples during a sampling profile. This is the desired
    // duration from the start of one sample to the start of the next sample.
    TimeDelta sampling_interval = TimeDelta::FromMilliseconds(100);

    // When true, keeps the average sampling interval = |sampling_interval|,
    // irrespective of how long each sample takes. If a sample takes too long,
    // keeping the interval constant will lock out the sampled thread. When
    // false, sample is created with an interval of |sampling_interval|,
    // excluding the time taken by a sample. The metrics collected will not be
    // accurate, since sampling could take arbitrary amount of time, but makes
    // sure that the sampled thread gets at least the interval amount of time to
    // run between samples.
    bool keep_consistent_sampling_interval = true;
  };

  // The ProfileBuilder interface allows the user to record profile information
  // on the fly in whatever format is desired. Functions are invoked by the
  // profiler on its own thread so must not block or perform expensive
  // operations.
  class BASE_EXPORT ProfileBuilder {
   public:
    ProfileBuilder() = default;
    virtual ~ProfileBuilder() = default;

    // Metadata associated with the sample to be saved off.
    // The code implementing this method must not do anything that could acquire
    // a mutex, including allocating memory (which includes LOG messages)
    // because that mutex could be held by a stopped thread, thus resulting in
    // deadlock.
    virtual void RecordAnnotations();

    // Records a new set of frames. Invoked when sampling a sample completes.
    virtual void OnSampleCompleted(std::vector<Frame> frames) = 0;

    // Finishes the profile construction with |profile_duration| and
    // |sampling_period|. Invoked when sampling a profile completes.
    virtual void OnProfileCompleted(TimeDelta profile_duration,
                                    TimeDelta sampling_period) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(ProfileBuilder);
  };

  // Creates a profiler for the CURRENT thread. An optional |test_delegate| can
  // be supplied by tests. The caller must ensure that this object gets
  // destroyed before the current thread exits.
  StackSamplingProfiler(
      const SamplingParams& params,
      std::unique_ptr<ProfileBuilder> profile_builder,
      NativeStackSamplerTestDelegate* test_delegate = nullptr);

  // Creates a profiler for ANOTHER thread. An optional |test_delegate| can be
  // supplied by tests.
  //
  // IMPORTANT: The caller must ensure that the thread being sampled does not
  // exit before this object gets destructed or Bad Things(tm) may occur.
  StackSamplingProfiler(
      PlatformThreadId thread_id,
      const SamplingParams& params,
      std::unique_ptr<ProfileBuilder> profile_builder,
      NativeStackSamplerTestDelegate* test_delegate = nullptr);

  // Same as above function, with custom |sampler| implementation. The sampler
  // on Android is not implemented in base.
  StackSamplingProfiler(
      PlatformThreadId thread_id,
      const SamplingParams& params,
      std::unique_ptr<ProfileBuilder> profile_builder,
      std::unique_ptr<NativeStackSampler> sampler,
      NativeStackSamplerTestDelegate* test_delegate = nullptr);

  // Stops any profiling currently taking place before destroying the profiler.
  // This will block until profile_builder_'s OnProfileCompleted function has
  // executed if profiling has started but not already finished.
  ~StackSamplingProfiler();

  // Initializes the profiler and starts sampling. Might block on a
  // WaitableEvent if this StackSamplingProfiler was previously started and
  // recently stopped, while the previous profiling phase winds down.
  void Start();

  // Stops the profiler and any ongoing sampling. This method will return
  // immediately with the profile_builder_'s OnProfileCompleted function being
  // run asynchronously. At most one more stack sample will be taken after this
  // method returns. Calling this function is optional; if not invoked profiling
  // terminates when all the profiling samples specified in the SamplingParams
  // are completed or the profiler object is destroyed, whichever occurs first.
  void Stop();

  // Test peer class. These functions are purely for internal testing of
  // StackSamplingProfiler; DO NOT USE within tests outside of this directory.
  // The functions are static because they interact with the sampling thread, a
  // singleton used by all StackSamplingProfiler objects.  The functions can
  // only be called by the same thread that started the sampling.
  class BASE_EXPORT TestPeer {
   public:
    // Resets the internal state to that of a fresh start. This is necessary
    // so that tests don't inherit state from previous tests.
    static void Reset();

    // Returns whether the sampling thread is currently running or not.
    static bool IsSamplingThreadRunning();

    // Disables inherent idle-shutdown behavior.
    static void DisableIdleShutdown();

    // Initiates an idle shutdown task, as though the idle timer had expired,
    // causing the thread to exit. There is no "idle" check so this must be
    // called only when all sampling tasks have completed. This blocks until
    // the task has been executed, though the actual stopping of the thread
    // still happens asynchronously. Watch IsSamplingThreadRunning() to know
    // when the thread has exited. If |simulate_intervening_start| is true then
    // this method will make it appear to the shutdown task that a new profiler
    // was started between when the idle-shutdown was initiated and when it
    // runs.
    static void PerformSamplingThreadIdleShutdown(
        bool simulate_intervening_start);
  };

 private:
  // SamplingThread is a separate thread used to suspend and sample stacks from
  // the target thread.
  class SamplingThread;

  // The thread whose stack will be sampled.
  PlatformThreadId thread_id_;

  const SamplingParams params_;

  // Receives the sampling data and builds a profile. The ownership of this
  // object will be transferred to the sampling thread when thread sampling
  // starts.
  std::unique_ptr<ProfileBuilder> profile_builder_;

  // Stack sampler which stops the thread and collects stack frames. The
  // ownership of this object will be transferred to the sampling thread when
  // thread sampling starts.
  std::unique_ptr<NativeStackSampler> native_sampler_;

  // This starts "signaled", is reset when sampling begins, and is signaled
  // when that sampling is complete and the profile_builder_'s
  // OnProfileCompleted function has executed.
  WaitableEvent profiling_inactive_;

  // An ID uniquely identifying this profiler to the sampling thread. This
  // will be an internal "null" value when no collection has been started.
  int profiler_id_;

  // Stored until it can be passed to the NativeStackSampler created in Start().
  NativeStackSamplerTestDelegate* const test_delegate_;

  DISALLOW_COPY_AND_ASSIGN(StackSamplingProfiler);
};

}  // namespace base

#endif  // BASE_PROFILER_STACK_SAMPLING_PROFILER_H_
