// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_features.h"

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_piece.h"
#include "components/variations/variations_params_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

class TabManagerFeaturesTest : public testing::Test {
 public:
  // Enables the proactive tab discarding feature, and sets up the associated
  // variations parameter values.
  void EnableProactiveTabFreezeAndDiscard() {
    std::set<std::string> features;
    features.insert(features::kProactiveTabFreezeAndDiscard.name);
    variations_manager_.SetVariationParamsWithFeatureAssociations(
        "DummyTrial", params_, features);
  }

  // Enables the site characteristics database feature, and sets up the
  // associated variations parameter values.
  void EnableSiteCharacteristicsDatabase() {
    std::set<std::string> features;
    features.insert(features::kSiteCharacteristicsDatabase.name);
    variations_manager_.SetVariationParamsWithFeatureAssociations(
        "DummyTrial", params_, features);
  }

  // Enables the site characteristics database feature, and sets up the
  // associated variations parameter values.
  void EnableInfiniteSessionRestore() {
    std::set<std::string> features;
    features.insert(features::kInfiniteSessionRestore.name);
    variations_manager_.SetVariationParamsWithFeatureAssociations(
        "DummyTrial", params_, features);
  }

  void SetParam(base::StringPiece key, base::StringPiece value) {
    params_[key.as_string()] = value.as_string();
  }

  void ExpectProactiveTabFreezeAndDiscardParams(
      bool should_proactively_discard,
      bool should_periodically_unfreeze,
      bool should_protect_tabs_sharing_browsing_instance,
      int low_loaded_tab_count,
      int moderate_loaded_tab_count,
      int high_loaded_tab_count,
      int memory_in_gb,
      base::TimeDelta low_occluded_timeout,
      base::TimeDelta moderate_occluded_timeout,
      base::TimeDelta high_occluded_timeout,
      base::TimeDelta freeze_timeout,
      base::TimeDelta unfreeze_timeout,
      base::TimeDelta refreeze_timeout,
      bool disable_heuristics_protections) {
    ProactiveTabFreezeAndDiscardParams params =
        GetProactiveTabFreezeAndDiscardParams(memory_in_gb);

    EXPECT_EQ(should_proactively_discard, params.should_proactively_discard);
    EXPECT_EQ(should_periodically_unfreeze,
              params.should_periodically_unfreeze);
    EXPECT_EQ(should_protect_tabs_sharing_browsing_instance,
              params.should_protect_tabs_sharing_browsing_instance);
    EXPECT_EQ(low_loaded_tab_count, params.low_loaded_tab_count);
    EXPECT_EQ(moderate_loaded_tab_count, params.moderate_loaded_tab_count);

    // Enforce that |moderate_loaded_tab_count| is within [low_loaded_tab_count,
    // high_loaded_tab_count].
    EXPECT_GE(params.moderate_loaded_tab_count, params.low_loaded_tab_count);
    EXPECT_LE(params.moderate_loaded_tab_count, params.high_loaded_tab_count);

    EXPECT_EQ(high_loaded_tab_count, params.high_loaded_tab_count);
    EXPECT_EQ(low_occluded_timeout, params.low_occluded_timeout);
    EXPECT_EQ(moderate_occluded_timeout, params.moderate_occluded_timeout);
    EXPECT_EQ(high_occluded_timeout, params.high_occluded_timeout);

    EXPECT_EQ(freeze_timeout, params.freeze_timeout);
    EXPECT_EQ(unfreeze_timeout, params.unfreeze_timeout);
    EXPECT_EQ(refreeze_timeout, params.refreeze_timeout);

    EXPECT_EQ(disable_heuristics_protections,
              params.disable_heuristics_protections);
  }

  void ExpectSiteCharacteristicsDatabaseParams(
      base::TimeDelta favicon_update_observation_window,
      base::TimeDelta title_update_observation_window,
      base::TimeDelta audio_usage_observation_window,
      base::TimeDelta notifications_usage_observation_window,
      base::TimeDelta title_or_favicon_change_grace_period,
      base::TimeDelta audio_usage_grace_period) {
    SiteCharacteristicsDatabaseParams params =
        GetSiteCharacteristicsDatabaseParams();

    EXPECT_EQ(favicon_update_observation_window,
              params.favicon_update_observation_window);
    EXPECT_EQ(title_update_observation_window,
              params.title_update_observation_window);
    EXPECT_EQ(audio_usage_observation_window,
              params.audio_usage_observation_window);
    EXPECT_EQ(notifications_usage_observation_window,
              params.notifications_usage_observation_window);
    EXPECT_EQ(title_or_favicon_change_grace_period,
              params.title_or_favicon_change_grace_period);
    EXPECT_EQ(audio_usage_grace_period, params.audio_usage_grace_period);
  }

