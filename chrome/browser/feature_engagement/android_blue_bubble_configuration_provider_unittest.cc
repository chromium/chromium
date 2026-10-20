// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/android_blue_bubble_configuration_provider.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

using ::testing::Contains;
using ::testing::Not;

FeatureConfig CreateBaseConfig(EventConfig trigger) {
  FeatureConfig config;
  config.valid = true;
  config.trigger = std::move(trigger);
  config.used = EventConfig("test_used", Comparator(EQUAL, 0), /*window=*/360,
                            /*storage=*/360);
  return config;
}

}  // namespace

class AndroidBlueBubbleConfigurationProviderTest : public testing::Test {
 protected:
  AndroidBlueBubbleConfigurationProvider provider_;
};

TEST_F(AndroidBlueBubbleConfigurationProviderTest, FlagDisabled_NoOp) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      chrome::android::kBlueBubbleIphCooldownGroup);

  FeatureConfig config =
      CreateBaseConfig(EventConfig("test_trigger", Comparator(LESS_THAN, 5),
                                   /*window=*/360, /*storage=*/360));
  EXPECT_FALSE(provider_.MaybeProvideFeatureConfiguration(
      kIPHTabSwitcherButtonFeature, config, {}, {}));
  EXPECT_EQ(config.trigger.comparator, Comparator(LESS_THAN, 5));
  EXPECT_TRUE(config.event_configs.empty());
}

TEST_F(AndroidBlueBubbleConfigurationProviderTest,
       NonBlueBubbleFeature_Ignored) {
  base::test::ScopedFeatureList feature_list{
      chrome::android::kBlueBubbleIphCooldownGroup};

  FeatureConfig config =
      CreateBaseConfig(EventConfig("test_trigger", Comparator(LESS_THAN, 5),
                                   /*window=*/360, /*storage=*/360));
  EXPECT_FALSE(provider_.MaybeProvideFeatureConfiguration(
      kIPHAppRatingPromptFeature, config, {}, {}));
  EXPECT_EQ(config.trigger.comparator, Comparator(LESS_THAN, 5));
  EXPECT_TRUE(config.event_configs.empty());
}

TEST_F(AndroidBlueBubbleConfigurationProviderTest,
       OnePerLifetime_PreservedWithoutModification) {
  base::test::ScopedFeatureList feature_list{
      chrome::android::kBlueBubbleIphCooldownGroup};

  FeatureConfig config = CreateBaseConfig(EventConfig(
      "test_trigger", Comparator(EQUAL, 0), /*window=*/360, /*storage=*/360));
  EXPECT_FALSE(provider_.MaybeProvideFeatureConfiguration(
      kIPHBottomToolbarTipFeature, config, {}, {}));
  EXPECT_EQ(config.trigger, EventConfig("test_trigger", Comparator(EQUAL, 0),
                                        /*window=*/360, /*storage=*/360));
  EXPECT_TRUE(config.event_configs.empty());
}

TEST_F(AndroidBlueBubbleConfigurationProviderTest,
       AlreadyCompliant_PreservedWithoutModification) {
  base::test::ScopedFeatureList feature_list{
      chrome::android::kBlueBubbleIphCooldownGroup};

  FeatureConfig config = CreateBaseConfig(EventConfig(
      "test_trigger", Comparator(EQUAL, 0), /*window=*/7, /*storage=*/360));
  config.event_configs.insert(EventConfig("test_trigger",
                                          Comparator(LESS_THAN, 3),
                                          /*window=*/360, /*storage=*/360));

  EXPECT_FALSE(provider_.MaybeProvideFeatureConfiguration(
      kIPHSiteControlsFeature, config, {}, {}));
  EXPECT_EQ(config.event_configs.size(), 1u);
}

