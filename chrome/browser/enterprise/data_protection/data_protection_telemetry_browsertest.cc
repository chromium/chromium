// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "chrome/browser/enterprise/data_protection/data_protection_page_user_data.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service_factory.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/realtime/fake_url_lookup_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {
std::unique_ptr<net::test_server::HttpResponse> HandleDelayedTitleHtml(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/delayed_title.html") {
    return nullptr;
  }

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(
      "<html><head><title>title1.html</title></head><body>Delayed!</body></"
      "html>");
  return response;
}

safe_browsing::RTLookupResponse CreateMatchedRuleResponse() {
  safe_browsing::RTLookupResponse response;
  auto* threat_info = response.add_threat_info();
  threat_info->set_threat_type(
      safe_browsing::RTLookupResponse::ThreatInfo::MANAGED_POLICY);
  threat_info->set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::SAFE);

  safe_browsing::MatchedUrlNavigationRule* rule =
      threat_info->mutable_matched_url_navigation_rule();
  rule->set_rule_id("test_rule_id");
  rule->set_rule_name("test_rule_name");
  rule->set_matched_url_category("test_category");
  rule->mutable_watermark_message()->set_watermark_message("test_watermark");
  return response;
}

class MockRealTimeUrlLookupService
    : public safe_browsing::testing::FakeRealTimeUrlLookupService {
 public:
  bool CanPerformFullURLLookupWithToken() const override { return true; }

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
}  // namespace

class TabTitleReportingBrowserTest : public InProcessBrowserTest {
 public:
  TabTitleReportingBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {enterprise_data_protection::kEnterpriseTabTitleReporting},
        {safe_browsing::kEnhancedFieldsForSecOps});
  }
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    event_report_validator_helper_ = std::make_unique<
        enterprise_connectors::test::EventReportValidatorHelper>(
        browser()->GetProfile(), /*browser_test=*/true);

    safe_browsing::ChromeEnterpriseRealTimeUrlLookupServiceFactory::
        GetInstance()
            ->SetTestingFactoryAndUse(
                browser()->GetProfile(),
                base::BindRepeating(
                    &TabTitleReportingBrowserTest::CreateMockLookupService,
                    base::Unretained(this)));

    browser()->GetProfile()->GetPrefs()->SetInteger(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
        enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);

    browser()->GetProfile()->GetPrefs()->SetInteger(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
        policy::POLICY_SCOPE_MACHINE);

    enterprise_connectors::test::SetOnSecurityEventReporting(
        browser()->GetProfile()->GetPrefs(), /*enabled=*/true,
        /*enabled_event_names=*/
        {enterprise_connectors::kKeyUrlFilteringInterstitialEvent});

    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleDelayedTitleHtml));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    event_report_validator_helper_.reset();
    mock_lookup_service_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<KeyedService> CreateMockLookupService(
      content::BrowserContext* context) {
    auto mock_service = std::make_unique<MockRealTimeUrlLookupService>();
    mock_lookup_service_ = mock_service.get();
    return mock_service;
  }

  void SetupRealtimeServiceMock(
      const GURL& expected_url,
      base::RepeatingClosure on_lookup_started = base::RepeatingClosure(),
      base::RepeatingClosure on_lookup_responded = base::RepeatingClosure(),
      int delay_ms = 0) {
    EXPECT_CALL(*mock_lookup_service_,
                StartMaybeCachedLookup(testing::_, testing::_, testing::_,
                                       testing::_, testing::_, testing::_))
        .WillRepeatedly(
            [expected_url, on_lookup_started, on_lookup_responded, delay_ms](
                const GURL& url,
                safe_browsing::RTLookupResponseCallback rt_lookup_callback,
                scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
                SessionID session_id,
                std::optional<safe_browsing::internal::ReferringAppInfo>
                    referring_app_info,
                bool use_cache) {
              if (url == expected_url) {
                if (on_lookup_started) {
                  on_lookup_started.Run();
                }

                auto deliver_response = base::BindOnce(
                    [](safe_browsing::RTLookupResponseCallback cb,
                       safe_browsing::RTLookupResponse resp,
                       base::RepeatingClosure responded_cb) {
                      std::move(cb).Run(
                          true, false,
                          std::make_unique<safe_browsing::RTLookupResponse>(
                              resp));
                      if (responded_cb) {
                        responded_cb.Run();
                      }
                    },
                    std::move(rt_lookup_callback), CreateMatchedRuleResponse(),
                    on_lookup_responded);

                if (delay_ms > 0) {
                  callback_task_runner->PostDelayedTask(
                      FROM_HERE, std::move(deliver_response),
                      base::Milliseconds(delay_ms));
                } else {
                  callback_task_runner->PostTask(FROM_HERE,
                                                 std::move(deliver_response));
                }
              } else {
                callback_task_runner->PostTask(
                    FROM_HERE,
                    base::BindOnce(
                        std::move(rt_lookup_callback), true, false,
                        std::make_unique<safe_browsing::RTLookupResponse>()));
              }
            });
  }

 protected:
  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent
  CreateExpectedUrlFilteringEvent(const GURL& url,
                                  const std::string& tab_title) {
    auto* reporting_client =
        enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
            browser()->GetProfile());
    return enterprise_connectors::GetUrlFilteringInterstitialEvent(
        url, "", CreateMatchedRuleResponse(),
        reporting_client->GetProfileIdentifier(),
        reporting_client->GetProfileUserName(),
        /*active_user=*/std::string(),
        /*referrer_chain=*/safe_browsing::ReferrerChain(), tab_title);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<enterprise_connectors::test::EventReportValidatorHelper>
      event_report_validator_helper_;
  raw_ptr<MockRealTimeUrlLookupService> mock_lookup_service_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(TabTitleReportingBrowserTest, StandardNavigation) {
  GURL target_url =
      embedded_test_server()->GetURL("example.com", "/delayed_title.html");
  SetupRealtimeServiceMock(target_url);

  GURL safe_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), safe_url));

  base::RunLoop run_loop;

  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());
  event_validator.ExpectUrlFilteringInterstitialEvent(
      CreateExpectedUrlFilteringEvent(target_url, /*tab_title=*/"title1.html"));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), target_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(TabTitleReportingBrowserTest, FallbackFlushNavigation) {
  GURL target_url = embedded_test_server()->GetURL("example.com", "/hung");
  base::RunLoop lookup_responded_loop;
  SetupRealtimeServiceMock(target_url,
                           /*on_lookup_started=*/base::RepeatingClosure(),
                           lookup_responded_loop.QuitClosure(), /*delay_ms=*/0);
  base::RunLoop run_loop;

  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());
  // The navigation never commits, so the event names the pending URL instead
  // of the tab's last committed URL (about:blank), and has no title.
  event_validator.ExpectUrlFilteringInterstitialEvent(
      CreateExpectedUrlFilteringEvent(target_url, /*tab_title=*/""));

  // Add a background tab to test the dynamic closure without terminating the
  // session
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, false);

  // Ensure the original tab (index 0) remains active and gets navigated
  browser()->GetTabStripModel()->ActivateTabAt(0);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), target_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  // Wait for the lookup response to arrive and be stored in rt_lookup_response_
  lookup_responded_loop.Run();

  // Close the active tab while navigation is still in-flight. The verdict that
  // already arrived is reported even though the navigation never finishes.
  ASSERT_TRUE(browser()->GetTabStripModel()->GetWebContentsAt(0));
  browser()->GetTabStripModel()->CloseWebContentsAt(0,
                                                    TabCloseTypes::CLOSE_NONE);

  // Wait for the mock to intercept the telemetry event before teardown
  run_loop.Run();
}

