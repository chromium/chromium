// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_feature.h"

#include <map>
#include <string>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {
namespace android {
namespace explore_sites {

TEST(ExploreSitesFeatureTest, ExploreSitesEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kExploreSites);
  EXPECT_EQ(ExploreSitesVariation::ENABLED, GetExploreSitesVariation());
}

TEST(ExploreSitesFeatureTest, ExploreSitesDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kExploreSites);
  EXPECT_EQ(ExploreSitesVariation::DISABLED, GetExploreSitesVariation());
}

TEST(ExploreSitesFeatureTest, ExploreSitesEnabledWithExperiment) {
  std::map<std::string, std::string> parameters;
  parameters[kExploreSitesVariationParameterName] =
      kExploreSitesVariationExperimental;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(kExploreSites,
                                                         parameters);
  EXPECT_EQ(ExploreSitesVariation::EXPERIMENT, GetExploreSitesVariation());
}

TEST(ExploreSitesFeatureTest, ExploreSitesEnabledWithDenseTitleBottom) {
  std::map<std::string, std::string> parameters;
  parameters[kExploreSitesDenseVariationParameterName] =
      kExploreSitesDenseVariationDenseTitleBottom;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(kExploreSites,
                                                         parameters);
  EXPECT_EQ(DenseVariation::DENSE_TITLE_BOTTOM, GetDenseVariation());
}

TEST(ExploreSitesFeatureTest, ExploreSitesEnabledWithDenseTitleRight) {
  std::map<std::string, std::string> parameters;
  parameters[kExploreSitesDenseVariationParameterName] =
      kExploreSitesDenseVariationDenseTitleRight;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(kExploreSites,
                                                         parameters);
  EXPECT_EQ(DenseVariation::DENSE_TITLE_RIGHT, GetDenseVariation());
}

TEST(ExploreSitesFeatureTest, ExploreSitesDenseVariationOriginal) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kExploreSites);
  EXPECT_EQ(DenseVariation::ORIGINAL, GetDenseVariation());
}

TEST(ExploreSitesFeatureTest, ExploreSitesEnabledWithIconArrow) {
  std::map<std::string, std::string> parameters;
  parameters[kExploreSitesVariationParameterName] =
      kExploreSitesVariationMostLikelyTile;
  parameters[kExploreSitesMostLikelyVariationParameterName] =
      kExploreSitesMostLikelyVariationIconArrow;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(kExploreSites,
                                                         parameters);
  EXPECT_EQ(ExploreSitesVariation::MOST_LIKELY, GetExploreSitesVariation());
  EXPECT_EQ(MostLikelyVariation::ICON_ARROW, GetMostLikelyVariation());
}

TEST(ExploreSitesFeatureTest, ExploreSitesEnabledWithIconDots) {
  std::map<std::string, std::string> parameters;
  parameters[kExploreSitesVariationParameterName] =
      kExploreSitesVariationMostLikelyTile;
  parameters[kExploreSitesMostLikelyVariationParameterName] =
      kExploreSitesMostLikelyVariationIconDots;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(kExploreSites,
                                                         parameters);
  EXPECT_EQ(ExploreSitesVariation::MOST_LIKELY, GetExploreSitesVariation());
  EXPECT_EQ(MostLikelyVariation::ICON_DOTS, GetMostLikelyVariation());
}

TEST(ExploreSitesFeatureTest, ExploreSitesEnabledWithIconGrouped) {
  std::map<std::string, std::string> parameters;
  parameters[kExploreSitesVariationParameterName] =
      kExploreSitesVariationMostLikelyTile;
  parameters[kExploreSitesMostLikelyVariationParameterName] =
      kExploreSitesMostLikelyVariationIconGrouped;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(kExploreSites,
                                                         parameters);
  EXPECT_EQ(ExploreSitesVariation::MOST_LIKELY, GetExploreSitesVariation());
  EXPECT_EQ(MostLikelyVariation::ICON_GROUPED, GetMostLikelyVariation());
}

TEST(ExploreSitesFeatureTest, ExploreSitesEnabledWithBogus) {
  const char bogusParamValue[] = "bogus";
  std::map<std::string, std::string> parameters;
  parameters[kExploreSitesVariationParameterName] = bogusParamValue;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(kExploreSites,
                                                         parameters);
  EXPECT_EQ(ExploreSitesVariation::ENABLED, GetExploreSitesVariation());
}

}  // namespace explore_sites
}  // namespace android
}  // namespace chrome
