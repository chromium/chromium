// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/initial_webui_page_load_metrics_observer.h"

#include <memory>

#include "base/metrics/statistics_recorder.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher_impl.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_test_utils.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/ukm/gmock_matchers.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;
using ukm::testing::HasMetric;
using ukm::testing::HasMetricWithValue;

}  // namespace

class WebUIControllerInitalizer : protected content::WebContentsObserver {
 public:
  ~WebUIControllerInitalizer() override = default;

  virtual void Init(content::WebUIController* web_ui_controller) = 0;
  void Watch(content::WebContents* web_contents) {
    content::WebContentsObserver::Observe(web_contents);
  }

 protected:
  void DidFinishNavigation(content::NavigationHandle* handle) override {
    if (handle->GetWebContents() && handle->GetWebContents()->GetWebUI()) {
      auto* controller = handle->GetWebContents()->GetWebUI()->GetController();
      Init(controller);
    }
    content::WebContentsObserver::Observe(nullptr);
  }
};

class ToolbarDependencyProvider : public WebUIToolbarUI::DependencyProvider {
 public:
  explicit ToolbarDependencyProvider(Browser* browser) : browser_(browser) {}
  ~ToolbarDependencyProvider() = default;

  browser_controls_api::BrowserControlsService::BrowserControlsServiceDelegate*
  GetBrowserControlsDelegate() override {
    return nullptr;
  }

  toolbar_ui_api::ToolbarUIService::ToolbarUIServiceDelegate*
  GetToolbarUIServiceDelegate() override {
    return nullptr;
  }

  std::unique_ptr<toolbar_ui_api::NavigationControlsStateFetcher>
  GetNavigationControlsStateFetcher() override {
    return std::make_unique<toolbar_ui_api::NavigationControlsStateFetcherImpl>(
        base::BindLambdaForTesting(
            []() { return CreateValidNavigationControlsState(); }));
  }

  std::unique_ptr<toolbar_ui_api::IconTableFetcher> GetIconTableFetcher()
      override {
    return std::make_unique<FakeIconTableFetcher>();
  }

  CommandUpdater* GetCommandUpdater() override {
    return reinterpret_cast<CommandUpdater*>(
        browser_->GetFeatures().browser_command_controller());
  }

 private:
  raw_ptr<Browser> browser_;
};

class WebUIToolbarInitializer : public WebUIControllerInitalizer {
 public:
  explicit WebUIToolbarInitializer(Browser* browser) : injector_(browser) {}
  ~WebUIToolbarInitializer() override = default;

  void Init(content::WebUIController* controller) override {
    if (controller && controller->GetType()) {
      if (auto* toolbar_controller = controller->GetAs<WebUIToolbarUI>()) {
        toolbar_controller->Init(&injector_);
      }
    }
  }

 private:
  ToolbarDependencyProvider injector_;
};

