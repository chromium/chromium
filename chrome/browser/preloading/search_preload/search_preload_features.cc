// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/search_preload/search_preload_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/omnibox/browser/omnibox.mojom-shared.h"

namespace features {

BASE_FEATURE(kDsePreload2, "DsePreload2", base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kDsePreload2PredictorMouseDown{
    &kDsePreload2, "kDsePreload2PredictorMouseDown", true};
const base::FeatureParam<bool> kDsePreload2PredictorUpOrDownArrowButton{
    &kDsePreload2, "kDsePreload2PredictorUpOrDownArrowButton", true};
const base::FeatureParam<bool> kDsePreload2PredictorTouchDown{
    &kDsePreload2, "kDsePreload2PredictorTouchDown", true};

bool IsDsePreload2Enabled() {
  return base::FeatureList::IsEnabled(kDsePreload2);
}

bool DsePreload2IsPredictorEnabled(
    omnibox::mojom::NavigationPredictor navigation_predictor) {
  switch (navigation_predictor) {
    case omnibox::mojom::NavigationPredictor::kMouseDown:
      return kDsePreload2PredictorMouseDown.Get();
    case omnibox::mojom::NavigationPredictor::kUpOrDownArrowButton:
      return kDsePreload2PredictorUpOrDownArrowButton.Get();
    case omnibox::mojom::NavigationPredictor::kTouchDown:
      return kDsePreload2PredictorTouchDown.Get();
  }
}

}  // namespace features
