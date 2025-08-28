// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/enterprise/connectors/referrer_cache_utils.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_test_utils.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::ReturnRef;

namespace enterprise_data_protection {

namespace {

constexpr char kAnalysisPolicy[] = R"({
  "service_provider": "google",
  "enable": [
    {
      "url_list": ["*"],
      "tags": ["dlp", "malware"]
    }
  ],
  "block_until_verdict": 1
})";

}  // namespace

class DataProtectionNavigationControllerTest : public InProcessBrowserTest {
 public:
  DataProtectionNavigationControllerTest() {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));
  }

  safe_browsing::NavigationEventList* navigation_event_list() {
    return safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
        GetForBrowserContext(browser()->profile())
            ->navigation_event_list();
  }

  content::WebContents* contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  GURL main_url() {
    return embedded_test_server()->GetURL("/simple_page.html");
  }

  GURL secondary_url() { return GURL("http://a.com/"); }

  void AddFakeNavigationsToChain() {
    base::Time now = base::Time::Now();
    base::Time one_second_ago = base::Time::FromSecondsSinceUnixEpoch(
        now.InSecondsFSinceUnixEpoch() - 1.0);
    auto tab_id = sessions::SessionTabHelper::IdForTab(
        browser()->tab_strip_model()->GetActiveWebContents());

    std::unique_ptr<safe_browsing::NavigationEvent> first_navigation =
        std::make_unique<safe_browsing::NavigationEvent>();
    first_navigation->original_request_url = secondary_url();
    first_navigation->last_updated = one_second_ago;
    first_navigation->navigation_initiation =
        safe_browsing::ReferrerChainEntry::BROWSER_INITIATED;
    first_navigation->target_tab_id = tab_id;
    navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

    std::unique_ptr<safe_browsing::NavigationEvent> second_navigation =
        std::make_unique<safe_browsing::NavigationEvent>();
    second_navigation->source_url = secondary_url();
    second_navigation->original_request_url = main_url();
    second_navigation->last_updated = now;
    second_navigation->navigation_initiation =
        safe_browsing::ReferrerChainEntry::BROWSER_INITIATED;
    second_navigation->target_tab_id = tab_id;
    navigation_event_list()->RecordNavigationEvent(
        std::move(second_navigation));
  }

  ~DataProtectionNavigationControllerTest() override = default;
};

IN_PROC_BROWSER_TEST_F(DataProtectionNavigationControllerTest, PolicyUnset) {
  auto chain = enterprise_connectors::GetReferrerChain(
      main_url(), *browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(chain.empty());

  AddFakeNavigationsToChain();

  // Without any of the policies using the referrer chain cache enabled,
  // navigating should NOT populate the cache.
  ASSERT_FALSE(enterprise_connectors::HasCachedChainForTesting(*contents()));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url()));
  ASSERT_FALSE(enterprise_connectors::HasCachedChainForTesting(*contents()));

  // The chain can still be retrieved using the SB cache.
  chain = enterprise_connectors::GetReferrerChain(main_url(), *contents());
  ASSERT_EQ(chain.size(), 2u);
  ASSERT_EQ(chain[0].url(), main_url());
  ASSERT_EQ(chain[1].url(), secondary_url());
}

IN_PROC_BROWSER_TEST_F(DataProtectionNavigationControllerTest, DownloadItem) {
  auto chain = enterprise_connectors::GetReferrerChain(
      main_url(), *browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(chain.empty());

  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(),
      enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED,
      kAnalysisPolicy);
  AddFakeNavigationsToChain();
  ASSERT_FALSE(enterprise_connectors::HasCachedChainForTesting(*contents()));

  GURL download_url("https://fake.download/");
  GURL download_tab_url = main_url();
  std::vector<GURL> download_redirects = {
      GURL("https://foo.com"),
      GURL("https://bar.com"),
  };
  download::MockDownloadItem download;
  EXPECT_CALL(download, GetURL()).WillRepeatedly(ReturnRef(download_url));
  EXPECT_CALL(download, GetTabUrl())
      .WillRepeatedly(ReturnRef(download_tab_url));
  EXPECT_CALL(download, GetUrlChain())
      .WillRepeatedly(ReturnRef(download_redirects));

  auto contents_chain =
      enterprise_connectors::GetReferrerChain(GURL(), *contents());
  ASSERT_TRUE(contents_chain.empty());

  // Since the contents have no chain, the download's chain only consists of its
  // own URL.
  auto download_chain =
      safe_browsing::GetOrIdentifyReferrerChainForEnterprise(download);
  ASSERT_EQ(download_chain.size(), 1u);
  ASSERT_EQ(download_chain[0].url(), download_url);
  ASSERT_TRUE(enterprise_connectors::HasCachedChainForTesting(download));

  // Navigating shouldn't affect the already-cached download chain, even though
  // the contents's chain itself will be updated.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url()));

  download_chain =
      safe_browsing::GetOrIdentifyReferrerChainForEnterprise(download);
  ASSERT_EQ(download_chain.size(), 1u);
  ASSERT_EQ(download_chain[0].url(), download_url);
  ASSERT_TRUE(enterprise_connectors::HasCachedChainForTesting(download));

  contents_chain =
      enterprise_connectors::GetReferrerChain(main_url(), *contents());
  ASSERT_EQ(contents_chain.size(), 2u);
  ASSERT_EQ(contents_chain[0].url(), main_url());
  ASSERT_EQ(contents_chain[1].url(), secondary_url());
}

