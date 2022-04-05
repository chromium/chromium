// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/share_features.h"

#include "base/metrics/field_trial_params.h"

namespace share {

const base::Feature kPersistShareHubOnAppSwitch{
    "PersistShareHubOnAppSwitch", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kSharingDesktopScreenshotsEdit{
    "SharingDesktopScreenshotsEdit", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kUpcomingSharingFeatures{"UpcomingSharingFeatures",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

#if !BUILDFLAG(IS_ANDROID)
const base::Feature kDesktopSharePreview{"DesktopSharePreview",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
extern const char kDesktopSharePreviewVariant_HQ[] = "HQ";
extern const char kDesktopSharePreviewVariant_HQFavicon[] = "HQFavicon";
extern const char kDesktopSharePreviewVariant_HQFaviconFallback[] =
    "HQFaviconFallback";
extern const char kDesktopSharePreviewVariant_HQFallback[] = "HQFallback";

const base::FeatureParam<std::string> kDesktopSharePreviewVariant{
    &kDesktopSharePreview, "variant",
    kDesktopSharePreviewVariant_HQFaviconFallback};

namespace {

DesktopSharePreviewVariant DesktopSharePreviewVariantFromName(
    const std::string& name) {
  if (name == kDesktopSharePreviewVariant_HQ)
    return DesktopSharePreviewVariant::kEnabledHQ;
  if (name == kDesktopSharePreviewVariant_HQFavicon)
    return DesktopSharePreviewVariant::kEnabledHQFavicon;
  if (name == kDesktopSharePreviewVariant_HQFaviconFallback)
    return DesktopSharePreviewVariant::kEnabledHQFaviconFallback;
  if (name == kDesktopSharePreviewVariant_HQFallback)
    return DesktopSharePreviewVariant::kEnabledHQFallback;
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
