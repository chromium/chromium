// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/features.h"

namespace features {

BASE_FEATURE(kAIAssistantOverrideConfiguration,
             "kAIAssistantOverrideConfiguration",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kAIAssistantOverrideConfigurationMaxTopK{
    &features::kAIAssistantOverrideConfiguration, "max_top_k", 8};

const base::FeatureParam<double>
    kAIAssistantOverrideConfigurationDefaultTemperature{
        &features::kAIAssistantOverrideConfiguration, "default_temperature",
        1.0};

}  // namespace features
