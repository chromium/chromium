// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_prefs.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/battery/battery_saver.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PrefetchPrefsTest, GetPreloadPagesState) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDefault));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kStandard));
  EXPECT_EQ(prefetch::GetPreloadPagesState(prefs),
            prefetch::PreloadPagesState::kStandardPreloading);

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kExtended));
  EXPECT_EQ(prefetch::GetPreloadPagesState(prefs),
            prefetch::PreloadPagesState::kExtendedPreloading);

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(
          prefetch::NetworkPredictionOptions::kWifiOnlyDeprecated));
  EXPECT_EQ(prefetch::GetPreloadPagesState(prefs),
            prefetch::PreloadPagesState::kStandardPreloading);

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDisabled));
  EXPECT_EQ(prefetch::GetPreloadPagesState(prefs),
            prefetch::PreloadPagesState::kNoPreloading);

  // Invalid value should result in disabled preloading.
  prefs.SetInteger(prefs::kNetworkPredictionOptions, 12345);
  EXPECT_EQ(prefetch::GetPreloadPagesState(prefs),
            prefetch::PreloadPagesState::kNoPreloading);
}

TEST(PrefetchPrefsTest, SetPreloadPagesState) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDefault));

  prefetch::SetPreloadPagesState(&prefs,
                                 prefetch::PreloadPagesState::kNoPreloading);
  EXPECT_EQ(prefs.GetInteger(prefs::kNetworkPredictionOptions),
            static_cast<int>(prefetch::NetworkPredictionOptions::kDisabled));

  prefetch::SetPreloadPagesState(
      &prefs, prefetch::PreloadPagesState::kStandardPreloading);
  EXPECT_EQ(prefs.GetInteger(prefs::kNetworkPredictionOptions),
            static_cast<int>(prefetch::NetworkPredictionOptions::kStandard));

  prefetch::SetPreloadPagesState(
      &prefs, prefetch::PreloadPagesState::kExtendedPreloading);
  EXPECT_EQ(prefs.GetInteger(prefs::kNetworkPredictionOptions),
            static_cast<int>(prefetch::NetworkPredictionOptions::kExtended));
}

TEST(PrefetchPrefsTest, IsSomePreloadingEnabled) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDefault));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDisabled));
  EXPECT_FALSE(prefetch::IsSomePreloadingEnabled(prefs));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kStandard));
  EXPECT_TRUE(prefetch::IsSomePreloadingEnabled(prefs));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(
          prefetch::NetworkPredictionOptions::kWifiOnlyDeprecated));
  EXPECT_TRUE(prefetch::IsSomePreloadingEnabled(prefs));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kExtended));
  EXPECT_TRUE(prefetch::IsSomePreloadingEnabled(prefs));
}

TEST(PrefetchPrefsTest, IsSomePreloadingEnabled_PreloadingHoldback) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(prefetch::kPreloadingHoldback);
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDefault));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDisabled));
  EXPECT_FALSE(prefetch::IsSomePreloadingEnabled(prefs));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kStandard));
  EXPECT_FALSE(prefetch::IsSomePreloadingEnabled(prefs));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(
          prefetch::NetworkPredictionOptions::kWifiOnlyDeprecated));
  EXPECT_FALSE(prefetch::IsSomePreloadingEnabled(prefs));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kExtended));
  EXPECT_FALSE(prefetch::IsSomePreloadingEnabled(prefs));
}

TEST(PrefetchPrefsTest, IsSomePreloadingEnabledIgnoringFinch) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(prefetch::kPreloadingHoldback);
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDefault));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDisabled));
  EXPECT_FALSE(prefetch::IsSomePreloadingEnabledIgnoringFinch(prefs));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kStandard));
  EXPECT_TRUE(prefetch::IsSomePreloadingEnabledIgnoringFinch(prefs));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(
          prefetch::NetworkPredictionOptions::kWifiOnlyDeprecated));
  EXPECT_TRUE(prefetch::IsSomePreloadingEnabledIgnoringFinch(prefs));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kExtended));
  EXPECT_TRUE(prefetch::IsSomePreloadingEnabledIgnoringFinch(prefs));
}

class PrefetchPrefsWithBatterySaverTest : public ::testing::Test {
 public:
  PrefetchPrefsWithBatterySaverTest() = default;
  ~PrefetchPrefsWithBatterySaverTest() override = default;

  void TearDown() override { battery::ResetIsBatterySaverEnabledForTesting(); }
};

TEST_F(PrefetchPrefsWithBatterySaverTest,
       IsSomePreloadingEnabledIgnoringFinch) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDefault));

  battery::OverrideIsBatterySaverEnabledForTesting(false);
  EXPECT_TRUE(prefetch::IsSomePreloadingEnabledIgnoringFinch(prefs));

  battery::OverrideIsBatterySaverEnabledForTesting(true);
  EXPECT_FALSE(prefetch::IsSomePreloadingEnabledIgnoringFinch(prefs));
}

TEST_F(PrefetchPrefsWithBatterySaverTest, IsSomePreloadingEnabled) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDefault));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kStandard));
  EXPECT_TRUE(prefetch::IsSomePreloadingEnabled(prefs));

  battery::OverrideIsBatterySaverEnabledForTesting(false);
  EXPECT_TRUE(prefetch::IsSomePreloadingEnabled(prefs));

  battery::OverrideIsBatterySaverEnabledForTesting(true);
  EXPECT_FALSE(prefetch::IsSomePreloadingEnabled(prefs));
}
