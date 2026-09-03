// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/page_load_metrics/chrome_initiator_location.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/google/core/common/google_switches.h"
#include "components/lens/lens_features.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/page_load_metrics/google/browser/google_url_util.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

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
                                base::Unretained(this))) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/
        // TODO(crbug.com/452061489): Fix tests that fail when the WebUI Omnibox
        // is enabled and then remove these two Features.
        {omnibox::internal::kWebUIOmniboxPopup,
         omnibox::internal::kWebUIOmniboxAimPopup,
         lens::features::kLensOverlay});
  }

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Allows the embedded test server's non-standard ports to be recognized as
    // valid Google search URLs (SRP).
    command_line->AppendSwitch(switches::kIgnoreGooglePortNumbers);
    InProcessBrowserTest::SetUpCommandLine(command_line);
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

  // Navigate away to flush PreloadServingMetrics.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.LinkClick.All",
                                      0 /* kNoInstantLoad */, 1);
  histogram_tester.ExpectTotalCount("PreloadServingMetrics.LinkClick.SRP", 0);
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

  // Navigate away to flush PreloadServingMetrics.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.LinkClick.All",
                                      0 /* kNoInstantLoad */, 1);
  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.LinkClick.SRP",
                                      0 /* kNoInstantLoad */, 1);
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

  // Navigate away to flush PreloadServingMetrics.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  histogram_tester.ExpectTotalCount("PreloadServingMetrics.LinkClick.All", 0);
  histogram_tester.ExpectTotalCount("PreloadServingMetrics.LinkClick.SRP", 0);
}

IN_PROC_BROWSER_TEST_F(NavigationInitiatorPageLoadMetricsBrowserTest,
                       LinkClickPrerender) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("www.example.com", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  // Metrics are collected for the initial navigation.
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.All", 1);
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.SRP", 0);

  GURL prerender_url =
      embedded_test_server()->GetURL("www.example.com", "/simple.html");
  prerender_helper().AddPrerender(prerender_url);

  // Before activation, no metrics should be recorded for the prerendered page.
  // We should only see the initial navigation.
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.All", 1);
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.SRP", 0);

  // Activate via link click.
  content::TestActivationManager activation_manager(GetActiveWebContents(),
                                                    prerender_url);
  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      content::JsReplace(R"(let a = document.createElement('a');
                                            a.href = $1;
                                            document.body.appendChild(a);
                                            a.click();)",
                                         prerender_url.spec())));
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  // After activation, the metric should be recorded. We expect 2 total
  // navigations.
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.All",
      MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kLinkClick)),
      1);
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.SRP",
      MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kLinkClick)),
      0);

  // Navigate away to flush PreloadServingMetrics.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.LinkClick.All",
                                      2 /* kPrerender */, 1);
  histogram_tester.ExpectTotalCount("PreloadServingMetrics.LinkClick.SRP", 0);
}

IN_PROC_BROWSER_TEST_F(NavigationInitiatorPageLoadMetricsBrowserTest,
                       LinkClickPrerenderSRP) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("www.google.com", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  // Metrics are collected for the initial navigation.
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.All", 1);
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.SRP", 0);

  GURL prerender_url =
      embedded_test_server()->GetURL("www.google.com", "/search?q=test");
  prerender_helper().AddPrerender(prerender_url);

  // Before activation, no metrics should be recorded for the prerendered page.
  // We should only see the initial navigation.
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.All", 1);
  histogram_tester.ExpectTotalCount("Navigation.InitiatorType.SRP", 0);

  // Activate via link click.
  content::TestActivationManager activation_manager(GetActiveWebContents(),
                                                    prerender_url);
  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      content::JsReplace(R"(let a = document.createElement('a');
                                            a.href = $1;
                                            document.body.appendChild(a);
                                            a.click();)",
                                         prerender_url.spec())));
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  // After activation, the metric should be recorded. We expect 2 total
  // navigations.
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.All",
      MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kLinkClick)),
      1);
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.SRP",
      MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kLinkClick)),
      1);

  // Navigate away to flush PreloadServingMetrics.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.LinkClick.All",
                                      2 /* kPrerender */, 1);
  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.LinkClick.SRP",
                                      2 /* kPrerender */, 1);
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

