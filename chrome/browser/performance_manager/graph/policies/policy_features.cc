// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/policies/policy_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if defined(OS_LINUX)
#include "base/allocator/buildflags.h"
#endif

namespace performance_manager {
namespace features {

#if defined(OS_WIN)
// Empty the working set of processes in which all frames are frozen.
const base::Feature kEmptyWorkingSet{"EmptyWorkingSet",
                                     base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_WIN)

#if defined(OS_CHROMEOS)

const base::Feature kTrimOnMemoryPressure{"TrimOnMemoryPressure",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTrimOnFreeze{"TrimOnFreeze",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kGraphWalkBackoffTimeSec = {
    &kTrimOnMemoryPressure, "GraphWalkBackoffTimeSec", 120};

// Specifies the minimum amount of time a parent frame node must be invisible
// before considering the process node for working set trim.
const base::FeatureParam<int> kNodeInvisibileTimeSec = {
    &kTrimOnMemoryPressure, "NodeInvisibleTimeSec", 360};

// Specifies the minimum amount of time a parent frame node must be invisible
// before considering the process node for working set trim.
const base::FeatureParam<int> kNodeTrimBackoffTimeSec = {
    &kTrimOnMemoryPressure, "NodeTrimBackoffTimeSec", 1200};

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
  return params;
}

#endif  // defined(OS_CHROMEOS)

#if defined(OS_LINUX)

#if BUILDFLAG(USE_TCMALLOC)
// This flag will allow the browser process to adjust the tcmalloc tunables to
// balance performance and memory utilization.
const base::Feature kDynamicTcmallocTuning{"DynamicTcmallocTuning",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// The time between attempting to update tcmalloc tunables.
const base::FeatureParam<int> kDynamicTuningTimeSec = {
    &kDynamicTcmallocTuning, "DynamicTcmallocTuneTimeSec", 120};

// The time a frame must be invisible before being additionally scaled. -1 will
// disable invisible scaling.
extern const base::FeatureParam<int> kDynamicTuningScaleInvisibleTimeSec = {
    &kDynamicTcmallocTuning, "DynamicTcmallocScaleInvisibleTimeSec", -1};
#endif  // BUILDFLAG(USE_TCMALLOC)

#endif  // defined(OS_LINUX)

}  // namespace features
}  // namespace performance_manager
