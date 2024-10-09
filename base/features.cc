// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/features.h"

#include "base/cpu_reduction_experiment.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/threading/platform_thread.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "base/message_loop/message_pump_epoll.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "base/files/file.h"
#include "base/message_loop/message_pump_apple.h"
#include "base/message_loop/message_pump_kqueue.h"
#include "base/synchronization/condition_variable.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/input_hint_checker.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/threading/platform_thread_win.h"
#endif

namespace base::features {

// Alphabetical:

// Enforce that writeable file handles passed to untrusted processes are not
// backed by executable files.
BASE_FEATURE(kEnforceNoExecutableFileHandles,
             "EnforceNoExecutableFileHandles",
             FEATURE_ENABLED_BY_DEFAULT);

// Activate base::FeatureParamWithCache internal cache.
// TODO(https://crbug.com/340824113): Remove the feature flag below.
BASE_FEATURE(kFeatureParamWithCache,
             "FeatureParamWithCache",
             FEATURE_ENABLED_BY_DEFAULT);

// Use the Rust JSON parser. Enabled everywhere except Android, where the switch
// from using the C++ parser in-thread to using the Rust parser in a thread-pool
// introduces too much latency.
BASE_FEATURE(kUseRustJsonParser,
             "UseRustJsonParser",
#if BUILDFLAG(IS_ANDROID)
             FEATURE_DISABLED_BY_DEFAULT
#else
             FEATURE_ENABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_ANDROID)
);

// If true, use the Rust JSON parser in-thread; otherwise, it runs in a thread
// pool.
const base::FeatureParam<bool> kUseRustJsonParserInCurrentSequence{
    &kUseRustJsonParser, "UseRustJsonParserInCurrentSequence", false};

// Use non default low memory device threshold.
// Value should be given via |LowMemoryDeviceThresholdMB|.
#if BUILDFLAG(IS_IOS)
// For M99, 45% of devices have 2GB of RAM, and 55% have more.
#define LOW_MEMORY_DEVICE_THRESHOLD_MB 1024
#else
// Updated Desktop default threshold to match the Android 2021 definition.
#define LOW_MEMORY_DEVICE_THRESHOLD_MB 2048
#endif
BASE_FEATURE(kLowEndMemoryExperiment,
             "LowEndMemoryExperiment",
             FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kLowMemoryDeviceThresholdMB{
    &kLowEndMemoryExperiment, "LowMemoryDeviceThresholdMB",
    LOW_MEMORY_DEVICE_THRESHOLD_MB};

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
// Force to enable LowEndDeviceMode partially on Android 3Gb devices.
// (see PartialLowEndModeOnMidRangeDevices below)
BASE_FEATURE(kPartialLowEndModeOn3GbDevices,
             "PartialLowEndModeOn3GbDevices",
             FEATURE_DISABLED_BY_DEFAULT);

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
             "PartialLowEndModeOnMidRangeDevices",
#if BUILDFLAG(IS_ANDROID)
             FEATURE_ENABLED_BY_DEFAULT);
#elif BUILDFLAG(IS_CHROMEOS)
             FEATURE_DISABLED_BY_DEFAULT);
#endif

#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
// Whether to report frame metrics to the Android.FrameTimeline.* histograms.
BASE_FEATURE(kCollectAndroidFrameTimelineMetrics,
             "CollectAndroidFrameTimelineMetrics",
             FEATURE_DISABLED_BY_DEFAULT);

// If enabled, post registering PowerMonitor broadcast receiver to a background
// thread,
BASE_FEATURE(kPostPowerMonitorBroadcastReceiverInitToBackground,
             "PostPowerMonitorBroadcastReceiverInitToBackground",
             FEATURE_DISABLED_BY_DEFAULT);
// If enabled, getMyMemoryState IPC will be posted to background.
BASE_FEATURE(kPostGetMyMemoryStateToBackground,
             "PostGetMyMemoryStateToBackground",
             FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

void Init(EmitThreadControllerProfilerMetadata
              emit_thread_controller_profiler_metadata) {
  InitializeCpuReductionExperiment();
  sequence_manager::internal::SequenceManagerImpl::InitializeFeatures();
  sequence_manager::internal::ThreadController::InitializeFeatures(
      emit_thread_controller_profiler_metadata);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  MessagePumpEpoll::InitializeFeatures();
#endif

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_CHROMEOS)
  PlatformThread::InitializeFeatures();
#endif

#if BUILDFLAG(IS_APPLE)
  ConditionVariable::InitializeFeatures();
  File::InitializeFeatures();
  MessagePumpCFRunLoopBase::InitializeFeatures();
  MessagePumpKqueue::InitializeFeatures();
#endif

#if BUILDFLAG(IS_ANDROID)
  android::InputHintChecker::InitializeFeatures();
#endif

#if BUILDFLAG(IS_WIN)
  InitializePlatformThreadFeatures();
#endif
}

}  // namespace base::features