// TODO(crbug.com/551914466): Flaky on Windows. Re-enable when fixed.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PrerenderSRPActivation DISABLED_PrerenderSRPActivation
#else
#define MAYBE_PrerenderSRPActivation PrerenderSRPActivation
#endif
IN_PROC_BROWSER_TEST_F(NavigationInitiatorPageLoadMetricsBrowserTest,
                       MAYBE_PrerenderSRPActivation) {
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

// Tests that searching selected text via the context menu records
// kContextMenuSearch in Navigation.InitiatorType.All, and does not record it in
// Navigation.InitiatorType.SRP for a non-Google search provider.
IN_PROC_BROWSER_TEST_F(NavigationInitiatorPageLoadMetricsBrowserTest,
                       ContextMenuSearchNavigation) {
  auto* model =
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  TemplateURLData data;
  data.SetShortName(u"test");
  data.SetKeyword(u"test");
  data.SetURL(embedded_test_server()
                  ->GetURL("www.example.com", "/search?q={searchTerms}")
                  .spec());
  TemplateURL* t_url = model->Add(std::make_unique<TemplateURL>(data));
  model->SetUserSelectedDefaultSearchProvider(t_url);

  base::HistogramTester histogram_tester;

  ASSERT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      GURL("data:text/html,<html><body>SearchMe</body></html>")));

  ASSERT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      "window.getSelection().selectAllChildren(document.body);"));

  // Register an observer to intercept the context menu and execute the search
  // command programmatically once shown.
  ContextMenuNotificationObserver menu_observer(
      IDC_CONTENT_CONTEXT_SEARCHWEBFOR);
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  // Simulate a right-click on the WebContents to trigger the context menu.
  // When the menu is shown, `menu_observer` automatically executes the search
  // command (IDC_CONTENT_CONTEXT_SEARCHWEBFOR), avoiding the need to simulate a
  // UI-layer mouse click on the native context menu.
  content::SimulateMouseClickAt(GetActiveWebContents(), 0,
                                blink::WebMouseEvent::Button::kRight,
                                gfx::Point(15, 15));

  content::WebContents* search_tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(search_tab));

  EXPECT_EQ(
      search_tab->GetLastCommittedURL(),
      embedded_test_server()->GetURL("www.example.com", "/search?q=SearchMe"));

  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.All",
      MetricValue(
          GetInitiatorLocation(ChromeInitiatorLocation::kContextMenuSearch)),
      1);
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.SRP",
      MetricValue(
          GetInitiatorLocation(ChromeInitiatorLocation::kContextMenuSearch)),
      0);
}

// Tests that searching selected text via the context menu records
// kContextMenuSearch in both Navigation.InitiatorType.All and
// Navigation.InitiatorType.SRP when navigating to Google Search.
IN_PROC_BROWSER_TEST_F(NavigationInitiatorPageLoadMetricsBrowserTest,
                       ContextMenuSearchNavigationSRP) {
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

  ASSERT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      GURL("data:text/html,<html><body>SearchMe</body></html>")));

  ASSERT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      "window.getSelection().selectAllChildren(document.body);"));

  // Register an observer to intercept the context menu and execute the search
  // command programmatically once shown.
  ContextMenuNotificationObserver menu_observer(
      IDC_CONTENT_CONTEXT_SEARCHWEBFOR);
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  // Simulate a right-click on the WebContents to trigger the context menu.
  // When the menu is shown, `menu_observer` automatically executes the search
  // command (IDC_CONTENT_CONTEXT_SEARCHWEBFOR), avoiding the need to simulate a
  // UI-layer mouse click on the native context menu.
  content::SimulateMouseClickAt(GetActiveWebContents(), 0,
                                blink::WebMouseEvent::Button::kRight,
                                gfx::Point(15, 15));

  content::WebContents* search_tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(search_tab));

  EXPECT_EQ(search_tab->GetLastCommittedURL(),
            embedded_test_server()->GetURL(
                "www.google.com", "/search?q=SearchMe&source=chrome.ctxt"));
  EXPECT_TRUE(page_load_metrics::IsGoogleSearchResultUrl(
      search_tab->GetLastCommittedURL()));

  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.All",
      MetricValue(
          GetInitiatorLocation(ChromeInitiatorLocation::kContextMenuSearch)),
      1);
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.SRP",
      MetricValue(
          GetInitiatorLocation(ChromeInitiatorLocation::kContextMenuSearch)),
      1);
}

