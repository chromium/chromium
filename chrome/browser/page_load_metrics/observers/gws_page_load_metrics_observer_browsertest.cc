// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/gws_page_load_metrics_observer.h"

#include "base/location.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lens/lens_features.h"
#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/google/browser/google_url_util.h"
#include "components/page_load_metrics/google/browser/gws_abandoned_page_load_metrics_observer.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

std::unique_ptr<net::test_server::HttpResponse> SRPHandler(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HttpStatusCode::HTTP_OK);
  http_response->set_content_type("text/html");
  // TODO(crbug.com/436345871): Consider sending the performance mark to test
  // the metrics.
  http_response->set_content(R"(
    <html>
      <body>
        <script>
          performance.mark('SearchAFTStart');
          performance.mark('trigger:SearchAFTEnd');
        </script>
        SRP Content
        <a id="thelink" href="/search?q=duplicate">Next SRP Link</a>
      </body>
    </html>
  )");
  return http_response;
}

using page_load_metrics::PageLoadMetricsTestWaiter;

class GWSPageLoadMetricsObserverBrowserTest : public MetricIntegrationTest {
 public:
  GWSPageLoadMetricsObserverBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &GWSPageLoadMetricsObserverBrowserTest::GetActiveWebContents,
            base::Unretained(this))) {}

  GWSPageLoadMetricsObserverBrowserTest(
      const GWSPageLoadMetricsObserverBrowserTest&) = delete;
  GWSPageLoadMetricsObserverBrowserTest& operator=(
      const GWSPageLoadMetricsObserverBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    MetricIntegrationTest::SetUpOnMainThread();
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/search",
                            base::BindRepeating(SRPHandler)));
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    Start();
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  std::unique_ptr<PageLoadMetricsTestWaiter> CreatePageLoadMetricsTestWaiter() {
    return std::make_unique<PageLoadMetricsTestWaiter>(web_contents());
  }

  GURL GetSrpUrl(std::optional<std::string> query) {
    constexpr char kSRPDomain[] = "www.google.com";
    constexpr char kSRPPath[] = "/search?q=";

    GURL url(embedded_test_server()->GetURL(kSRPDomain,
                                            kSRPPath + query.value_or("")));
    EXPECT_TRUE(page_load_metrics::IsGoogleSearchResultUrl(url));
    return url;
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(GWSPageLoadMetricsObserverBrowserTest,
                       PrerenderSRPAndActivate) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial SRP page and wait until load event.
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetSrpUrl(std::nullopt)));
  waiter->Wait();

  // Check that the prerender metrics are not recorded.
  histogram_tester.ExpectTotalCount(internal::kHistogramPrerenderHostReused, 0);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrerenderNavigationToActivation, 0);

  GURL url_srp_prerender = GetSrpUrl("prerender");
  prerender_helper_.AddPrerender(url_srp_prerender);

  // Activate the prerendered SRP on the initial WebContents.
  content::TestActivationManager activation_manager(web_contents(),
                                                    url_srp_prerender);
  ASSERT_TRUE(
      content::ExecJs(web_contents()->GetPrimaryMainFrame(),
                      content::JsReplace("location = $1", url_srp_prerender)));
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  // Flush metrics.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(url::kAboutBlankURL)));

  // Check that the prerender metrics are recorded.
  histogram_tester.ExpectTotalCount(internal::kHistogramPrerenderHostReused, 1);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrerenderNavigationToActivation, 1);
}

