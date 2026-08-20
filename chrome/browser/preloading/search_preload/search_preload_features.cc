// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/search_preload/search_preload_features.h"

#include "base/byte_size.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox.mojom-shared.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace features {

BASE_FEATURE(kDsePreload2, base::FEATURE_DISABLED_BY_DEFAULT);

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
// An empty string means that do not use initial No-Vary-Search hint.
const base::FeatureParam<std::string> kDsePreload2InitialNoVarySearchHint{
    &kDsePreload2, "dse_preload2_initial_no_vary_search_hint", ""};

const base::FeatureParam<DsePreload2PrefetchPriorityPolicy>::Option
    kDsePreload2PrefetchPriorityPolicyOptions[] = {
        {DsePreload2PrefetchPriorityPolicy::kSearchPrefetchCompat,
         "SearchPrefetchCompat"},
        {DsePreload2PrefetchPriorityPolicy::kAlwaysHighest, "AlwaysHighest"},
        {DsePreload2PrefetchPriorityPolicy::kNull, "Null"}};
const base::FeatureParam<DsePreload2PrefetchPriorityPolicy>
    kDsePreload2PrefetchPriorityPolicy{
        &kDsePreload2, "dse_preload2_prefetch_priority_policy",
        DsePreload2PrefetchPriorityPolicy::kSearchPrefetchCompat,
        &kDsePreload2PrefetchPriorityPolicyOptions};

BASE_FEATURE(kDsePreload2OnPress, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kDsePreload2OnPressMouseDown{
    &kDsePreload2OnPress, "kDsePreload2OnPressMouseDown", true};
const base::FeatureParam<bool> kDsePreload2OnPressUpOrDownArrowButton{
    &kDsePreload2OnPress, "kDsePreload2OnPressUpOrDownArrowButton", true};
const base::FeatureParam<bool> kDsePreload2OnPressTouchDown{
    &kDsePreload2OnPress, "kDsePreload2OnPressTouchDown", true};
const base::FeatureParam<bool> kDsePreload2OnPressIgnoreSaverModes{
    &kDsePreload2OnPress, "kDsePreload2OnPressIgnoreSaverModes",
#if BUILDFLAG(IS_ANDROID)
    true
#else
    false
#endif  // BUILDFLAG(IS_ANDROID)
};

BASE_FEATURE(kDsePreload2OnPressIncognito, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDsePreload2OnSuggestNonDefaultMatch,
             "kDsePreload2OnSuggestNonDefaultMatch",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDsePreload2Enabled() {
  return base::FeatureList::IsEnabled(kDsePreload2) &&
         static_cast<size_t>(
             base::SysInfo::AmountOfTotalPhysicalMemory().InMiB()) >=
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

bool IsDsePreload2IgnoreSaverModesOnPressEnabled() {
  return IsDsePreload2OnPressEnabled() &&
         kDsePreload2OnPressIgnoreSaverModes.Get();
}

BASE_FEATURE(kDsePreload2SuppressForUnsupportedSearchMode,
             "kDsePreload2SuppressForUnsupportedSearchMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kDsePreload2UnsupportedSearchModes{
    &kDsePreload2SuppressForUnsupportedSearchMode,
    "unsupported_search_prefetch_modes", "udm=50"};

bool ShouldDsePreload2SuppressForUnsupportedMode(
    const AutocompleteMatch& match) {
  if (!base::FeatureList::IsEnabled(
          kDsePreload2SuppressForUnsupportedSearchMode)) {
    return false;
  }

  return ShouldSuppressPreloadForUnsupportedModeInternal(
      match, kDsePreload2UnsupportedSearchModes.Get());
}

}  // namespace features
