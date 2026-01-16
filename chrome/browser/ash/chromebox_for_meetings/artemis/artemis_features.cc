// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/artemis/artemis_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace ash::cfm::features {

BASE_FEATURE(kArtemisDynamicCloudLogging, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kSupplementaryLogs{
    &kArtemisDynamicCloudLogging, "SupplementaryLogs", ""};

constexpr base::FeatureParam<TelemetryVerbosity>::Option
    kTelemetryVerbosityOptions[] = {
        {TelemetryVerbosity::kWatchdog, "WATCHDOG"},
        {TelemetryVerbosity::kInfo, "INFO"},
        {TelemetryVerbosity::kVerbose, "VERBOSE"},
};

const base::FeatureParam<TelemetryVerbosity> kTelemetryVerbosity{
    &kArtemisDynamicCloudLogging, "TelemetryVerbosity",
    TelemetryVerbosity::kWatchdog, &kTelemetryVerbosityOptions};

}  // namespace ash::cfm::features