  void ExpectInfiniteSessionRestoreParams(
      uint32_t min_simultaneous_tab_loads,
      uint32_t max_simultaneous_tab_loads,
      uint32_t cores_per_simultaneous_tab_load,
      uint32_t min_tabs_to_restore,
      uint32_t max_tabs_to_restore,
      uint32_t mb_free_memory_per_tab_to_restore,
      base::TimeDelta max_time_since_last_use_to_restore,
      uint32_t min_site_engagement_to_restore) {
    InfiniteSessionRestoreParams params = GetInfiniteSessionRestoreParams();
    EXPECT_EQ(min_simultaneous_tab_loads, params.min_simultaneous_tab_loads);
    EXPECT_EQ(max_simultaneous_tab_loads, params.max_simultaneous_tab_loads);
    EXPECT_EQ(cores_per_simultaneous_tab_load,
              params.cores_per_simultaneous_tab_load);
    EXPECT_EQ(min_tabs_to_restore, params.min_tabs_to_restore);
    EXPECT_EQ(max_tabs_to_restore, params.max_tabs_to_restore);
    EXPECT_EQ(mb_free_memory_per_tab_to_restore,
              params.mb_free_memory_per_tab_to_restore);
    EXPECT_EQ(max_time_since_last_use_to_restore,
              params.max_time_since_last_use_to_restore);
    EXPECT_EQ(min_site_engagement_to_restore,
              params.min_site_engagement_to_restore);
  }

  void ExpectDefaultProactiveTabFreezeAndDiscardParams() {
    int memory_in_gb = 4;
    ExpectProactiveTabFreezeAndDiscardParams(
        ProactiveTabFreezeAndDiscardParams::kShouldProactivelyDiscard
            .default_value,
        ProactiveTabFreezeAndDiscardParams::kShouldPeriodicallyUnfreeze
            .default_value,
        ProactiveTabFreezeAndDiscardParams::
            kShouldProtectTabsSharingBrowsingInstance.default_value,
        ProactiveTabFreezeAndDiscardParams::kLowLoadedTabCount.default_value,
        ProactiveTabFreezeAndDiscardParams::kModerateLoadedTabsPerGbRam
                .default_value *
            memory_in_gb,
        ProactiveTabFreezeAndDiscardParams::kHighLoadedTabCount.default_value,
        memory_in_gb,
        base::TimeDelta::FromSeconds(
            ProactiveTabFreezeAndDiscardParams::kLowOccludedTimeout
                .default_value),
        base::TimeDelta::FromSeconds(
            ProactiveTabFreezeAndDiscardParams::kModerateOccludedTimeout
                .default_value),
        base::TimeDelta::FromSeconds(
            ProactiveTabFreezeAndDiscardParams::kHighOccludedTimeout
                .default_value),
        base::TimeDelta::FromSeconds(
            ProactiveTabFreezeAndDiscardParams::kFreezeTimeout.default_value),
        base::TimeDelta::FromSeconds(
            ProactiveTabFreezeAndDiscardParams::kUnfreezeTimeout.default_value),
        base::TimeDelta::FromSeconds(
            ProactiveTabFreezeAndDiscardParams::kRefreezeTimeout.default_value),
        ProactiveTabFreezeAndDiscardParams::kDisableHeuristicsProtections
            .default_value);
  }

  void ExpectDefaultSiteCharacteristicsDatabaseParams() {
    ExpectSiteCharacteristicsDatabaseParams(
        base::TimeDelta::FromSeconds(
            SiteCharacteristicsDatabaseParams::kFaviconUpdateObservationWindow
                .default_value),
        base::TimeDelta::FromSeconds(
            SiteCharacteristicsDatabaseParams::kTitleUpdateObservationWindow
                .default_value),
        base::TimeDelta::FromSeconds(
            SiteCharacteristicsDatabaseParams::kAudioUsageObservationWindow
                .default_value),
        base::TimeDelta::FromSeconds(
            SiteCharacteristicsDatabaseParams::
                kNotificationsUsageObservationWindow.default_value),
        base::TimeDelta::FromSeconds(
            SiteCharacteristicsDatabaseParams::kTitleOrFaviconChangeGracePeriod
                .default_value),
        base::TimeDelta::FromSeconds(
            SiteCharacteristicsDatabaseParams::kAudioUsageGracePeriod
                .default_value));
  }

