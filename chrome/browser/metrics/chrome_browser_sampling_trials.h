// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_BROWSER_SAMPLING_TRIALS_H_
#define CHROME_BROWSER_METRICS_CHROME_BROWSER_SAMPLING_TRIALS_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"

namespace metrics {

// Create sampling trials to control metrics/crash sampling on Windows/Android
// if they do not exist (e.g., no variations seed was applied, or the variations
// seed did not contain the trials). On Windows, there is only one trial to
// control sampling. On Android, there are two (with different sampling rates,
// see crbug/1306481 for more details), but the client will only use one. The
// trial used depends on when metrics reporting was enabled. We create both
// trials regardless of which one the client would use at the time this is
// called, because the trial used may change during the session (e.g., if the
// user disables then re-enables metrics reporting during the same session).
void CreateFallbackSamplingTrialsIfNeeded(
    const base::FieldTrial::EntropyProvider& entropy_providers,
    base::FeatureList* feature_list);

// Create a field trial to control UKM sampling for Stable if it does not exist
// (e.g., no variations seed was applied, or the variations seed did not contain
// the trial). Note that UKM sampling is not per-client such as metrics/crash
// sampling (see CreateFallbackSamplingTrialsIfNeeded() above), but rather
// per-metric.
void CreateFallbackUkmSamplingTrialIfNeeded(
    const base::FieldTrial::EntropyProvider& entropy_providers,
    base::FeatureList* feature_list);

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_CHROME_BROWSER_SAMPLING_TRIALS_H_
