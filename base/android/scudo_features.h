// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_SCUDO_FEATURES_H_
#define BASE_ANDROID_SCUDO_FEATURES_H_

#include "base/base_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace base::features {

BASE_EXPORT BASE_DECLARE_FEATURE(kPeriodicScudoPurge);

BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                                       kScudoInitialPurgeDelay);
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                                       kScudoPeriodicPurgeInterval);
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                                       kScudoBackgroundPurgeDelay);
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(bool, kScudoEnableForegroundPeriodic);
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(bool, kScudoEnableBackgroundPurge);

}  // namespace base::features

#endif  // BASE_ANDROID_SCUDO_FEATURES_H_
