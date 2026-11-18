// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/features.h"

#include "base/feature_list.h"

// Feature flag for enabling foundational models in the AI API, requires the
// field param kModelVersionParam to specify the model version. Example:
// --enable-features=AIApiFoundationalModel:model_version/v4
BASE_FEATURE(kAIApiFoundationalModel, base::FEATURE_DISABLED_BY_DEFAULT);

namespace features {

BASE_FEATURE(kAILanguageModelOverrideConfiguration,
             base::FEATURE_ENABLED_BY_DEFAULT);

// The number of tokens to use as a buffer for generating output. At least this
// many tokens will be available between the language model token limit and the
// max model tokens.
const base::FeatureParam<int> kAILanguageModelOverrideConfigurationOutputBuffer{
    &features::kAILanguageModelOverrideConfiguration,
    "ai_language_model_output_buffer", 1024};

}  // namespace features
