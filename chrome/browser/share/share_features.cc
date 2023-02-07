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

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kDesktopSharePreview,
             "DesktopSharePreview",
             base::FEATURE_ENABLED_BY_DEFAULT);
extern const char kDesktopSharePreviewVariant16[] = "16";
extern const char kDesktopSharePreviewVariant40[] = "40";
extern const char kDesktopSharePreviewVariant72[] = "72";

const base::FeatureParam<std::string> kDesktopSharePreviewVariant{
    &kDesktopSharePreview, "variant", kDesktopSharePreviewVariant16};

namespace {

DesktopSharePreviewVariant DesktopSharePreviewVariantFromName(
    const std::string& name) {
  if (name == kDesktopSharePreviewVariant16)
    return DesktopSharePreviewVariant::kEnabled16;
  if (name == kDesktopSharePreviewVariant40)
    return DesktopSharePreviewVariant::kEnabled40;
  if (name == kDesktopSharePreviewVariant72)
    return DesktopSharePreviewVariant::kEnabled72;
  return DesktopSharePreviewVariant::kDisabled;
}

}  // namespace

DesktopSharePreviewVariant GetDesktopSharePreviewVariant() {
  // Note that if DesktopSharePreview is not enabled, this will return the
  // empty string.
  std::string variant_name =
      base::GetFieldTrialParamValueByFeature(kDesktopSharePreview, "variant");

  if (!variant_name.empty())
    return DesktopSharePreviewVariantFromName(variant_name);

  if (base::FeatureList::IsEnabled(kDesktopSharePreview) ||
      AreUpcomingSharingFeaturesEnabled()) {
    return DesktopSharePreviewVariantFromName(
        kDesktopSharePreviewVariant.default_value);
  }

  return DesktopSharePreviewVariant::kDisabled;
}
#endif  // !BUILDFLAG(IS_ANDROID)

bool AreUpcomingSharingFeaturesEnabled() {
  return base::FeatureList::IsEnabled(kUpcomingSharingFeatures);
}

}  // namespace share
