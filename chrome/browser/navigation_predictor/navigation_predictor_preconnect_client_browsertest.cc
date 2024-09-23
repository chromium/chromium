// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor_preconnect_client.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_features.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace {

class NavigationPredictorPreconnectClientBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest,
      public predictors::PreconnectManager::Observer {
 public:
  NavigationPredictorPreconnectClientBrowserTest()
      : subresource_filter::SubresourceFilterBrowserTest() {
    feature_list_.InitFromCommandLine(
        std::string(),
        "NavigationPredictorPreconnectHoldback,PreconnectToSearch");
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  NavigationPredictorPreconnectClientBrowserTest(
      const NavigationPredictorPreconnectClientBrowserTest&) = delete;
  NavigationPredictorPreconnectClientBrowserTest& operator=(
      const NavigationPredictorPreconnectClientBrowserTest&) = delete;

  void SetUp() override {
    https_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/navigation_predictor");
    ASSERT_TRUE(https_server_->Start());

    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    subresource_filter::SubresourceFilterBrowserTest::SetUpOnMainThread();
    host_resolver()->ClearRules();
    NavigationPredictorPreconnectClient::EnablePreconnectsForLocalIPsForTesting(
        true);

    auto* loading_predictor =
        predictors::LoadingPredictorFactory::GetForProfile(
            browser()->profile());
    ASSERT_TRUE(loading_predictor);
    loading_predictor->preconnect_manager()->SetObserverForTesting(this);
  }

  const GURL GetTestURL(const char* file) const {
    return https_server_->GetURL(file);
  }

  void OnPreresolveFinished(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      bool success) override {
    // The tests do not care about preresolves to non-test server (e.g., hard
    // coded preconnects to google.com).
    if (url::Origin::Create(url) !=
        url::Origin::Create(https_server_->base_url())) {
      return;
    }
    EXPECT_TRUE(success);
    preresolve_done_count_++;
    if (run_loop_)
      run_loop_->Quit();
  }

  void WaitForPreresolveCount(int expected_count) {
    while (preresolve_done_count_ < expected_count) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }
  }

 protected:
  int preresolve_done_count_ = 0;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

IN_PROC_BROWSER_TEST_F(NavigationPredictorPreconnectClientBrowserTest,
                       NoPreconnectSearch) {
  static const char16_t kShortName[] = u"test";
  static const char kSearchURL[] =
      "/anchors_different_area.html?q={searchTerms}";
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(kShortName);
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());

  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);
  const GURL& url = GetTestURL("/anchors_different_area.html?q=cats");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // There should be preconnect from navigation, but not preconnect client.
  EXPECT_EQ(1, preresolve_done_count_);
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorPreconnectClientBrowserTest,
                       PreconnectNotSearch) {
  base::HistogramTester histogram_tester;
  const GURL& url = GetTestURL("/anchors_different_area.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // There should be one preconnect from navigation and one from preconnect
  // client.
  WaitForPreresolveCount(2);
  EXPECT_EQ(2, preresolve_done_count_);
  histogram_tester.ExpectUniqueSample("NavigationPredictor.IsPubliclyRoutable",
                                      true, 1);
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorPreconnectClientBrowserTest,
                       PreconnectNotSearchBackgroundForeground) {
  const GURL& url = GetTestURL("/anchors_different_area.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // There should be one preconnect from navigation and one from preconnect
  // client.
  WaitForPreresolveCount(2);
  EXPECT_EQ(2, preresolve_done_count_);

  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();

  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();

  // After showing the contents again, there should be another preconnect client
  // preconnect.
  WaitForPreresolveCount(3);
  EXPECT_EQ(3, preresolve_done_count_);
}

class NavigationPredictorPreconnectClientBrowserTestWithUnusedIdleSocketTimeout
    : public NavigationPredictorPreconnectClientBrowserTest {
 public:
  NavigationPredictorPreconnectClientBrowserTestWithUnusedIdleSocketTimeout()
      : NavigationPredictorPreconnectClientBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kNetUnusedIdleSocketTimeout,
        {{"unused_idle_socket_timeout_seconds", "0"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that we preconnect after the last preconnect timed out.
IN_PROC_BROWSER_TEST_F(
    NavigationPredictorPreconnectClientBrowserTestWithUnusedIdleSocketTimeout,
    ActionAccuracy_timeout) {
  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  WaitForPreresolveCount(3);
  EXPECT_LE(3, preresolve_done_count_);

  // Expect another one.
  WaitForPreresolveCount(4);
  EXPECT_LE(4, preresolve_done_count_);
}

// Test that we preconnect after the last preconnect timed out.
IN_PROC_BROWSER_TEST_F(
    NavigationPredictorPreconnectClientBrowserTestWithUnusedIdleSocketTimeout,
    CappedAtFiveAttempts) {
  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Expect 1 navigation preresolve and 5 repeated onLoad calls.
  WaitForPreresolveCount(6);
  EXPECT_EQ(6, preresolve_done_count_);

  // We should not see additional preresolves.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();

  EXPECT_EQ(6, preresolve_done_count_);

  // By default, same document navigation should not trigger new preconnects.
  const GURL& same_document_url =
      GetTestURL("/page_with_same_host_anchor_element.html#foobar");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_document_url));
  // Expect another one.
  WaitForPreresolveCount(6);
  EXPECT_EQ(6, preresolve_done_count_);
}

class NavigationPredictorPreconnectClientBrowserTestWithHoldback
    : public NavigationPredictorPreconnectClientBrowserTest {
 public:
  NavigationPredictorPreconnectClientBrowserTestWithHoldback()
      : NavigationPredictorPreconnectClientBrowserTest() {
    feature_list_.InitFromCommandLine("NavigationPredictorPreconnectHoldback",
                                      "PreconnectToSearch");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    NavigationPredictorPreconnectClientBrowserTestWithHoldback,
    NoPreconnectHoldback) {
  const GURL& url = GetTestURL("/anchors_different_area.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // There should be preconnect from navigation, but not preconnect client.
  EXPECT_EQ(1, preresolve_done_count_);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // There should be 2 preconnects from navigation, but not any from preconnect
  // client.
  EXPECT_EQ(2, preresolve_done_count_);
}

BASE_FEATURE(kPreconnectOnDidFinishNavigation,
             "PreconnectOnDidFinishNavigation",
             base::FEATURE_DISABLED_BY_DEFAULT);

class
    NavigationPredictorPreconnectClientBrowserTestPreconnectOnDidFinishNavigationSecondDelay
    : public NavigationPredictorPreconnectClientBrowserTest {
 public:
  NavigationPredictorPreconnectClientBrowserTestPreconnectOnDidFinishNavigationSecondDelay()
      : NavigationPredictorPreconnectClientBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        kPreconnectOnDidFinishNavigation,
        {{"delay_after_commit_in_ms", "1000"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    NavigationPredictorPreconnectClientBrowserTestPreconnectOnDidFinishNavigationSecondDelay,
    PreconnectNotSearch) {
  const GURL& url = GetTestURL("/anchors_different_area.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // There should be preconnect from navigation and one from OnLoad client.
  WaitForPreresolveCount(2);
  EXPECT_EQ(2, preresolve_done_count_);
}

class
    NavigationPredictorPreconnectClientBrowserTestPreconnectOnDidFinishNavigationNoDelay
    : public NavigationPredictorPreconnectClientBrowserTest {
 public:
  NavigationPredictorPreconnectClientBrowserTestPreconnectOnDidFinishNavigationNoDelay()
      : NavigationPredictorPreconnectClientBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        kPreconnectOnDidFinishNavigation, {{"delay_after_commit_in_ms", "0"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    NavigationPredictorPreconnectClientBrowserTestPreconnectOnDidFinishNavigationNoDelay,
    PreconnectNotSearch) {
  const GURL& url = GetTestURL("/anchors_different_area.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // There should be a navigation preconnect, a commit preconnect, and an OnLoad
  // preconnect.
  WaitForPreresolveCount(3);
  EXPECT_EQ(3, preresolve_done_count_);
}

class NavigationPredictorSameDocumentPreconnectClientBrowserTest
    : public NavigationPredictorPreconnectClientBrowserTest {
 public:
  NavigationPredictorSameDocumentPreconnectClientBrowserTest()
      : NavigationPredictorPreconnectClientBrowserTest() {
    // Configure kDelayRequestsOnMultiplexedConnections experiment params.
    base::FieldTrialParams params_kNetUnusedIdleSocketTimeout;
    params_kNetUnusedIdleSocketTimeout["unused_idle_socket_timeout_seconds"] =
        "0";

    // Configure kThrottleDelayable experiment params.
    base::FieldTrialParams
        params_kNavigationPredictorEnablePreconnectOnSameDocumentNavigations;
    feature_list_.InitWithFeaturesAndParameters(
        {{net::features::kNetUnusedIdleSocketTimeout,
          params_kNetUnusedIdleSocketTimeout},
         {features::
              kNavigationPredictorEnablePreconnectOnSameDocumentNavigations,
          params_kNavigationPredictorEnablePreconnectOnSameDocumentNavigations}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that we preconnect after the last preconnect timed out.
IN_PROC_BROWSER_TEST_F(
    NavigationPredictorSameDocumentPreconnectClientBrowserTest,
    SameDocumentNavigation) {
  const GURL& url = GetTestURL("/page_with_same_host_anchor_element.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  WaitForPreresolveCount(3);
  EXPECT_LE(3, preresolve_done_count_);

  // Expect another one.
  WaitForPreresolveCount(4);
  EXPECT_LE(4, preresolve_done_count_);

  const GURL& same_document_url =
      GetTestURL("/page_with_same_host_anchor_element.html#foobar");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_document_url));
  // Expect another one.
  WaitForPreresolveCount(8);
  EXPECT_LE(8, preresolve_done_count_);
}

namespace {
// Feature to control preconnect to search.
BASE_FEATURE(kPreconnectToSearchTest,
             "PreconnectToSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Feature to control preconnecting with privacy mode enabled.
BASE_FEATURE(kPreconnectToSearchWithPrivacyModeEnabledTest,
             "PreconnectToSearchWithPrivacyModeEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace

class NavigationPredictorPreconnectClientBrowserTestWithSearch
    : public NavigationPredictorPreconnectClientBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  NavigationPredictorPreconnectClientBrowserTestWithSearch()
      : NavigationPredictorPreconnectClientBrowserTest() {
    if (PreconnectWithPrivacyModeEnabled()) {
      feature_list_.InitWithFeatures(
          {kPreconnectToSearchTest,
           kPreconnectToSearchWithPrivacyModeEnabledTest},
          {});
    } else {
      feature_list_.InitWithFeatures(
          {kPreconnectToSearchTest},
          {kPreconnectToSearchWithPrivacyModeEnabledTest});
    }
  }

  bool PreconnectWithPrivacyModeEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    NavigationPredictorPreconnectClientBrowserTestWithSearch,
    testing::Bool());

#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_PreconnectSearchWithFeature DISABLED_PreconnectSearchWithFeature
#else
#define MAYBE_PreconnectSearchWithFeature PreconnectSearchWithFeature
#endif
IN_PROC_BROWSER_TEST_P(NavigationPredictorPreconnectClientBrowserTestWithSearch,
                       MAYBE_PreconnectSearchWithFeature) {
  static const char16_t kShortName[] = u"test";
  static const char kSearchURL[] =
      "/anchors_different_area.html?q={searchTerms}";
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(kShortName);
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());
  data.preconnect_to_search_url = true;

  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);
  const GURL& url = GetTestURL("/anchors_different_area.html?q=cats");

  NavigationPredictorKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()->profile()))
      ->search_engine_preconnector()
      ->StartPreconnecting(/*with_startup_delay=*/false);

  if (PreconnectWithPrivacyModeEnabled()) {
    // There should be 2 DSE preconnects (2 NAKs).
    WaitForPreresolveCount(2);
    EXPECT_EQ(2, preresolve_done_count_);

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    // Now there should be an onload preconnect as well as a navigation
    // preconnect.
    WaitForPreresolveCount(4);
    EXPECT_EQ(4, preresolve_done_count_);
  } else {
    // There should be a DSE preconnect.
    WaitForPreresolveCount(1);
    EXPECT_EQ(1, preresolve_done_count_);

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    // Now there should be an onload preconnect as well as a navigation
    // preconnect.
    WaitForPreresolveCount(3);
    EXPECT_EQ(3, preresolve_done_count_);
  }
}

class NavigationPredictorPreconnectClientLocalURLBrowserTest
    : public NavigationPredictorPreconnectClientBrowserTest {
 public:
  NavigationPredictorPreconnectClientLocalURLBrowserTest() = default;

  NavigationPredictorPreconnectClientLocalURLBrowserTest(
      const NavigationPredictorPreconnectClientLocalURLBrowserTest&) = delete;
  NavigationPredictorPreconnectClientLocalURLBrowserTest& operator=(
      const NavigationPredictorPreconnectClientLocalURLBrowserTest&) = delete;

 private:
  void SetUpOnMainThread() override {
    NavigationPredictorPreconnectClientBrowserTest::SetUpOnMainThread();
    NavigationPredictorPreconnectClient::EnablePreconnectsForLocalIPsForTesting(
        false);
  }
};

IN_PROC_BROWSER_TEST_F(NavigationPredictorPreconnectClientLocalURLBrowserTest,
                       NoPreconnectSearch) {
  base::HistogramTester histogram_tester;
  const GURL& url = GetTestURL("/anchors_different_area.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // There should not be any preconnects to non-public addresses.
  histogram_tester.ExpectUniqueSample("NavigationPredictor.IsPubliclyRoutable",
                                      false, 1);
}

class NavigationPredictorPreconnectClientPrerenderBrowserTestNoDelay
    : public NavigationPredictorPreconnectClientBrowserTestPreconnectOnDidFinishNavigationNoDelay {
 public:
  NavigationPredictorPreconnectClientPrerenderBrowserTestNoDelay()
      : prerender_test_helper_(base::BindRepeating(
            &NavigationPredictorPreconnectClientPrerenderBrowserTestNoDelay::
                GetWebContents,
            base::Unretained(this))) {}
  ~NavigationPredictorPreconnectClientPrerenderBrowserTestNoDelay() override =
      default;
  NavigationPredictorPreconnectClientPrerenderBrowserTestNoDelay(
      const NavigationPredictorPreconnectClientPrerenderBrowserTestNoDelay&) =
      delete;

  NavigationPredictorPreconnectClientPrerenderBrowserTestNoDelay& operator=(
      const NavigationPredictorPreconnectClientPrerenderBrowserTestNoDelay&) =
      delete;

  void SetUp() override {
    prerender_test_helper_.RegisterServerRequestMonitor(https_server_.get());
    NavigationPredictorPreconnectClientBrowserTestPreconnectOnDidFinishNavigationNoDelay::
        SetUp();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_test_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_test_helper_;
};

IN_PROC_BROWSER_TEST_F(
    NavigationPredictorPreconnectClientPrerenderBrowserTestNoDelay,
    NoAdditionalPreresolves) {
  const GURL& url = GetTestURL("/anchors_different_area.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Start prerendering. The NavigationPredictorClient should ignore
  // non-primary page navigations.
  content::FrameTreeNodeId host_id = prerender_test_helper().AddPrerender(url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());

  // There should be a navigation preconnect, a commit preconnect, and an OnLoad
  // preconnect, none from Prerenders.
  WaitForPreresolveCount(3);
  EXPECT_EQ(3, preresolve_done_count_);
}

class NavigationPredictorPreconnectClientFencedFrameBrowserTest
    : public NavigationPredictorPreconnectClientBrowserTest {
 public:
  NavigationPredictorPreconnectClientFencedFrameBrowserTest() = default;
  ~NavigationPredictorPreconnectClientFencedFrameBrowserTest() override =
      default;
  NavigationPredictorPreconnectClientFencedFrameBrowserTest(
      const NavigationPredictorPreconnectClientFencedFrameBrowserTest&) =
      delete;

  NavigationPredictorPreconnectClientFencedFrameBrowserTest& operator=(
      const NavigationPredictorPreconnectClientFencedFrameBrowserTest&) =
      delete;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(
    NavigationPredictorPreconnectClientFencedFrameBrowserTest,
    FencedFrameDoesNotCountIsPubliclyRoutable) {
  base::HistogramTester histogram_tester;
  const GURL& url = GetTestURL("/anchors_different_area.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // There should be one preconnect from navigation and one from preconnect
  // client.
  WaitForPreresolveCount(2);
  EXPECT_EQ(2, preresolve_done_count_);
  histogram_tester.ExpectTotalCount("NavigationPredictor.IsPubliclyRoutable",
                                    1);

  // Create a fenced frame.
  const GURL& fenced_frame_url =
      GetTestURL("/fenced_frames/anchors_different_area.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  // The count should not increase in DidFinishLoad method.
  histogram_tester.ExpectTotalCount("NavigationPredictor.IsPubliclyRoutable",
                                    1);

  fenced_frame_test_helper().NavigateFrameInFencedFrameTree(fenced_frame_host,
                                                            fenced_frame_url);
  // Histogram count should not increase after navigating the fenced frame.
  histogram_tester.ExpectTotalCount("NavigationPredictor.IsPubliclyRoutable",
                                    1);
}

}  // namespace
