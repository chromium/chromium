// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/search_preload/search_preload_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/omnibox/browser/omnibox.mojom-shared.h"

namespace features {

BASE_FEATURE(kDsePreload2, "DsePreload2", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDsePreload2OnPress,
             "DsePreload2OnPress",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kDsePreload2OnPressMouseDown{
    &kDsePreload2, "kDsePreload2OnPressMouseDown", true};
const base::FeatureParam<bool> kDsePreload2OnPressUpOrDownArrowButton{
    &kDsePreload2, "kDsePreload2OnPressUpOrDownArrowButton", true};
const base::FeatureParam<bool> kDsePreload2OnPressTouchDown{
    &kDsePreload2, "kDsePreload2OnPressTouchDown", true};

BASE_FEATURE(kDsePreload2OnPressIncognito,
             "DsePreload2OnPressIncognito",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDsePreload2OnSuggestNonDefalutMatch,
             "kDsePreload2OnSuggestNonDefalutMatch",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDsePreload2Enabled() {
  return base::FeatureList::IsEnabled(kDsePreload2);
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