TEST_F(AndroidBlueBubbleConfigurationProviderTest,
       TightensLooseLifetimeAndAddsCooldown) {
  base::test::ScopedFeatureList feature_list{
      chrome::android::kBlueBubbleIphCooldownGroup};

  FeatureConfig config =
      CreateBaseConfig(EventConfig("test_trigger", Comparator(LESS_THAN, 5),
                                   /*window=*/360, /*storage=*/360));
  EXPECT_TRUE(provider_.MaybeProvideFeatureConfiguration(
      kIPHTabSwitcherButtonFeature, config, {}, {}));

  EXPECT_EQ(config.trigger,
            EventConfig("test_trigger", Comparator(LESS_THAN, 3),
                        /*window=*/360, /*storage=*/360));
  EXPECT_THAT(config.event_configs,
              Contains(EventConfig("test_trigger", Comparator(EQUAL, 0),
                                   /*window=*/7, /*storage=*/360)));
}

TEST_F(AndroidBlueBubbleConfigurationProviderTest,
       TightensShortCooldownAndAddsLifetimeCap) {
  base::test::ScopedFeatureList feature_list{
      chrome::android::kBlueBubbleIphCooldownGroup};

  FeatureConfig config = CreateBaseConfig(EventConfig(
      "test_trigger", Comparator(LESS_THAN, 1), /*window=*/1, /*storage=*/90));
  EXPECT_TRUE(provider_.MaybeProvideFeatureConfiguration(
      kIPHReadLaterAppMenuBookmarkThisPageFeature, config, {}, {}));

  EXPECT_EQ(config.trigger,
            EventConfig("test_trigger", Comparator(LESS_THAN, 1), /*window=*/7,
                        /*storage=*/360));
  EXPECT_THAT(config.event_configs,
              Contains(EventConfig("test_trigger", Comparator(LESS_THAN, 3),
                                   /*window=*/360, /*storage=*/360)));
}

TEST_F(AndroidBlueBubbleConfigurationProviderTest,
       ReplacesShortCooldownInEventConfigs) {
  base::test::ScopedFeatureList feature_list{
      chrome::android::kBlueBubbleIphCooldownGroup};

  FeatureConfig config =
      CreateBaseConfig(EventConfig("test_trigger", Comparator(LESS_THAN, 3),
                                   /*window=*/360, /*storage=*/360));
  config.event_configs.insert(EventConfig(
      "test_trigger", Comparator(LESS_THAN, 1), /*window=*/1, /*storage=*/360));

  EXPECT_TRUE(provider_.MaybeProvideFeatureConfiguration(kIPHCCTMinimized,
                                                         config, {}, {}));

  EXPECT_THAT(config.event_configs,
              Not(Contains(EventConfig("test_trigger", Comparator(LESS_THAN, 1),
                                       /*window=*/1, /*storage=*/360))));
  EXPECT_THAT(config.event_configs,
              Contains(EventConfig("test_trigger", Comparator(EQUAL, 0),
                                   /*window=*/7, /*storage=*/360)));
}

TEST_F(AndroidBlueBubbleConfigurationProviderTest,
       PreservesStricterCooldownWhileAddingLifetimeCap) {
  base::test::ScopedFeatureList feature_list{
      chrome::android::kBlueBubbleIphCooldownGroup};

  FeatureConfig config = CreateBaseConfig(EventConfig(
      "test_trigger", Comparator(EQUAL, 0), /*window=*/90, /*storage=*/90));
  EXPECT_TRUE(provider_.MaybeProvideFeatureConfiguration(
      kIPHSiteControlsFeature, config, {}, {}));

  EXPECT_EQ(config.trigger, EventConfig("test_trigger", Comparator(EQUAL, 0),
                                        /*window=*/90, /*storage=*/360));
  EXPECT_THAT(config.event_configs,
              Contains(EventConfig("test_trigger", Comparator(LESS_THAN, 3),
                                   /*window=*/360, /*storage=*/360)));
  EXPECT_EQ(config.event_configs.size(), 1u);
}

}  // namespace feature_engagement
