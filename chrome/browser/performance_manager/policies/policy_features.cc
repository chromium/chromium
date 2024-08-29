// Copyright 2019 The Chromium Authors
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

#if BUILDFLAG(IS_CHROMEOS_ASH)

BASE_FEATURE(kTrimOnMemoryPressure,
             "TrimOnMemoryPressure",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTrimArcOnMemoryPressure,
             "TrimArcOnMemoryPressure",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTrimArcVmOnMemoryPressure,
             "TrimArcVmOnMemoryPressure",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTrimOnFreeze, "TrimOnFreeze", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDisableTrimmingWhileSuspended,
             "DisableTrimmingWhileSuspended",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kGraphWalkBackoffTimeSec = {
    &kTrimOnMemoryPressure, "GraphWalkBackoffTimeSec", 180};

const base::FeatureParam<int> kArcProcessListFetchBackoffTimeSec = {
    &kTrimArcOnMemoryPressure, "ArcProcessListFetchBackoffTimeSec", 600};

const base::FeatureParam<int> kArcProcessTrimBackoffTimeSec = {
    &kTrimArcOnMemoryPressure, "ArcProcessTrimBackoffTimeSec", 1200};

const base::FeatureParam<bool> kTrimArcAppProcesses = {
    &kTrimArcOnMemoryPressure, "ArcTrimAppProcesses", true};

const base::FeatureParam<bool> kTrimArcSystemProcesses = {
    &kTrimArcOnMemoryPressure, "ArcTrimSystemProcesses", true};

const base::FeatureParam<bool> kTrimArcAggressive = {
    &kTrimArcOnMemoryPressure, "ArcTrimAggressive", false};

const base::FeatureParam<int> kArcMaxProcessesPerTrim = {
    &kTrimArcOnMemoryPressure, "ArcMaxProcessesPerTrim", 10};

const base::FeatureParam<int> kArcProcessInactivityTimeSec = {
    &kTrimArcOnMemoryPressure, "ArcProcessInactivityTimeSec", 600};

const base::FeatureParam<base::TimeDelta> kArcVmInactivityTimeMs = {
    &kTrimArcVmOnMemoryPressure, "ArcVmInactivityTimeMs", base::Seconds(1200)};

const base::FeatureParam<base::TimeDelta> kArcVmTrimBackoffTimeMs = {
    &kTrimArcVmOnMemoryPressure, "ArcVmTrimBackoffTimeMs", base::Seconds(1800)};

const base::FeatureParam<bool> kTrimArcVmOnCriticalPressure = {
    &kTrimArcVmOnMemoryPressure, "TrimArcVmOnCriticalPressure", false};

const base::FeatureParam<bool> kTrimArcVmOnFirstMemoryPressureAfterArcVmBoot = {
    &kTrimArcVmOnMemoryPressure, "TrimArcVmOnFirstMemoryPressureAfterArcVmBoot",
    true};

const base::FeatureParam<bool>
    kOnlyDropCachesOnFirstMemoryPressureAfterArcVmBoot = {
        &kTrimArcVmOnMemoryPressure,
        "OnlyDropCachesOnFirstMemoryPressureAfterArcVmBoot", true};

const base::FeatureParam<int> kTrimArcVmMaxPagesPerIteration = {
    &kTrimArcVmOnMemoryPressure, "MaxPageLimit", 300000};

const base::FeatureParam<int> kTrimArcVmPagesPerMinute = {
    &kTrimArcVmOnMemoryPressure, "PagesPerMinute", 15000};

// Specifies the minimum amount of time a parent frame node must be invisible
// before considering the process node for working set trim.
const int kNodeInvisibleTimeSec = 900;

// Specifies the minimum amount of time a parent frame node must be invisible
// before considering the process node for working set trim.
const int kNodeTrimBackoffTimeSec = 1800;

// Specifies the duration trimming is disabled just after suspend is done.
// Disabling trimming workingset for 15 mins after device is resumed has 2
// purposes:
//
// * To mitigate load pressure on system because the system is busy just after
//   resuming for a while.
// * GetTimeSinceLastVisibilityChange() of each node become meaningless because
//   the monotonic clock keeps proceeding during dark resume. Waiting for
//   kNodeInvisibleTimeSec after resuming ensures that enough time has elapsed
//   so that inappropriately added time from dark resume can no longer affect
//   whether or not a tab has been invisible for long enough to be eligible for
//   trimming.
const int kSuspendBackoffTimeSec = kNodeInvisibleTimeSec;

TrimOnMemoryPressureParams::TrimOnMemoryPressureParams() = default;
TrimOnMemoryPressureParams::TrimOnMemoryPressureParams(
    const TrimOnMemoryPressureParams&) = default;
TrimOnMemoryPressureParams& TrimOnMemoryPressureParams::operator=(
    const TrimOnMemoryPressureParams&) = default;

TrimOnMemoryPressureParams TrimOnMemoryPressureParams::GetParams() {
  TrimOnMemoryPressureParams params;
  params.graph_walk_backoff_time =
      base::Seconds(kGraphWalkBackoffTimeSec.Get());
  params.node_invisible_time = base::Seconds(kNodeInvisibleTimeSec);
  params.node_trim_backoff_time = base::Seconds(kNodeTrimBackoffTimeSec);
  params.suspend_backoff_time = base::Seconds(kSuspendBackoffTimeSec);

  params.arc_process_trim_backoff_time =
      base::Seconds(kArcProcessTrimBackoffTimeSec.Get());
  params.arc_process_list_fetch_backoff_time =
      base::Seconds(kArcProcessListFetchBackoffTimeSec.Get());
  params.trim_arc_system_processes = kTrimArcSystemProcesses.Get();
  params.trim_arc_app_processes = kTrimArcAppProcesses.Get();
  params.trim_arc_aggressive = kTrimArcAggressive.Get();
  params.arc_max_number_processes_per_trim = kArcMaxProcessesPerTrim.Get();

  const int arc_inactivity_time = kArcProcessInactivityTimeSec.Get();
  if (arc_inactivity_time > 0) {
    params.arc_process_inactivity_time = base::Seconds(arc_inactivity_time);
  } else {
    // This causes us to ignore the last activity time if it was not configured.
    params.arc_process_inactivity_time = base::TimeDelta::Min();
  }

  params.arcvm_inactivity_time = kArcVmInactivityTimeMs.Get();
  params.arcvm_trim_backoff_time = kArcVmTrimBackoffTimeMs.Get();
  params.trim_arcvm_on_critical_pressure = kTrimArcVmOnCriticalPressure.Get();
  params.trim_arcvm_on_first_memory_pressure_after_arcvm_boot =
      kTrimArcVmOnFirstMemoryPressureAfterArcVmBoot.Get();
  params.only_drop_caches_on_first_memory_pressure_after_arcvm_boot =
      kOnlyDropCachesOnFirstMemoryPressureAfterArcVmBoot.Get();

  params.trim_arcvm_max_pages_per_iteration =
      kTrimArcVmMaxPagesPerIteration.Get();
  params.trim_arcvm_pages_per_minute = kTrimArcVmPagesPerMinute.Get();

  return params;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace features
}  // namespace performance_manager
