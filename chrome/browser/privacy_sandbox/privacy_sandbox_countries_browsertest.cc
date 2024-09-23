// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries_impl.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/variations/service/variations_service.h"
#include "content/public/test/browser_test.h"

namespace privacy_sandbox {
namespace {

struct PrivacySandboxCountriesTestData {
  // Inputs
  std::vector<base::test::FeatureRefAndParams> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  std::string variation_country;
  // Expectations
  bool is_consent_country;
  bool is_rest_of_world;
  bool is_varation_service_ready;
  bool is_variation_country_empty;
};

class PrivacySandboxCountriesBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<PrivacySandboxCountriesTestData> {
 public:
  PrivacySandboxCountriesBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(GetParam().enabled_features,
                                                GetParam().disabled_features);
    privacy_sandbox_countries_ =
        std::make_unique<PrivacySandboxCountriesImpl>();
  }

  void TearDown() override { privacy_sandbox_countries_.reset(); }

  PrivacySandboxCountries* privacy_sandbox_countries() {
    return privacy_sandbox_countries_.get();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<PrivacySandboxCountriesImpl> privacy_sandbox_countries_;
};

IN_PROC_BROWSER_TEST_P(PrivacySandboxCountriesBrowserTest, ConsentCountryTest) {
  base::HistogramTester histogram_tester;
  g_browser_process->variations_service()->OverrideStoredPermanentCountry(
      GetParam().variation_country);
  EXPECT_EQ(privacy_sandbox_countries()->IsConsentCountry(),
            GetParam().is_consent_country);
  EXPECT_EQ(privacy_sandbox_countries()->IsRestOfWorldCountry(),
            GetParam().is_rest_of_world);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.NoticeRequirement.IsVariationServiceReady",
      GetParam().is_varation_service_ready, 1);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.NoticeRequirement.IsVariationCountryEmpty",
      GetParam().is_variation_country_empty, 1);
}

INSTANTIATE_TEST_SUITE_P(,
                         PrivacySandboxCountriesBrowserTest,
                         testing::Values(
                             PrivacySandboxCountriesTestData{
                                 // Inputs
                                 .variation_country = "gb",
                                 // Expectations
                                 .is_consent_country = true,
                                 .is_rest_of_world = false,
                                 .is_varation_service_ready = true,
                                 .is_variation_country_empty = false,
                             },
                             PrivacySandboxCountriesTestData{
                                 // Inputs
                                 .variation_country = "GB",
                                 // Expectations
                                 .is_consent_country = true,
                                 .is_rest_of_world = false,
                                 .is_varation_service_ready = true,
                                 .is_variation_country_empty = false,
                             },
                             PrivacySandboxCountriesTestData{
                                 // Inputs
                                 .variation_country = "us",
                                 // Expectations
                                 .is_consent_country = false,
                                 .is_rest_of_world = true,
                                 .is_varation_service_ready = true,
                                 .is_variation_country_empty = false,
                             },
                             PrivacySandboxCountriesTestData{
                                 // Inputs
                                 .variation_country = "",
                                 // Expectations
                                 .is_consent_country = false,
                                 .is_rest_of_world = false,
                                 .is_varation_service_ready = true,
                                 .is_variation_country_empty = true,
                             }));

}  // namespace
}  // namespace privacy_sandbox