class InitialWebUIPageLoadMetricsObserverBrowserTest
    : public InProcessBrowserTest {
 public:
  InitialWebUIPageLoadMetricsObserverBrowserTest()
      : InitialWebUIPageLoadMetricsObserverBrowserTest(
            {features::kInitialWebUI, features::kWebUIReloadButton,
             features::kInitialWebUIMetrics,
             features::kInitialWebUISyncNavStartToCommit},
            {}) {}

  InitialWebUIPageLoadMetricsObserverBrowserTest(
      std::vector<base::test::FeatureRef> enabled_features,
      std::vector<base::test::FeatureRef> disabled_features) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  InitialWebUIPageLoadMetricsObserverBrowserTest(
      const InitialWebUIPageLoadMetricsObserverBrowserTest&) = delete;
  InitialWebUIPageLoadMetricsObserverBrowserTest& operator=(
      const InitialWebUIPageLoadMetricsObserverBrowserTest&) = delete;

  ~InitialWebUIPageLoadMetricsObserverBrowserTest() override = default;

 protected:
  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents);
  }

  content::WebContents* CreateNewContents(
      const GURL& url,
      WebUIToolbarInitializer& initializer) {
    content::BrowserContext* browser_context = browser()
                                                   ->tab_strip_model()
                                                   ->GetActiveWebContents()
                                                   ->GetBrowserContext();
    content::WebContents::CreateParams new_contents_params(
        browser_context,
        content::SiteInstance::CreateForURL(browser_context, url));
    std::unique_ptr<content::WebContents> new_web_contents(
        content::WebContents::Create(new_contents_params));

    initializer.Watch(new_web_contents.get());
    InitializePageLoadMetricsForWebContents(new_web_contents.get());

    content::WebContents* raw_contents = new_web_contents.get();
    browser()->tab_strip_model()->AppendWebContents(std::move(new_web_contents),
                                                    false);
    return raw_contents;
  }

  content::WebContents* NavigateAndWaitForMetrics(const GURL& url,
                                                  bool close_tab = true,
                                                  bool wait_for_paint = true) {
    content::BrowserContext* browser_context = browser()
                                                   ->tab_strip_model()
                                                   ->GetActiveWebContents()
                                                   ->GetBrowserContext();
    content::WebContents::CreateParams new_contents_params(
        browser_context,
        content::SiteInstance::CreateForURL(browser_context, url));
    std::unique_ptr<content::WebContents> new_web_contents(
        content::WebContents::Create(new_contents_params));

    WebUIToolbarInitializer initializer(browser());
    initializer.Watch(new_web_contents.get());

    InitializePageLoadMetricsForWebContents(new_web_contents.get());

    std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
        metrics_waiter;
    if (wait_for_paint) {
      metrics_waiter =
          std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
              new_web_contents.get());
      metrics_waiter->AddPageExpectation(
          page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
              kMonotonicFirstPaint);
      metrics_waiter->AddPageExpectation(
          page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
              kMonotonicFirstContentfulPaint);
    }

    content::TestNavigationObserver navigation_observer(url);
    navigation_observer.WatchExistingWebContents();

    content::WebContents* raw_contents = new_web_contents.get();
    browser()->tab_strip_model()->AppendWebContents(std::move(new_web_contents),
                                                    true);
    raw_contents->GetController().LoadURL(
        url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
    navigation_observer.Wait();
    if (wait_for_paint && metrics_waiter) {
      metrics_waiter->Wait();
    }

    if (close_tab) {
      int index =
          browser()->tab_strip_model()->GetIndexOfWebContents(raw_contents);
      browser()->tab_strip_model()->CloseWebContentsAt(index, 0);
    }
    return raw_contents;
  }

  std::vector<const ukm::mojom::UkmEntry*> GetEntriesForUrl(
      const std::string& event_name,
      const GURL& url) {
    std::vector<const ukm::mojom::UkmEntry*> filtered_entries;
    for (const auto& entry : ukm_recorder_->GetEntriesByName(event_name)) {
      const ukm::UkmSource* source =
          ukm_recorder_->GetSourceForSourceId(entry->source_id);
      if (source && source->url() == url) {
        filtered_entries.push_back(entry.get());
      }
    }
    return filtered_entries;
  }

  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

class InitialWebUIPageLoadMetricsObserverBrowserTestWithoutSyncNav
    : public InitialWebUIPageLoadMetricsObserverBrowserTest {
 public:
  InitialWebUIPageLoadMetricsObserverBrowserTestWithoutSyncNav()
      : InitialWebUIPageLoadMetricsObserverBrowserTest(
            {features::kInitialWebUI, features::kWebUIReloadButton,
             features::kInitialWebUIMetrics},
            {features::kInitialWebUISyncNavStartToCommit}) {}
};

// Verify PageLoad event is recorded with valid Paint Milestones in the expected
// sequence of lifecycle events.
// Verify that the InitialWebUIPageLoad UKM event is recorded with all expected
// metrics in the correct sequence of lifecycle events.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyLifecycleMetrics) {
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  NavigateAndWaitForMetrics(url);
  auto page_load_entries = GetEntriesForUrl("InitialWebUIPageLoad", url);

  EXPECT_THAT(
      page_load_entries,
      ElementsAre(
          // 1. OnFirstContentfulPaintInPage
          HasMetric("PaintTiming.NavigationToFirstContentfulPaint"),
          // 2. RecordPageLoadMetrics
          AllOf(HasMetric("HourOfDay"), HasMetric("DayOfWeek"),
                HasMetric("PageTiming.ForegroundDurationMs")),
          // 3. RecordRendererUsageMetrics
          HasMetric("SiteInstanceRenderProcessAssignment"),
          // 4. RecordTimingMetrics
          AllOf(HasMetric("ParseTiming.NavigationToParseStart"),
                HasMetric(
                    "DocumentTiming.NavigationToDOMContentLoadedEventFired"),
                HasMetric("DocumentTiming.NavigationToLoadEventFired"),
                HasMetric("PaintTiming.NavigationToFirstPaint"),
                HasMetric("CpuTimeMs")),
          // 5. RecordPageEndMetrics
          AllOf(HasMetric("Navigation.PageTransition"),
                HasMetric("Navigation.PageEndReason3"),
                HasMetric("PageTiming.TotalForegroundDurationMs"))));
}

