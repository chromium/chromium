// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains blocking page tests that are relevant both to Desktop
// and to Android (more specifically, if safe_browsing_mode > 0).

#include "base/test/metrics/histogram_tester.h"
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
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
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

}  // namespace safe_browsing
