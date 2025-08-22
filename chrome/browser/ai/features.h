// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_FEATURES_H_
#define CHROME_BROWSER_AI_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

BASE_DECLARE_FEATURE(kAILanguageModelOverrideConfiguration);
extern const base::FeatureParam<int>
    kAILanguageModelOverrideConfigurationOutputBuffer;

// When enabled some amount of bytes will not be loadable when creating one of
// the built-in AI APIs.
BASE_DECLARE_FEATURE(kAIModelUnloadableProgress);
extern const base::FeatureParam<int> kAIModelUnloadableProgressBytes;

}  // namespace features

#endif  // CHROME_BROWSER_AI_FEATURES_H_
