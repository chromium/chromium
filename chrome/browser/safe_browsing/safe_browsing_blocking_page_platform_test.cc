// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains blocking page tests that are relevant both to Desktop
// and to Android (more specifically, if safe_browsing_mode > 0).

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page_platform_test_helper.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/db/fake_database_manager.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace safe_browsing {
namespace {
const char kEmptyPage[] = "/empty.html";
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageRealTimeUrlCheckTest,
                       EnterpriseRealTimeUrlCheck_NoWarning) {
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);

  // Set up enterprise lookup, including DM token.
  profile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  profile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);
  SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));

  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  SetupUrlRealTimeVerdictInCacheManager(url, profile(),
                                        RTLookupResponse::ThreatInfo::SAFE,
                                        /*threat_type=*/std::nullopt);
  NavigateToURL(url, /*expect_success=*/true);
  ASSERT_FALSE(
      chrome_browser_interstitials::IsShowingInterstitial(web_contents()));
  ASSERT_EQ(blocking_page_factory_ptr_->GetShownInterstitial(),
            TestSafeBrowsingBlockingPageFactory::InterstitialShown::kNone);
}
IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageRealTimeUrlCheckTest,
                       EnterpriseRealTimeUrlCheck_RegularWarningShown) {
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);

  // Set up enterprise lookup, including DM token.
  profile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  profile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);
  SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));

  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  SetupUrlRealTimeVerdictInCacheManager(
      url, profile(), RTLookupResponse::ThreatInfo::DANGEROUS,
      RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING);
  NavigateToURL(url, /*expect_success=*/false);
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingInterstitial(web_contents()));
  ASSERT_EQ(blocking_page_factory_ptr_->GetShownInterstitial(),
            TestSafeBrowsingBlockingPageFactory::InterstitialShown::kConsumer);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageRealTimeUrlCheckTest,
                       EnterpriseRealTimeUrlCheck_EnterpriseBlockPageShown) {
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);

  // Set up enterprise lookup, including DM token.
  profile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  profile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);
  SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));

  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  SetupUrlRealTimeVerdictInCacheManager(
      url, profile(), RTLookupResponse::ThreatInfo::DANGEROUS,
      RTLookupResponse::ThreatInfo::MANAGED_POLICY);
  NavigateToURL(url, /*expect_success=*/false);
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingInterstitial(web_contents()));
  ASSERT_EQ(
      blocking_page_factory_ptr_->GetShownInterstitial(),
      TestSafeBrowsingBlockingPageFactory::InterstitialShown::kEnterpriseBlock);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageRealTimeUrlCheckTest,
                       EnterpriseRealTimeUrlCheck_EnterpriseWarnPageShown) {
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);

  // Set up enterprise lookup, including DM token.
  profile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  profile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);
  SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));

  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  SetupUrlRealTimeVerdictInCacheManager(
      url, profile(), RTLookupResponse::ThreatInfo::WARN,
      RTLookupResponse::ThreatInfo::MANAGED_POLICY);
  NavigateToURL(url, /*expect_success=*/false);
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingInterstitial(web_contents()));
  ASSERT_EQ(
      blocking_page_factory_ptr_->GetShownInterstitial(),
      TestSafeBrowsingBlockingPageFactory::InterstitialShown::kEnterpriseWarn);
}

