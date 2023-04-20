// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/share_features.h"

#include "base/metrics/field_trial_params.h"

namespace share {

BASE_FEATURE(kScreenshotsForAndroidV2,
             "ScreenshotsForAndroidV2",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kUpcomingSharingFeatures,
             "UpcomingSharingFeatures",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kShareToGoogleCollections,
             "ShareToGoogleCollections",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kCrowLaunchTab,
             "ShareCrowLaunchTab",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

bool AreUpcomingSharingFeaturesEnabled() {
  return base::FeatureList::IsEnabled(kUpcomingSharingFeatures);
}

}  // namespace share
