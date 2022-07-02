// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_SHARE_FEATURES_H_
#define CHROME_BROWSER_SHARE_SHARE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/buildflag.h"

namespace share {

extern const base::Feature kPersistShareHubOnAppSwitch;
extern const base::Feature kScreenshotsForAndroidV2;
extern const base::Feature kSharingDesktopScreenshotsEdit;
extern const base::Feature kUpcomingSharingFeatures;

#if !BUILDFLAG(IS_ANDROID)
extern const base::Feature kDesktopSharePreview;

extern const char kDesktopSharePreviewVariant16[];
extern const char kDesktopSharePreviewVariant40[];
extern const char kDesktopSharePreviewVariant72[];

extern const base::FeatureParam<std::string> kDesktopSharePreviewVariant;

enum class DesktopSharePreviewVariant {
  kDisabled,
  kEnabled16,
  kEnabled40,
  kEnabled72,
};

DesktopSharePreviewVariant GetDesktopSharePreviewVariant();
#endif  // !BUILDFLAG(IS_ANDROID)

bool AreUpcomingSharingFeaturesEnabled();

}  // namespace share

#endif  // CHROME_BROWSER_SHARE_SHARE_FEATURES_H_