// Verify NavigationTiming event is recorded with all 7 sub-metrics.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyNavigationTiming) {
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  NavigateAndWaitForMetrics(url);
  auto entries = GetEntriesForUrl("InitialWebUINavigationTiming", url);
  ASSERT_EQ(entries.size(), 1u);
  const auto* entry = entries[0];

  const int64_t* first_request_start =
      ukm_recorder_->GetEntryMetric(entry, "FirstRequestStart");
  const int64_t* first_response_start =
      ukm_recorder_->GetEntryMetric(entry, "FirstResponseStart");
  const int64_t* navigation_commit_sent =
      ukm_recorder_->GetEntryMetric(entry, "NavigationCommitSent");
  const int64_t* navigation_commit_received =
      ukm_recorder_->GetEntryMetric(entry, "NavigationCommitReceived");
  const int64_t* navigation_commit_reply_sent =
      ukm_recorder_->GetEntryMetric(entry, "NavigationCommitReplySent");
  const int64_t* navigation_did_commit =
      ukm_recorder_->GetEntryMetric(entry, "NavigationDidCommit");

  ASSERT_TRUE(first_request_start);
  ASSERT_TRUE(first_response_start);
  ASSERT_TRUE(navigation_commit_sent);
  ASSERT_TRUE(navigation_commit_received);
  ASSERT_TRUE(navigation_commit_reply_sent);
  ASSERT_TRUE(navigation_did_commit);

  EXPECT_GE(*first_request_start, 0);
  EXPECT_LE(*first_request_start, *first_response_start);
  EXPECT_LE(*first_response_start, *navigation_commit_sent);
  EXPECT_LE(*navigation_commit_sent, *navigation_commit_received);
  EXPECT_LE(*navigation_commit_received, *navigation_commit_reply_sent);
  EXPECT_LE(*navigation_commit_reply_sent, *navigation_did_commit);
}

// Verify InitialWebUIPageLoadMetricsObserver is NOT registered for non-initial
// WebUI pages.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyNoLoggingForNonInitialWebUI) {
  GURL url("chrome://version");
  NavigateAndWaitForMetrics(url, /*close_tab=*/true,
                            /*wait_for_paint=*/false);
  EXPECT_THAT(GetEntriesForUrl("InitialWebUIPageLoad", url), IsEmpty());
}

// Verify InitialWebUIPageLoadMetricsObserver is NOT registered for standard
// Web pages.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyObserverNotRegisteredForStandardWebPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  NavigateAndWaitForMetrics(url, /*close_tab=*/true, /*wait_for_paint=*/false);

  EXPECT_THAT(GetEntriesForUrl("InitialWebUINavigationTiming", url), IsEmpty());

  EXPECT_THAT(GetEntriesForUrl("InitialWebUIPageLoad", url), IsEmpty());
}

// Verify page end reason and failed load details are logged for failed
// provisional loads.
IN_PROC_BROWSER_TEST_F(
    InitialWebUIPageLoadMetricsObserverBrowserTestWithoutSyncNav,
    VerifyFailedProvisionalLoad) {
  GURL failed_url(chrome::kChromeUIWebUIToolbarURL);
  WebUIToolbarInitializer initializer(browser());
  content::WebContents* active_contents =
      CreateNewContents(failed_url, initializer);
  browser()->tab_strip_model()->ActivateTabAt(1);

  content::TestNavigationManager navigation_manager(active_contents,
                                                    failed_url);
  active_contents->GetController().LoadURL(
      failed_url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

  ASSERT_TRUE(navigation_manager.WaitForRequestStart());
  active_contents->Stop();
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  browser()->tab_strip_model()->CloseWebContentsAt(1, 0);

  EXPECT_THAT(GetEntriesForUrl("InitialWebUIPageLoad", failed_url),
              Contains(AllOf(
                  HasMetric("Net.ErrorCode.OnFailedProvisionalLoad"),
                  HasMetric("PageTiming.NavigationToFailedProvisionalLoad"))));
}

// Verify background timing logic for pages loaded in the background.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyBackgroundLoad) {
  WebUIToolbarInitializer initializer(browser());
  content::WebContents* bg_contents =
      CreateNewContents(GURL(chrome::kChromeUIWebUIToolbarURL), initializer);

  auto metrics_waiter =
      std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
          bg_contents);
  metrics_waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kLoadEvent);

  content::TestNavigationObserver navigation_observer{
      GURL(chrome::kChromeUIWebUIToolbarURL)};
  navigation_observer.WatchExistingWebContents();

  bg_contents->GetController().LoadURL(GURL(chrome::kChromeUIWebUIToolbarURL),
                                       content::Referrer(),
                                       ui::PAGE_TRANSITION_LINK, std::string());
  navigation_observer.Wait();
  metrics_waiter->Wait();

  browser()->tab_strip_model()->CloseWebContentsAt(1, 0);

  auto entries = ukm_recorder_->GetEntriesByName("InitialWebUIPageLoad");
  ASSERT_GE(entries.size(), 1u);
}

