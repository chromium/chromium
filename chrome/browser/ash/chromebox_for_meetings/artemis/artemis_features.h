// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_ARTEMIS_FEATURES_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_ARTEMIS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace ash::cfm::features {

// Enables dynamic cloud logging for Artemis.
BASE_DECLARE_FEATURE(kArtemisDynamicCloudLogging);

// A list of supplementary logs to collect, separated by the `|` symbol.
// NOTE: Forward slashes and periods must be %-encoded, i.e. %2F and %2E,
// respectively.
extern const base::FeatureParam<std::string> kSupplementaryLogs;

// The verbosity level for telemetry. Defaults to kWatchdog.
enum class TelemetryVerbosity {
  kWatchdog,
  kInfo,
  kVerbose,
};

extern const base::FeatureParam<TelemetryVerbosity> kTelemetryVerbosity;

// Configures various fetch/upload parameters in Artemis.
BASE_DECLARE_FEATURE(kArtemisConfig);

// Defines how often the data aggregator fetches data from each source.
extern const base::FeatureParam<base::TimeDelta> kFetchFrequency;

// Defines how often each log source ingests a new batch of logs.
extern const base::FeatureParam<base::TimeDelta> kLogPollFrequency;

// Defines the number of lines ingested in each log batch.
extern const base::FeatureParam<size_t> kLogBatchSize;

// Defines the size at which payloads are queued for upload.
extern const base::FeatureParam<size_t> kPayloadMaxSizeBytes;

// Defines the max internal payload queue size. The aggregator will temporarily
// halt fetches if we reach this size.
extern const base::FeatureParam<size_t> kPayloadQueueMaxSize;

}  // namespace ash::cfm::features

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_ARTEMIS_FEATURES_H_
