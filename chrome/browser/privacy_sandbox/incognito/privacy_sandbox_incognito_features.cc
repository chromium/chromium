// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privacy_sandbox_incognito_features.h"

#include "base/feature_list.h"

namespace privacy_sandbox {

BASE_FEATURE(kPrivacySandboxActSurvey,
             "PrivacySandboxActSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kPrivacySandboxActSurveyDelay{
    &kPrivacySandboxActSurvey, "delay", base::Seconds(0)};

const base::FeatureParam<bool> kPrivacySandboxActSurveyDelayRandomize{
    &kPrivacySandboxActSurvey, "delay_randomize", false};

const base::FeatureParam<base::TimeDelta> kPrivacySandboxActSurveyDelayMin{
    &kPrivacySandboxActSurvey, "delay_min", base::Seconds(0)};

const base::FeatureParam<base::TimeDelta> kPrivacySandboxActSurveyDelayMax{
    &kPrivacySandboxActSurvey, "delay_max", base::Seconds(0)};

}  // namespace privacy_sandbox
