// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/page_load_metrics/chrome_initiator_location.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"

namespace {

std::unique_ptr<net::test_server::HttpResponse> SRPHandler(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content("<html><body></body></html>");
  return response;
}

int64_t MetricValue(
    page_load_metrics::NavigationHandleUserData::InitiatorLocation location) {
  return static_cast<int64_t>(location);
}

void AttachInitiatorLocation(
    page_load_metrics::NavigationHandleUserData::InitiatorLocation location,
    content::NavigationHandle& navigation_handle) {
  page_load_metrics::NavigationHandleUserData::CreateForNavigationHandle(
      navigation_handle, location,
      StringifyChromeInitiatorLocation(GetChromeInitiatorLocation(location)));
}

}  // namespace

class NavigationInitiatorPageLoadMetricsBrowserTest
    : public InProcessBrowserTest {
 public:
  NavigationInitiatorPageLoadMetricsBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&NavigationInitiatorPageLoadMetricsBrowserTest::
                                    GetActiveWebContents,
                                base::Unretained(this))) {}

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/
        // TODO(crbug.com/452061489): Fix tests that fail when the WebUI Omnibox
        // is enabled and then remove these two Features.
        {omnibox::internal::kWebUIOmniboxPopup,
         omnibox::internal::kWebUIOmniboxAimPopup});

    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/search",
                            base::BindRepeating(SRPHandler)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(NavigationInitiatorPageLoadMetricsBrowserTest,
                       NewTabPageTrigger) {
  base::HistogramTester histogram_tester;

  base::RepeatingCallback<void(content::NavigationHandle&)>
      navigation_handle_callback = base::BindRepeating(
          &AttachInitiatorLocation,
          GetInitiatorLocation(ChromeInitiatorLocation::kNewTabPage));

  GURL url = embedded_test_server()->GetURL("/empty.html");
  GetActiveWebContents()->OpenURL(
      content::OpenURLParams(url, content::Referrer(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_TYPED, false),
      std::move(navigation_handle_callback));

  // Wait for the navigation to finish.
  content::WaitForLoadStop(GetActiveWebContents());

  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.All",
      MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kNewTabPage)),
      1);
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.SRP",
      MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kNewTabPage)),
      0);
}

IN_PROC_BROWSER_TEST_F(NavigationInitiatorPageLoadMetricsBrowserTest, Basic) {
  base::HistogramTester histogram_tester;

  GURL url = embedded_test_server()->GetURL("www.example.com", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.All",
      MetricValue(
          page_load_metrics::NavigationHandleUserData::kInitiatorLocationOther),
      1);
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.SRP",
      MetricValue(
          page_load_metrics::NavigationHandleUserData::kInitiatorLocationOther),
      0);
}

IN_PROC_BROWSER_TEST_F(NavigationInitiatorPageLoadMetricsBrowserTest,
                       SearchResultsPage) {
  base::HistogramTester histogram_tester;

  GURL url = embedded_test_server()->GetURL("www.google.com", "/search?q=test");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.All",
      MetricValue(
          page_load_metrics::NavigationHandleUserData::kInitiatorLocationOther),
      1);
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.SRP",
      MetricValue(
          page_load_metrics::NavigationHandleUserData::kInitiatorLocationOther),
      1);
}

IN_PROC_BROWSER_TEST_F(NavigationInitiatorPageLoadMetricsBrowserTest,
                       NonSearchResultsPage) {
  base::HistogramTester histogram_tester;

  GURL url = embedded_test_server()->GetURL("www.google.com", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.All",
      MetricValue(
          page_load_metrics::NavigationHandleUserData::kInitiatorLocationOther),
      1);
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.SRP",
      MetricValue(
          page_load_metrics::NavigationHandleUserData::kInitiatorLocationOther),
      0);
}

IN_PROC_BROWSER_TEST_F(NavigationInitiatorPageLoadMetricsBrowserTest,
                       LinkClick) {
  base::HistogramTester histogram_tester;

  GURL url = embedded_test_server()->GetURL("www.example.com", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Simulate a link click from the renderer.
  GURL link_url =
      embedded_test_server()->GetURL("www.example.com", "/simple.html");
  content::TestNavigationManager navigation_manager(GetActiveWebContents(),
                                                    link_url);
  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      content::JsReplace(R"(let a = document.createElement('a');
                                            a.href = $1;
                                            document.body.appendChild(a);
                                            a.click();)",
                                         link_url.spec())));
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.All",
      MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kLinkClick)),
      1);
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.SRP",
      MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kLinkClick)),
      0);
}

IN_PROC_BROWSER_TEST_F(NavigationInitiatorPageLoadMetricsBrowserTest,
                       LinkClickSRP) {
  base::HistogramTester histogram_tester;

  GURL url = embedded_test_server()->GetURL("www.example.com", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Simulate a link click from the renderer to SRP.
  GURL link_url =
      embedded_test_server()->GetURL("www.google.com", "/search?q=test");
  content::TestNavigationManager navigation_manager(GetActiveWebContents(),
                                                    link_url);
  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      content::JsReplace(R"(let a = document.createElement('a');
                                            a.href = $1;
                                            document.body.appendChild(a);
                                            a.click();)",
                                         link_url.spec())));
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.All",
      MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kLinkClick)),
      1);
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.SRP",
      MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kLinkClick)),
      1);
}

