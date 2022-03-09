// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_SHARE_FEATURES_H_
#define CHROME_BROWSER_SHARE_SHARE_FEATURES_H_

#include "base/feature_list.h"

namespace share {

extern const base::Feature kPersistShareHubOnAppSwitch;
extern const base::Feature kSharingDesktopScreenshotsEdit;
extern const base::Feature kUpcomingSharingFeatures;

bool AreUpcomingSharingFeaturesEnabled();

}  // namespace share

#endif  // CHROME_BROWSER_SHARE_SHARE_FEATURES_H_
