// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#import <Foundation/Foundation.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <stddef.h>
#include <sys/resource.h>

#include <algorithm>
#include <atomic>

#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/mach_logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/threading_features.h"
#include "build/build_config.h"

namespace base {

namespace {
NSString* const kThreadPriorityKey = @"CrThreadPriorityKey";
NSString* const kRealtimePeriodNsKey = @"CrRealtimePeriodNsKey";
}  // namespace

// If Cocoa is to be used on more than one thread, it must know that the
// application is multithreaded.  Since it's possible to enter Cocoa code
// from threads created by pthread_thread_create, Cocoa won't necessarily
// be aware that the application is multithreaded.  Spawning an NSThread is
// enough to get Cocoa to set up for multithreaded operation, so this is done
// if necessary before pthread_thread_create spawns any threads.
//
// http://developer.apple.com/documentation/Cocoa/Conceptual/Multithreading/CreatingThreads/chapter_4_section_4.html
void InitThreading() {
  static BOOL multithreaded = [NSThread isMultiThreaded];
  if (!multithreaded) {
    // +[NSObject class] is idempotent.
    [NSThread detachNewThreadSelector:@selector(class)
                             toTarget:[NSObject class]
                           withObject:nil];
    multithreaded = YES;

    DCHECK([NSThread isMultiThreaded]);
  }
}

// static
void PlatformThread::SetName(const std::string& name) {
  ThreadIdNameManager::GetInstance()->SetName(name);

  // Mac OS X does not expose the length limit of the name, so
  // hardcode it.
  const int kMaxNameLength = 63;
  std::string shortened_name = name.substr(0, kMaxNameLength);
  // pthread_setname() fails (harmlessly) in the sandbox, ignore when it does.
  // See http://crbug.com/47058
  pthread_setname_np(shortened_name.c_str());
}

// Whether optimized realt-time thread config should be used for audio.
const Feature kOptimizedRealtimeThreadingMac {
  "OptimizedRealtimeThreadingMac",
#if defined(OS_MAC)
      FEATURE_ENABLED_BY_DEFAULT
#else
      FEATURE_DISABLED_BY_DEFAULT
#endif
};

namespace {

bool IsOptimizedRealtimeThreadingMacEnabled() {
#if defined(OS_MAC)
  // There is some platform bug on 10.14.
  if (mac::IsOS10_14())
    return false;
#endif

  return FeatureList::IsEnabled(kOptimizedRealtimeThreadingMac);
}

}  // namespace

// Fine-tuning optimized realt-time thread config:
// Whether or not the thread should be preeptible.
const FeatureParam<bool> kOptimizedRealtimeThreadingMacPreemptible{
    &kOptimizedRealtimeThreadingMac, "preemptible", true};
// Portion of the time quantum the thread is expected to be busy, (0, 1].
const FeatureParam<double> kOptimizedRealtimeThreadingMacBusy{
    &kOptimizedRealtimeThreadingMac, "busy", 0.5};
// Maximum portion of the time quantum the thread is expected to be busy,
// (kOptimizedRealtimeThreadingMacBusy, 1].
const FeatureParam<double> kOptimizedRealtimeThreadingMacBusyLimit{
    &kOptimizedRealtimeThreadingMac, "busy_limit", 1.0};

namespace {

struct TimeConstraints {
  bool preemptible{kOptimizedRealtimeThreadingMacPreemptible.default_value};
  double busy{kOptimizedRealtimeThreadingMacBusy.default_value};
  double busy_limit{kOptimizedRealtimeThreadingMacBusyLimit.default_value};

