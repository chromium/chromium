// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/chrome_feature_flags/arc_feature_string_builder.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr char kTestParamsFeatureName1[] = "test_params_feature1";
constexpr char kTestParamsFeatureName2[] = "test_params_feature2";
constexpr char kTestParamsFeatureName3[] = "test_params_feature3";

}  // namespace

TEST(ArcFeatureStringBuilderTest, AddFeatureParams) {
  base::test::ScopedFeatureList feature_list;
  static BASE_FEATURE(test_feature1, kTestParamsFeatureName1,
                      base::FEATURE_ENABLED_BY_DEFAULT);
  static BASE_FEATURE(test_feature2, kTestParamsFeatureName2,
                      base::FEATURE_ENABLED_BY_DEFAULT);
  static BASE_FEATURE(test_feature3, kTestParamsFeatureName3,
                      base::FEATURE_ENABLED_BY_DEFAULT);

  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{test_feature1, {{"boolkey", "false"}, {"intkey", "111"}}},
       {test_feature2, {{"stringkey", "name"}, {"doublekey", "3.14"}}}},
      /*disabled_features=*/{});

  ArcFeatureStringBuilder builder;
  builder.AddFeature(test_feature1);
  builder.AddFeature(test_feature2);
  builder.AddFeature(test_feature3);
  std::string expected =
      "test_params_feature1:boolkey/false/intkey/"
      "111,test_params_feature2:doublekey/3.14/stringkey/"
      "name,test_params_feature3";
  EXPECT_EQ(expected, builder.ToString());
}

TEST(ArcFeatureStringBuilderTest, DisableFeature) {
  base::test::ScopedFeatureList feature_list;
  static BASE_FEATURE(test_feature1, kTestParamsFeatureName1,
                      base::FEATURE_DISABLED_BY_DEFAULT);
  static BASE_FEATURE(test_feature2, kTestParamsFeatureName2,
                      base::FEATURE_DISABLED_BY_DEFAULT);
  static BASE_FEATURE(test_feature3, kTestParamsFeatureName3,
                      base::FEATURE_ENABLED_BY_DEFAULT);

  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{test_feature3, {{"boolkey", "true"}, {"stringkey", "abcdefg"}}}},
      /*disabled_features=*/{{test_feature1}, {test_feature2}});

  ArcFeatureStringBuilder builder;

  builder.AddFeature(test_feature1);
  builder.AddFeature(test_feature2);
  builder.AddFeature(test_feature3);

  std::string expected =
      "test_params_feature1,test_params_feature2,test_params_feature3:boolkey/"
      "true/stringkey/abcdefg";

  // The disabled feature flag name can be added to the formatted string.
  // (In this test, the scoped_feature_list not support add "disabled" flag
  // with params.)
  EXPECT_EQ(expected, builder.ToString());
}

}  // namespace arc
