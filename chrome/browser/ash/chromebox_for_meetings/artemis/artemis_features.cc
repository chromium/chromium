// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/artemis/artemis_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

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

BASE_FEATURE(kArtemisConfig, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kFetchFrequency{
    &kArtemisConfig, "FetchFrequency", base::Minutes(1)};

const base::FeatureParam<base::TimeDelta> kLogPollFrequency{
    &kArtemisConfig, "LogPollFrequency", base::Seconds(10)};

const base::FeatureParam<size_t> kLogBatchSize{&kArtemisConfig, "LogBatchSize",
                                               100};  // # lines

const base::FeatureParam<size_t> kPayloadMaxSizeBytes{
    &kArtemisConfig, "PayloadMaxSizeBytes", 50 * 1000};  // 50Kb

const base::FeatureParam<size_t> kPayloadQueueMaxSize{
    &kArtemisConfig, "PayloadQueueMaxSize", 10};  // # payloads

}  // namespace ash::cfm::features
