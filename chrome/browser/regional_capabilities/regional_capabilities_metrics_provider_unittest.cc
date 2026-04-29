// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_metrics_provider.h"

#include <map>
#include <memory>
#include <string>

#include "base/strings/to_string.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/metrics/profile_metrics_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/country_codes/country_codes.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/metrics/profile_metrics_service.h"
#include "components/regional_capabilities/regional_capabilities_metrics.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace regional_capabilities {

namespace {

std::unique_ptr<KeyedService> BuildServiceWithFakeClient(
    country_codes::CountryId country_id,
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return CreateServiceWithFakeClient(*profile->GetPrefs(), country_id);
}

}  // namespace

class RegionalCapabilitiesMetricsProviderTest : public testing::Test {
 public:
  RegionalCapabilitiesMetricsProviderTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
  }

  void TearDown() override {
    // The profile manager needs to be destroyed before the task environment.
    profile_manager_.DeleteAllTestingProfiles();
  }

  // Creates a profile and associates it with a country.
  // `profile_context` is used to construct the profile name and must not be
  // std::nullopt.
  void CreateProfileWithCountry(metrics::ProfileMetricsContext profile_context,
                                country_codes::CountryId country_id) {
    CHECK(profile_context.has_value());
    profile_manager_.CreateTestingProfile(
        base::StrCat({"profile", base::ToString(profile_context.value())}),
        {TestingProfile::TestingFactory(
             RegionalCapabilitiesServiceFactory::GetInstance(),
             base::BindRepeating(&BuildServiceWithFakeClient, country_id)),
         TestingProfile::TestingFactory(
             ProfileMetricsServiceFactory::GetInstance(),
             base::BindRepeating(
                 [](metrics::ProfileMetricsContext profile_context,
                    content::BrowserContext* context)
                     -> std::unique_ptr<KeyedService> {
                   return std::make_unique<metrics::ProfileMetricsService>(
                       profile_context);
                 },
                 profile_context))});
  }

 protected:
  base::HistogramTester histogram_tester_;
  RegionalCapabilitiesMetricsProvider metrics_provider_;
  TestingProfileManager profile_manager_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(RegionalCapabilitiesMetricsProviderTest, NoProfiles_Default) {
  metrics_provider_.ProvideCurrentSessionData(nullptr);

  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.Debug.HasActiveRegionalProgram", false, 1);
  histogram_tester_.ExpectTotalCount(
      "RegionalCapabilities.ActiveRegionalProgram2", 0);

  histogram_tester_.ExpectTotalCount(
      "RegionalCapabilities.ActiveRegionalProgram3", 0);
  histogram_tester_.ExpectTotalCount(
      "RegionalCapabilities.ActiveRegionalProgram3.Profile1", 0);
}

TEST_F(RegionalCapabilitiesMetricsProviderTest, SingleDefault_Default) {
  CreateProfileWithCountry(metrics::ProfileMetricsContext{1},
                           country_codes::CountryId("US"));

  metrics_provider_.ProvideCurrentSessionData(nullptr);

  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram2",
      ActiveRegionalProgram::kDefault, 1);

  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram3",
      ActiveRegionalProgram::kDefault, 1);
  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram3.Profile1",
      ActiveRegionalProgram::kDefault, 1);
}

TEST_F(RegionalCapabilitiesMetricsProviderTest, SingleWaffle_Waffle) {
  CreateProfileWithCountry(metrics::ProfileMetricsContext{1},
                           country_codes::CountryId("FR"));

  metrics_provider_.ProvideCurrentSessionData(nullptr);

  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram2",
      ActiveRegionalProgram::kWaffle, 1);

  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram3",
      ActiveRegionalProgram::kWaffle, 1);
  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram3.Profile1",
      ActiveRegionalProgram::kWaffle, 1);
}

// Skip on platforms that don't have a system profile.
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(RegionalCapabilitiesMetricsProviderTest,
       SystemProfileAndWaffle_Waffle) {
  profile_manager_.CreateSystemProfile();
  CreateProfileWithCountry(metrics::ProfileMetricsContext{1},
                           country_codes::CountryId("FR"));

  metrics_provider_.ProvideCurrentSessionData(nullptr);

  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram2",
      ActiveRegionalProgram::kWaffle, 1);

  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram3",
      ActiveRegionalProgram::kWaffle, 1);
  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram3.Profile1",
      ActiveRegionalProgram::kWaffle, 1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

TEST_F(RegionalCapabilitiesMetricsProviderTest, MultipleWaffle_Waffle) {
  CreateProfileWithCountry(metrics::ProfileMetricsContext{1},
                           country_codes::CountryId("FR"));
  CreateProfileWithCountry(metrics::ProfileMetricsContext{2},
                           country_codes::CountryId("FR"));

  metrics_provider_.ProvideCurrentSessionData(nullptr);

  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram2",
      ActiveRegionalProgram::kWaffle, 1);

  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram3",
      ActiveRegionalProgram::kWaffle, 2);
  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram3.Profile1",
      ActiveRegionalProgram::kWaffle, 1);
  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram3.Profile2",
      ActiveRegionalProgram::kWaffle, 1);
}

TEST_F(RegionalCapabilitiesMetricsProviderTest, DefaultAndWaffle_Waffle) {
  // Default.
  CreateProfileWithCountry(metrics::ProfileMetricsContext{1},
                           country_codes::CountryId("US"));
  // Waffle.
  CreateProfileWithCountry(metrics::ProfileMetricsContext{2},
                           country_codes::CountryId("FR"));

  metrics_provider_.ProvideCurrentSessionData(nullptr);

  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram2",
      ActiveRegionalProgram::kWaffle, 1);

  histogram_tester_.ExpectBucketCount(
      "RegionalCapabilities.ActiveRegionalProgram3",
      ActiveRegionalProgram::kDefault, 1);
  histogram_tester_.ExpectBucketCount(
      "RegionalCapabilities.ActiveRegionalProgram3",
      ActiveRegionalProgram::kWaffle, 1);
  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram3.Profile1",
      ActiveRegionalProgram::kDefault, 1);
  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram3.Profile2",
      ActiveRegionalProgram::kWaffle, 1);
}

// There is no test case for multiple non-default programs, because this isn't
// currently possible: Android is single-profile, and Desktop only supports
// Waffle.

}  // namespace regional_capabilities
