// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_FEATURES_H_
#define CHROME_BROWSER_AI_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

// Feature flag for enabling foundational models in the AI API, requires the
// field param kModelVersionParam to specify the model version. Example:
// --enable-features=AIApiFoundationalModel:model_version/v4
BASE_DECLARE_FEATURE(kAIApiFoundationalModel);

namespace features {

BASE_DECLARE_FEATURE(kAILanguageModelOverrideConfiguration);
extern const base::FeatureParam<int>
    kAILanguageModelOverrideConfigurationOutputBuffer;

}  // namespace features

#endif  // CHROME_BROWSER_AI_FEATURES_H_