// A 204 response ends the navigation without committing, so the tab keeps
// the previous page. The event must name the URL that matched the rule rather
// than the previous page, have no title, and leave the previous page's data
// protection settings untouched.
IN_PROC_BROWSER_TEST_F(TabTitleReportingBrowserTest, NoContentNavigation) {
  GURL target_url = embedded_test_server()->GetURL("example.com", "/nocontent");
  SetupRealtimeServiceMock(target_url);

  GURL safe_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), safe_url));

  base::RunLoop run_loop;

  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());
  event_validator.ExpectUrlFilteringInterstitialEvent(
      CreateExpectedUrlFilteringEvent(target_url, /*tab_title=*/""));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), target_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  run_loop.Run();

  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  EXPECT_EQ(web_contents->GetLastCommittedURL(), safe_url);
  auto* user_data =
      enterprise_data_protection::DataProtectionPageUserData::GetForPage(
          web_contents->GetPrimaryMainFrame()->GetPage());
  ASSERT_TRUE(user_data);
  EXPECT_TRUE(user_data->settings().watermark_text.empty());
}

IN_PROC_BROWSER_TEST_F(TabTitleReportingBrowserTest,
                       PrivacyIncognitoNavigation) {
  // Incognito navigations must not fall back to the regular profile's lookup
  // service.
  EXPECT_CALL(*mock_lookup_service_, StartMaybeCachedLookup).Times(0);

  auto* incognito_browser = CreateIncognitoBrowser();

  // Incognito profiles have no enterprise lookup service of their own.
  ASSERT_FALSE(safe_browsing::ChromeEnterpriseRealTimeUrlLookupServiceFactory::
                   GetForProfile(incognito_browser->GetProfile()));

  incognito_browser->GetProfile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);

  incognito_browser->GetProfile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);

  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.ExpectNoReport();

  GURL target_url =
      embedded_test_server()->GetURL("example.com", "/delayed_title.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, target_url));
}
