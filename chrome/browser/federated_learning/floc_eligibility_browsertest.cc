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
#include "components/federated_learning/features/features.h"
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
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/federated_learning/floc.mojom.h"

namespace {

class FixedFlocIdProvider : public federated_learning::FlocIdProvider {
 public:
  FixedFlocIdProvider() = default;
  ~FixedFlocIdProvider() override = default;

  blink::mojom::InterestCohortPtr GetInterestCohortForJsApi(
      const GURL& url,
      const base::Optional<url::Origin>& top_frame_origin) const override {
    blink::mojom::InterestCohortPtr cohort =
        blink::mojom::InterestCohort::New();
    cohort->id = "12345";
    cohort->version = "chrome.6.7.8.9";
    return cohort;
  }

  void MaybeRecordFlocToUkm(ukm::SourceId source_id) override {}
};

}  // namespace

// Tests behaviors that affect whether the floc API is allowed and/or whether
// the navigation's associated history entry is eligible for floc computation.
class FlocEligibilityBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  FlocEligibilityBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kInterestCohortFeaturePolicy},
        /*disabled_features=*/{
            federated_learning::
                kFlocPagesWithAdResourcesDefaultIncludedInFlocComputation});
  }

  void SetUpOnMainThread() override {
    subresource_filter::SubresourceFilterBrowserTest::SetUpOnMainThread();

    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    content::SetupCrossSiteRedirector(&https_server_);
    ASSERT_TRUE(https_server_.Start());
  }

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
      if (!(document.interestCohort instanceof Function)) {
        'not a function';
      } else {
        document.interestCohort()
        .then(floc => JSON.stringify(floc, Object.keys(floc).sort()))
        .catch(error => 'rejected');
      }
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

  void NavigateAndWaitForResourcesCompeletion(const GURL& url,
                                              int expected_complete_resources) {
    auto waiter =
        std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
            web_contents());

    ui_test_utils::NavigateToURL(browser(), url);

    waiter->AddMinimumCompleteResourcesExpectation(expected_complete_resources);
    waiter->Wait();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest,
                       NotEligibleForHistoryByDefault) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  GURL main_page_url = https_server_.GetURL(
      "a.test", "/federated_learning/page_with_script_and_iframe.html");

  // Three resources in the main frame and one favicon.
  NavigateAndWaitForResourcesCompeletion(main_page_url, 4);

  ASSERT_TRUE(HistoryContainsUrlVisit(main_page_url));

  // Expect that the navigation history is not eligible for floc computation.
  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(main_page_url));
}

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest,
                       NotEligibleForHistoryAfterAdResource) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("maybe_ad_script.js")});

  GURL main_page_url = https_server_.GetURL(
      "a.test", "/federated_learning/page_with_script_and_iframe.html");

  // Three resources in the main frame and one favicon.
  NavigateAndWaitForResourcesCompeletion(main_page_url, 4);

  // Expect that the navigation history is not eligible for floc computation.
  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(main_page_url));
}

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest,
                       EligibleForHistoryAfterApiCall) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  GURL main_page_url = https_server_.GetURL(
      "a.test", "/federated_learning/page_with_script_and_iframe.html");

  // Three resources in the main frame and one favicon.
  NavigateAndWaitForResourcesCompeletion(main_page_url, 4);

  // Expect that the navigation history is eligible for floc computation after
  // an API call.
  EXPECT_EQ("{\"id\":\"12345\",\"version\":\"chrome.6.7.8.9\"}",
            InvokeInterestCohortJsApi(web_contents()));
  EXPECT_TRUE(IsUrlVisitEligibleToComputeFloc(main_page_url));
}

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest,
                       NotEligibleForHistoryDueToFeaturePolicy) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  GURL main_page_url = https_server_.GetURL(
      "a.test", "/federated_learning/feature_policy_interest_cohort_none.html");

  // Three resources in the main frame and one favicon.
  NavigateAndWaitForResourcesCompeletion(main_page_url, 4);

  InvokeInterestCohortJsApi(web_contents());

  // Expect that the navigation history is not eligible for floc computation as
  // the feature policy disallows it.
  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(main_page_url));
}

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest,
                       NotEligibleForHistoryDueToPrivateIP) {
  GURL main_page_url = https_server_.GetURL(
      "a.test", "/federated_learning/page_with_script_and_iframe.html");

  // Three resources in the main frame and one favicon.
  NavigateAndWaitForResourcesCompeletion(main_page_url, 4);

  InvokeInterestCohortJsApi(web_contents());

  // Expect that the navigation history is not eligible for floc computation as
  // the IP was not publicly routable.
  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(main_page_url));
}

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest,
                       NotEligibleForHistorySubframeCommit) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  GURL main_page_url(https_server_.GetURL(
      "a.test", "/federated_learning/page_with_script_and_iframe.html"));
  GURL auto_subframe_url(https_server_.GetURL("a.test", "/title1.html"));

  // Navigate to a page that contains an iframe ("title1.html").
  // Three resources in the main frame and one favicon.
  NavigateAndWaitForResourcesCompeletion(main_page_url, 4);

  ASSERT_TRUE(HistoryContainsUrlVisit(main_page_url));
  ASSERT_FALSE(HistoryContainsUrlVisit(auto_subframe_url));

  // Trigger an user-initiated navigation on the iframe, so that it will show up
  // in history.
  GURL manual_subframe_url(https_server_.GetURL("a.test", "/title2.html"));
  content::NavigateIframeToURL(web_contents(),
                               /*iframe_id=*/"test", manual_subframe_url);
  ASSERT_TRUE(HistoryContainsUrlVisit(manual_subframe_url));

  EXPECT_EQ("{\"id\":\"12345\",\"version\":\"chrome.6.7.8.9\"}",
            InvokeInterestCohortJsApi(web_contents()));
  EXPECT_EQ("{\"id\":\"12345\",\"version\":\"chrome.6.7.8.9\"}",
            InvokeInterestCohortJsApi(
                content::ChildFrameAt(web_contents()->GetMainFrame(), 0)));

  // Expect that only the main frame navigation history is eligible for floc
  // computation.
  EXPECT_TRUE(IsUrlVisitEligibleToComputeFloc(main_page_url));
  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(manual_subframe_url));
}

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest,
                       SettingFlocAllowedNoopOnDeletedHistory) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  GURL main_page_url(https_server_.GetURL(
      "a.test", "/federated_learning/page_with_script_and_iframe.html"));

  // Three resources in the main frame and one favicon.
  NavigateAndWaitForResourcesCompeletion(main_page_url, 4);

  ASSERT_TRUE(HistoryContainsUrlVisit(main_page_url));

  DeleteAllHistory();

  // Expect that attempting to set the "floc allowed" bit will be a no-op if the
  // page visit doesn't exist.
  EXPECT_EQ("{\"id\":\"12345\",\"version\":\"chrome.6.7.8.9\"}",
            InvokeInterestCohortJsApi(web_contents()));
  ASSERT_FALSE(HistoryContainsUrlVisit(main_page_url));
}

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest, ApiAllowedByDefault) {
  GURL main_page_url(https_server_.GetURL(
      "a.test", "/federated_learning/page_with_script_and_iframe.html"));

  // Three resources in the main frame and one favicon.
  NavigateAndWaitForResourcesCompeletion(main_page_url, 4);

  // Navigate the iframe to a cross-origin site.
  GURL subframe_url(https_server_.GetURL("b.test", "/title1.html"));
  content::NavigateIframeToURL(web_contents(),
                               /*iframe_id=*/"test", subframe_url);

  content::RenderFrameHost* child =
      content::ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Expect that both main frame and subframe are allowed to access floc.
  EXPECT_EQ("{\"id\":\"12345\",\"version\":\"chrome.6.7.8.9\"}",
            InvokeInterestCohortJsApi(web_contents()));
  EXPECT_EQ("{\"id\":\"12345\",\"version\":\"chrome.6.7.8.9\"}",
            InvokeInterestCohortJsApi(child));
}

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest,
                       ApiNotAllowedDueToInsecureContext) {
  GURL main_page_url(embedded_test_server()->GetURL(
      "a.test", "/federated_learning/page_with_script_and_iframe.html"));

  // Three resources in the main frame and one favicon.
  NavigateAndWaitForResourcesCompeletion(main_page_url, 4);

  // Navigate the iframe to a https site.
  GURL subframe_url(https_server_.GetURL("b.test", "/title1.html"));
  content::NavigateIframeToURL(web_contents(),
                               /*iframe_id=*/"test", subframe_url);

  content::RenderFrameHost* child =
      content::ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Expect that both main frame and subframe are not allowed to access floc.
  EXPECT_EQ("not a function", InvokeInterestCohortJsApi(web_contents()));
  EXPECT_EQ("not a function", InvokeInterestCohortJsApi(child));
}

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest,
                       ApiNotAllowedDueToFeaturePolicy) {
  GURL main_page_url(https_server_.GetURL(
      "a.test",
      "/federated_learning/feature_policy_interest_cohort_none.html"));

  // Three resources in the main frame and one favicon.
  NavigateAndWaitForResourcesCompeletion(main_page_url, 4);

  // Navigate the iframe to a cross-origin site.
  GURL subframe_url(https_server_.GetURL("b.test", "/title1.html"));
  content::NavigateIframeToURL(web_contents(),
                               /*iframe_id=*/"test", subframe_url);

  content::RenderFrameHost* child =
      content::ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Expect that both main frame and subframe are not allowed to access floc.
  EXPECT_EQ("rejected", InvokeInterestCohortJsApi(web_contents()));
  EXPECT_EQ("rejected", InvokeInterestCohortJsApi(child));
}

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest,
                       ApiNotAllowedInSubframeDueToFeaturePolicySelf) {
  GURL main_page_url(https_server_.GetURL(
      "a.test",
      "/federated_learning/feature_policy_interest_cohort_self.html"));

  // Three resources in the main frame and one favicon.
  NavigateAndWaitForResourcesCompeletion(main_page_url, 4);

  // Navigate the iframe to a cross-origin site.
  GURL subframe_url(https_server_.GetURL("b.test", "/title1.html"));
  content::NavigateIframeToURL(web_contents(),
                               /*iframe_id=*/"test", subframe_url);

  content::RenderFrameHost* child =
      content::ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Expect that only the main frame can access floc.
  EXPECT_EQ("{\"id\":\"12345\",\"version\":\"chrome.6.7.8.9\"}",
            InvokeInterestCohortJsApi(web_contents()));
  EXPECT_EQ("rejected", InvokeInterestCohortJsApi(child));
}

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTest,
                       ApiNotAllowedInDetachedDocument) {
  ui_test_utils::NavigateToURL(
      browser(),
      https_server_.GetURL(
          "a.test",
          "/federated_learning/interest_cohort_api_in_detached_document.html"));

  EXPECT_EQ(
      "[error from subframe document] InvalidAccessError: Failed to execute "
      "'interestCohort' on 'Document': A browsing context is required when "
      "calling document.interestCohort.",
      EvalJs(web_contents()->GetMainFrame(), R"(
        document.body.textContent
      )")
          .ExtractString());
}

