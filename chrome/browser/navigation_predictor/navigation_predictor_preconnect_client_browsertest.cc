// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
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
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

class NavigationPredictorPreconnectClientBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest,
      public predictors::PreconnectManager::Observer {
 public:
  NavigationPredictorPreconnectClientBrowserTest()
      : subresource_filter::SubresourceFilterBrowserTest() {
    feature_list_.InitFromCommandLine(std::string(),
                                      "NavigationPredictorPreconnectHoldback");
  }

  void SetUp() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/navigation_predictor");
    ASSERT_TRUE(https_server_->Start());

    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    subresource_filter::SubresourceFilterBrowserTest::SetUpOnMainThread();
    host_resolver()->ClearRules();

    auto* loading_predictor =
        predictors::LoadingPredictorFactory::GetForProfile(
            browser()->profile());
    ASSERT_TRUE(loading_predictor);
    loading_predictor->preconnect_manager()->SetObserverForTesting(this);
  }

  const GURL GetTestURL(const char* file) const {
    return https_server_->GetURL(file);
  }

  void OnPreresolveFinished(const GURL& url, bool success) override {
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

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(NavigationPredictorPreconnectClientBrowserTest);
};

IN_PROC_BROWSER_TEST_F(NavigationPredictorPreconnectClientBrowserTest,
                       NoPreconnectSearch) {
  static const char kShortName[] = "test";
  static const char kSearchURL[] =
      "/anchors_different_area.html?q={searchTerms}";
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16(kShortName));
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());

  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);
  const GURL& url = GetTestURL("/anchors_different_area.html?q=cats");

  ui_test_utils::NavigateToURL(browser(), url);
  // There should be preconnect from navigation, but not preconnect client.
  EXPECT_EQ(1, preresolve_done_count_);
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorPreconnectClientBrowserTest,
                       PreconnectNotSearch) {
  const GURL& url = GetTestURL("/anchors_different_area.html");

  ui_test_utils::NavigateToURL(browser(), url);
  // There should be one preconnect from navigation and one from preconnect
  // client.
  WaitForPreresolveCount(2);
  EXPECT_EQ(2, preresolve_done_count_);
}

IN_PROC_BROWSER_TEST_F(NavigationPredictorPreconnectClientBrowserTest,
                       PreconnectNotSearchBackgroundForeground) {
  const GURL& url = GetTestURL("/anchors_different_area.html");

  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();

  ui_test_utils::NavigateToURL(browser(), url);

  // There should be a navigational preconnect.
  EXPECT_EQ(1, preresolve_done_count_);

  // Change to visible.
  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();

  // After showing the contents, there should be a preconnect client preconnect.
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
  ui_test_utils::NavigateToURL(browser(), url);

  WaitForPreresolveCount(3);
  EXPECT_EQ(3, preresolve_done_count_);

  // Expect another one.
  WaitForPreresolveCount(4);
  EXPECT_EQ(4, preresolve_done_count_);
}

class NavigationPredictorPreconnectClientBrowserTestWithHoldback
    : public NavigationPredictorPreconnectClientBrowserTest {
 public:
  NavigationPredictorPreconnectClientBrowserTestWithHoldback()
      : NavigationPredictorPreconnectClientBrowserTest() {
    feature_list_.InitFromCommandLine("NavigationPredictorPreconnectHoldback",
                                      std::string());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    NavigationPredictorPreconnectClientBrowserTestWithHoldback,
    NoPreconnectHoldback) {
  const GURL& url = GetTestURL("/anchors_different_area.html");

  ui_test_utils::NavigateToURL(browser(), url);
  // There should be preconnect from navigation, but not preconnect client.
  EXPECT_EQ(1, preresolve_done_count_);

  ui_test_utils::NavigateToURL(browser(), url);
  // There should be 2 preconnects from navigation, but not any from preconnect
  // client.
  EXPECT_EQ(2, preresolve_done_count_);
}

const base::Feature kPreconnectOnDidFinishNavigation{
    "PreconnectOnDidFinishNavigation", base::FEATURE_DISABLED_BY_DEFAULT};

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

  ui_test_utils::NavigateToURL(browser(), url);
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

  ui_test_utils::NavigateToURL(browser(), url);
  // There should be a navigation preconnect, a commit preconnect, and an OnLoad
  // preconnect.
  WaitForPreresolveCount(3);
  EXPECT_EQ(3, preresolve_done_count_);
}

}  // namespace
