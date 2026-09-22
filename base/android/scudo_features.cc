// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scudo_features.h"

#include "base/feature_list.h"
#include "base/time/time.h"

namespace base::features {

BASE_FEATURE(kPeriodicScudoPurge, FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kScudoInitialPurgeDelay,
                   &kPeriodicScudoPurge,
                   base::Seconds(60));

BASE_FEATURE_PARAM(base::TimeDelta,
                   kScudoPeriodicPurgeInterval,
                   &kPeriodicScudoPurge,
                   base::Seconds(60));

BASE_FEATURE_PARAM(base::TimeDelta,
                   kScudoBackgroundPurgeDelay,
                   &kPeriodicScudoPurge,
                   base::Seconds(10));

BASE_FEATURE_PARAM(bool,
                   kScudoEnableForegroundPeriodic,
                   &kPeriodicScudoPurge,
                   true);

BASE_FEATURE_PARAM(bool,
                   kScudoEnableBackgroundPurge,
                   &kPeriodicScudoPurge,
                   true);

}  // namespace base::features
