// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_USAGE_SCENARIO_H_
#define CHROME_BROWSER_METRICS_POWER_USAGE_SCENARIO_H_

#include <vector>

#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Contains data to determine when and how to generate histograms and trace
// events for a usage scenario.
struct ScenarioParams {
  const char* histogram_suffix;
  // CPU usage threshold to emit a "high CPU" trace event.
  double short_interval_cpu_threshold;
  const char* trace_event_title;
};

// Returns the suffixes to use for histograms and UKMs related to a long
// interval described by `interval_data`.
std::vector<const char*> GetLongIntervalSuffixes(
    const UsageScenarioDataStore::IntervalData& interval_data);

#if BUILDFLAG(IS_MAC)
// Returns params to use for histograms and trace events related to a short
// interval described by `short_interval_data`. `pre_interval_data` describes
// a long interval ending simultaneously with the short interval.
//
// `pre_interval_data` is required to decide whether "_Recent" is appended to
// the ".ZeroWindow" or ".AllTabsHidden_NoVideoCaptureOrAudio" suffixes.
// Appending "_Recent" is useful  to isolate cases where the scenario changed
// recently (e.g. CPU usage in a short interval with zero window might be
// affected by cleanup tasks from recently closed tabs).
const ScenarioParams& GetShortIntervalScenarioParams(
    const UsageScenarioDataStore::IntervalData& short_interval_data,
    const UsageScenarioDataStore::IntervalData& pre_interval_data);
#endif  // BUILDFLAG(IS_MAC)

#endif  // CHROME_BROWSER_METRICS_POWER_USAGE_SCENARIO_H_
