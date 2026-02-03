// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_service_accessor.h"

#include <algorithm>
#include <map>
#include <vector>

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "components/variations/synthetic_trial_registry.h"

namespace android_webview {
namespace {

struct ExperimentState {
  base::Lock lock;
  base::flat_set<int> experiments GUARDED_BY(lock);
};

ExperimentState& GetExperimentState() {
  static base::NoDestructor<ExperimentState> state;
  return *state;
}

}  // namespace

// static
void AwMetricsServiceAccessor::RegisterExternalExperiment(
    const std::vector<int>& experiment_ids) {
  TRACE_EVENT0("android_webview",
               "AwMetricsServiceAccessor::RegisterExternalExperiment");

  base::flat_set<int> incoming_ids(experiment_ids);

  ExperimentState& state = GetExperimentState();
  std::vector<int> final_ids;
  {
    base::AutoLock lock(state.lock);
    // Check if the experiment IDs have actually changed to avoid redundant
    // calls.
    if (state.experiments == incoming_ids) {
      return;
    }

    state.experiments = std::move(incoming_ids);
    final_ids.assign(state.experiments.begin(), state.experiments.end());
  }

  // We always use kOverrideExistingIds to ensure the latest set of active
  // experiments is reflected in the registry.
  AwMetricsServiceClient* client = AwMetricsServiceClient::GetInstance();
  if (metrics::MetricsService* service = client->GetMetricsService()) {
    service->GetSyntheticTrialRegistry()->RegisterExternalExperiments(
        base::PassKey<AwMetricsServiceAccessor>(), final_ids,
        variations::SyntheticTrialRegistry::kOverrideExistingIds);
  }
}

// static
void AwMetricsServiceAccessor::ClearAllExternalExperimentsForTesting() {
  ExperimentState& state = GetExperimentState();
  base::AutoLock lock(state.lock);
  state.experiments.clear();
}

}  // namespace android_webview