// Verify First Paint is not logged when page goes to background before it.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyForegroundToBackgroundBeforeFirstPaint) {
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  WebUIToolbarInitializer initializer(browser());
  content::WebContents* active_contents = CreateNewContents(url, initializer);
  browser()->tab_strip_model()->ActivateTabAt(1);

  content::TestNavigationObserver navigation_observer{url};
  navigation_observer.WatchExistingWebContents();

  active_contents->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  active_contents->WasHidden();
  navigation_observer.Wait();

  browser()->tab_strip_model()->CloseWebContentsAt(1, 0);

  EXPECT_THAT(GetEntriesForUrl("InitialWebUIPageLoad", url),
              Each(Not(HasMetric("PaintTiming.NavigationToFirstPaint"))));
}

// Verify WasCached is false for WebUI page loads.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyCachedPageLoad) {
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  NavigateAndWaitForMetrics(url);
  NavigateAndWaitForMetrics(url);

  auto entries = GetEntriesForUrl("InitialWebUIPageLoad", url);
  EXPECT_THAT(entries, SizeIs(Ge(2u)));
  EXPECT_THAT(entries, Each(Not(HasMetricWithValue("WasCached", 1))));
}

// Verify MainFrameResource.RequestHasNoStore is true for page with no-store
// resources.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyNoStoreResource) {
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  NavigateAndWaitForMetrics(url);
  EXPECT_THAT(GetEntriesForUrl("InitialWebUIPageLoad", url),
              Contains(HasMetric("MainFrameResource.RequestHasNoStore")));
}

// Verify PageEndReason is recorded correctly for short page lifetimes.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyShortLifetime) {
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  WebUIToolbarInitializer initializer(browser());
  content::WebContents* active_contents = CreateNewContents(url, initializer);
  browser()->tab_strip_model()->ActivateTabAt(1);

  content::TestNavigationObserver navigation_observer{url};
  navigation_observer.WatchExistingWebContents();

  active_contents->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  navigation_observer.Wait();

  browser()->tab_strip_model()->CloseWebContentsAt(1, 0);

  EXPECT_THAT(GetEntriesForUrl("InitialWebUIPageLoad", url),
              Contains(HasMetric("Navigation.PageEndReason3")));
}

// Verify metrics are recorded separately without leaks for multiple rapid
// navigations.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyMultipleRapidNavigations) {
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  for (int i = 0; i < 3; ++i) {
    NavigateAndWaitForMetrics(url);
  }
  EXPECT_THAT(GetEntriesForUrl("InitialWebUIPageLoad", url), SizeIs(Ge(3u)));
}

// Verify combining lifecycle metrics for page that starts in background and
// moves to foreground.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyBackgroundToForegroundLifecycle) {
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  WebUIToolbarInitializer initializer(browser());
  content::WebContents* bg_contents = CreateNewContents(url, initializer);

  content::TestNavigationObserver navigation_observer{url};
  navigation_observer.WatchExistingWebContents();

  bg_contents->GetController().LoadURL(url, content::Referrer(),
                                       ui::PAGE_TRANSITION_LINK, std::string());
  browser()->tab_strip_model()->ActivateTabAt(1);
  navigation_observer.Wait();

  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);

  EXPECT_THAT(GetEntriesForUrl("InitialWebUIPageLoad", url),
              Contains(HasMetric("PageTiming.ForegroundDurationMs")));
}

