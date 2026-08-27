// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_FIRST_RUN_DESKTOP_REFRESH_FIELD_TRIAL_H_
#define CHROME_BROWSER_SIGNIN_FIRST_RUN_DESKTOP_REFRESH_FIELD_TRIAL_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"

namespace signin {

// Creates the field trial to control `kFirstRunDesktopRefresh` and
// `kFirstRunDesktopChoiceScreenRefresh`.
//
// The trial is client controlled on Mac and Linux because the first run
// experience is shown on the very first run of Chrome. On these platforms, the
// variations seed is not available on the first run.
//
// Given that these features don't apply to subsequent runs (the first run
// experience is only triggered on the first run), it doesn't persist the trial
// group state to local prefs.
void CreateFirstRunDesktopRefreshFieldTrial(
    base::FeatureList& feature_list,
    const base::FieldTrial::EntropyProvider& entropy_provider);

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_FIRST_RUN_DESKTOP_REFRESH_FIELD_TRIAL_H_
