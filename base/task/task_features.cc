// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_features.h"

#include <atomic>

#include "base/base_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"

namespace base {

// Note to implementers: thread pool code using task features must absolutely
// not invoke FeatureList::IsEnabled outside of the main thread. Doing so
// causes data races between worker threads and ~FeatureList when tests end
// (crbug.com/1344573). A reliable moment to query and cache the feature state
// is on ThreadPoolImpl::Start (and thus also on the first WorkerThread::Start,
// not the later ones) as this is invoked from the main thread after
// initializing the FeatureList. If caching the feature state in a static, you
// must be aware that all tests sharing a process will have the same state,
// regardless of future ScopedFeatureList instances.

BASE_FEATURE(kUseUtilityThreadGroup,
             "UseUtilityThreadGroup",
             FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAddTaskLeewayFeature,
             "AddTaskLeeway",
             FEATURE_ENABLED_BY_DEFAULT);

// Note: Do not use the prepared macro as of no need for a local cache.
constinit const FeatureParam<TimeDelta> kTaskLeewayParam{
    &kAddTaskLeewayFeature, "leeway", kDefaultLeeway};
BASE_FEATURE_PARAM(TimeDelta,
                   kMaxPreciseDelay,
                   &kAddTaskLeewayFeature,
                   "max_precise_delay",
                   kDefaultMaxPreciseDelay);

BASE_FEATURE(kAlignWakeUps, "AlignWakeUps", FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTimerSlackMac, "TimerSlackMac", FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExplicitHighResolutionTimerWin,
             "ExplicitHighResolutionTimerWin",
             FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUIPumpImprovementsWin,
             "UIPumpImprovementsWin",
             FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPumpFastToSleepAndroid,
             "PumpFastToSleepAndroid",
             FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRunTasksByBatches,
             "RunTasksByBatches",
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
             FEATURE_ENABLED_BY_DEFAULT);
#else
             FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kThreadPoolCap2, "ThreadPoolCap2", FEATURE_DISABLED_BY_DEFAULT);

// Note: Do not use the prepared macro as of no need for a local cache.
constinit const FeatureParam<int> kThreadPoolCapRestrictedCount{
    &kThreadPoolCap2, "restricted_count", 3};

}  // namespace base