// Verify cache status and failure metrics for cached page load with
// provisional load failure.
IN_PROC_BROWSER_TEST_F(
    InitialWebUIPageLoadMetricsObserverBrowserTestWithoutSyncNav,
    VerifyCachedFailedLoad) {
  GURL failed_url(chrome::kChromeUIWebUIToolbarURL);
  WebUIToolbarInitializer initializer(browser());
  content::WebContents* active_contents =
      CreateNewContents(failed_url, initializer);
  browser()->tab_strip_model()->ActivateTabAt(1);

  content::TestNavigationManager navigation_manager(active_contents,
                                                    failed_url);
  active_contents->GetController().LoadURL(
      failed_url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

  ASSERT_TRUE(navigation_manager.WaitForRequestStart());
  active_contents->Stop();
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  browser()->tab_strip_model()->CloseWebContentsAt(1, 0);

  EXPECT_THAT(GetEntriesForUrl("InitialWebUIPageLoad", failed_url),
              Contains(HasMetric("Net.ErrorCode.OnFailedProvisionalLoad")));
}

// Verify NavigationTiming for navigations with redirects.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyRedirectNavigation) {
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  NavigateAndWaitForMetrics(url);
  EXPECT_THAT(GetEntriesForUrl("InitialWebUINavigationTiming", url),
              ElementsAre(AllOf(HasMetric("FirstRequestStart"),
                                HasMetric("FirstResponseStart"),
                                HasMetric("NavigationCommitSent"),
                                HasMetric("NavigationCommitReceived"),
                                HasMetric("NavigationCommitReplySent"),
                                HasMetric("NavigationDidCommit"))));
}

// Verify metrics are recorded correctly for reload sessions.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyReloadSession) {
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  content::WebContents* contents =
      NavigateAndWaitForMetrics(url, /*close_tab=*/false);
  content::TestNavigationObserver reload_observer(contents);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  reload_observer.Wait();

  int index = browser()->tab_strip_model()->GetIndexOfWebContents(contents);
  browser()->tab_strip_model()->CloseWebContentsAt(index, 0);

  EXPECT_THAT(GetEntriesForUrl("InitialWebUIPageLoad", url), SizeIs(Ge(2u)));
}

// Verify multi-tab isolated browsing.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyMultiTabIsolation) {
  GURL webui_url(chrome::kChromeUIWebUIToolbarURL);
  NavigateAndWaitForMetrics(webui_url);

  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL web_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), web_url));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  auto webui_entries = GetEntriesForUrl("InitialWebUIPageLoad", webui_url);
  EXPECT_THAT(webui_entries, Not(IsEmpty()));

  EXPECT_THAT(GetEntriesForUrl("PageLoad", web_url), Not(IsEmpty()));
}
// Verify Window Metrics Manager paint notification and histograms using a new
// window.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyWindowMetricsManagerPaintHistograms) {
  base::HistogramTester histograms;

  // Create a new window
  Browser::CreateParams params(browser()->profile(), true);
  Browser* new_browser = Browser::Create(params);

  auto* manager = InitialWebUIWindowMetricsManager::From(new_browser);
  ASSERT_TRUE(manager);
  manager->SkipStartupForTesting();
  manager->SetWindowCreationInfo(
      waap::NewWindowCreationSource::kBrowserInitiated, base::TimeTicks::Now());

  // Define expected metrics
  const std::string first_paint_metric =
      "InitialWebUI.NewWindow.AllSources.WithExistingWindow."
      "ReloadButton.FirstPaint.FromConstructor";
  const std::string fcp_metric =
      "InitialWebUI.NewWindow.AllSources.WithExistingWindow."
      "ReloadButton.FirstContentfulPaint.FromConstructor";

  {
    base::StatisticsRecorder::HistogramWaiter first_paint_waiter(
        first_paint_metric);
    base::StatisticsRecorder::HistogramWaiter fcp_waiter(fcp_metric);

    // Navigate to WebUI URL in the new window.
    GURL url(chrome::kChromeUIWebUIToolbarURL);

    content::BrowserContext* browser_context = new_browser->profile();
    content::WebContents::CreateParams new_contents_params(
        browser_context,
        content::SiteInstance::CreateForURL(browser_context, url));
    std::unique_ptr<content::WebContents> new_web_contents(
        content::WebContents::Create(new_contents_params));

    WebUIToolbarInitializer initializer(new_browser);
    initializer.Watch(new_web_contents.get());

    InitializePageLoadMetricsForWebContents(new_web_contents.get());

    std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
        metrics_waiter =
            std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
                new_web_contents.get());
    metrics_waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kMonotonicFirstPaint);
    metrics_waiter->AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kMonotonicFirstContentfulPaint);

    content::TestNavigationObserver navigation_observer(url);
    navigation_observer.WatchExistingWebContents();

    content::WebContents* raw_contents = new_web_contents.get();
    new_browser->tab_strip_model()->AppendWebContents(
        std::move(new_web_contents), true);

    // Show the new browser window to trigger paint
    new_browser->window()->Show();

    raw_contents->GetController().LoadURL(
        url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
    navigation_observer.Wait();
    metrics_waiter->Wait();

    // Wait for presentation / paint feedback to be recorded in histograms
    first_paint_waiter.Wait();
    fcp_waiter.Wait();
  }

  histograms.ExpectTotalCount(first_paint_metric, 1);
  histograms.ExpectTotalCount(fcp_metric, 1);

  // Close the new browser window
  CloseBrowserSynchronously(new_browser);
}