class DataProtectionNavigationControllerPolicyTest
    : public DataProtectionNavigationControllerTest,
      public testing::WithParamInterface<
          base::RepeatingCallback<void(PrefService*)>> {
 public:
  void EnablePolicy() { GetParam().Run(browser()->profile()->GetPrefs()); }

  void TearDownOnMainThread() override {
    enterprise_connectors::test::ClearAnalysisConnector(
        browser()->profile()->GetPrefs(),
        enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY);
    enterprise_connectors::test::ClearAnalysisConnector(
        browser()->profile()->GetPrefs(),
        enterprise_connectors::AnalysisConnector::FILE_ATTACHED);
    enterprise_connectors::test::ClearAnalysisConnector(
        browser()->profile()->GetPrefs(),
        enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED);
#if BUILDFLAG(IS_CHROMEOS)
    enterprise_connectors::test::ClearAnalysisConnector(
        browser()->profile()->GetPrefs(),
        enterprise_connectors::AnalysisConnector::FILE_TRANSFER);
#endif
    enterprise_connectors::test::ClearAnalysisConnector(
        browser()->profile()->GetPrefs(),
        enterprise_connectors::AnalysisConnector::PRINT);
    enterprise_connectors::test::SetOnSecurityEventReporting(
        browser()->profile()->GetPrefs(), false);
    browser()->profile()->GetPrefs()->ClearPref(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckMode);
    browser()->profile()->GetPrefs()->ClearPref(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckScope);
  }
};

IN_PROC_BROWSER_TEST_P(DataProtectionNavigationControllerPolicyTest,
                       PolicySet) {
  auto chain = enterprise_connectors::GetReferrerChain(
      main_url(), *browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(chain.empty());

  EnablePolicy();
  AddFakeNavigationsToChain();

  // With one of the policies using the referrer chain cache enabled, navigating
  // should automatically populate the cache.
  ASSERT_FALSE(enterprise_connectors::HasCachedChainForTesting(*contents()));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url()));
  ASSERT_TRUE(enterprise_connectors::HasCachedChainForTesting(*contents()));

  chain = enterprise_connectors::GetReferrerChain(
      main_url(), *browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_EQ(chain.size(), 2u);
  ASSERT_EQ(chain[0].url(), main_url());
  ASSERT_EQ(chain[1].url(), secondary_url());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    DataProtectionNavigationControllerPolicyTest,
    testing::Values(
        base::BindRepeating([](PrefService* prefs) {
          enterprise_connectors::test::SetAnalysisConnector(
              prefs, enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY,
              kAnalysisPolicy);
        }),
        base::BindRepeating([](PrefService* prefs) {
          enterprise_connectors::test::SetAnalysisConnector(
              prefs, enterprise_connectors::AnalysisConnector::FILE_ATTACHED,
              kAnalysisPolicy);
        }),
        base::BindRepeating([](PrefService* prefs) {
          enterprise_connectors::test::SetAnalysisConnector(
              prefs, enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED,
              kAnalysisPolicy);
        }),
#if BUILDFLAG(IS_CHROMEOS)
        base::BindRepeating([](PrefService* prefs) {
          enterprise_connectors::test::SetAnalysisConnector(
              prefs, enterprise_connectors::AnalysisConnector::FILE_TRANSFER,
              kAnalysisPolicy);
        }),
#endif
        base::BindRepeating([](PrefService* prefs) {
          enterprise_connectors::test::SetAnalysisConnector(
              prefs, enterprise_connectors::AnalysisConnector::PRINT,
              kAnalysisPolicy);
        }),
        base::BindRepeating([](PrefService* prefs) {
          enterprise_connectors::test::SetOnSecurityEventReporting(prefs, true);
        }),
        base::BindRepeating([](PrefService* prefs) {
          prefs->SetInteger(
              enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
              enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
          prefs->SetInteger(
              enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
              policy::POLICY_SCOPE_MACHINE);
        })));

}  // namespace enterprise_data_protection
