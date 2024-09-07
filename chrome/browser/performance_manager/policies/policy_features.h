// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/session/arc_session.h"
#include "base/allocator/buildflags.h"
#endif

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_POLICY_FEATURES_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_POLICY_FEATURES_H_

namespace performance_manager {
namespace features {

#if BUILDFLAG(IS_CHROMEOS_ASH)

// The trim on Memory Pressure feature will trim a process nodes working set
// according to the parameters below.
BASE_DECLARE_FEATURE(kTrimOnMemoryPressure);

// If enabled we will periodically walk procfs looking for ARC++ processes to
// trim under memory pressure.
BASE_DECLARE_FEATURE(kTrimArcOnMemoryPressure);

// If enabled we will try to trim ARCVM's crosvm under memory pressure.
BASE_DECLARE_FEATURE(kTrimArcVmOnMemoryPressure);

// The trim on freeze feature will trim the working set of a process when all
// frames are frozen.
BASE_DECLARE_FEATURE(kTrimOnFreeze);

// If enabled, this disables trimming process nodes and ARC++ processes and
// ARCVM under memory pressure while the system is suspended. The system can run
// while the device is suspended if dark resume feature is enabled.
BASE_DECLARE_FEATURE(kDisableTrimmingWhileSuspended);

// The graph walk backoff is the _minimum_ backoff time between graph walks
// under moderate pressure in seconds. By default we will not walk more than
// once every 2 minutes.
extern const base::FeatureParam<int> kGraphWalkBackoffTimeSec;

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

// If true then we will drop ARCVM guest page caches once on the first moderate
// (or critical though unlikely) memory pressure after ARCVM boot. The regular
// trimming (i.e. moving pages to zram) is not performed, and the page cache
// drop is done regardless of the user's interactions with ARCVM.
extern const base::FeatureParam<bool>
    kTrimArcVmOnFirstMemoryPressureAfterArcVmBoot;

// Deprecated.
// TODO(yusukes): Remove this once ChromeOSARCVMReclaimThrottle.gcl Finch
// experiment is done.
extern const base::FeatureParam<bool>
    kOnlyDropCachesOnFirstMemoryPressureAfterArcVmBoot;

// Limits the number of pages to reclaim on each iteration.
// Zero means "no ceiling limit" - though reclaim is still possibly limited by
// kTrimArcVmPagesPerMinute, if that is set.
// When both limits are set, the lesser (stricter, lower limit) is used.
// This limits jank caused by reclaim, by making
// each reclaim operation short.
extern const base::FeatureParam<int> kTrimArcVmMaxPagesPerIteration;

// Works in combination with kTrimArcVmMaxPagesPerIteration. The intent
// is to limit the rate of pages reclaimed over time, so we specify that
// explicitly.
// Zero means "no per-minute page limit", though reclaim is still possibly
// limited by kTrimArcVmMaxPagesPerIteration.
// When both limits are set, the lesser (stricter, lower limit) is used.
extern const base::FeatureParam<int> kTrimArcVmPagesPerMinute;

struct TrimOnMemoryPressureParams {
  TrimOnMemoryPressureParams();
  TrimOnMemoryPressureParams(const TrimOnMemoryPressureParams&);
  TrimOnMemoryPressureParams& operator=(const TrimOnMemoryPressureParams&);

  // GetParams will return this struct with the populated parameters below.
  static TrimOnMemoryPressureParams GetParams();

  base::TimeDelta graph_walk_backoff_time;
  base::TimeDelta node_invisible_time;
  base::TimeDelta node_trim_backoff_time;
  base::TimeDelta suspend_backoff_time;

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
  int trim_arcvm_max_pages_per_iteration = arc::ArcSession::kNoPageLimit;
  int trim_arcvm_pages_per_minute = arc::ArcSession::kNoPageLimit;
};

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace features
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_POLICY_FEATURES_H_