class SafeBrowsingBlockingPageHatsSurveyPlatformTest
    : public SafeBrowsingBlockingPageRealTimeUrlCheckTest,
      public testing::WithParamInterface<bool> {
 public:
  SafeBrowsingBlockingPageHatsSurveyPlatformTest() = default;

  void SetUp() override {
#if BUILDFLAG(IS_ANDROID)
    scoped_feature_list_.InitWithFeatures(
        {kRedWarningSurveyAndroid, kExtendedReportingRemovePrefDependency},
        /*disabled_features=*/{kDelayedWarnings});
#else
    scoped_feature_list_.InitWithFeatures(
        {kRedWarningSurvey, kExtendedReportingRemovePrefDependency},
        /*disabled_features=*/{kDelayedWarnings});
#endif
    SafeBrowsingBlockingPagePlatformBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SafeBrowsingBlockingPageRealTimeUrlCheckTest::SetUpCommandLine(
        command_line);
    if (IsSiteIsolationEnabled()) {
      content::IsolateAllSitesForTesting(command_line);
    }
  }

  bool IsSiteIsolationEnabled() const { return GetParam(); }

 protected:
  void SendCommand(
      security_interstitials::SecurityInterstitialCommand command) {
    auto* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            web_contents());
    ASSERT_TRUE(helper);
    auto* interstitial_page = static_cast<SafeBrowsingBlockingPage*>(
        helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting());
    ASSERT_TRUE(interstitial_page);
    interstitial_page->CommandReceived(base::NumberToString(command));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    SafeBrowsingBlockingPageHatsSurveyPlatformTestWithIsolationSetting,
    SafeBrowsingBlockingPageHatsSurveyPlatformTest,
    testing::Bool());

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageHatsSurveyPlatformTest,
                       ReportNotSentToSbButAttachedForHats) {
  GetUiManager()->SetExpectEmptyReportForHats(false);
  GetUiManager()->SetExpectReportUrlForHats(true);
  GetUiManager()->SetExpectInterstitialInteractions(2);

  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  SetUpUnsafeUrl(url);
  EnableExtendedReporting(false);

  EXPECT_CALL(*GetUiManager(),
              OnAttachThreatDetailsAndLaunchSurvey(/*is_tab_closed=*/false))
      .Times(1);

  NavigateToURL(url, /*expect_success=*/false);
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingInterstitial(web_contents()));

  content::TestNavigationObserver observer(web_contents());
  // Generate interstitial interactions.
  SendCommand(security_interstitials::CMD_SHOW_MORE_SECTION);
  SendCommand(security_interstitials::CMD_SHOW_MORE_SECTION);

  // Bypass warning.
  // This triggers AttachThreatDetailsAndLaunchSurvey.
  SendCommand(security_interstitials::CMD_PROCEED);
  observer.WaitForNavigationFinished();
  std::string report = GetUiManager()->GetReport();
  EXPECT_TRUE(report.empty());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageHatsSurveyPlatformTest,
                       ReportSentToSbAndAttachedForHats) {
  GetUiManager()->SetExpectEmptyReportForHats(false);
  GetUiManager()->SetExpectReportUrlForHats(true);
  GetUiManager()->SetExpectInterstitialInteractions(2);

  base::RunLoop run_loop;
  SetReportSentCallback(run_loop.QuitClosure());

  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  SetUpUnsafeUrl(url);
  EnableExtendedReporting(true);

  EXPECT_CALL(*GetUiManager(),
              OnAttachThreatDetailsAndLaunchSurvey(/*is_tab_closed=*/false))
      .Times(1);

  NavigateToURL(url, /*expect_success=*/false);
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingInterstitial(web_contents()));

  content::TestNavigationObserver observer(web_contents());
  // Generate interstitial interactions.
  SendCommand(security_interstitials::CMD_SHOW_MORE_SECTION);
  SendCommand(security_interstitials::CMD_SHOW_MORE_SECTION);

  // Bypass warning.
  // This triggers AttachThreatDetailsAndLaunchSurvey.
  SendCommand(security_interstitials::CMD_PROCEED);
  observer.WaitForNavigationFinished();
  run_loop.Run();
  std::string report = GetUiManager()->GetReport();
  EXPECT_FALSE(report.empty());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageHatsSurveyPlatformTest,
                       NoHatsSurveyWhenSafeBrowsingSurveysDisabled) {
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingSurveysEnabled, false);

  GetUiManager()->SetExpectEmptyReportForHats(true);

  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  SetUpUnsafeUrl(url);
  EnableExtendedReporting(false);

  EXPECT_CALL(*GetUiManager(), OnAttachThreatDetailsAndLaunchSurvey).Times(0);

  NavigateToURL(url, /*expect_success=*/false);
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingInterstitial(web_contents()));

  content::TestNavigationObserver observer(web_contents());
  // Bypass warning.
  SendCommand(security_interstitials::CMD_PROCEED);
  observer.WaitForNavigationFinished();
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageHatsSurveyPlatformTest,
                       NoHatsSurveyWhenProceedDisabled) {
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingProceedAnywayDisabled,
                                    true);

  GetUiManager()->SetExpectEmptyReportForHats(true);

  // Navigate to an initial page so GoBack has a previous navigation.
  NavigateToURL(embedded_test_server()->GetURL("/simple.html"));

  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  SetUpUnsafeUrl(url);
  EnableExtendedReporting(false);

  EXPECT_CALL(*GetUiManager(), OnAttachThreatDetailsAndLaunchSurvey).Times(0);

  NavigateToURL(url, /*expect_success=*/false);
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingInterstitial(web_contents()));

  content::TestNavigationObserver observer(web_contents());
  // Go back.
  SendCommand(security_interstitials::CMD_DONT_PROCEED);
  observer.WaitForNavigationFinished();
}

}  // namespace safe_browsing
