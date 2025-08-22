// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/features.h"

namespace features {

BASE_FEATURE(kAILanguageModelOverrideConfiguration,
             "kAILanguageModelOverrideConfiguration",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The number of tokens to use as a buffer for generating output. At least this
// many tokens will be available between the language model token limit and the
// max model tokens.
const base::FeatureParam<int> kAILanguageModelOverrideConfigurationOutputBuffer{
    &features::kAILanguageModelOverrideConfiguration,
    "ai_language_model_output_buffer", 1024};

BASE_FEATURE(kAIModelUnloadableProgress,
             "kAIModelUnloadableProgress",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The number of bytes that won't load when reporting downloadprogress until
// creation is completed.
//
// Calculated to occupy 10% of the loading bar when the model (currently
// 3556255776 bytes) isn't downloaded.
const base::FeatureParam<int> kAIModelUnloadableProgressBytes{
    &features::kAIModelUnloadableProgress, "ai_model_unloadable_progress_bytes",
    3556255776 / 9};

}  // namespace features
