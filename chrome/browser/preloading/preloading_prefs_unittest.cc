// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/preloading_prefs.h"

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

TEST(PreloadingPrefsTest, GetPreloadPagesState) {
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

TEST(PreloadingPrefsTest, SetPreloadPagesState) {
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

class PreloadingPrefsPreloadingTest : public ::testing::Test {
 public:
  PreloadingPrefsPreloadingTest() = default;
  ~PreloadingPrefsPreloadingTest() override = default;

  // IsSomePreloadingEnabled() requires a threaded environment.
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PreloadingPrefsPreloadingTest, IsSomePreloadingEnabled) {
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

class PreloadingPrefsWithBatterySaverTest
    : public PreloadingPrefsPreloadingTest {
 public:
  PreloadingPrefsWithBatterySaverTest() = default;
  ~PreloadingPrefsWithBatterySaverTest() override = default;

  void TearDown() override { battery::ResetIsBatterySaverEnabledForTesting(); }
};

TEST_F(PreloadingPrefsWithBatterySaverTest, IsSomePreloadingEnabled) {
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

class PreloadingPrefsWithDataSaverTest : public PreloadingPrefsPreloadingTest {
 public:
  PreloadingPrefsWithDataSaverTest() = default;
  ~PreloadingPrefsWithDataSaverTest() override = default;

  void TearDown() override { data_saver::ResetIsDataSaverEnabledForTesting(); }
};

TEST_F(PreloadingPrefsWithDataSaverTest, IsSomePreloadingEnabled) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(prefetch::NetworkPredictionOptions::kDefault));
  data_saver::OverrideIsDataSaverEnabledForTesting(false);
  EXPECT_EQ(prefetch::IsSomePreloadingEnabled(prefs),
            content::PreloadingEligibility::kEligible);

  data_saver::OverrideIsDataSaverEnabledForTesting(true);
  EXPECT_EQ(prefetch::IsSomePreloadingEnabled(prefs),
            content::PreloadingEligibility::kDataSaverEnabled);
}