  void ExpectDefaultInfiniteSessionRestoreParams() {
    ExpectInfiniteSessionRestoreParams(
        InfiniteSessionRestoreParams::kMinSimultaneousTabLoads.default_value,
        InfiniteSessionRestoreParams::kMaxSimultaneousTabLoads.default_value,
        InfiniteSessionRestoreParams::kCoresPerSimultaneousTabLoad
            .default_value,
        InfiniteSessionRestoreParams::kMinTabsToRestore.default_value,
        InfiniteSessionRestoreParams::kMaxTabsToRestore.default_value,
        InfiniteSessionRestoreParams::kMbFreeMemoryPerTabToRestore
            .default_value,
        base::TimeDelta::FromSeconds(
            InfiniteSessionRestoreParams::kMaxTimeSinceLastUseToRestore
                .default_value),
        InfiniteSessionRestoreParams::kMinSiteEngagementToRestore
            .default_value);
  }

 private:
  std::map<std::string, std::string> params_;
  variations::testing::VariationParamsManager variations_manager_;
};

}  // namespace

TEST_F(TabManagerFeaturesTest,
       GetProactiveTabFreezeAndDiscardParamsDisabledFeatureGoesToDefault) {
  // Do not enable the proactive tab discarding feature.
  ExpectDefaultProactiveTabFreezeAndDiscardParams();
}

TEST_F(TabManagerFeaturesTest,
       GetProactiveTabFreezeAndDiscardParamsNoneGoesToDefault) {
  EnableProactiveTabFreezeAndDiscard();
  ExpectDefaultProactiveTabFreezeAndDiscardParams();
}

TEST_F(TabManagerFeaturesTest,
       GetProactiveTabFreezeAndDiscardParamsInvalidGoesToDefault) {
  SetParam(ProactiveTabFreezeAndDiscardParams::kShouldProactivelyDiscard.name,
           "blah");
  SetParam(ProactiveTabFreezeAndDiscardParams::kShouldPeriodicallyUnfreeze.name,
           "blah");
  SetParam(ProactiveTabFreezeAndDiscardParams::
               kShouldProtectTabsSharingBrowsingInstance.name,
           "bleh");
  SetParam(ProactiveTabFreezeAndDiscardParams::kLowLoadedTabCount.name, "ab");
  SetParam(ProactiveTabFreezeAndDiscardParams::kModerateLoadedTabsPerGbRam.name,
           "27.8");
  SetParam(ProactiveTabFreezeAndDiscardParams::kHighLoadedTabCount.name, "4e8");
  SetParam(ProactiveTabFreezeAndDiscardParams::kLowOccludedTimeout.name, "---");
  SetParam(ProactiveTabFreezeAndDiscardParams::kModerateOccludedTimeout.name,
           " ");
  SetParam(ProactiveTabFreezeAndDiscardParams::kHighOccludedTimeout.name, "");
  SetParam(ProactiveTabFreezeAndDiscardParams::kFreezeTimeout.name, "b");
  SetParam(ProactiveTabFreezeAndDiscardParams::kUnfreezeTimeout.name, "i");
  SetParam(ProactiveTabFreezeAndDiscardParams::kRefreezeTimeout.name, "m");
  SetParam(
      ProactiveTabFreezeAndDiscardParams::kDisableHeuristicsProtections.name,
      "bleh");
  EnableProactiveTabFreezeAndDiscard();
  ExpectDefaultProactiveTabFreezeAndDiscardParams();
}