IN_PROC_BROWSER_TEST_F(GWSPageLoadMetricsObserverBrowserTest,
                       TraverseNavigationBackToSRP) {
  // Disable back/forward cache to avoid reusing the page when going back.
  // For now we do not track BFCache restore pages because we do not think it
  // will be a bottleneck.
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::DisableForTestingReason::
                          TEST_REQUIRES_NO_CACHING);

  base::HistogramTester histogram_tester;

  // Navigate to an initial SRP page.
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);
  const GURL srp_url = GetSrpUrl("initial");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), srp_url));
  waiter->Wait();

  // Navigate to another page to have a page to go back from.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(url::kAboutBlankURL)));

  // Go back to the SRP. This is a traverse navigation.
  web_contents()->GetController().GoBack();

  // Now that the navigation is paused, we can safely set up our waiter.
  auto back_waiter = CreatePageLoadMetricsTestWaiter();
  back_waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);

  // Now wait for the paint metric.
  back_waiter->Wait();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), srp_url);

  // Flush metrics by navigating away again.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(url::kAboutBlankURL)));

  // Check that the LCP metrics are recorded for both navigations.
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSLargestContentfulPaint, 2);

  // Check that the traverse navigation metrics are recorded for the back
  // navigation.
  const std::string traverse_lcp_histogram =
      base::StrCat({internal::kHistogramGWSLargestContentfulPaint,
                    internal::kTraverseNavigation});
  histogram_tester.ExpectTotalCount(traverse_lcp_histogram, 1);

  const std::string non_restored_lcp_histogram =
      base::StrCat({traverse_lcp_histogram, internal::kNonRestoreNavigation});
  histogram_tester.ExpectTotalCount(non_restored_lcp_histogram, 1);

  const std::string restored_lcp_histogram =
      base::StrCat({traverse_lcp_histogram, internal::kRestoreNavigation});
  histogram_tester.ExpectTotalCount(restored_lcp_histogram, 0);
}

IN_PROC_BROWSER_TEST_F(GWSPageLoadMetricsObserverBrowserTest,
                       RestoreNavigationToSRP) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial SRP page in a new tab.
  const GURL srp_url = GetSrpUrl("initial");

  content::WebContents* new_tab = GetActiveWebContents()->OpenURL(
      content::OpenURLParams(
          GURL(url::kAboutBlankURL), content::Referrer(),
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
  content::WaitForLoadStop(new_tab);

  // The new tab is now active. Create a waiter for it.
  auto waiter = std::make_unique<PageLoadMetricsTestWaiter>(new_tab);
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);
  ASSERT_TRUE(content::NavigateToURL(new_tab, srp_url));
  waiter->Wait();

  // We should have two tabs now. The active one is the SRP.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  const int srp_tab_index = 1;
  ASSERT_EQ(srp_tab_index, browser()->tab_strip_model()->active_index());

  // Close the SRP tab. This will flush the metrics.
  {
    content::WebContentsDestroyedWatcher destroyed_watcher(
        browser()->tab_strip_model()->GetWebContentsAt(srp_tab_index));
    browser()->tab_strip_model()->CloseWebContentsAt(
        srp_tab_index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
    destroyed_watcher.Wait();
  }
  // Check histograms for the initial load. One LCP, no traverse.
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSLargestContentfulPaint, 1);
  const std::string traverse_lcp_histogram =
      base::StrCat({internal::kHistogramGWSLargestContentfulPaint,
                    internal::kTraverseNavigation});
  histogram_tester.ExpectTotalCount(traverse_lcp_histogram, 0);

  // // Restore the tab.
  ui_test_utils::AllBrowserTabAddedWaiter tab_added_waiter;
  chrome::RestoreTab(browser());
  content::WebContents* restored_web_contents = tab_added_waiter.Wait();
  ASSERT_TRUE(restored_web_contents);

  auto restore_waiter =
      std::make_unique<PageLoadMetricsTestWaiter>(restored_web_contents);
  content::WaitForLoadStop(restored_web_contents);

  restore_waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);
  restore_waiter->Wait();
  EXPECT_EQ(restored_web_contents->GetLastCommittedURL(), srp_url);

  // Flush metrics by navigating away again.
  ASSERT_TRUE(
      content::NavigateToURL(restored_web_contents, GURL(url::kAboutBlankURL)));

  // Check that the LCP metrics are recorded for both navigations.
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSLargestContentfulPaint, 2);

  // Check that the traverse navigation metrics are recorded for the back
  // navigation.
  histogram_tester.ExpectTotalCount(traverse_lcp_histogram, 1);

  const std::string non_restored_lcp_histogram =
      base::StrCat({traverse_lcp_histogram, internal::kNonRestoreNavigation});
  histogram_tester.ExpectTotalCount(non_restored_lcp_histogram, 0);

  const std::string restored_lcp_histogram =
      base::StrCat({traverse_lcp_histogram, internal::kRestoreNavigation});
  histogram_tester.ExpectTotalCount(restored_lcp_histogram, 1);
}

