// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_prefs.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/battery/battery_saver.h"
#include "chrome/browser/data_saver/data_saver.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
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

class PrefetchPrefsPreloadingTest : public ::testing::Test {
 public:
  PrefetchPrefsPreloadingTest() = default;
  ~PrefetchPrefsPreloadingTest() override = default;

  // IsSomePreloadingEnabled[IgnoringFinch]() requires a threaded environment.
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PrefetchPrefsPreloadingTest, IsSomePreloadingEnabled) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDefault));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDisabled));
  EXPECT_EQ(prefetch::IsSomePreloadingEnabled(prefs),
            content::PreloadingEligibility::kPreloadingDisabled);

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kStandard));
  EXPECT_EQ(prefetch::IsSomePreloadingEnabled(prefs),
            content::PreloadingEligibility::kEligible);

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(
          prefetch::NetworkPredictionOptions::kWifiOnlyDeprecated));
  EXPECT_EQ(prefetch::IsSomePreloadingEnabled(prefs),
            content::PreloadingEligibility::kEligible);

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kExtended));
  EXPECT_EQ(prefetch::IsSomePreloadingEnabled(prefs),
            content::PreloadingEligibility::kEligible);
}

TEST_F(PrefetchPrefsPreloadingTest,
       IsSomePreloadingEnabled_PreloadingHoldback) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kPreloadingHoldback);
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDefault));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDisabled));
  EXPECT_EQ(prefetch::IsSomePreloadingEnabled(prefs),
            content::PreloadingEligibility::kPreloadingDisabled);

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kStandard));
  EXPECT_EQ(prefetch::IsSomePreloadingEnabled(prefs),
            content::PreloadingEligibility::kPreloadingDisabled);

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(
          prefetch::NetworkPredictionOptions::kWifiOnlyDeprecated));
  EXPECT_EQ(prefetch::IsSomePreloadingEnabled(prefs),
            content::PreloadingEligibility::kPreloadingDisabled);

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kExtended));
  EXPECT_EQ(prefetch::IsSomePreloadingEnabled(prefs),
            content::PreloadingEligibility::kPreloadingDisabled);
}

TEST_F(PrefetchPrefsPreloadingTest, IsSomePreloadingEnabledIgnoringFinch) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kPreloadingHoldback);
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDefault));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDisabled));
  EXPECT_EQ(prefetch::IsSomePreloadingEnabledIgnoringFinch(prefs),
            content::PreloadingEligibility::kPreloadingDisabled);

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kStandard));
  EXPECT_EQ(prefetch::IsSomePreloadingEnabledIgnoringFinch(prefs),
            content::PreloadingEligibility::kEligible);

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(
          prefetch::NetworkPredictionOptions::kWifiOnlyDeprecated));
  EXPECT_EQ(prefetch::IsSomePreloadingEnabledIgnoringFinch(prefs),
            content::PreloadingEligibility::kEligible);

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kExtended));
  EXPECT_EQ(prefetch::IsSomePreloadingEnabledIgnoringFinch(prefs),
            content::PreloadingEligibility::kEligible);
}

class PrefetchPrefsWithBatterySaverTest : public ::testing::Test {
 public:
  PrefetchPrefsWithBatterySaverTest() = default;
  ~PrefetchPrefsWithBatterySaverTest() override = default;

  void TearDown() override { battery::ResetIsBatterySaverEnabledForTesting(); }

  // IsSomePreloadingEnabled[IgnoringFinch]() requires a threaded environment.
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PrefetchPrefsWithBatterySaverTest,
       IsSomePreloadingEnabledIgnoringFinch) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDefault));

  battery::OverrideIsBatterySaverEnabledForTesting(false);
  EXPECT_EQ(prefetch::IsSomePreloadingEnabledIgnoringFinch(prefs),
            content::PreloadingEligibility::kEligible);

  battery::OverrideIsBatterySaverEnabledForTesting(true);
  EXPECT_EQ(prefetch::IsSomePreloadingEnabledIgnoringFinch(prefs),
            content::PreloadingEligibility::kBatterySaverEnabled);
}

TEST_F(PrefetchPrefsWithBatterySaverTest, IsSomePreloadingEnabled) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDefault));

  prefs.SetInteger(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kStandard));

  battery::OverrideIsBatterySaverEnabledForTesting(false);
  EXPECT_EQ(prefetch::IsSomePreloadingEnabled(prefs),
            content::PreloadingEligibility::kEligible);

  battery::OverrideIsBatterySaverEnabledForTesting(true);
  EXPECT_EQ(prefetch::IsSomePreloadingEnabled(prefs),
            content::PreloadingEligibility::kBatterySaverEnabled);
}

class PrefetchPrefsWithDataSaverTest : public ::testing::Test {
 public:
  PrefetchPrefsWithDataSaverTest() = default;
  ~PrefetchPrefsWithDataSaverTest() override = default;

  void TearDown() override { data_saver::ResetIsDataSaverEnabledForTesting(); }

  // IsSomePreloadingEnabledIgnoringFinch() requires a threaded environment.
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PrefetchPrefsWithDataSaverTest, IsSomePreloadingEnabledIgnoringFinch) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDefault));
  data_saver::OverrideIsDataSaverEnabledForTesting(false);
  EXPECT_EQ(prefetch::IsSomePreloadingEnabledIgnoringFinch(prefs),
            content::PreloadingEligibility::kEligible);

  data_saver::OverrideIsDataSaverEnabledForTesting(true);
  EXPECT_EQ(prefetch::IsSomePreloadingEnabledIgnoringFinch(prefs),
            content::PreloadingEligibility::kDataSaverEnabled);
}
