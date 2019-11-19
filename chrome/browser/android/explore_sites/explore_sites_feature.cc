// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_feature.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/android/chrome_feature_list.h"

namespace chrome {
namespace android {
namespace explore_sites {

const char kExploreSitesVariationParameterName[] = "variation";

const char kExploreSitesVariationExperimental[] = "experiment";
const char kExploreSitesVariationPersonalized[] = "personalized";
const char kExploreSitesVariationMostLikelyTile[] = "mostLikelyTile";

const char kExploreSitesMostLikelyVariationParameterName[] =
    "mostLikelyVariation";
const char kExploreSitesHeadersExperimentParameterName[] = "exp";

const char kExploreSitesMostLikelyVariationIconArrow[] = "arrowIcon";
const char kExploreSitesMostLikelyVariationIconDots[] = "dotsIcon";
const char kExploreSitesMostLikelyVariationIconGrouped[] = "groupedIcon";

const char kExploreSitesDenseVariationParameterName[] = "denseVariation";
const char kExploreSitesDenseVariationOriginal[] = "original";
const char kExploreSitesDenseVariationDenseTitleBottom[] = "titleBottom";
const char kExploreSitesDenseVariationDenseTitleRight[] = "titleRight";

const char kExploreSitesGamesTopExperiment[] = "games-top";

ExploreSitesVariation GetExploreSitesVariation() {
  if (base::FeatureList::IsEnabled(kExploreSites)) {
    const std::string feature_param = base::GetFieldTrialParamValueByFeature(
        kExploreSites, kExploreSitesVariationParameterName);
    if (feature_param == kExploreSitesVariationExperimental) {
      return ExploreSitesVariation::EXPERIMENT;
    } else if (feature_param == kExploreSitesVariationPersonalized) {
      return ExploreSitesVariation::PERSONALIZED;
    } else if (feature_param == kExploreSitesVariationMostLikelyTile) {
      return ExploreSitesVariation::MOST_LIKELY;
    } else {
      return ExploreSitesVariation::ENABLED;
    }
  }
  return ExploreSitesVariation::DISABLED;
}

MostLikelyVariation GetMostLikelyVariation() {
  if (base::FeatureList::IsEnabled(kExploreSites) &&
      base::GetFieldTrialParamValueByFeature(
          kExploreSites, kExploreSitesVariationParameterName) ==
          kExploreSitesVariationMostLikelyTile) {
    if (base::GetFieldTrialParamValueByFeature(
            kExploreSites, kExploreSitesMostLikelyVariationParameterName) ==
        kExploreSitesMostLikelyVariationIconArrow) {
      return MostLikelyVariation::ICON_ARROW;
    }
    if (base::GetFieldTrialParamValueByFeature(
            kExploreSites, kExploreSitesMostLikelyVariationParameterName) ==
        kExploreSitesMostLikelyVariationIconDots) {
      return MostLikelyVariation::ICON_DOTS;
    }
    if (base::GetFieldTrialParamValueByFeature(
            kExploreSites, kExploreSitesMostLikelyVariationParameterName) ==
        kExploreSitesMostLikelyVariationIconGrouped) {
      return MostLikelyVariation::ICON_GROUPED;
    }
  }
  return MostLikelyVariation::NONE;
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