struct GWSActualNavigationStartBasedBrowserTestParam {
  std::string test_name;
  bool is_srp;
};

const GWSActualNavigationStartBasedBrowserTestParam
    kGWSActualNavigationStartBasedBrowserTestParams[]{
        {
            .test_name = "ForSrp",
            .is_srp = true,
        },
        {
            .test_name = "ForNonSrp",
            .is_srp = false,
        },
    };

class GWSActualNavigationStartBasedBrowserTest
    : public GWSPageLoadMetricsObserverBrowserTest,
      public testing::WithParamInterface<
          GWSActualNavigationStartBasedBrowserTestParam> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    GWSActualNavigationStartBasedBrowserTest,
    testing::ValuesIn(kGWSActualNavigationStartBasedBrowserTestParams),
    [](const ::testing::TestParamInfo<
        GWSActualNavigationStartBasedBrowserTestParam>& info) {
      return info.param.test_name;
    });

IN_PROC_BROWSER_TEST_P(GWSActualNavigationStartBasedBrowserTest, Uma) {
  base::HistogramTester histogram_tester;
  bool is_srp = GetParam().is_srp;

  // 1. Navigate to an initial page.
  GURL target_url =
      is_srp ? GetSrpUrl("test")
             : embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      GURL(base::StrCat({"data:text/html,<html><body><a id='link' href='",
                         target_url.spec(), "'>Link</a></body></html>"}))));

  // 2. Click the link to trigger a renderer-initiated navigation with user
  // gesture.
  std::unique_ptr<PageLoadMetricsTestWaiter> waiter =
      CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "link");
  waiter->Wait();

  // 3. Flush metrics by navigating away.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(url::kAboutBlankURL)));

  // 4. Check that metrics are recorded and match core metrics.
  auto check_metric = [&](const std::string& core_name,
                          const std::string& gws_name,
                          base::Location location = FROM_HERE) {
    SCOPED_TRACE(location.ToString());
    histogram_tester.ExpectTotalCount(core_name, 1);
    histogram_tester.ExpectTotalCount(gws_name, is_srp ? 1 : 0);
    if (is_srp) {
      EXPECT_EQ(histogram_tester.GetTotalSum(core_name),
                histogram_tester.GetTotalSum(gws_name))
          << "Value mismatch between: \n - " << core_name << "\n - "
          << gws_name;
    }
  };

  check_metric(
      "Navigation.Timeline.InteractionToActualNavigationStart.MainFrameOnly."
      "Duration",
      internal::kHistogramGWSInteractionToActualNavigationStart);
  check_metric(internal::kHistogramInteractionToNavigationStart,
               internal::kHistogramGWSInteractionToNavigationStart);
  check_metric(
      internal::kHistogramNavigationTimingNavigationStartToNavigationCommitSent,
      internal::kHistogramGWSNavigationStartToNavigationCommitSent);
  check_metric(internal::kHistogramNavigationCommitSentToParseStart,
               internal::kHistogramGWSNavigationCommitSentToParseStart);
  check_metric("PageLoad.PaintTiming.ParseStartToFirstContentfulPaint",
               internal::kHistogramGWSParseStartToFirstContentfulPaint);
  check_metric(internal::kHistogramParseStartToDOMContentLoaded,
               internal::kHistogramGWSParseStartToDOMContentLoaded);
  check_metric(internal::kHistogramParseStartToLargestContentfulPaint,
               internal::kHistogramGWSParseStartToLargestContentfulPaint);

  check_metric(internal::kHistogramActualNavigationStartToNavigationStart,
               internal::kHistogramGWSActualNavigationStartToNavigationStart);
  check_metric(
      internal::kHistogramActualNavigationStartToNavigationCommitSent,
      internal::kHistogramGWSActualNavigationStartToNavigationCommitSent);
  check_metric(internal::kHistogramActualNavigationStartToParseStart,
               internal::kHistogramGWSActualNavigationStartToParseStart);
  check_metric(
      internal::kHistogramActualNavigationStartToFirstContentfulPaint,
      internal::kHistogramGWSActualNavigationStartToFirstContentfulPaint);
  check_metric(internal::kHistogramActualNavigationStartToDOMContentLoaded,
               internal::kHistogramGWSActualNavigationStartToDOMContentLoaded);
  check_metric(
      internal::kHistogramActualNavigationStartToLargestContentfulPaint,
      internal::kHistogramGWSActualNavigationStartToLargestContentfulPaint);

  histogram_tester.ExpectTotalCount(internal::kHistogramGWSAFTEnd,
                                    is_srp ? 1 : 0);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSAFTEndWithPreNavigationLatency, is_srp ? 1 : 0);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSActualNavigationStartToAFTEnd, is_srp ? 1 : 0);
  histogram_tester.ExpectTotalCount(
      internal::
          kHistogramGWSActualNavigationStartToAFTEndWithPreNavigationLatency,
      is_srp ? 1 : 0);
}

