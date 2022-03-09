// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/share_features.h"

namespace share {

const base::Feature kPersistShareHubOnAppSwitch{
    "PersistShareHubOnAppSwitch", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kSharingDesktopScreenshotsEdit{
    "SharingDesktopScreenshotsEdit", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kUpcomingSharingFeatures{"UpcomingSharingFeatures",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

bool AreUpcomingSharingFeaturesEnabled() {
  return base::FeatureList::IsEnabled(kUpcomingSharingFeatures);
}

}  // namespace share