class FlocEligibilityBrowserTestPagesWithAdResourcesDefaultIncluded
    : public FlocEligibilityBrowserTest {
 public:
  FlocEligibilityBrowserTestPagesWithAdResourcesDefaultIncluded() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kInterestCohortFeaturePolicy,
         federated_learning::
             kFlocPagesWithAdResourcesDefaultIncludedInFlocComputation},
        {});
  }
};

IN_PROC_BROWSER_TEST_F(
    FlocEligibilityBrowserTestPagesWithAdResourcesDefaultIncluded,
    EligibleForHistoryAfterAdResource) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("maybe_ad_script.js")});

  GURL main_page_url = https_server_.GetURL(
      "a.test", "/federated_learning/page_with_script_and_iframe.html");

  // Three resources in the main frame and one favicon.
  NavigateAndWaitForResourcesCompeletion(main_page_url, 4);

  // Expect that the navigation history is eligible for floc computation as the
  // page contains an ad resource.
  EXPECT_TRUE(IsUrlVisitEligibleToComputeFloc(main_page_url));
}

IN_PROC_BROWSER_TEST_F(
    FlocEligibilityBrowserTestPagesWithAdResourcesDefaultIncluded,
    NotEligibleForHistoryAfterNonAdResource) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  GURL main_page_url = https_server_.GetURL(
      "a.test", "/federated_learning/page_with_script_and_iframe.html");

  // Three resources in the main frame and one favicon.
  NavigateAndWaitForResourcesCompeletion(main_page_url, 4);

  // Expect that the navigation history is not eligible for floc computation.
  EXPECT_FALSE(IsUrlVisitEligibleToComputeFloc(main_page_url));
}