class GWSPageLoadMetricsObserverContextMenuNaviBrowserTest
    : public GWSPageLoadMetricsObserverBrowserTest {
 public:
  GWSPageLoadMetricsObserverContextMenuNaviBrowserTest() {
    feature_list_.InitAndDisableFeature(lens::features::kLensOverlay);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GWSPageLoadMetricsObserverContextMenuNaviBrowserTest,
                       ContextMenuSearchNavigation) {
  // 1. Setup the test server as the default search engine.
  auto* model = TemplateURLServiceFactory::GetForProfile(browser()->profile());
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  TemplateURLData data;
  data.SetShortName(u"test");
  data.SetKeyword(u"test");
  // Point the search URL to your embedded test server's search handler.
  data.SetURL(embedded_test_server()
                  ->GetURL("www.google.com", "/search?q={searchTerms}")
                  .spec());
  TemplateURL* t_url = model->Add(std::make_unique<TemplateURL>(data));
  model->SetUserSelectedDefaultSearchProvider(t_url);

  base::HistogramTester histogram_tester;

  // 2. Navigate to an initial page with text.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      GURL("data:text/html,<html><body>SearchMe</body></html>")));

  // 3. Select the text.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "window.getSelection().selectAllChildren(document.body);"));

  // 4. Set up the observer to click the search item.
  ContextMenuNotificationObserver menu_observer(
      IDC_CONTENT_CONTEXT_SEARCHWEBFOR);

  // 5. Capture the new tab that search will open.
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  // 6. Trigger the context menu by right-clicking.
  content::SimulateMouseClickAt(web_contents(), 0,
                                blink::WebMouseEvent::Button::kRight,
                                gfx::Point(15, 15));

  // 7. Wait for the search results page.
  content::WebContents* search_tab = add_tab.Wait();
  PageLoadMetricsTestWaiter waiter{search_tab};
  waiter.AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);
  EXPECT_TRUE(content::WaitForLoadStop(search_tab));
  waiter.Wait();

  // 8. Verify we are on a Google Search page.
  EXPECT_TRUE(page_load_metrics::IsGoogleSearchResultUrl(
      search_tab->GetLastCommittedURL()));

  // 9. Flush metrics by navigating away.
  ASSERT_TRUE(content::NavigateToURL(search_tab, GURL(url::kAboutBlankURL)));

  // 10. Check if GWS metrics were recorded.
  histogram_tester.ExpectTotalCount(
      base::StrCat({internal::kHistogramGWSLargestContentfulPaint,
                    internal::kStartedFromContextMenu}),
      1);
}