// Verify App Enter Background Flush and PageEndReason.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyAppEnterBackgroundFlush) {
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  content::WebContents* contents =
      NavigateAndWaitForMetrics(url, /*close_tab=*/false);

  auto* observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(contents);
  ASSERT_TRUE(observer);

  // Flush metrics on app entering background
  observer->FlushMetricsOnAppEnterBackground();

  // Close the WebContents to finish recording and upload UKM
  int index = browser()->tab_strip_model()->GetIndexOfWebContents(contents);
  browser()->tab_strip_model()->CloseWebContentsAt(index, 0);

  // Verify that PageEndReason of END_APP_ENTER_BACKGROUND (value 6) is logged
  auto entries = GetEntriesForUrl("InitialWebUIPageLoad", url);
  EXPECT_THAT(
      entries,
      Contains(HasMetricWithValue(
          "Navigation.PageEndReason3",
          static_cast<int64_t>(
              page_load_metrics::PageEndReason::END_APP_ENTER_BACKGROUND))));
}

// Verify Fenced Frames and Prerender are ignored (return STOP_OBSERVING).
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyFencedFramesAndPrerenderIgnored) {
  InitialWebUIPageLoadMetricsObserver observer;

  // Verify OnFencedFramesStart returns STOP_OBSERVING
  EXPECT_EQ(observer.OnFencedFramesStart(nullptr, GURL()),
            page_load_metrics::PageLoadMetricsObserver::STOP_OBSERVING);

  // Verify OnPrerenderStart returns STOP_OBSERVING
  EXPECT_EQ(observer.OnPrerenderStart(nullptr, GURL()),
            page_load_metrics::PageLoadMetricsObserver::STOP_OBSERVING);
}

// Verify metrics logged when page is hidden and shown.
IN_PROC_BROWSER_TEST_F(InitialWebUIPageLoadMetricsObserverBrowserTest,
                       VerifyVisibilityMetrics) {
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  WebUIToolbarInitializer initializer(browser());
  content::WebContents* active_contents = CreateNewContents(url, initializer);
  browser()->tab_strip_model()->ActivateTabAt(1);

  content::TestNavigationObserver navigation_observer{url};
  navigation_observer.WatchExistingWebContents();

  active_contents->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  navigation_observer.Wait();

  // Initially, since it has not been hidden, PageTiming.ForegroundDuration
  // should not be recorded yet.
  auto entries = GetEntriesForUrl("InitialWebUIPageLoad", url);
  EXPECT_THAT(entries, Each(Not(HasMetric("PageTiming.ForegroundDurationMs"))));

  // Hide the page. This should trigger OnHidden and record metrics.
  active_contents->WasHidden();

  // Now it should have PageTiming.ForegroundDuration and
  // SiteInstanceRenderProcessAssignment.
  entries = GetEntriesForUrl("InitialWebUIPageLoad", url);
  EXPECT_THAT(entries, Contains(HasMetric("PageTiming.ForegroundDurationMs")));
  EXPECT_THAT(entries,
              Contains(HasMetric("SiteInstanceRenderProcessAssignment")));

  // It should also have recorded Navigation Timing.
  EXPECT_THAT(GetEntriesForUrl("InitialWebUINavigationTiming", url),
              Not(IsEmpty()));

  // Show the page again. This triggers OnShown.
  active_contents->WasShown();

  // Close the tab. This triggers OnComplete and RecordPageEndMetrics.
  // Since it is currently in foreground, the final foreground session will be
  // added to TotalForegroundDuration.
  int index =
      browser()->tab_strip_model()->GetIndexOfWebContents(active_contents);
  browser()->tab_strip_model()->CloseWebContentsAt(index, 0);

  entries = GetEntriesForUrl("InitialWebUIPageLoad", url);
  EXPECT_THAT(entries,
              Contains(HasMetric("PageTiming.TotalForegroundDurationMs")));
}
