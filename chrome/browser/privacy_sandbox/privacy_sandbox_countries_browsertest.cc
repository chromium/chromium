// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_test.h"

namespace privacy_sandbox {
namespace {

struct PrivacySandboxCountriesTestData {
  // Inputs
  std::string stored_permanent_country;
  // Expectations
  bool is_consent_country;
  bool is_rest_of_world;
  bool is_varation_service_ready;
  bool is_variation_stored_permanent_country_empty;
};

class PrivacySandboxCountriesBrowserTestBase : public PlatformBrowserTest {
 public:
  PrivacySandboxCountriesBrowserTestBase() {
    privacy_sandbox_countries_ = GetSingletonPrivacySandboxCountries();
  }

  PrivacySandboxCountries* privacy_sandbox_countries() {
    return privacy_sandbox_countries_.get();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<PrivacySandboxCountries> privacy_sandbox_countries_;
};

class PrivacySandboxCountriesBrowserTest
    : public PrivacySandboxCountriesBrowserTestBase,
      public testing::WithParamInterface<PrivacySandboxCountriesTestData> {};

IN_PROC_BROWSER_TEST_P(PrivacySandboxCountriesBrowserTest,
                       ValidateCorrectCountriesAndHistograms) {
  base::HistogramTester histogram_tester;
  g_browser_process->variations_service()->OverrideStoredPermanentCountry(
      GetParam().stored_permanent_country);
  EXPECT_EQ(privacy_sandbox_countries()->IsConsentCountry(),
            GetParam().is_consent_country);
  EXPECT_EQ(privacy_sandbox_countries()->IsRestOfWorldCountry(),
            GetParam().is_rest_of_world);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.NoticeRequirement.IsVariationServiceReady",
      GetParam().is_varation_service_ready, 1);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.NoticeRequirement.IsVariationCountryEmpty",
      GetParam().is_variation_stored_permanent_country_empty, 1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PrivacySandboxCountriesBrowserTest,
    testing::Values(
        PrivacySandboxCountriesTestData{
            // Inputs
            .stored_permanent_country = "gb",
            // Expectations
            .is_consent_country = true,
            .is_rest_of_world = false,
            .is_varation_service_ready = true,
            .is_variation_stored_permanent_country_empty = false,
        },
        PrivacySandboxCountriesTestData{
            // Inputs
            .stored_permanent_country = "GB",
            // Expectations
            .is_consent_country = true,
            .is_rest_of_world = false,
            .is_varation_service_ready = true,
            .is_variation_stored_permanent_country_empty = false,
        },
        PrivacySandboxCountriesTestData{
            // Inputs
            .stored_permanent_country = "us",
            // Expectations
            .is_consent_country = false,
            .is_rest_of_world = true,
            .is_varation_service_ready = true,
            .is_variation_stored_permanent_country_empty = false,
        },
        PrivacySandboxCountriesTestData{
            // Inputs
            .stored_permanent_country = "",
            // Expectations
            .is_consent_country = false,
            .is_rest_of_world = false,
            .is_varation_service_ready = true,
            .is_variation_stored_permanent_country_empty = true,
        }));

class PrivacySandboxLatestCountryChinaBrowserTest
    : public PrivacySandboxCountriesBrowserTestBase {};

IN_PROC_BROWSER_TEST_F(PrivacySandboxLatestCountryChinaBrowserTest,
                       ValidateIsLatestCountryChina) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      variations::switches::kVariationsOverrideCountry, "cn");
  EXPECT_EQ(privacy_sandbox_countries()->IsLatestCountryChina(), true);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxLatestCountryChinaBrowserTest,
                       ValidateIsNotLatestCountryChina) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      variations::switches::kVariationsOverrideCountry, "us");
  EXPECT_EQ(privacy_sandbox_countries()->IsLatestCountryChina(), false);
}

}  // namespace
}  // namespace privacy_sandbox