// Tests that opening a link in a new tab via the context menu records kOther
// (and not kContextMenuSearch) in Navigation.InitiatorType.All, and does not
// record it in Navigation.InitiatorType.SRP for a non-Google URL.
IN_PROC_BROWSER_TEST_F(NavigationInitiatorPageLoadMetricsBrowserTest,
                       ContextMenuOpenLinkInNewTab) {
  base::HistogramTester histogram_tester;

  GURL link_url =
      embedded_test_server()->GetURL("www.example.com", "/simple.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL("data:text/html,<a id='link' href='" +
                                          link_url.spec() + "'>ClickMe</a>")));

  // Register an observer to intercept the context menu and execute the open
  // link command programmatically once shown.
  ContextMenuNotificationObserver menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  gfx::Point center =
      gfx::ToFlooredPoint(content::GetCenterCoordinatesOfElementWithId(
          GetActiveWebContents(), "link"));
  // Simulate a right-click on the link to trigger the context menu. When the
  // menu is shown, `menu_observer` automatically executes the open link in new
  // tab command (IDC_CONTENT_CONTEXT_OPENLINKNEWTAB), avoiding the need to
  // simulate a UI-layer mouse click on the native context menu.
  content::SimulateMouseClickAt(GetActiveWebContents(), 0,
                                blink::WebMouseEvent::Button::kRight, center);

  content::WebContents* new_tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(new_tab));

  EXPECT_EQ(new_tab->GetLastCommittedURL(), link_url);

  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.All",
      MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kOther)), 1);
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.SRP",
      MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kOther)), 0);
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.All",
      MetricValue(
          GetInitiatorLocation(ChromeInitiatorLocation::kContextMenuSearch)),
      0);
}

// Tests that opening a link to Google Search in a new tab via the context menu
// records kOther (and not kContextMenuSearch) in both
// Navigation.InitiatorType.All and Navigation.InitiatorType.SRP.
IN_PROC_BROWSER_TEST_F(NavigationInitiatorPageLoadMetricsBrowserTest,
                       ContextMenuOpenLinkInNewTabSRP) {
  base::HistogramTester histogram_tester;

  GURL link_url =
      embedded_test_server()->GetURL("www.google.com", "/search?q=test");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL("data:text/html,<a id='link' href='" +
                                          link_url.spec() + "'>ClickMe</a>")));

  // Register an observer to intercept the context menu and execute the open
  // link command programmatically once shown.
  ContextMenuNotificationObserver menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  gfx::Point center =
      gfx::ToFlooredPoint(content::GetCenterCoordinatesOfElementWithId(
          GetActiveWebContents(), "link"));
  // Simulate a right-click on the link to trigger the context menu. When the
  // menu is shown, `menu_observer` automatically executes the open link in new
  // tab command (IDC_CONTENT_CONTEXT_OPENLINKNEWTAB), avoiding the need to
  // simulate a UI-layer mouse click on the native context menu.
  content::SimulateMouseClickAt(GetActiveWebContents(), 0,
                                blink::WebMouseEvent::Button::kRight, center);

  content::WebContents* new_tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(new_tab));

  EXPECT_EQ(new_tab->GetLastCommittedURL(), link_url);
  EXPECT_TRUE(page_load_metrics::IsGoogleSearchResultUrl(
      new_tab->GetLastCommittedURL()));

  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.All",
      MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kOther)), 1);
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.SRP",
      MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kOther)), 1);
  histogram_tester.ExpectBucketCount(
      "Navigation.InitiatorType.All",
      MetricValue(
          GetInitiatorLocation(ChromeInitiatorLocation::kContextMenuSearch)),
      0);
}

