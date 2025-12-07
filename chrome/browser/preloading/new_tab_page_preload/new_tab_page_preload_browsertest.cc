// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline_manager.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prefetch_test_util.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
#include "ui/base/device_form_factor.h"
#endif

namespace {

// Following definitions are equal to `content::PrerenderFinalStatus`.
constexpr int kFinalStatusActivated = 0;
constexpr int kPrerenderFailedDuringPrefetch = 86;

// Following definitions are equal to `content::PrefetchStatus`.
constexpr int kPrefetchFailedNon2XX = 12;
constexpr int kPrefetchResponseUsed = 42;

class NewTabPagePreloadBrowserTest : public PlatformBrowserTest {
 public:
  NewTabPagePreloadBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &NewTabPagePreloadBrowserTest::GetActiveWebContents,
            base::Unretained(this))) {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kNewTabPageTriggerForPrefetch);
    prerender_helper_.RegisterServerRequestMonitor(
        embedded_https_test_server());
    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void StartServer() {
    embedded_https_test_server().SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    embedded_https_test_server().ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    EXPECT_TRUE(embedded_https_test_server().Start());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_https_test_server().ShutdownAndWaitUntilComplete());
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  GURL GetUrl(const std::string& path) {
    return embedded_https_test_server().GetURL("a.test", path);
  }

  NewTabPagePreloadPipelineManager* GetNewTabPagePreloadPipelineManager() {
    return browser()
        ->tab_strip_model()
        ->GetActiveTab()
        ->GetTabFeatures()
        ->new_tab_page_preload_pipeline_manager();
  }

  void SimulateNewTabNavigation(const GURL& url) {
    GetActiveWebContents()->OpenURL(
        content::OpenURLParams(
            url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
            ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_BOOKMARK),
            /*is_renderer_initiated=*/false),
        base::BindRepeating(&page_load_metrics::NavigationHandleUserData::
                                AttachNewTabPageNavigationHandleUserData));
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(NewTabPagePreloadBrowserTest,
                       SuccessfulPrefetchAndSuccessfulPrerender) {
  base::HistogramTester histogram_tester;
  StartServer();

  // Navigate to an initial page.
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), GetUrl("/empty.html")));

  GURL preload_url = GetUrl("/simple.html");
  {
    content::test::TestPrefetchWatcher test_prefetch_watcher;
    GetNewTabPagePreloadPipelineManager()->StartPrefetch(preload_url);
    test_prefetch_watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                                             preload_url);
    GetNewTabPagePreloadPipelineManager()->StartPrerender(
        preload_url, chrome_preloading_predictor::kPointerDownOnNewTabPage);
  }

  // Activate.
  content::TestActivationManager activation_manager(GetActiveWebContents(),
                                                    preload_url);
  SimulateNewTabNavigation(preload_url);
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  histogram_tester.ExpectUniqueSample("Preloading.Prefetch.PrefetchStatus",
                                      kPrefetchResponseUsed, 1);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_NewTabPage",
      kFinalStatusActivated, 1);
}

IN_PROC_BROWSER_TEST_F(NewTabPagePreloadBrowserTest,
                       PrefetchResponseFailureAndPrerenderFailure) {
  base::HistogramTester histogram_tester;
  net::test_server::ControllableHttpResponse prefetch_response(
      &embedded_https_test_server(), "/simple.html");

  StartServer();
  // Navigate to an initial page.
  GURL preload_url = GetUrl("/simple.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), GetUrl("/empty.html")));

  GetNewTabPagePreloadPipelineManager()->StartPrefetch(preload_url);
  prefetch_response.WaitForRequest();
  prefetch_response.Send(net::HTTP_INTERNAL_SERVER_ERROR);
  prefetch_response.Done();

  GetNewTabPagePreloadPipelineManager()->StartPrerender(
      preload_url, chrome_preloading_predictor::kPointerDownOnNewTabPage);

  content::TestNavigationObserver navigation_observer(GetActiveWebContents());
  SimulateNewTabNavigation(preload_url);
  navigation_observer.WaitForNavigationFinished();

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrefetchAheadOfPrerenderFailed.PrefetchStatus."
      "Embedder_NewTabPage",
      kPrefetchFailedNon2XX, 1);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_NewTabPage",
      kPrerenderFailedDuringPrefetch, 1);
}

}  // namespace
