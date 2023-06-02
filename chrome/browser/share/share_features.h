// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_SHARE_FEATURES_H_
#define CHROME_BROWSER_SHARE_SHARE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/buildflag.h"

namespace share {

BASE_DECLARE_FEATURE(kScreenshotsForAndroidV2);

bool AreUpcomingSharingFeaturesEnabled();

}  // namespace share

#endif  // CHROME_BROWSER_SHARE_SHARE_FEATURES_H_
