// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_ARTEMIS_FEATURES_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_ARTEMIS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

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

}  // namespace ash::cfm::features

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_ARTEMIS_FEATURES_H_
