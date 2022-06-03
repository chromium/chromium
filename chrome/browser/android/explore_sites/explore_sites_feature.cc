// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_feature.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"

namespace chrome {
namespace android {
namespace explore_sites {

const char kExploreSitesVariationParameterName[] = "variation";
const char kExploreSitesVariationExperimental[] = "experiment";

const char kExploreSitesHeadersExperimentParameterName[] = "exp";

const char kExploreSitesDenseVariationParameterName[] = "denseVariation";
const char kExploreSitesDenseVariationOriginal[] = "original";
const char kExploreSitesDenseVariationDenseTitleBottom[] = "titleBottom";
const char kExploreSitesDenseVariationDenseTitleRight[] = "titleRight";

ExploreSitesVariation GetExploreSitesVariation() {
  if (base::FeatureList::IsEnabled(kExploreSites)) {
    const std::string feature_param = base::GetFieldTrialParamValueByFeature(
        kExploreSites, kExploreSitesVariationParameterName);
    if (feature_param == kExploreSitesVariationExperimental) {
      return ExploreSitesVariation::EXPERIMENT;
    } else {
      return ExploreSitesVariation::ENABLED;
    }
  }
  return ExploreSitesVariation::DISABLED;
}

DenseVariation GetDenseVariation() {
  if (base::GetFieldTrialParamValueByFeature(
          kExploreSites, kExploreSitesDenseVariationParameterName) ==
      kExploreSitesDenseVariationDenseTitleBottom) {
    return DenseVariation::DENSE_TITLE_BOTTOM;
  }
  if (base::GetFieldTrialParamValueByFeature(
          kExploreSites, kExploreSitesDenseVariationParameterName) ==
      kExploreSitesDenseVariationDenseTitleRight) {
    return DenseVariation::DENSE_TITLE_RIGHT;
  }
  return DenseVariation::ORIGINAL;
}

}  // namespace explore_sites
}  // namespace android
}  // namespace chrome
