// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "chrome/browser/federated_learning/floc_id_provider.h"
#include "chrome/browser/federated_learning/floc_id_provider_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_host_resolver.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"

namespace {

class FixedFlocIdProvider : public federated_learning::FlocIdProvider {
 public:
  FixedFlocIdProvider() = default;
  ~FixedFlocIdProvider() override = default;

  std::string GetInterestCohortForJsApi(
      const GURL& url,
      const base::Optional<url::Origin>& top_frame_origin) const override {
    return "12345.6.7.8.9";
  }
};

}  // namespace

class FlocEligibilityBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "InterestCohortAPI");
  }

  // BrowserTestBase::SetUpInProcessBrowserTestFixture
  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &FlocEligibilityBrowserTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  std::string InvokeInterestCohortJsApi(
      const content::ToRenderFrameHost& adapter) {
    return EvalJs(adapter, R"(
      document.interestCohort()
      .then(floc => floc)
      .catch(error => 'rejected');
    )")
        .ExtractString();
  }

  bool HistoryContainsUrlVisit(const GURL& url) {
    return QueryUrl(url).success;
  }

  bool IsUrlVisitEligibleToComputeFloc(const GURL& url) {
    history::QueryURLResult result = QueryUrl(url);
    EXPECT_EQ(1u, result.visits.size());
    return result.visits[0].floc_allowed;
  }

  history::QueryURLResult QueryUrl(const GURL& url) {
    history::QueryURLResult query_url_result;

    base::RunLoop run_loop;
    base::CancelableTaskTracker tracker;
    history_service()->QueryURL(
        url, /*want_visits=*/true,
        base::BindLambdaForTesting([&](history::QueryURLResult result) {
          query_url_result = std::move(result);
          run_loop.Quit();
        }),
        &tracker);
    run_loop.Run();

    return query_url_result;
  }

  void DeleteAllHistory() {
    base::RunLoop run_loop;
    base::CancelableTaskTracker tracker;
    HistoryServiceFactory::GetForProfile(browser()->profile(),
                                         ServiceAccessType::EXPLICIT_ACCESS)
        ->ExpireHistoryBetween(
            /*restrict_urls=*/{}, /*begin_time=*/base::Time(),
            base::Time::Max(),
            /*user_initiated=*/true,
            base::BindLambdaForTesting([&]() { run_loop.Quit(); }), &tracker);
    run_loop.Run();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  history::HistoryService* history_service() {
    return HistoryServiceFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    federated_learning::FlocIdProviderFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(
                     &FlocEligibilityBrowserTest::CreateFixedFlocIdProvider,
                     base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreateFixedFlocIdProvider(
      content::BrowserContext* context) {
    return std::make_unique<FixedFlocIdProvider>();
  }

  GURL NavigateToTestPage() {
    auto waiter =
        std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
            web_contents());

    GURL main_page_url(embedded_test_server()->GetURL(
        "a.test", "/ad_tagging/frame_factory.html"));

    ui_test_utils::NavigateToURL(browser(), main_page_url);

    // Four resources in the main frame and one favicon.
    waiter->AddMinimumCompleteResourcesExpectation(5);
    waiter->Wait();

    return main_page_url;
  }

 protected:
  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest, NotEligibleByDefault) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  GURL main_page_url = NavigateToTestPage();

  ASSERT_TRUE(HistoryContainsUrlVisit(main_page_url));

  // Expect that the navigation history is not eligible for floc computation.
  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(main_page_url));
}

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest, EligibleAfterAdResource) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("ad_script.js")});

  GURL main_page_url = NavigateToTestPage();

  // Expect that the navigation history is eligible for floc computation as the
  // page contains an ad resource.
  EXPECT_TRUE(IsUrlVisitEligibleToComputeFloc(main_page_url));
}

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest, EligibleAfterApiCall) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  GURL main_page_url = NavigateToTestPage();

  ASSERT_TRUE(HistoryContainsUrlVisit(main_page_url));
  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(main_page_url));

  // Expect that the navigation history is eligible for floc computation after
  // an API call.
  EXPECT_EQ("12345.6.7.8.9", InvokeInterestCohortJsApi(web_contents()));
  EXPECT_TRUE(IsUrlVisitEligibleToComputeFloc(main_page_url));
}

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest, NotEligibleDueToPrivateIP) {
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("ad_script.js")});

  GURL main_page_url = NavigateToTestPage();

  // Expect that the navigation history is not eligible for floc computation as
  // the IP was not publicly routable.
  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(main_page_url));
}

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest, NotEligibleSubframeHistory) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  GURL main_page_url(embedded_test_server()->GetURL("a.test", "/iframe.html"));
  GURL auto_subframe_url(
      embedded_test_server()->GetURL("a.test", "/title1.html"));

  // Navigate to a page that contains an iframe ("title1.html").
  ui_test_utils::NavigateToURL(browser(), main_page_url);

  ASSERT_TRUE(HistoryContainsUrlVisit(main_page_url));
  ASSERT_FALSE(HistoryContainsUrlVisit(auto_subframe_url));

  // Trigger an user-initiated navigation on the iframe, so that it will show up
  // in history.
  GURL manual_subframe_url(
      embedded_test_server()->GetURL("a.test", "/title2.html"));
  content::NavigateIframeToURL(web_contents(),
                               /*iframe_id=*/"test", manual_subframe_url);
  ASSERT_TRUE(HistoryContainsUrlVisit(manual_subframe_url));

  EXPECT_EQ("12345.6.7.8.9", InvokeInterestCohortJsApi(web_contents()));
  EXPECT_EQ("12345.6.7.8.9", InvokeInterestCohortJsApi(content::ChildFrameAt(
                                 web_contents()->GetMainFrame(), 0)));

  // Expect that only the main frame navigation history is eligible for floc
  // computation.
  EXPECT_TRUE(IsUrlVisitEligibleToComputeFloc(main_page_url));
  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(manual_subframe_url));
}

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest,
                       SettingFlocAllowedNoopOnDeletedHistory) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  GURL main_page_url = NavigateToTestPage();

  ASSERT_TRUE(HistoryContainsUrlVisit(main_page_url));

  DeleteAllHistory();

  // Expect that attempting to set the "floc allowed" bit will be a no-op if the
  // page visit doesn't exist.
  EXPECT_EQ("12345.6.7.8.9", InvokeInterestCohortJsApi(web_contents()));
  ASSERT_FALSE(HistoryContainsUrlVisit(main_page_url));
}