class NavigationInitiatorPageLoadMetricsBFCacheBrowserTest
    : public NavigationInitiatorPageLoadMetricsBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  NavigationInitiatorPageLoadMetricsBFCacheBrowserTest() {
    if (IsBfcacheEnabled()) {
      bfcache_feature_list_.InitWithFeaturesAndParameters(
          content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
          content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
    } else {
      bfcache_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kBackForwardCache});
    }
  }

  bool IsBfcacheEnabled() const { return GetParam(); }

  void SetUpOnMainThread() override {
    NavigationInitiatorPageLoadMetricsBrowserTest::SetUpOnMainThread();
    if (!IsBfcacheEnabled()) {
      content::DisableBackForwardCacheForTesting(
          GetActiveWebContents(),
          content::BackForwardCache::DisableForTestingReason::
              TEST_REQUIRES_NO_CACHING);
    }
  }

 private:
  base::test::ScopedFeatureList bfcache_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         NavigationInitiatorPageLoadMetricsBFCacheBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(NavigationInitiatorPageLoadMetricsBFCacheBrowserTest,
                       BackForwardAndReload) {
  GURL url_a = embedded_test_server()->GetURL("a.com", "/empty.html");
  GURL url_b = embedded_test_server()->GetURL("b.com", "/empty.html");

  base::HistogramTester preload_histogram_tester;

  // Navigate to url_a.
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));

    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.All",
        MetricValue(page_load_metrics::NavigationHandleUserData::
                        kInitiatorLocationOther),
        1);
    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.SRP",
        MetricValue(page_load_metrics::NavigationHandleUserData::
                        kInitiatorLocationOther),
        0);
  }
  content::RenderFrameHostWrapper rfh_a(
      GetActiveWebContents()->GetPrimaryMainFrame());

  // Navigate to url_b.
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
    if (IsBfcacheEnabled()) {
      EXPECT_EQ(rfh_a->GetLifecycleState(),
                content::RenderFrameHost::LifecycleState::kInBackForwardCache);
    } else {
      EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
    }

    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.All",
        MetricValue(page_load_metrics::NavigationHandleUserData::
                        kInitiatorLocationOther),
        1);
    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.SRP",
        MetricValue(page_load_metrics::NavigationHandleUserData::
                        kInitiatorLocationOther),
        0);
  }
  content::RenderFrameHostWrapper rfh_b(
      GetActiveWebContents()->GetPrimaryMainFrame());

  {
    // Navigate back to url_a.
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(content::HistoryGoBack(GetActiveWebContents()));
    if (IsBfcacheEnabled()) {
      EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
      EXPECT_EQ(rfh_b->GetLifecycleState(),
                content::RenderFrameHost::LifecycleState::kInBackForwardCache);
    } else {
      EXPECT_TRUE(rfh_b.WaitUntilRenderFrameDeleted());
    }

    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.All",
        MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kBackward)),
        1);
    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.SRP",
        MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kBackward)),
        0);
  }
  content::RenderFrameHostWrapper rfh_a2(
      GetActiveWebContents()->GetPrimaryMainFrame());

  {
    // Navigate forward to url_b.
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(content::HistoryGoForward(GetActiveWebContents()));
    if (IsBfcacheEnabled()) {
      EXPECT_TRUE(rfh_b->IsInPrimaryMainFrame());
    } else {
      EXPECT_TRUE(rfh_a2.WaitUntilRenderFrameDeleted());
    }

    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.All",
        MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kForward)),
        1);
    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.SRP",
        MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kForward)),
        0);
  }

  {
    // Reload url_b.
    base::HistogramTester histogram_tester;
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    EXPECT_TRUE(content::WaitForLoadStop(GetActiveWebContents()));

    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.All",
        MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kReload)), 1);
    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.SRP",
        MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kReload)), 0);
  }

  // Navigate away to flush PreloadServingMetrics.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  int expected_bfcache_bucket = IsBfcacheEnabled() ? 3 /* kBFCache */ : 0;
  preload_histogram_tester.ExpectBucketCount("PreloadServingMetrics.Other.All",
                                             0 /* kNoPreload */, 2);
  preload_histogram_tester.ExpectBucketCount(
      "PreloadServingMetrics.Backward.All", expected_bfcache_bucket, 1);
  preload_histogram_tester.ExpectBucketCount(
      "PreloadServingMetrics.Forward.All", expected_bfcache_bucket, 1);
  preload_histogram_tester.ExpectBucketCount("PreloadServingMetrics.Reload.All",
                                             0 /* kNoPreload */, 1);
}

