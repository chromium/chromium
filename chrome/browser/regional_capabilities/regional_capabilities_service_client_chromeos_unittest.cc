// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_client_chromeos.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_test_environment.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/country_codes/country_codes.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/test_variations_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::country_codes::CountryId;

namespace regional_capabilities {
namespace {

class RegionalCapabilitiesServiceClientChromeOSTest : public ::testing::Test {
 public:
  void SetUp() override {
    ash::system::StatisticsProvider::SetTestProvider(&sys_info_);
  }

  void TearDown() override {
    ash::system::StatisticsProvider::SetTestProvider(nullptr);
  }

  void SetLoadingState(ash::system::StatisticsProvider::LoadingState state) {
    sys_info_.SetLoadingState(state);
  }

  void SetRegion(const std::string& region) {
    sys_info_.SetMachineStatistic(ash::system::kRegionKey, region);
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  RegionalCapabilitiesTestEnvironment& rcaps_env() { return rcaps_env_; }

 private:
  base::test::TaskEnvironment task_environment_;

  RegionalCapabilitiesTestEnvironment rcaps_env_;

  base::HistogramTester histogram_tester_;
  ash::system::FakeStatisticsProvider sys_info_;
};

TEST_F(RegionalCapabilitiesServiceClientChromeOSTest,
       GetFallbackCountryId_LoadingState) {
  RegionalCapabilitiesServiceClientChromeOS client(
      /* variations_service= */ nullptr);

  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData,
      ChromeOSFallbackCountry::kStatisticsLoadingNotFinished, 0);

  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kNotStarted);
  EXPECT_EQ(client.GetFallbackCountryId(),
            country_codes::GetCurrentCountryID());
  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData,
      ChromeOSFallbackCountry::kStatisticsLoadingNotFinished, 1);

  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kStarted);
  EXPECT_EQ(client.GetFallbackCountryId(),
            country_codes::GetCurrentCountryID());
  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData,
      ChromeOSFallbackCountry::kStatisticsLoadingNotFinished, 2);

  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kFinished);
  EXPECT_EQ(client.GetFallbackCountryId(),
            country_codes::GetCurrentCountryID());
  histogram_tester().ExpectBucketCount(
      kCrOSMissingVariationData,
      ChromeOSFallbackCountry::kStatisticsLoadingNotFinished, 2);
}

TEST_F(RegionalCapabilitiesServiceClientChromeOSTest,
       GetFallbackCountryId_GroupedRegions) {
  RegionalCapabilitiesServiceClientChromeOS client(
      /* variations_service= */ nullptr);
  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kFinished);

  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kGroupedRegion, 0);
  int i = 0;
  for (const std::string region : {"gcc", "LaTaM-Es-419", "NORDIC"}) {
    SetRegion(region);
    histogram_tester().ExpectUniqueSample(
        kCrOSMissingVariationData, ChromeOSFallbackCountry::kGroupedRegion, i);
    EXPECT_EQ(client.GetFallbackCountryId(),
              country_codes::GetCurrentCountryID());
    histogram_tester().ExpectUniqueSample(
        kCrOSMissingVariationData, ChromeOSFallbackCountry::kGroupedRegion,
        ++i);
  }
}

TEST_F(RegionalCapabilitiesServiceClientChromeOSTest,
       GetFallbackCountryId_RegionTooShort) {
  RegionalCapabilitiesServiceClientChromeOS client(
      /* variations_service= */ nullptr);
  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kFinished);

  histogram_tester().ExpectTotalCount(kCrOSMissingVariationData, 0);
  EXPECT_EQ(client.GetFallbackCountryId(),
            country_codes::GetCurrentCountryID());
  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kRegionAbsent, 1);

  SetRegion("");
  EXPECT_EQ(client.GetFallbackCountryId(),
            country_codes::GetCurrentCountryID());
  histogram_tester().ExpectBucketCount(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kRegionAbsent, 1);
  histogram_tester().ExpectBucketCount(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kRegionEmpty, 1);

  SetRegion("a");
  EXPECT_EQ(client.GetFallbackCountryId(),
            country_codes::GetCurrentCountryID());
  histogram_tester().ExpectBucketCount(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kRegionAbsent, 1);
  histogram_tester().ExpectBucketCount(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kRegionEmpty, 1);
  histogram_tester().ExpectBucketCount(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kRegionTooShort, 1);
}

