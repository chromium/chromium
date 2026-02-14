// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/gemini_antiscam_protection/gemini_antiscam_protection_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/gemini_antiscam_protection/gemini_antiscam_protection_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
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
    feature_list_.InitWithFeatures(
        {kGeminiAntiscamProtectionForMetricsCollection},
        {optimization_guide::features::kPreventLongRunningPredictionModels});
  }
  ~GeminiAntiscamProtectionServiceBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Mock the `ModelQualityLogsUploaderService`.
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->SetModelQualityLogsUploaderServiceForTesting(
            std::make_unique<
                optimization_guide::TestModelQualityLogsUploaderService>(
                g_browser_process->local_state()));
  }

  void SetUpSuccessfulModelExecution(float scam_score,
                                     std::string content_category,
                                     std::string justification) {
    optimization_guide::proto::GeminiAntiscamProtectionResponse response;
    response.set_scam_score(scam_score);
    response.set_content_category(content_category);
    response.set_justification(justification);
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

  optimization_guide::TestModelQualityLogsUploaderService* logs_uploader() {
    return static_cast<
        optimization_guide::TestModelQualityLogsUploaderService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile())
            ->GetModelQualityLogsUploaderService());
  }

  const std::vector<
      std::unique_ptr<optimization_guide::proto::LogAiDataRequest>>&
  uploaded_logs() {
    return logs_uploader()->uploaded_logs();
  }

  void VerifyUniqueQualityLog(const GURL& url,
                              const std::string& page_text,
                              float scam_score,
                              const std::string& content_category,
                              const std::string& justification) {
    ASSERT_EQ(1u, uploaded_logs().size());
    const auto& log = uploaded_logs()[0]->gemini_antiscam_protection();
    EXPECT_EQ(url.spec(), log.request().url());
    EXPECT_EQ(page_text, log.request().page_content());
    EXPECT_EQ(scam_score, log.response().scam_score());
    EXPECT_EQ(content_category, log.response().content_category());
    EXPECT_EQ(justification, log.response().justification());
    EXPECT_EQ(false, log.metadata().page_contains_financial_fields());
    EXPECT_EQ(false, log.metadata().page_contains_password_field());
    EXPECT_EQ(false, log.metadata().page_contains_identity_fields());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
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
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/")));
  base::HistogramTester histogram_tester;
  auto* service = GeminiAntiscamProtectionServiceFactory::GetForProfile(
      browser()->profile());
  ASSERT_NE(nullptr, service);
  service->MaybeStartAntiscamProtection(
      GeminiAntiscamProtectionService::BuildGeminiAntiscamProtectionMetadata(
          web_contents()),
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
      ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
      "page text");
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(
      3u, histogram_tester
              .GetTotalCountsForPrefix("SafeBrowsing.GeminiAntiscamProtection")
              .size());
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.IsHistoryServiceResultValid",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.ShouldSkipDueToPreviousVisit",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.GeminiAntiscamProtection.FailedEmptyResponse.Latency", 1);
}

IN_PROC_BROWSER_TEST_F(GeminiAntiscamProtectionServiceBrowserTest,
                       EnhancedProtection_FailedParsingError) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced,
                                               true);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/")));
  base::HistogramTester histogram_tester;
  auto* service = GeminiAntiscamProtectionServiceFactory::GetForProfile(
      browser()->profile());
  ASSERT_NE(nullptr, service);
  SetUpFailedParsingModelExecution();
  service->MaybeStartAntiscamProtection(
      GeminiAntiscamProtectionService::BuildGeminiAntiscamProtectionMetadata(
          web_contents()),
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
      ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
      "page text");
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(
      3u, histogram_tester
              .GetTotalCountsForPrefix("SafeBrowsing.GeminiAntiscamProtection")
              .size());
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.IsHistoryServiceResultValid",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.ShouldSkipDueToPreviousVisit",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.GeminiAntiscamProtection.FailedParsingError.Latency", 1);
}

IN_PROC_BROWSER_TEST_F(
    GeminiAntiscamProtectionServiceBrowserTest,
    EnhancedProtection_SuccessfulResponseReturnsScamVerdict) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced,
                                               true);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/")));
  base::HistogramTester histogram_tester;
  auto* service = GeminiAntiscamProtectionServiceFactory::GetForProfile(
      browser()->profile());
  ASSERT_NE(nullptr, service);

  float scam_score = 0.6;
  std::string content_category = "investment(non-crypto)";
  std::string justification = "gemini is smart";
  SetUpSuccessfulModelExecution(scam_score, content_category, justification);
  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader()->WaitForLogUpload(log_uploaded_signal.GetCallback());

  std::string page_text = "page text";
  service->MaybeStartAntiscamProtection(
      GeminiAntiscamProtectionService::BuildGeminiAntiscamProtectionMetadata(
          web_contents()),
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
      ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(), page_text);
  ASSERT_TRUE(log_uploaded_signal.Wait());

  EXPECT_EQ(
      4u, histogram_tester
              .GetTotalCountsForPrefix("SafeBrowsing.GeminiAntiscamProtection")
              .size());
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.IsHistoryServiceResultValid",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.ShouldSkipDueToPreviousVisit",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.GeminiAntiscamProtection.Success.Latency", 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.Investment.ScamScore",
      /*sample=*/scam_score * 100, /*expected_bucket_count=*/1);
  VerifyUniqueQualityLog(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(), page_text,
      scam_score, content_category, justification);
}

IN_PROC_BROWSER_TEST_F(
    GeminiAntiscamProtectionServiceBrowserTest,
    EnhancedProtection_SuccessfulResponseReturnsBenignVerdict) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced,
                                               true);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/")));
  base::HistogramTester histogram_tester;
  auto* service = GeminiAntiscamProtectionServiceFactory::GetForProfile(
      browser()->profile());
  ASSERT_NE(nullptr, service);

  float scam_score = 0.3;
  std::string content_category = "charity";
  std::string justification = "";
  SetUpSuccessfulModelExecution(scam_score, content_category, justification);
  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader()->WaitForLogUpload(log_uploaded_signal.GetCallback());

  std::string page_text = "page text";
  service->MaybeStartAntiscamProtection(
      GeminiAntiscamProtectionService::BuildGeminiAntiscamProtectionMetadata(
          web_contents()),
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
      ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(), page_text);
  ASSERT_TRUE(log_uploaded_signal.Wait());

  EXPECT_EQ(
      4u, histogram_tester
              .GetTotalCountsForPrefix("SafeBrowsing.GeminiAntiscamProtection")
              .size());
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.IsHistoryServiceResultValid",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.ShouldSkipDueToPreviousVisit",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.GeminiAntiscamProtection.Success.Latency", 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.Charity.ScamScore",
      /*sample=*/scam_score * 100, /*expected_bucket_count=*/1);
  // Page text is not logged for benign verdicts.
  VerifyUniqueQualityLog(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
      /*page_text=*/"", scam_score, content_category, justification);
}

}  // namespace safe_browsing
