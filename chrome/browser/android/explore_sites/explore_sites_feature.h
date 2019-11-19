// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_FEATURE_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_FEATURE_H_

namespace chrome {
namespace android {
namespace explore_sites {

extern const char kExploreSitesVariationParameterName[];

extern const char kExploreSitesVariationExperimental[];
extern const char kExploreSitesVariationPersonalized[];
extern const char kExploreSitesVariationMostLikelyTile[];

extern const char kExploreSitesMostLikelyVariationParameterName[];
extern const char kExploreSitesHeadersExperimentParameterName[];

extern const char kExploreSitesMostLikelyVariationIconArrow[];
extern const char kExploreSitesMostLikelyVariationIconDots[];
extern const char kExploreSitesMostLikelyVariationIconGrouped[];

extern const char kExploreSitesDenseVariationParameterName[];
extern const char kExploreSitesDenseVariationOriginal[];
extern const char kExploreSitesDenseVariationDenseTitleBottom[];
extern const char kExploreSitesDenseVariationDenseTitleRight[];

extern const char kExploreSitesGamesTopExperiment[];

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.explore_sites
enum class ExploreSitesVariation {
  ENABLED,
  EXPERIMENT,
  PERSONALIZED,
  MOST_LIKELY,
  DISABLED
};

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.explore_sites
enum class MostLikelyVariation { NONE, ICON_ARROW, ICON_DOTS, ICON_GROUPED };

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.explore_sites
enum class DenseVariation { ORIGINAL, DENSE_TITLE_BOTTOM, DENSE_TITLE_RIGHT };

ExploreSitesVariation GetExploreSitesVariation();

MostLikelyVariation GetMostLikelyVariation();

DenseVariation GetDenseVariation();

}  // namespace explore_sites
}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_FEATURE_H_
