// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/gemini_antiscam_protection/gemini_antiscam_protection_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/gemini_antiscam_protection/gemini_antiscam_protection_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/optimization_guide/proto/features/gemini_antiscam_protection.pb.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {

class GeminiAntiscamProtectionServiceBrowserTest : public InProcessBrowserTest {
 public:
  GeminiAntiscamProtectionServiceBrowserTest() {
    feature_list_.InitAndEnableFeature(
        kGeminiAntiscamProtectionForMetricsCollection);
  }
  ~GeminiAntiscamProtectionServiceBrowserTest() override = default;

  void SetUpSuccessfulModelExecution() {
    optimization_guide::proto::GeminiAntiscamProtectionResponse response;
    response.set_scam_score(0.6);
    response.set_content_category("phishing");
    response.set_justification("gemini is smart");
    std::string serialized_metadata;
    response.SerializeToString(&serialized_metadata);
    optimization_guide::proto::Any any_result;
    any_result.set_type_url(
        base::StrCat({"type.googleapis.com/", response.GetTypeName()}));
    any_result.set_value(serialized_metadata);
    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->GetForProfile(browser()->profile())
        ->AddExecutionResultForTesting(
            optimization_guide::ModelBasedCapabilityKey::
                kGeminiAntiscamProtection,
            optimization_guide::OptimizationGuideModelExecutionResult(
                any_result, nullptr));
  }

  void SetUpFailedParsingModelExecution() {
    optimization_guide::proto::Any any_result;
    std::string serialized_metadata;
    any_result.SerializeToString(&serialized_metadata);
    any_result.set_type_url(
        base::StrCat({"type.googleapis.com/", any_result.GetTypeName()}));
    any_result.set_value(serialized_metadata);
    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->GetForProfile(browser()->profile())
        ->AddExecutionResultForTesting(
            optimization_guide::ModelBasedCapabilityKey::
                kGeminiAntiscamProtection,
            optimization_guide::OptimizationGuideModelExecutionResult(
                any_result, nullptr));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GeminiAntiscamProtectionServiceBrowserTest,
                       StandardProtection_NoService) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced,
                                               false);
  EXPECT_EQ(nullptr, GeminiAntiscamProtectionServiceFactory::GetForProfile(
                         browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(GeminiAntiscamProtectionServiceBrowserTest,
                       IncognitoProfile_NoService) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced,
                                               true);
  EXPECT_EQ(nullptr, GeminiAntiscamProtectionServiceFactory::GetForProfile(
                         browser()->profile()->GetOffTheRecordProfile(
                             Profile::OTRProfileID::CreateUniqueForTesting(),
                             /*create_if_needed=*/true)));
}

IN_PROC_BROWSER_TEST_F(GeminiAntiscamProtectionServiceBrowserTest,
                       EnhancedProtection_HasService) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced,
                                               true);
  auto* service = GeminiAntiscamProtectionServiceFactory::GetForProfile(
      browser()->profile());
  EXPECT_NE(nullptr, service);
}

IN_PROC_BROWSER_TEST_F(GeminiAntiscamProtectionServiceBrowserTest,
                       EnhancedProtection_EmptyResponse) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced,
                                               true);
  base::HistogramTester histogram_tester;
  auto* service = GeminiAntiscamProtectionServiceFactory::GetForProfile(
      browser()->profile());
  ASSERT_NE(nullptr, service);
  service->MaybeStartAntiscamProtection(
      GURL("https://example.com"), ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      /*should_show_scam_warning=*/false,
      /*is_phishing=*/false, "page text");
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(
      1u, histogram_tester
              .GetTotalCountsForPrefix("SafeBrowsing.GeminiAntiscamProtection")
              .size());
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.GeminiAntiscamProtection.FailedEmptyResponse.Latency", 1);
}

IN_PROC_BROWSER_TEST_F(GeminiAntiscamProtectionServiceBrowserTest,
                       EnhancedProtection_FailedParsingError) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced,
                                               true);
  base::HistogramTester histogram_tester;
  auto* service = GeminiAntiscamProtectionServiceFactory::GetForProfile(
      browser()->profile());
  ASSERT_NE(nullptr, service);
  SetUpFailedParsingModelExecution();
  service->MaybeStartAntiscamProtection(
      GURL("https://example.com"), ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      /*should_show_scam_warning=*/false,
      /*is_phishing=*/false, "page text");
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(
      1u, histogram_tester
              .GetTotalCountsForPrefix("SafeBrowsing.GeminiAntiscamProtection")
              .size());
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.GeminiAntiscamProtection.FailedParsingError.Latency", 1);
}

IN_PROC_BROWSER_TEST_F(GeminiAntiscamProtectionServiceBrowserTest,
                       EnhancedProtection_SuccessfulResponse) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced,
                                               true);
  base::HistogramTester histogram_tester;
  auto* service = GeminiAntiscamProtectionServiceFactory::GetForProfile(
      browser()->profile());
  ASSERT_NE(nullptr, service);
  SetUpSuccessfulModelExecution();
  service->MaybeStartAntiscamProtection(
      GURL("https://example.com"), ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      /*should_show_scam_warning=*/false,
      /*is_phishing=*/false, "page text");
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(
      1u, histogram_tester
              .GetTotalCountsForPrefix("SafeBrowsing.GeminiAntiscamProtection")
              .size());
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.GeminiAntiscamProtection.Success.Latency", 1);
}

}  // namespace safe_browsing
