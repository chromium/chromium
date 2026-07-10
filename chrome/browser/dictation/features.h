// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_FEATURES_H_
#define CHROME_BROWSER_DICTATION_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace dictation {

// Enables the dictation feature.
BASE_DECLARE_FEATURE(kDictation);

// Runs dictation streams in "eval mode" which can be used to record inputs
// usable for an eval.
extern const base::FeatureParam<bool> kDictationEvalMode;

// If true, uses the component extension for dictation. Can be set to false
// which prevents installation of the component extension and relies on the user
// or test installing a regular extension to handle communication via the
// dictationPrivate API. This is used in tests and for local development of the
// extension.
extern const base::FeatureParam<bool> kUseComponentExtension;

// If true, the dictation context is provided asynchronously after the stream is
// started. If false dictation context blocks stream creation and context is
// provided in the StartStream message.
extern const base::FeatureParam<bool> kSendContextAsync;

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_FEATURES_H_