TEST_F(TabManagerFeaturesTest, GetProactiveTabFreezeAndDiscardParams) {
  SetParam(ProactiveTabFreezeAndDiscardParams::kShouldProactivelyDiscard.name,
           "true");
  SetParam(ProactiveTabFreezeAndDiscardParams::kShouldPeriodicallyUnfreeze.name,
           "true");
  SetParam(ProactiveTabFreezeAndDiscardParams::
               kShouldProtectTabsSharingBrowsingInstance.name,
           "true");
  SetParam(ProactiveTabFreezeAndDiscardParams::kLowLoadedTabCount.name, "7");
  SetParam(ProactiveTabFreezeAndDiscardParams::kModerateLoadedTabsPerGbRam.name,
           "4");
  SetParam(ProactiveTabFreezeAndDiscardParams::kHighLoadedTabCount.name, "42");
  // These are expressed in seconds.
  SetParam(ProactiveTabFreezeAndDiscardParams::kLowOccludedTimeout.name, "60");
  SetParam(ProactiveTabFreezeAndDiscardParams::kModerateOccludedTimeout.name,
           "120");
  SetParam(ProactiveTabFreezeAndDiscardParams::kHighOccludedTimeout.name,
           "247");
  SetParam(ProactiveTabFreezeAndDiscardParams::kFreezeTimeout.name, "10");
  SetParam(ProactiveTabFreezeAndDiscardParams::kUnfreezeTimeout.name, "20");
  SetParam(ProactiveTabFreezeAndDiscardParams::kRefreezeTimeout.name, "30");
  SetParam(
      ProactiveTabFreezeAndDiscardParams::kDisableHeuristicsProtections.name,
      "true");
  EnableProactiveTabFreezeAndDiscard();

  // Should snap |moderate_loaded_tab_count| to |low_loaded_tab_count|, when the
  // amount of physical memory is so low that (|memory_in_gb| *
  // |moderate_tab_count_per_gb_ram|) < |low_loaded_tab_count|).
  int memory_in_gb_low = 1;
  ExpectProactiveTabFreezeAndDiscardParams(
      true, true, true, 7, 7, 42, memory_in_gb_low,
      base::TimeDelta::FromSeconds(60), base::TimeDelta::FromSeconds(120),
      base::TimeDelta::FromSeconds(247), base::TimeDelta::FromSeconds(10),
      base::TimeDelta::FromSeconds(20), base::TimeDelta::FromSeconds(30), true);

  // Should snap |moderate_loaded_tab_count| to |high_loaded_tab_count|, when
  // the amount of physical memory is so high that (|memory_in_gb| *
  // |moderate_tab_count_per_gb_ram|) > |high_loaded_tab_count|).
  int memory_in_gb_high = 100;
  ExpectProactiveTabFreezeAndDiscardParams(
      true, true, true, 7, 42, 42, memory_in_gb_high,
      base::TimeDelta::FromSeconds(60), base::TimeDelta::FromSeconds(120),
      base::TimeDelta::FromSeconds(247), base::TimeDelta::FromSeconds(10),
      base::TimeDelta::FromSeconds(20), base::TimeDelta::FromSeconds(30), true);

  // Tests normal case where |memory_in gb| * |moderate_tab_count_per_gb_ram| is
  // in the interval [low_loaded_tab_count, high_loaded_tab_count].
  int memory_in_gb_normal = 4;
  ExpectProactiveTabFreezeAndDiscardParams(
      true, true, true, 7, 16, 42, memory_in_gb_normal,
      base::TimeDelta::FromSeconds(60), base::TimeDelta::FromSeconds(120),
      base::TimeDelta::FromSeconds(247), base::TimeDelta::FromSeconds(10),
      base::TimeDelta::FromSeconds(20), base::TimeDelta::FromSeconds(30), true);
}

TEST_F(TabManagerFeaturesTest,
       GetSiteCharacteristicsDatabaseParamsDisabledFeatureGoesToDefault) {
  // Do not enable the site characteristics database.
  ExpectDefaultSiteCharacteristicsDatabaseParams();
}

TEST_F(TabManagerFeaturesTest,
       GetSiteCharacteristicsDatabaseParamsParamsNoneGoesToDefault) {
  ExpectDefaultSiteCharacteristicsDatabaseParams();
}

