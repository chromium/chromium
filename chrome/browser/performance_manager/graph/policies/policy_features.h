// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if defined(OS_LINUX)
#include "base/allocator/buildflags.h"
#endif

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_POLICY_FEATURES_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_POLICY_FEATURES_H_

namespace performance_manager {
namespace features {

#if defined(OS_WIN)
// The EmptyWorkingSet feature as used on Windows.
extern const base::Feature kEmptyWorkingSet;
#endif  // defined(OS_WIN)

#if defined(OS_CHROMEOS)

// The trim on Memory Pressure feature will trim a process nodes working set
// according to the parameters below.
extern const base::Feature kTrimOnMemoryPressure;

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

struct TrimOnMemoryPressureParams {
  TrimOnMemoryPressureParams();
  TrimOnMemoryPressureParams(const TrimOnMemoryPressureParams& other);

  // GetParams will return this struct with the populated parameters below.
  static TrimOnMemoryPressureParams GetParams();

  base::TimeDelta graph_walk_backoff_time;
  base::TimeDelta node_invisible_time;
  base::TimeDelta node_trim_backoff_time;
};

#endif

#if defined(OS_LINUX)

#if BUILDFLAG(USE_TCMALLOC)
// If enabled then tcmalloc will be tuned dynamically based on system memory
// pressure.
extern const base::Feature kDynamicTcmallocTuning;

// The time in seconds between trying to tune renderers tcmalloc params.
extern const base::FeatureParam<int> kDynamicTuningTimeSec;

// The time in seconds a frame needs to be invisible before being further scaled
// down, -1 will disable this.
extern const base::FeatureParam<int> kDynamicTuningScaleInvisibleTimeSec;
#endif  // BUILDFLAG(USE_TCMALLOC)

#endif  // defined(OS_LINUX)

}  // namespace features
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_POLICY_FEATURES_H_
