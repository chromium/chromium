// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/search_preload/search_preload_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "components/omnibox/browser/omnibox.mojom-shared.h"

namespace features {

BASE_FEATURE(kDsePreload2, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kDsePreload2UsePreloadServingMetrics{
    &kDsePreload2, "kDsePreload2UsePreloadServingMetrics", false};

const base::FeatureParam<size_t> kDsePreload2DeviceMemoryThresholdMiB{
    &kDsePreload2, "kDsePreload2DeviceMemoryThresholdMiB",
    // 3 GiB = 3 * 2**10 * 2**20
    3072};
const base::FeatureParam<base::TimeDelta> kDsePreload2ErrorBackoffDuration{
    &kDsePreload2, "kDsePreload2ErrorBackoffDuration",
    base::Milliseconds(60000)};
const base::FeatureParam<size_t> kDsePreload2MaxPrefetch{
    &kDsePreload2, "kDsePreload2MaxPrefetch", 7};
const base::FeatureParam<base::TimeDelta> kDsePreload2PrefetchTtl{
    &kDsePreload2, "kDsePreload2PrefetchTtl", base::Milliseconds(60000)};

BASE_FEATURE(kDsePreload2OnPress, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kDsePreload2OnPressMouseDown{
    &kDsePreload2, "kDsePreload2OnPressMouseDown", true};
const base::FeatureParam<bool> kDsePreload2OnPressUpOrDownArrowButton{
    &kDsePreload2, "kDsePreload2OnPressUpOrDownArrowButton", true};
const base::FeatureParam<bool> kDsePreload2OnPressTouchDown{
    &kDsePreload2, "kDsePreload2OnPressTouchDown", true};

BASE_FEATURE(kDsePreload2OnPressIncognito, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDsePreload2OnSuggestNonDefalutMatch,
             "kDsePreload2OnSuggestNonDefalutMatch",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDsePreload2Enabled() {
  return base::FeatureList::IsEnabled(kDsePreload2) &&
         static_cast<size_t>(base::SysInfo::AmountOfPhysicalMemory().InMiB()) >=
             kDsePreload2DeviceMemoryThresholdMiB.Get();
}

bool IsDsePreload2OnPressEnabled() {
  return base::FeatureList::IsEnabled(kDsePreload2) &&
         base::FeatureList::IsEnabled(kDsePreload2OnPress);
}

bool DsePreload2OnPressIsPredictorEnabled(
    omnibox::mojom::NavigationPredictor navigation_predictor) {
  switch (navigation_predictor) {
    case omnibox::mojom::NavigationPredictor::kMouseDown:
      return kDsePreload2OnPressMouseDown.Get();
    case omnibox::mojom::NavigationPredictor::kUpOrDownArrowButton:
      return kDsePreload2OnPressUpOrDownArrowButton.Get();
    case omnibox::mojom::NavigationPredictor::kTouchDown:
      return kDsePreload2OnPressTouchDown.Get();
  }
}

bool IsDsePreload2OnPressIncognitoEnabled() {
  return IsDsePreload2OnPressEnabled() &&
         base::FeatureList::IsEnabled(kDsePreload2OnPressIncognito);
}

}  // namespace features
