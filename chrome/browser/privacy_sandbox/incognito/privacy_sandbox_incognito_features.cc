// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privacy_sandbox_incognito_features.h"

#include "base/feature_list.h"

namespace privacy_sandbox {

BASE_FEATURE(kPrivacySandboxActWhatsNew, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxWhatsNewSurvey, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kPrivacySandboxWhatsNewSurveyDelay{
    &kPrivacySandboxWhatsNewSurvey, "delay", base::Seconds(1)};

}  // namespace privacy_sandbox
