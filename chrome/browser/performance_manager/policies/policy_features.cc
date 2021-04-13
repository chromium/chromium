// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/policy_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/allocator/buildflags.h"
#endif

namespace performance_manager {
namespace features {

#if defined(OS_WIN)
// Empty the working set of processes in which all frames are frozen.
const base::Feature kEmptyWorkingSet{"EmptyWorkingSet",
                                     base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)

const base::Feature kTrimOnMemoryPressure{"TrimOnMemoryPressure",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kTrimArcOnMemoryPressure{"TrimArcOnMemoryPressure",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTrimOnFreeze{"TrimOnFreeze",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kGraphWalkBackoffTimeSec = {
    &kTrimOnMemoryPressure, "GraphWalkBackoffTimeSec", 180};

const base::FeatureParam<int> kArcProcessListFetchBackoffTimeSec = {
    &kTrimArcOnMemoryPressure, "ArcProcessListFetchBackoffTimeSec", 180};

const base::FeatureParam<int> kArcProcessTrimBackoffTimeSec = {
    &kTrimArcOnMemoryPressure, "ArcProcessTrimBackoffTimeSec", 1800};

const base::FeatureParam<bool> kTrimArcAppProcesses = {
    &kTrimArcOnMemoryPressure, "ArcTrimAppProcesses", true};

const base::FeatureParam<bool> kTrimArcSystemProcesses = {
    &kTrimArcOnMemoryPressure, "ArcTrimSystemProcesses", true};

const base::FeatureParam<bool> kTrimArcAggressive = {
    &kTrimArcOnMemoryPressure, "ArcTrimAggressive", false};

const base::FeatureParam<int> kArcMaxProcessesPerTrim = {
    &kTrimArcOnMemoryPressure, "ArcMaxProcessesPerTrim", -1};

const base::FeatureParam<int> kArcProcessInactivityTimeSec = {
    &kTrimArcOnMemoryPressure, "ArcProcessInactivityTimeSec", 300};

// Specifies the minimum amount of time a parent frame node must be invisible
// before considering the process node for working set trim.
const base::FeatureParam<int> kNodeInvisibileTimeSec = {
    &kTrimOnMemoryPressure, "NodeInvisibleTimeSec", 900};

// Specifies the minimum amount of time a parent frame node must be invisible
// before considering the process node for working set trim.
const base::FeatureParam<int> kNodeTrimBackoffTimeSec = {
    &kTrimOnMemoryPressure, "NodeTrimBackoffTimeSec", 1800};

TrimOnMemoryPressureParams::TrimOnMemoryPressureParams() = default;
TrimOnMemoryPressureParams::TrimOnMemoryPressureParams(
    const TrimOnMemoryPressureParams& other) = default;

TrimOnMemoryPressureParams TrimOnMemoryPressureParams::GetParams() {
  TrimOnMemoryPressureParams params;
  params.graph_walk_backoff_time =
      base::TimeDelta::FromSeconds(kGraphWalkBackoffTimeSec.Get());
  params.node_invisible_time =
      base::TimeDelta::FromSeconds(kNodeInvisibileTimeSec.Get());
  params.node_trim_backoff_time =
      base::TimeDelta::FromSeconds(kNodeTrimBackoffTimeSec.Get());

  params.arc_process_trim_backoff_time =
      base::TimeDelta::FromSeconds(kArcProcessTrimBackoffTimeSec.Get());
  params.arc_process_list_fetch_backoff_time =
      base::TimeDelta::FromSeconds(kArcProcessListFetchBackoffTimeSec.Get());
  params.trim_arc_system_processes = kTrimArcSystemProcesses.Get();
  params.trim_arc_app_processes = kTrimArcAppProcesses.Get();
  params.trim_arc_aggressive = kTrimArcAggressive.Get();
  params.arc_max_number_processes_per_trim = kArcMaxProcessesPerTrim.Get();

  const int arc_inactivity_time = kArcProcessInactivityTimeSec.Get();
  if (arc_inactivity_time > 0) {
    params.arc_process_inactivity_time =
        base::TimeDelta::FromSeconds(arc_inactivity_time);
  } else {
    // This causes us to ignore the last activity time if it was not configured.
    params.arc_process_inactivity_time = base::TimeDelta::Min();
  }
  return params;
}

#if BUILDFLAG(USE_TCMALLOC)
// This flag will allow the browser process to adjust the tcmalloc tunables to
// balance performance and memory utilization.
const base::Feature kDynamicTcmallocTuning{"DynamicTcmallocTuning",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// The time between attempting to update tcmalloc tunables.
const base::FeatureParam<int> kDynamicTuningTimeSec = {
    &kDynamicTcmallocTuning, "DynamicTcmallocTuneTimeSec", 120};

// The time a frame must be invisible before being additionally scaled. -1 will
// disable invisible scaling.
const base::FeatureParam<int> kDynamicTuningScaleInvisibleTimeSec = {
    &kDynamicTcmallocTuning, "DynamicTcmallocScaleInvisibleTimeSec", -1};
#endif  // BUILDFLAG(USE_TCMALLOC)

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !defined(OS_ANDROID)
const base::Feature kPageFreezingFromPerformanceManager{
    "PageFreezingFromPerformanceManager", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUrgentDiscardingFromPerformanceManager{
  "UrgentDiscardingFromPerformanceManager",
#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_LINUX)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

UrgentDiscardingParams::UrgentDiscardingParams() = default;
UrgentDiscardingParams::UrgentDiscardingParams(
    const UrgentDiscardingParams& rhs) = default;
UrgentDiscardingParams::~UrgentDiscardingParams() = default;

constexpr base::FeatureParam<int> UrgentDiscardingParams::kDiscardStrategy;

// static
UrgentDiscardingParams UrgentDiscardingParams::GetParams() {
  UrgentDiscardingParams params = {};
  params.discard_strategy_ = static_cast<DiscardStrategy>(
      UrgentDiscardingParams::kDiscardStrategy.Get());
  return params;
}

const base::Feature kBackgroundTabLoadingFromPerformanceManager{
    "BackgroundTabLoadingFromPerformanceManager",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kHighPMFDiscardPolicy{"HighPMFDiscardPolicy",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace features
}  // namespace performance_manager
