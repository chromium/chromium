// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/features.h"

#include <atomic>

#include "base/containers/variant_map.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/thread_pool/job_task_source.h"
#include "base/threading/platform_thread.h"
#include "build/blink_buildflags.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "base/message_loop/message_pump_epoll.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "base/files/file.h"
#include "base/message_loop/message_pump_apple.h"
#include "base/synchronization/condition_variable.h"

#if !BUILDFLAG(IS_IOS) || !BUILDFLAG(USE_BLINK)
#include "base/message_loop/message_pump_kqueue.h"
#endif

#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/input_hint_checker.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/task/sequence_manager/thread_controller_power_monitor.h"
#endif

namespace base::features {

namespace {

// An atomic is used because this can be queried racily by a thread checking if
// an optimization is enabled and a thread initializing this from the
// FeatureList. All operations use std::memory_order_relaxed because there are
// no dependent memory operations.
std::atomic_bool g_is_reduce_ppms_enabled{false};

}  // namespace

// Alphabetical:

// When enabled, the compositor threads (including GPU) will be boosted to
// kInteractive when not in input or loading scenarios.
BASE_FEATURE(kBoostCompositorThreadsPriorityWhenIdle,
             FEATURE_DISABLED_BY_DEFAULT);

// Controls caching within BASE_FEATURE_PARAM(). This is feature-controlled
// so that ScopedFeatureList can disable it to turn off caching.
BASE_FEATURE(kFeatureParamWithCache, FEATURE_ENABLED_BY_DEFAULT);

// Whether a fast implementation of FilePath::IsParent is used. This feature
// exists to ensure that the fast implementation can be disabled quickly if
// issues are found with it.
BASE_FEATURE(kFastFilePathIsParent, FEATURE_ENABLED_BY_DEFAULT);

// Use non default low memory device threshold.
// Value should be given via |LowMemoryDeviceThresholdMB|.
#if BUILDFLAG(IS_ANDROID)
// LINT.IfChange
#define LOW_MEMORY_DEVICE_THRESHOLD_MB 1024
// LINT.ThenChange(//base/android/java/src/org/chromium/base/SysUtils.java)
#elif BUILDFLAG(IS_IOS)
// For M99, 45% of devices have 2GB of RAM, and 55% have more.
#define LOW_MEMORY_DEVICE_THRESHOLD_MB 1024
#else
// Updated Desktop default threshold to match the Android 2021 definition.
#define LOW_MEMORY_DEVICE_THRESHOLD_MB 2048
#endif
BASE_FEATURE(kLowEndMemoryExperiment, FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kLowMemoryDeviceThresholdMB,
                   &kLowEndMemoryExperiment,
                   "LowMemoryDeviceThresholdMB",
                   LOW_MEMORY_DEVICE_THRESHOLD_MB);

BASE_FEATURE(kReducePPMs, FEATURE_DISABLED_BY_DEFAULT);

// Apply base::ScopedBestEffortExecutionFence to registered task queues as well
// as the thread pool.
BASE_FEATURE(kScopedBestEffortExecutionFenceForTaskQueue,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to restrict the max gap between the frame pointer and the stack end
// for stack scanning. If the gap is beyond the given gap threshold, the stack
// end is treated as unreliable. Stack scanning stops when that happens.
// This feature is only in effect when BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
// is on and `TraceStackFramePointers` would run stack scanning. Default gap
// threshold is an absurdly large 100MB.
// The feature is enabled by default on ChromeOS where crashes caused by
// unreliable stack end are found. See https://crbug.com/402542102
BASE_FEATURE(kStackScanMaxFramePointerToStackEndGap,
#if BUILDFLAG(IS_CHROMEOS)
             FEATURE_ENABLED_BY_DEFAULT
#else
             FEATURE_DISABLED_BY_DEFAULT
#endif
);
BASE_FEATURE_PARAM(int,
                   kStackScanMaxFramePointerToStackEndGapThresholdMB,
                   &kStackScanMaxFramePointerToStackEndGap,
                   "StackScanMaxFramePointerToStackEndGapThresholdMB",
                   100);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
// Force to enable LowEndDeviceMode partially on Android 3Gb devices.
// (see PartialLowEndModeOnMidRangeDevices below)
BASE_FEATURE(kPartialLowEndModeOn3GbDevices, FEATURE_DISABLED_BY_DEFAULT);

// Used to enable LowEndDeviceMode partially on Android and ChromeOS mid-range
// devices. Such devices aren't considered low-end, but we'd like experiment
// with a subset of low-end features to see if we get a good memory vs.
// performance tradeoff.
//
// TODO(crbug.com/40264947): |#if| out 32-bit before launching or going to
// high Stable %, because we will enable the feature only for <8GB 64-bit
// devices, where we didn't ship yet. However, we first need a larger
// population to collect data.
BASE_FEATURE(kPartialLowEndModeOnMidRangeDevices,
#if BUILDFLAG(IS_ANDROID)
             FEATURE_ENABLED_BY_DEFAULT);
#elif BUILDFLAG(IS_CHROMEOS)
             FEATURE_DISABLED_BY_DEFAULT);
#endif

#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
// Enable not perceptible binding without cpu priority boosting.
BASE_FEATURE(kBackgroundNotPerceptibleBinding, FEATURE_ENABLED_BY_DEFAULT);

// Whether to use effective binding state to manage child process bindings.
// ChildProcessConnection will binds at most 2 service connections only,
// the connection for the effective binding state and waived binding.
BASE_FEATURE(kEffectiveBindingState, FEATURE_DISABLED_BY_DEFAULT);

// If enabled, post registering PowerMonitor broadcast receiver to a background
// thread,
BASE_FEATURE(kPostPowerMonitorBroadcastReceiverInitToBackground,
             FEATURE_ENABLED_BY_DEFAULT);
// If enabled, getMyMemoryState IPC will be posted to background.
BASE_FEATURE(kPostGetMyMemoryStateToBackground, FEATURE_ENABLED_BY_DEFAULT);

// Use a single connection and rebindService() to manage the binding to a child
// process service.
BASE_FEATURE(kRebindingChildServiceConnectionController,
             FEATURE_DISABLED_BY_DEFAULT);

// Use a batch API to rebind service connections.
BASE_FEATURE(kRebindServiceBatchApi, FEATURE_DISABLED_BY_DEFAULT);

// Use ChildServiceConnectionController.isUnbound() instead of isConnected() to
// check the connection state in ChildProcessConnection.
BASE_FEATURE(kUseIsUnboundCheck, FEATURE_DISABLED_BY_DEFAULT);

// Use shared service connection to rebind a service binding to update the LRU
// in the ProcessList of OomAdjuster.
BASE_FEATURE(kUseSharedRebindServiceConnection, FEATURE_ENABLED_BY_DEFAULT);

// Use madvise MADV_WILLNEED to prefetch the native library. This replaces the
// default mechanism of pre-reading the memory from a forked process.
BASE_FEATURE(kLibraryPrefetcherMadvise, FEATURE_DISABLED_BY_DEFAULT);

// If > 0, split the madvise range into chunks of this many bytes, rounded up to
// a page size. The default of 1 therefore rounds to a whole page.
BASE_FEATURE_PARAM(size_t,
                   kLibraryPrefetcherMadviseLength,
                   &kLibraryPrefetcherMadvise,
                   "length",
                   1);

// Whether to fall back to the fork-and-read method if madvise is not supported.
// Does not trigger fork-and-read if madvise failed during the actual prefetch.
BASE_FEATURE_PARAM(bool,
                   kLibraryPrefetcherMadviseFallback,
                   &kLibraryPrefetcherMadvise,
                   "fallback",
                   true);
#endif  // BUILDFLAG(IS_ANDROID)

// When enabled, GetTerminationStatus() returns
// TERMINATION_STATUS_EVICTED_FOR_MEMORY for processes terminated due to commit
// failures. Otherwise, it returns TERMINATION_STATUS_OOM.
BASE_FEATURE(kUseTerminationStatusMemoryExhaustion,
             FEATURE_DISABLED_BY_DEFAULT);

bool IsReducePPMsEnabled() {
  return g_is_reduce_ppms_enabled.load(std::memory_order_relaxed);
}

void Init() {
  g_is_reduce_ppms_enabled.store(FeatureList::IsEnabled(kReducePPMs),
                                 std::memory_order_relaxed);

  sequence_manager::internal::SequenceManagerImpl::InitializeFeatures();
  sequence_manager::internal::ThreadController::InitializeFeatures();
  base::internal::JobTaskSource::InitializeFeatures();

  debug::StackTrace::InitializeFeatures();
  FilePath::InitializeFeatures();
  InitializeVariantMapFeatures();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  MessagePumpEpoll::InitializeFeatures();
#endif

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_CHROMEOS)
  PlatformThread::InitializeFeatures();
#endif

#if BUILDFLAG(IS_APPLE)
  MessagePumpCFRunLoopBase::InitializeFeatures();

// Kqueue is not used for ios blink.
#if !BUILDFLAG(IS_IOS) || !BUILDFLAG(USE_BLINK)
  MessagePumpKqueue::InitializeFeatures();
#endif

#endif

#if BUILDFLAG(IS_ANDROID)
  android::InputHintChecker::InitializeFeatures();
#endif

#if BUILDFLAG(IS_WIN)
  sequence_manager::internal::ThreadControllerPowerMonitor::
      InitializeFeatures();
#endif
}

}  // namespace base::features
