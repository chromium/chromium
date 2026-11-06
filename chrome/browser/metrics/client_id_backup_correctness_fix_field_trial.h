// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CLIENT_ID_BACKUP_CORRECTNESS_FIX_FIELD_TRIAL_H_
#define CHROME_BROWSER_METRICS_CLIENT_ID_BACKUP_CORRECTNESS_FIX_FIELD_TRIAL_H_

#include <string_view>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/version_info/channel.h"

namespace metrics {

inline constexpr std::string_view kClientIdBackupCorrectnessFixTrialName =
    "ClientIdBackupCorrectnessFix";

// Creates the client-side field trial controlling
// `features::kClientIdBackupCorrectnessFix` on POSIX platforms.
//
// TODO(crbug.com/497679558): Clean up once experimentation concludes.
void CreateClientIdBackupCorrectnessFixFieldTrial(
    base::FeatureList& feature_list,
    const base::FieldTrial::EntropyProvider& entropy_provider);

// Exposed for testing with an explicit `channel`.
void CreateClientIdBackupCorrectnessFixFieldTrialForTesting(
    base::FeatureList& feature_list,
    const base::FieldTrial::EntropyProvider& entropy_provider,
    version_info::Channel channel);

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_CLIENT_ID_BACKUP_CORRECTNESS_FIX_FIELD_TRIAL_H_