IN_PROC_BROWSER_TEST_F(NavigationInitiatorPageLoadMetricsBrowserTest,
                       LinkClick_NoUserGesture) {
  base::HistogramTester histogram_tester;

  GURL url = embedded_test_server()->GetURL("www.example.com", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Simulate a link click from the renderer without a user gesture.
  GURL link_url =
      embedded_test_server()->GetURL("www.example.com", "/simple.html");
  content::TestNavigationManager navigation_manager(GetActiveWebContents(),
                                                    link_url);
  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      content::JsReplace("location = $1;", link_url.spec()),
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.All",
      MetricValue(
          page_load_metrics::NavigationHandleUserData::kInitiatorLocationOther),
      2);
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.SRP",
      MetricValue(
          page_load_metrics::NavigationHandleUserData::kInitiatorLocationOther),
      0);
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.All",
      MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kLinkClick)),
      0);
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.SRP",
      MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kLinkClick)),
      0);
}

IN_PROC_BROWSER_TEST_F(NavigationInitiatorPageLoadMetricsBrowserTest,
                       PrerenderActivation) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  // Metrics are collected for the initial navigation.
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.All", 1);

  GURL prerender_url = embedded_test_server()->GetURL("/simple.html");

  // Start Omnibox triggered prerendering.
  auto* preloading_data = content::PreloadingData::GetOrCreateForWebContents(
      GetActiveWebContents());
  content::PreloadingURLMatchCallback same_url_matcher =
      content::PreloadingData::GetSameURLMatcher(prerender_url);
  content::PreloadingAttempt* preloading_attempt =
      preloading_data->AddPreloadingAttempt(
          chrome_preloading_predictor::kOmniboxDirectURLInput,
          content::PreloadingType::kPrerender, same_url_matcher,
          GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId());
  PrerenderManager::CreateForWebContents(GetActiveWebContents());
  base::WeakPtr<content::PrerenderHandle> prerender_handle =
      PrerenderManager::FromWebContents(GetActiveWebContents())
          ->StartPrerenderDirectUrlInput(prerender_url, *preloading_attempt);
  EXPECT_TRUE(prerender_handle);
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);

  // Before activation, no metrics should be recorded for the prerendered page.
  // We should only see the initial navigation.
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.All", 1);
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.SRP", 0);

  // Activate.
  content::TestActivationManager activation_manager(GetActiveWebContents(),
                                                    prerender_url);
  // Simulate an Omnibox triggered navigation.
  prerender_helper().NavigatePrimaryPageAsync(
      prerender_url,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  // After activation, the metric should be recorded. We expect 2 total
  // navigations.
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.All", 2);
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.SRP", 0);
}

IN_PROC_BROWSER_TEST_F(NavigationInitiatorPageLoadMetricsBrowserTest,
                       PrerenderSRPActivation) {
  auto* model =
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  TemplateURLData data;
  data.SetShortName(u"test");
  data.SetKeyword(u"test");
  data.SetURL(embedded_test_server()
                  ->GetURL("www.google.com", "/search?q={searchTerms}")
                  .spec());
  TemplateURL* t_url = model->Add(std::make_unique<TemplateURL>(data));
  model->SetUserSelectedDefaultSearchProvider(t_url);

  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("www.google.com", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  // Metrics are collected for the initial navigation.
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.All", 1);
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.SRP", 0);

  GURL prerender_url =
      embedded_test_server()->GetURL("www.google.com", "/search?q=test");

  // Start Omnibox triggered prerendering.
  auto* preloading_data = content::PreloadingData::GetOrCreateForWebContents(
      GetActiveWebContents());
  content::PreloadingURLMatchCallback same_url_matcher =
      content::PreloadingData::GetSameURLMatcher(prerender_url);
  content::PreloadingAttempt* preloading_attempt =
      preloading_data->AddPreloadingAttempt(
          chrome_preloading_predictor::kDefaultSearchEngine,
          content::PreloadingType::kPrerender, same_url_matcher,
          GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId());
  PrerenderManager::CreateForWebContents(GetActiveWebContents());
  PrerenderManager::FromWebContents(GetActiveWebContents())
      ->StartPrerenderSearchResult(prerender_url, prerender_url,
                                   preloading_attempt->GetWeakPtr());
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);

  // Before activation, no metrics should be recorded for the prerendered page.
  // We should only see the initial navigation.
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.All", 1);
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.SRP", 0);

  // Activate.
  content::TestActivationManager activation_manager(GetActiveWebContents(),
                                                    prerender_url);
  // Simulate an Omnibox triggered navigation.
  prerender_helper().NavigatePrimaryPageAsync(
      prerender_url,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  // After activation, the metric should be recorded. We expect 2 total
  // navigations.
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.All", 2);
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.SRP", 1);
}