TEST_F(RegionalCapabilitiesServiceClientChromeOSTest,
       GetFallbackCountryId_RegionTooLong) {
  RegionalCapabilitiesServiceClientChromeOS client(
      /* variations_service= */ nullptr);
  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kFinished);

  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kRegionTooLong, 0);
  SetRegion("en_US");
  EXPECT_EQ(client.GetFallbackCountryId(),
            country_codes::GetCurrentCountryID());
  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kRegionTooLong, 1);
}

TEST_F(RegionalCapabilitiesServiceClientChromeOSTest,
       GetFallbackCountryId_ValidRegion) {
  RegionalCapabilitiesServiceClientChromeOS client(
      /* variations_service= */ nullptr);
  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kFinished);

  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kValidCountryCode, 0);
  std::string country_code = "DE";
  if (country_code == country_codes::GetCurrentCountryID().CountryCode()) {
    country_code = "BE";
  }
  SetRegion(country_code);
  const CountryId fallback_id = client.GetFallbackCountryId();
  ASSERT_NE(fallback_id, country_codes::GetCurrentCountryID());
  EXPECT_EQ(fallback_id, country_codes::CountryId(country_code));
  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kValidCountryCode, 1);
}

TEST_F(RegionalCapabilitiesServiceClientChromeOSTest,
       GetFallbackCountryId_StripKeyboardLayout) {
  RegionalCapabilitiesServiceClientChromeOS client(
      /* variations_service= */ nullptr);
  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kFinished);

  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kValidCountryCode, 0);
  std::string country_code = "DE";
  if (country_code == country_codes::GetCurrentCountryID().CountryCode()) {
    country_code = "BE";
  }
  const std::string country_with_layout_info = country_code + ".us-intl";
  SetRegion(country_with_layout_info);
  const CountryId fallback_id = client.GetFallbackCountryId();
  ASSERT_NE(fallback_id, country_codes::GetCurrentCountryID());
  EXPECT_EQ(fallback_id, country_codes::CountryId(country_code));
  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kValidCountryCode, 1);
  histogram_tester().ExpectUniqueSample(kVpdRegionSplittingOutcome, true, 1);
}

TEST_F(RegionalCapabilitiesServiceClientChromeOSTest,
       GetFallbackCountryId_StripKeyboardLayoutFailure) {
  RegionalCapabilitiesServiceClientChromeOS client(
      /* variations_service= */ nullptr);
  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kFinished);

  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kValidCountryCode, 0);
  SetRegion("99.us-intl");
  const CountryId fallback_id = client.GetFallbackCountryId();
  ASSERT_EQ(fallback_id, country_codes::GetCurrentCountryID());
  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kInvalidCountryCode,
      1);
  histogram_tester().ExpectUniqueSample(kVpdRegionSplittingOutcome, false, 1);
}

TEST_F(RegionalCapabilitiesServiceClientChromeOSTest, FetchCountryId) {
  // Set up variations_service::GetLatestCountry().
  rcaps_env().pref_service().SetString(variations::prefs::kVariationsCountry,
                                       "fr");

  // Set up variations_service::GetStoredPermanentCountry().
  rcaps_env().variations_service().OverrideStoredPermanentCountry("DE");

  RegionalCapabilitiesServiceClientChromeOS client(
      &rcaps_env().variations_service());

  base::test::TestFuture<CountryId> future;
  client.FetchCountryId(future.GetCallback());
  EXPECT_EQ(future.Get(), CountryId("DE"));
}

}  // namespace
}  // namespace regional_capabilities
