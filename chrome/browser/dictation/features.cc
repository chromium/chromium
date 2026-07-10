// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/features.h"

#include "base/metrics/field_trial_params.h"

namespace dictation {

BASE_FEATURE(kDictation, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kDictationEvalMode{&kDictation, "eval_mode",
                                                  false};

const base::FeatureParam<bool> kUseComponentExtension{
    &kDictation, "use_component_extension", true};

const base::FeatureParam<bool> kSendContextAsync{&kDictation,
                                                 "send_context_async", false};

}  // namespace dictation
