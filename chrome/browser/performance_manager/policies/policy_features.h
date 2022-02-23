// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/allocator/buildflags.h"
#endif

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_POLICY_FEATURES_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_POLICY_FEATURES_H_

namespace performance_manager {
namespace features {

#if BUILDFLAG(IS_WIN)
// The EmptyWorkingSet feature as used on Windows.
extern const base::Feature kEmptyWorkingSet;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)

// The trim on Memory Pressure feature will trim a process nodes working set
// according to the parameters below.
extern const base::Feature kTrimOnMemoryPressure;

// If enabled we will periodically walk procfs looking for ARC++ processes to
// trim under memory pressure.
extern const base::Feature kTrimArcOnMemoryPressure;

// If enabled we will try to trim ARCVM's crosvm under memory pressure.
extern const base::Feature kTrimArcVmOnMemoryPressure;

// The trim on freeze feature will trim the working set of a process when all
// frames are frozen.
extern const base::Feature kTrimOnFreeze;

// The graph walk backoff is the _minimum_ backoff time between graph walks
// under moderate pressure in seconds. By default we will not walk more than
// once every 2 minutes.
extern const base::FeatureParam<int> kGraphWalkBackoffTimeSec;

// Specifies the minimum amount of time a parent frame node must be invisible
// before considering the process node for working set trim.
extern const base::FeatureParam<int> kNodeInvisibileTimeSec;

// Specifies the minimum amount of time a parent frame node must be invisible
// before considering the process node for working set trim.
extern const base::FeatureParam<int> kNodeTrimBackoffTimeSec;

// Specifies the frequency in which we will fetch the arc process list.
extern const base::FeatureParam<int> kArcProcessListFetchBackoffTimeSec;

// Specifies the frequency at which an individual arc++ process can be trimmed.
extern const base::FeatureParam<int> kArcProcessTrimBackoffTimeSec;

// If true then we will trim ARC App processes.
extern const base::FeatureParam<bool> kTrimArcAppProcesses;

// If true then we will trim ARC System processes.
extern const base::FeatureParam<bool> kTrimArcSystemProcesses;

// If true then we will trim all processes, regardless of state this is for
// experimentation to see the tradeoff of trimming all apps vs. just the
// unimportant ones.
extern const base::FeatureParam<bool> kTrimArcAggressive;

// If set to a value greater than -1, this is the maximum number of processes we
// will target on each iteration. Where the frequency is defined by
// kArcProcessListFetchBackoffTimeSec. NOTE: This value is the max for APP or
// SYSTEM processes, meaning a value of 5 would allow 5 apps to be trimmed and 5
// system processes to be trimmed.
extern const base::FeatureParam<int> kArcMaxProcessesPerTrim;

// If set to a value greater than -1, this is the minimum amount of time an ARC
// process must have been inactive before it's eligible for reclaim.
extern const base::FeatureParam<int> kArcProcessInactivityTimeSec;

// The minimum amount of time an ARCVM must have been inactive before it's
// eligible for reclaim.
extern const base::FeatureParam<base::TimeDelta> kArcVmInactivityTimeMs;

// Specifies the frequency at which ARCVM's crosvm process can be trimmed.
extern const base::FeatureParam<base::TimeDelta> kArcVmTrimBackoffTimeMs;

// If true then we will trim ARCVM's crosvm on critical memory pressure
// regardless of the user's interactions with ARCVM.
extern const base::FeatureParam<bool> kTrimArcVmOnCriticalPressure;

// If true then we will trim ARCVM's crosvm once on the first moderate (or
// critical though unlikely) memory pressure after ARCVM boot. The trimming is
// done regardless of the user's interactions with ARCVM.
extern const base::FeatureParam<bool>
    kTrimArcVmOnFirstMemoryPressureAfterArcVmBoot;

// If true then we will drop ARCVM guest's page caches when the trimmer does
// kTrimArcVmOnFirstMemoryPressureAfterArcVmBoot. If false (default), it also
// trims ARCVM's shared memory.
extern const base::FeatureParam<bool>
    kOnlyDropCachesOnFirstMemoryPressureAfterArcVmBoot;

struct TrimOnMemoryPressureParams {
  TrimOnMemoryPressureParams();
  TrimOnMemoryPressureParams(const TrimOnMemoryPressureParams&);
  TrimOnMemoryPressureParams& operator=(const TrimOnMemoryPressureParams&);

  // GetParams will return this struct with the populated parameters below.
  static TrimOnMemoryPressureParams GetParams();

  base::TimeDelta graph_walk_backoff_time;
  base::TimeDelta node_invisible_time;
  base::TimeDelta node_trim_backoff_time;

  // These are used when kTrimArcOnMemoryPressure is enabled.
  base::TimeDelta arc_process_trim_backoff_time;
  base::TimeDelta arc_process_list_fetch_backoff_time;
  bool trim_arc_app_processes = false;
  bool trim_arc_system_processes = false;
  bool trim_arc_aggressive = false;
  int arc_max_number_processes_per_trim = -1;
  base::TimeDelta arc_process_inactivity_time;

  // These are used when kTrimArcVmOnMemoryPressure is enabled.
  base::TimeDelta arcvm_inactivity_time;
  base::TimeDelta arcvm_trim_backoff_time;
  bool trim_arcvm_on_critical_pressure = false;
  bool trim_arcvm_on_first_memory_pressure_after_arcvm_boot = false;
  bool only_drop_caches_on_first_memory_pressure_after_arcvm_boot = false;
};

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace features
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_POLICY_FEATURES_H_
