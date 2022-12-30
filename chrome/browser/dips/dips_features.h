// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_FEATURES_H_
#define CHROME_BROWSER_DIPS_DIPS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "chrome/browser/dips/dips_utils.h"

namespace dips {

BASE_DECLARE_FEATURE(kFeature);
extern const base::FeatureParam<bool> kPersistedDatabaseEnabled;
extern const base::FeatureParam<base::TimeDelta> kGracePeriod;
extern const base::FeatureParam<base::TimeDelta> kTimerDelay;
extern const base::FeatureParam<base::TimeDelta> kInteractionTtl;
extern const base::FeatureParam<DIPSTriggeringAction> kTriggeringAction;

}  // namespace dips

#endif  // CHROME_BROWSER_DIPS_DIPS_FEATURES_H_