TEST_F(TabManagerFeaturesTest,
       GetSiteCharacteristicsDatabaseParamsInvalidGoesToDefault) {
  SetParam(
      SiteCharacteristicsDatabaseParams::kFaviconUpdateObservationWindow.name,
      "    ");
  SetParam(
      SiteCharacteristicsDatabaseParams::kTitleUpdateObservationWindow.name,
      "foo");
  SetParam(SiteCharacteristicsDatabaseParams::kAudioUsageObservationWindow.name,
           ".");
  SetParam(
      SiteCharacteristicsDatabaseParams::kNotificationsUsageObservationWindow
          .name,
      "abc");
  SetParam(
      SiteCharacteristicsDatabaseParams::kTitleOrFaviconChangeGracePeriod.name,
      "bleh");
  SetParam(SiteCharacteristicsDatabaseParams::kAudioUsageGracePeriod.name,
           "!!!");
  EnableSiteCharacteristicsDatabase();
  ExpectDefaultSiteCharacteristicsDatabaseParams();
}

TEST_F(TabManagerFeaturesTest, GetSiteCharacteristicsDatabaseParams) {
  SetParam(
      SiteCharacteristicsDatabaseParams::kFaviconUpdateObservationWindow.name,
      "3600");
  SetParam(
      SiteCharacteristicsDatabaseParams::kTitleUpdateObservationWindow.name,
      "36000");
  SetParam(SiteCharacteristicsDatabaseParams::kAudioUsageObservationWindow.name,
           "360000");
  SetParam(
      SiteCharacteristicsDatabaseParams::kNotificationsUsageObservationWindow
          .name,
      "3600000");
  SetParam(
      SiteCharacteristicsDatabaseParams::kTitleOrFaviconChangeGracePeriod.name,
      "42");
  SetParam(SiteCharacteristicsDatabaseParams::kAudioUsageGracePeriod.name,
           "43");

  EnableSiteCharacteristicsDatabase();

  ExpectSiteCharacteristicsDatabaseParams(
      base::TimeDelta::FromSeconds(3600), base::TimeDelta::FromSeconds(36000),
      base::TimeDelta::FromSeconds(360000),
      base::TimeDelta::FromSeconds(3600000), base::TimeDelta::FromSeconds(42),
      base::TimeDelta::FromSeconds(43));
}

TEST_F(TabManagerFeaturesTest,
       GetInfiniteSessionRestoreParamsInvalidGoesToDefault) {
  SetParam(InfiniteSessionRestoreParams::kMinSimultaneousTabLoads.name, "  ");
  SetParam(InfiniteSessionRestoreParams::kMaxSimultaneousTabLoads.name, "a.b");
  SetParam(InfiniteSessionRestoreParams::kCoresPerSimultaneousTabLoad.name,
           "-- ");
  SetParam(InfiniteSessionRestoreParams::kMinTabsToRestore.name, "hey");
  SetParam(InfiniteSessionRestoreParams::kMaxTabsToRestore.name, ".");
  SetParam(InfiniteSessionRestoreParams::kMbFreeMemoryPerTabToRestore.name,
           "0x0");
  SetParam(InfiniteSessionRestoreParams::kMaxTimeSinceLastUseToRestore.name,
           "foo");
  SetParam(InfiniteSessionRestoreParams::kMinSiteEngagementToRestore.name,
           "bar");
  EnableInfiniteSessionRestore();
  ExpectDefaultInfiniteSessionRestoreParams();
}

TEST_F(TabManagerFeaturesTest, GetInfiniteSessionRestoreParams) {
  SetParam(InfiniteSessionRestoreParams::kMinSimultaneousTabLoads.name, "10");
  SetParam(InfiniteSessionRestoreParams::kMaxSimultaneousTabLoads.name, "20");
  SetParam(InfiniteSessionRestoreParams::kCoresPerSimultaneousTabLoad.name,
           "2");
  SetParam(InfiniteSessionRestoreParams::kMinTabsToRestore.name, "13");
  SetParam(InfiniteSessionRestoreParams::kMaxTabsToRestore.name, "27");
  SetParam(InfiniteSessionRestoreParams::kMbFreeMemoryPerTabToRestore.name,
           "1337");
  SetParam(InfiniteSessionRestoreParams::kMaxTimeSinceLastUseToRestore.name,
           "60");
  SetParam(InfiniteSessionRestoreParams::kMinSiteEngagementToRestore.name, "9");
  EnableInfiniteSessionRestore();
  ExpectInfiniteSessionRestoreParams(10, 20, 2, 13, 27, 1337,
                                     base::TimeDelta::FromMinutes(1), 9);
}

}  // namespace resource_coordinator
