// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/view_source_navigation_throttle.h"

#include "base/path_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/realtime/fake_url_lookup_service.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;

namespace enterprise_data_protection {

class MockRealTimeUrlLookupService
    : public safe_browsing::testing::FakeRealTimeUrlLookupService {
 public:
  MOCK_METHOD(void,
              StartMaybeCachedLookup,
              (const GURL& url,
               safe_browsing::RTLookupResponseCallback rt_lookup_callback,
               scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
               SessionID session_id,
               std::optional<safe_browsing::internal::ReferringAppInfo>
                   referring_app_info,
               bool use_cache),
              (override));
};

class ViewSourceNavigationThrottleBrowserTest : public InProcessBrowserTest {
 public:
  ViewSourceNavigationThrottleBrowserTest() = default;
  ~ViewSourceNavigationThrottleBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    safe_browsing::ChromeEnterpriseRealTimeUrlLookupServiceFactory::
        GetInstance()
            ->SetTestingFactoryAndUse(
                browser()->profile(),
                base::BindRepeating(&ViewSourceNavigationThrottleBrowserTest::
                                        CreateMockLookupService,
                                    base::Unretained(this)));

    // Set a DM Token to enable enterprise features.
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("test_dm_token"));

    // Enable real-time URL checks.
    browser()->profile()->GetPrefs()->SetInteger(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
        enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
    browser()->profile()->GetPrefs()->SetInteger(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
        policy::POLICY_SCOPE_MACHINE);

    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());

    SetupRealtimeServiceMock();
  }

  void TearDownOnMainThread() override {
    mock_lookup_service_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<KeyedService> CreateMockLookupService(
      content::BrowserContext* context) {
    auto mock_service = std::make_unique<MockRealTimeUrlLookupService>();
    mock_lookup_service_ = mock_service.get();
    return mock_service;
  }

 protected:
  bool IsInterstitialBeingShown() {
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    security_interstitials::SecurityInterstitialTabHelper* tab_helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            contents);
    return tab_helper && tab_helper->IsDisplayingInterstitial();
  }

  void SetupRealtimeServiceMock() {
    EXPECT_CALL(*mock_lookup_service_,
                StartMaybeCachedLookup(_, _, _, _, _, false))
        .WillRepeatedly(
            [&](const GURL& url,
                safe_browsing::RTLookupResponseCallback rt_lookup_callback,
                scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
                SessionID session_id,
                std::optional<safe_browsing::internal::ReferringAppInfo>
                    referring_app_info,
                bool use_cache) {
              // Post the callback to the provided task runner.
              callback_task_runner->PostTask(
                  FROM_HERE,
                  base::BindOnce(
                      std::move(rt_lookup_callback),
                      /*is_rt_lookup_successful=*/true,
                      /*is_cached_response=*/false,
                      std::make_unique<safe_browsing::RTLookupResponse>(
                          api_responses_[url])));
            });
  }

  base::flat_map<GURL, safe_browsing::RTLookupResponse> api_responses_;
  raw_ptr<MockRealTimeUrlLookupService> mock_lookup_service_;  // Not owned
};

IN_PROC_BROWSER_TEST_F(ViewSourceNavigationThrottleBrowserTest, ShouldBlock) {
  GURL inner_url(embedded_test_server()->GetURL("/simple.html"));
  GURL view_source_url("view-source:" + inner_url.spec());

  {
    safe_browsing::RTLookupResponse response;
    auto* threat_info = response.add_threat_info();
    // This combination should map to SB_THREAT_TYPE_MANAGED_POLICY_BLOCK
    threat_info->set_verdict_type(
        safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS);
    threat_info->set_threat_type(
        safe_browsing::RTLookupResponse::ThreatInfo::MANAGED_POLICY);
    api_responses_[view_source_url] = response;
  }
  {
    safe_browsing::RTLookupResponse response;
    auto* threat_info = response.add_threat_info();
    // This combination should map to SB_THREAT_TYPE_SAFE
    threat_info->set_verdict_type(
        safe_browsing::RTLookupResponse::ThreatInfo::SAFE);
    threat_info->set_threat_type(
        safe_browsing::RTLookupResponse::ThreatInfo::ThreatType_MIN);
    api_responses_[inner_url] = response;
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), view_source_url));
  EXPECT_TRUE(IsInterstitialBeingShown());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), inner_url));
  EXPECT_FALSE(IsInterstitialBeingShown());
}

IN_PROC_BROWSER_TEST_F(ViewSourceNavigationThrottleBrowserTest,
                       ShouldBlock_Redirect) {
  GURL redirect_url(embedded_test_server()->GetURL("/simple.html"));
  GURL initial_url(embedded_test_server()->GetURL("/server-redirect?" +
                                                  redirect_url.spec()));
  GURL view_source_url("view-source:" + initial_url.spec());
  GURL banned_view_source_url("view-source:" + redirect_url.spec());

  {
    safe_browsing::RTLookupResponse response;
    auto* threat_info = response.add_threat_info();
    // This combination should map to SB_THREAT_TYPE_MANAGED_POLICY_BLOCK
    threat_info->set_verdict_type(
        safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS);
    threat_info->set_threat_type(
        safe_browsing::RTLookupResponse::ThreatInfo::MANAGED_POLICY);
    api_responses_[banned_view_source_url] = response;
  }
  {
    safe_browsing::RTLookupResponse response;
    auto* threat_info = response.add_threat_info();
    // This combination should map to SB_THREAT_TYPE_SAFE
    threat_info->set_verdict_type(
        safe_browsing::RTLookupResponse::ThreatInfo::SAFE);
    threat_info->set_threat_type(
        safe_browsing::RTLookupResponse::ThreatInfo::ThreatType_MIN);
    api_responses_[view_source_url] = response;
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), view_source_url));
  EXPECT_TRUE(IsInterstitialBeingShown());
}

}  // namespace enterprise_data_protection
