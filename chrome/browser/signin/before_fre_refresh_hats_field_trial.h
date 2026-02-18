// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BEFORE_FRE_REFRESH_HATS_FIELD_TRIAL_H_
#define CHROME_BROWSER_SIGNIN_BEFORE_FRE_REFRESH_HATS_FIELD_TRIAL_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"

namespace signin {

// Creates the field trial to control `kBeforeFirstRunDesktopRefreshSurvey`.
//
// The trial is client controlled on Mac and Linux because the survey is
// triggered on the very first run of Chrome. On these platforms, the
// variations seed is not available on the first run.
//
// Given that this feature doesn't apply to subsequent runs (the survey is only
// triggered on the first run), it doesn't persist the trial group state to
// local prefs.
void CreateBeforeFreRefreshHatsFieldTrial(
    base::FeatureList& feature_list,
    const base::FieldTrial::EntropyProvider& entropy_provider);

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_BEFORE_FRE_REFRESH_HATS_FIELD_TRIAL_H_