class GWSPageLoadMetricsObserverIgnoreDuplicateNavsBrowserTest
    : public GWSPageLoadMetricsObserverBrowserTest {
 public:
  GWSPageLoadMetricsObserverIgnoreDuplicateNavsBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kIgnoreDuplicateNavs);
  }

  void SetUpInProcessBrowserTestFixture() override {
    GWSPageLoadMetricsObserverBrowserTest::SetUpInProcessBrowserTestFixture();
    // By default, IgnoreDuplicateNavs is disabled in tests to prevent
    // navigations from being unintentionally ignored. This test requires the
    // feature, so remove the switch.
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kDisableIgnoreDuplicateNavsForTesting);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that a link click navigation that's a duplicate of an ongoing link
// click navigation gets ignored.
IN_PROC_BROWSER_TEST_F(GWSPageLoadMetricsObserverIgnoreDuplicateNavsBrowserTest,
                       DuplicateLinkClickIsIgnored) {
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetSrpUrl("initial")));
  waiter->Wait();

  GURL srp_link_url = GetSrpUrl("duplicate");

  base::HistogramTester histogram_tester;
  auto srp_waiter = CreatePageLoadMetricsTestWaiter();
  srp_waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kFirstContentfulPaint);
  // 1. Navigate to an SRP page.
  content::TestNavigationManager nav_manager(web_contents(), srp_link_url);
  std::string click_script = "document.getElementById('thelink').click()";
  ASSERT_TRUE(content::ExecJs(web_contents(), click_script));

  // Pause the navigation at request start.
  ASSERT_TRUE(nav_manager.WaitForRequestStart());

  // 2. Click the link again.
  ASSERT_TRUE(content::ExecJs(web_contents(), click_script));
  EXPECT_TRUE(ExecJs(web_contents(), "console.log('Success');"));

  // Wait for the first navigation to finish.
  EXPECT_TRUE(nav_manager.WaitForNavigationFinished());
  srp_waiter->Wait();

  // Flush metrics by navigating away again.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(url::kAboutBlankURL)));

  // Check that the FCP metrics are recorded only for the first navigation.
  histogram_tester.ExpectTotalCount(internal::kHistogramGWSFirstContentfulPaint,
                                    1);
  // Check that the FCP metrics for duplicate navigations are also recorded.
  histogram_tester.ExpectTotalCount(
      base::StrCat({internal::kHistogramGWSFirstContentfulPaint,
                    internal::kHistogramDuplicateIgnoredSuffix}),
      1);
}

// Tests that a browser-initiated navigation that's a duplicate of an ongoing
// browser-initiated navigation gets ignored.
IN_PROC_BROWSER_TEST_F(GWSPageLoadMetricsObserverIgnoreDuplicateNavsBrowserTest,
                       DuplicateLoadURLIsIgnored) {
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLargestContentfulPaint);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetSrpUrl("initial")));
  waiter->Wait();

  GURL srp_url = GetSrpUrl("duplicate");

  base::HistogramTester histogram_tester;
  auto srp_waiter = CreatePageLoadMetricsTestWaiter();
  srp_waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kFirstContentfulPaint);
  // 1. Navigate to an SRP page.
  content::TestNavigationManager nav_manager(web_contents(), srp_url);
  web_contents()->GetController().LoadURL(srp_url, content::Referrer(),
                                          ui::PAGE_TRANSITION_TYPED,
                                          std::string());
  // Pause the navigation at request start.
  ASSERT_TRUE(nav_manager.WaitForRequestStart());

  // 2. Load the same URL again.
  web_contents()->GetController().LoadURL(srp_url, content::Referrer(),
                                          ui::PAGE_TRANSITION_TYPED,
                                          std::string());
  // Wait for the first navigation to finish.
  EXPECT_TRUE(nav_manager.WaitForNavigationFinished());
  srp_waiter->Wait();

  // Flush metrics by navigating away again.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(url::kAboutBlankURL)));

  // Check that the FCP metrics are recorded only for the first navigation.
  histogram_tester.ExpectTotalCount(internal::kHistogramGWSFirstContentfulPaint,
                                    1);
  // Check that the FCP metrics for duplicate navigations are also recorded.
  histogram_tester.ExpectTotalCount(
      base::StrCat({internal::kHistogramGWSFirstContentfulPaint,
                    internal::kHistogramDuplicateIgnoredSuffix}),
      1);
}

}  // namespace