class FlocEligibilityBrowserTestChromeFeaturePolicyDisabled
    : public FlocEligibilityBrowserTest {
 public:
  FlocEligibilityBrowserTestChromeFeaturePolicyDisabled() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kInterestCohortFeaturePolicy);
  }
};

IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTestChromeFeaturePolicyDisabled,
                       FeaturePolicyFeatureNotAvailable) {
  GURL main_page_url(https_server_.GetURL("a.test", "/title1.html"));
  ui_test_utils::NavigateToURL(browser(), main_page_url);

  EXPECT_FALSE(EvalJs(web_contents(), R"(
      document.featurePolicy.features().includes("interest-cohort")
    )")
                   .ExtractBool());
}

// Try configuring the feature policy anyway. Check that the API succeeds and
// the history is eligible for floc computation.
IN_PROC_BROWSER_TEST_F(FlocEligibilityBrowserTestChromeFeaturePolicyDisabled,
                       FeaturePolicyFeatureNotEffective) {
  net::IPAddress::ConsiderLoopbackIPToBePubliclyRoutableForTesting();

  GURL main_page_url(https_server_.GetURL(
      "a.test",
      "/federated_learning/feature_policy_interest_cohort_none.html"));

  // Three resources in the main frame and one favicon.
  NavigateAndWaitForResourcesCompeletion(main_page_url, 4);

  // Navigate the iframe to a cross-origin site.
  GURL subframe_url(https_server_.GetURL("b.test", "/title1.html"));
  content::NavigateIframeToURL(web_contents(),
                               /*iframe_id=*/"test", subframe_url);

  content::RenderFrameHost* child =
      content::ChildFrameAt(web_contents()->GetMainFrame(), 0);

  // Expect that both main frame and subframe are allowed to access floc.
  EXPECT_EQ("{\"id\":\"12345\",\"version\":\"chrome.6.7.8.9\"}",
            InvokeInterestCohortJsApi(web_contents()));
  EXPECT_EQ("{\"id\":\"12345\",\"version\":\"chrome.6.7.8.9\"}",
            InvokeInterestCohortJsApi(child));

  // Expect that the navigation history is eligible for floc computation.
  EXPECT_TRUE(IsUrlVisitEligibleToComputeFloc(main_page_url));
}