IN_PROC_BROWSER_TEST_P(NavigationInitiatorPageLoadMetricsBFCacheBrowserTest,
                       BackForwardAndReloadSRP) {
  GURL url_srp =
      embedded_test_server()->GetURL("www.google.com", "/search?q=test");
  GURL url_b = embedded_test_server()->GetURL("b.com", "/empty.html");

  base::HistogramTester preload_histogram_tester;

  // Navigate to url_srp.
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_srp));

    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.All",
        MetricValue(page_load_metrics::NavigationHandleUserData::
                        kInitiatorLocationOther),
        1);
    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.SRP",
        MetricValue(page_load_metrics::NavigationHandleUserData::
                        kInitiatorLocationOther),
        1);
  }
  content::RenderFrameHostWrapper rfh_srp(
      GetActiveWebContents()->GetPrimaryMainFrame());

  // Navigate to url_b.
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
    if (IsBfcacheEnabled()) {
      EXPECT_EQ(rfh_srp->GetLifecycleState(),
                content::RenderFrameHost::LifecycleState::kInBackForwardCache);
    } else {
      EXPECT_TRUE(rfh_srp.WaitUntilRenderFrameDeleted());
    }

    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.All",
        MetricValue(page_load_metrics::NavigationHandleUserData::
                        kInitiatorLocationOther),
        1);
    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.SRP",
        MetricValue(page_load_metrics::NavigationHandleUserData::
                        kInitiatorLocationOther),
        0);
  }
  content::RenderFrameHostWrapper rfh_b(
      GetActiveWebContents()->GetPrimaryMainFrame());

  {
    // Navigate back to url_srp.
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(content::HistoryGoBack(GetActiveWebContents()));
    if (IsBfcacheEnabled()) {
      EXPECT_TRUE(rfh_srp->IsInPrimaryMainFrame());
      EXPECT_EQ(rfh_b->GetLifecycleState(),
                content::RenderFrameHost::LifecycleState::kInBackForwardCache);
    } else {
      EXPECT_TRUE(rfh_b.WaitUntilRenderFrameDeleted());
    }

    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.All",
        MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kBackward)),
        1);
    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.SRP",
        MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kBackward)),
        1);
  }
  content::RenderFrameHostWrapper rfh_srp2(
      GetActiveWebContents()->GetPrimaryMainFrame());

  {
    // Navigate forward to url_b.
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(content::HistoryGoForward(GetActiveWebContents()));
    if (IsBfcacheEnabled()) {
      EXPECT_TRUE(rfh_b->IsInPrimaryMainFrame());
    } else {
      EXPECT_TRUE(rfh_srp2.WaitUntilRenderFrameDeleted());
    }

    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.All",
        MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kForward)),
        1);
    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.SRP",
        MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kForward)),
        0);
  }
  content::RenderFrameHostWrapper rfh_b2(
      GetActiveWebContents()->GetPrimaryMainFrame());

  {
    // Navigate back to url_srp again.
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(content::HistoryGoBack(GetActiveWebContents()));
    if (IsBfcacheEnabled()) {
      EXPECT_TRUE(rfh_srp->IsInPrimaryMainFrame());
    } else {
      EXPECT_TRUE(rfh_b2.WaitUntilRenderFrameDeleted());
    }

    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.All",
        MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kBackward)),
        1);
    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.SRP",
        MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kBackward)),
        1);
  }

  {
    // Reload url_srp.
    base::HistogramTester histogram_tester;
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    EXPECT_TRUE(content::WaitForLoadStop(GetActiveWebContents()));

    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.All",
        MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kReload)), 1);
    histogram_tester.ExpectUniqueSample(
        "Navigation.InitiatorType.SRP",
        MetricValue(GetInitiatorLocation(ChromeInitiatorLocation::kReload)), 1);
  }

  // Navigate away to flush PreloadServingMetrics.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  int expected_bfcache_bucket = IsBfcacheEnabled() ? 3 /* kBFCache */ : 0;
  preload_histogram_tester.ExpectBucketCount("PreloadServingMetrics.Other.All",
                                             0 /* kNoPreload */, 2);
  preload_histogram_tester.ExpectBucketCount("PreloadServingMetrics.Other.SRP",
                                             0 /* kNoPreload */, 1);
  preload_histogram_tester.ExpectBucketCount(
      "PreloadServingMetrics.Backward.All", expected_bfcache_bucket, 2);
  preload_histogram_tester.ExpectBucketCount(
      "PreloadServingMetrics.Backward.SRP", expected_bfcache_bucket, 2);
  preload_histogram_tester.ExpectBucketCount(
      "PreloadServingMetrics.Forward.All", expected_bfcache_bucket, 1);
  preload_histogram_tester.ExpectBucketCount(
      "PreloadServingMetrics.Forward.SRP", expected_bfcache_bucket, 0);
  preload_histogram_tester.ExpectBucketCount("PreloadServingMetrics.Reload.All",
                                             0 /* kNoPreload */, 1);
  preload_histogram_tester.ExpectBucketCount("PreloadServingMetrics.Reload.SRP",
                                             0 /* kNoPreload */, 1);
}
