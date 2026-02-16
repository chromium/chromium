// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"

#include "base/test/scoped_feature_list.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"
#include "url/gurl.h"

class FieldTrialSettingsTest : public testing::Test {
 public:
  FieldTrialSettingsTest() = default;
  ~FieldTrialSettingsTest() override = default;
};

TEST_F(FieldTrialSettingsTest, SuppressPrefetchForUnsupportedModeDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(kSuppressPrefetchForUnsupportedSearchMode);
  EXPECT_FALSE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&udm=50")));
}

TEST_F(FieldTrialSettingsTest,
       SuppressPrefetchForUnsupportedModeEnabled_DefaultParams) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      kSuppressPrefetchForUnsupportedSearchMode,
      {{"unsupported_search_prefetch_modes", "udm=50"}});
  EXPECT_TRUE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&udm=50")));
  EXPECT_FALSE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&udm=14")));
  EXPECT_FALSE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo")));
  EXPECT_FALSE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&udm")));
  EXPECT_FALSE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&udm=")));
  EXPECT_FALSE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&udm=5")));
  EXPECT_FALSE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&udm=500")));
}

TEST_F(FieldTrialSettingsTest, SuppressPrefetchForUnsupportedModeCustomParams) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      kSuppressPrefetchForUnsupportedSearchMode,
      {{"unsupported_search_prefetch_modes", "udm=1,udm=2,foo=bar,a=b,a=c"}});
  EXPECT_TRUE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&udm=1")));
  EXPECT_TRUE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&udm=2")));
  EXPECT_TRUE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&foo=bar")));
  EXPECT_TRUE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&a=b")));
  EXPECT_TRUE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&a=c")));
  EXPECT_FALSE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&udm=3")));
  EXPECT_FALSE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&foo=baz")));
  EXPECT_FALSE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&a=d")));
}

TEST_F(FieldTrialSettingsTest,
       SuppressPrefetchForUnsupportedModeEnabled_CustomMultipleValues) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      kSuppressPrefetchForUnsupportedSearchMode,
      {{"unsupported_search_prefetch_modes", "udm=50=15,a=1=2"}});
  EXPECT_TRUE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&udm=50")));
  EXPECT_TRUE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&udm=15")));
  EXPECT_FALSE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&udm=33")));
  EXPECT_FALSE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&a=33")));
  EXPECT_TRUE(ShouldSuppressPrefetchForUnsupportedMode(
      GURL("https://www.google.com/search?q=foo&a=2")));
}

TEST_F(FieldTrialSettingsTest,
       SuppressPrefetchForUnsupportedModeAutocompleteMatch) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kSuppressPrefetchForUnsupportedSearchMode);

  // IsSearchAimSuggestion() is true, so it should be suppressed.
  AutocompleteMatch search_aim_match;
  search_aim_match.destination_url =
      GURL("https://www.google.com/search?q=foo&udm=50");
  omnibox::SuggestTemplateInfo suggest_template;
  (*suggest_template.mutable_default_search_parameters())["udm"] = "50";
  search_aim_match.suggest_template = suggest_template;

  EXPECT_TRUE(ShouldSuppressPrefetchForUnsupportedMode(search_aim_match));

  // IsSearchAimSuggestion() is false.
  AutocompleteMatch normal_match;
  normal_match.destination_url = GURL("https://www.google.com/search?q=foo");
  EXPECT_FALSE(ShouldSuppressPrefetchForUnsupportedMode(normal_match));

  AutocompleteMatch normal_match_supported;
  normal_match_supported.destination_url =
      GURL("https://www.google.com/search?q=foo&udm=14");
  EXPECT_FALSE(
      ShouldSuppressPrefetchForUnsupportedMode(normal_match_supported));
}