  static TimeConstraints ReadFromFeatureParams() {
    double busy_limit = kOptimizedRealtimeThreadingMacBusyLimit.Get();
    return TimeConstraints{
        kOptimizedRealtimeThreadingMacPreemptible.Get(),
        std::min(busy_limit, kOptimizedRealtimeThreadingMacBusy.Get()),
        busy_limit};
  }
};

// Use atomics to access FeatureList values when setting up a thread, since
// there are cases when FeatureList initialization is not synchronized with
// PlatformThread creation.
std::atomic<bool> g_use_optimized_realtime_threading(
    kOptimizedRealtimeThreadingMac.default_state == FEATURE_ENABLED_BY_DEFAULT);
std::atomic<TimeConstraints> g_time_constraints;

}  // namespace

// static
void PlatformThread::InitializeOptimizedRealtimeThreadingFeature() {
  // A DCHECK is triggered on FeatureList initialization if the state of a
  // feature has been checked before. To avoid triggering this DCHECK in unit
  // tests that call this before initializing the FeatureList, only check the
  // state of the feature if the FeatureList is initialized.
  if (FeatureList::GetInstance()) {
    g_time_constraints.store(TimeConstraints::ReadFromFeatureParams());
    g_use_optimized_realtime_threading.store(
        IsOptimizedRealtimeThreadingMacEnabled());
  }
}

// static
void PlatformThread::SetCurrentThreadRealtimePeriodValue(
    TimeDelta realtime_period) {
  if (g_use_optimized_realtime_threading.load()) {
    [[NSThread currentThread] threadDictionary][kRealtimePeriodNsKey] =
        @(realtime_period.InNanoseconds());
  }
}

namespace {

TimeDelta GetCurrentThreadRealtimePeriod() {
  NSNumber* period = mac::ObjCCast<NSNumber>(
      [[NSThread currentThread] threadDictionary][kRealtimePeriodNsKey]);

  return period ? TimeDelta::FromNanoseconds(period.longLongValue)
                : TimeDelta();
}

// Calculates time constrints for THREAD_TIME_CONSTRAINT_POLICY.
// |realtime_period| is used as a base if it's non-zero.
// Otherwise we fall back to empirical values.
thread_time_constraint_policy_data_t GetTimeConstraints(
    TimeDelta realtime_period) {
  thread_time_constraint_policy_data_t time_constraints;
  mach_timebase_info_data_t tb_info;
  mach_timebase_info(&tb_info);

  if (!realtime_period.is_zero()) {
    // Limit the lowest value to 2.9 ms we used to have historically. The lower
    // the period, the more CPU frequency may go up, and we don't want to risk
    // worsening the thermal situation.
    uint32_t abs_realtime_period = saturated_cast<uint32_t>(
        std::max(realtime_period.InNanoseconds(), 2900000LL) *
        (double(tb_info.denom) / tb_info.numer));
    TimeConstraints config = g_time_constraints.load();
    time_constraints.period = abs_realtime_period;
    time_constraints.constraint = std::min(
        abs_realtime_period, uint32_t(abs_realtime_period * config.busy_limit));
    time_constraints.computation =
        std::min(time_constraints.constraint,
                 uint32_t(abs_realtime_period * config.busy));
    time_constraints.preemptible = config.preemptible ? YES : NO;
    return time_constraints;
  }

  // Empirical configuration.

  // Define the guaranteed and max fraction of time for the audio thread.
  // These "duty cycle" values can range from 0 to 1.  A value of 0.5
  // means the scheduler would give half the time to the thread.
  // These values have empirically been found to yield good behavior.
  // Good means that audio performance is high and other threads won't starve.
  const double kGuaranteedAudioDutyCycle = 0.75;
  const double kMaxAudioDutyCycle = 0.85;

  // Define constants determining how much time the audio thread can
  // use in a given time quantum.  All times are in milliseconds.

  // About 128 frames @44.1KHz
  const double kTimeQuantum = 2.9;

  // Time guaranteed each quantum.
  const double kAudioTimeNeeded = kGuaranteedAudioDutyCycle * kTimeQuantum;

  // Maximum time each quantum.
  const double kMaxTimeAllowed = kMaxAudioDutyCycle * kTimeQuantum;

  // Get the conversion factor from milliseconds to absolute time
  // which is what the time-constraints call needs.
  double ms_to_abs_time = double(tb_info.denom) / tb_info.numer * 1000000;

  time_constraints.period = kTimeQuantum * ms_to_abs_time;
  time_constraints.computation = kAudioTimeNeeded * ms_to_abs_time;
  time_constraints.constraint = kMaxTimeAllowed * ms_to_abs_time;
  time_constraints.preemptible = 0;
  return time_constraints;
}

// Enables time-contraint policy and priority suitable for low-latency,
// glitch-resistant audio.
void SetPriorityRealtimeAudio(TimeDelta realtime_period) {
  // Increase thread priority to real-time.

  // Please note that the thread_policy_set() calls may fail in
  // rare cases if the kernel decides the system is under heavy load
  // and is unable to handle boosting the thread priority.
  // In these cases we just return early and go on with life.

  mach_port_t mach_thread_id =
      pthread_mach_thread_np(PlatformThread::CurrentHandle().platform_handle());

  // Make thread fixed priority.
  thread_extended_policy_data_t policy;
  policy.timeshare = 0;  // Set to 1 for a non-fixed thread.
  kern_return_t result = thread_policy_set(
      mach_thread_id, THREAD_EXTENDED_POLICY,
      reinterpret_cast<thread_policy_t>(&policy), THREAD_EXTENDED_POLICY_COUNT);
  if (result != KERN_SUCCESS) {
    MACH_DVLOG(1, result) << "thread_policy_set";
    return;
  }

  // Set to relatively high priority.
  thread_precedence_policy_data_t precedence;
  precedence.importance = 63;
  result = thread_policy_set(mach_thread_id, THREAD_PRECEDENCE_POLICY,
                             reinterpret_cast<thread_policy_t>(&precedence),
                             THREAD_PRECEDENCE_POLICY_COUNT);
  if (result != KERN_SUCCESS) {
    MACH_DVLOG(1, result) << "thread_policy_set";
    return;
  }

  // Most important, set real-time constraints.

  thread_time_constraint_policy_data_t time_constraints =
      GetTimeConstraints(realtime_period);

  result =
      thread_policy_set(mach_thread_id, THREAD_TIME_CONSTRAINT_POLICY,
                        reinterpret_cast<thread_policy_t>(&time_constraints),
                        THREAD_TIME_CONSTRAINT_POLICY_COUNT);
  MACH_DVLOG_IF(1, result != KERN_SUCCESS, result) << "thread_policy_set";

  UmaHistogramCustomMicrosecondsTimes(
      "PlatformThread.Mac.AttemptedRealtimePeriod", realtime_period,
      base::TimeDelta(), base::TimeDelta::FromMilliseconds(100), 100);

  if (result == KERN_SUCCESS) {
    UmaHistogramCustomMicrosecondsTimes(
        "PlatformThread.Mac.SucceededRealtimePeriod", realtime_period,
        base::TimeDelta(), base::TimeDelta::FromMilliseconds(100), 100);
  }
  return;
}

}  // anonymous namespace

// static
bool PlatformThread::CanIncreaseThreadPriority(ThreadPriority priority) {
  return true;
}

// static
void PlatformThread::SetCurrentThreadPriorityImpl(ThreadPriority priority) {
  // Changing the priority of the main thread causes performance regressions.
  // https://crbug.com/601270
  DCHECK(![[NSThread currentThread] isMainThread]);

  switch (priority) {
    case ThreadPriority::BACKGROUND:
      [[NSThread currentThread] setThreadPriority:0];
      break;
    case ThreadPriority::NORMAL:
      [[NSThread currentThread] setThreadPriority:0.5];
      break;
    case ThreadPriority::DISPLAY: {
      // Apple has suggested that insufficient priority may be the reason for
      // Metal shader compilation hangs. A priority of 50 is higher than user
      // input.
      // https://crbug.com/974219.
      [[NSThread currentThread] setThreadPriority:1.0];
      sched_param param;
      int policy;
      pthread_t thread = pthread_self();
      if (!pthread_getschedparam(thread, &policy, &param)) {
        param.sched_priority = 50;
        pthread_setschedparam(thread, policy, &param);
      }
      break;
    }
    case ThreadPriority::REALTIME_AUDIO:
      SetPriorityRealtimeAudio(GetCurrentThreadRealtimePeriod());
      DCHECK_EQ([[NSThread currentThread] threadPriority], 1.0);
      break;
  }

  [[NSThread currentThread] threadDictionary][kThreadPriorityKey] =
      @(static_cast<int>(priority));
}

// static
ThreadPriority PlatformThread::GetCurrentThreadPriority() {
  NSNumber* priority = base::mac::ObjCCast<NSNumber>(
      [[NSThread currentThread] threadDictionary][kThreadPriorityKey]);

  if (!priority)
    return ThreadPriority::NORMAL;

  ThreadPriority thread_priority =
      static_cast<ThreadPriority>(priority.intValue);
  switch (thread_priority) {
    case ThreadPriority::BACKGROUND:
    case ThreadPriority::NORMAL:
    case ThreadPriority::DISPLAY:
    case ThreadPriority::REALTIME_AUDIO:
      return thread_priority;
    default:
      NOTREACHED() << "Unknown priority.";
      return ThreadPriority::NORMAL;
  }
}

size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes) {
#if defined(OS_IOS)
  return 0;
#else
  // The Mac OS X default for a pthread stack size is 512kB.
  // Libc-594.1.4/pthreads/pthread.c's pthread_attr_init uses
  // DEFAULT_STACK_SIZE for this purpose.
  //
  // 512kB isn't quite generous enough for some deeply recursive threads that
  // otherwise request the default stack size by specifying 0. Here, adopt
  // glibc's behavior as on Linux, which is to use the current stack size
  // limit (ulimit -s) as the default stack size. See
  // glibc-2.11.1/nptl/nptl-init.c's __pthread_initialize_minimal_internal. To
  // avoid setting the limit below the Mac OS X default or the minimum usable
  // stack size, these values are also considered. If any of these values
  // can't be determined, or if stack size is unlimited (ulimit -s unlimited),
  // stack_size is left at 0 to get the system default.
  //
  // Mac OS X normally only applies ulimit -s to the main thread stack. On
  // contemporary OS X and Linux systems alike, this value is generally 8MB
  // or in that neighborhood.
  size_t default_stack_size = 0;
  struct rlimit stack_rlimit;
  if (pthread_attr_getstacksize(&attributes, &default_stack_size) == 0 &&
      getrlimit(RLIMIT_STACK, &stack_rlimit) == 0 &&
      stack_rlimit.rlim_cur != RLIM_INFINITY) {
    default_stack_size =
        std::max(std::max(default_stack_size,
                          static_cast<size_t>(PTHREAD_STACK_MIN)),
                 static_cast<size_t>(stack_rlimit.rlim_cur));
  }
  return default_stack_size;
#endif
}

void TerminateOnThread() {
}

}  // namespace base
