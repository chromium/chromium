// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/optimization_guide_service_observer.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/test_component_creator.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {

// A test observer which can be configured to wait until the server hints are
// processed.
class TestOptimizationGuideServiceObserver
    : public optimization_guide::OptimizationGuideServiceObserver {
 public:
  TestOptimizationGuideServiceObserver()
      : run_loop_(std::make_unique<base::RunLoop>()) {}

  ~TestOptimizationGuideServiceObserver() override {}

  void WaitForNotification() {
    run_loop_->Run();
    run_loop_.reset(new base::RunLoop());
  }

 private:
  void OnHintsProcessed(
      const optimization_guide::proto::Configuration& config,
      const optimization_guide::ComponentInfo& component_info) override {
    run_loop_->Quit();
  }

  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestOptimizationGuideServiceObserver);
};

}  // namespace

class PreviewsBrowserTest : public InProcessBrowserTest {
 public:
  PreviewsBrowserTest() = default;

  ~PreviewsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    // Set up https server with resource monitor.
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    https_server_->RegisterRequestMonitor(base::BindRepeating(
        &PreviewsBrowserTest::MonitorResourceRequest, base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());

    https_url_ = https_server_->GetURL("/noscript_test.html");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    https_no_transform_url_ =
        https_server_->GetURL("/noscript_test_with_no_transform_header.html");
    ASSERT_TRUE(https_no_transform_url_.SchemeIs(url::kHttpsScheme));

    // Set up http server with resource monitor and redirect handler.
    http_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTP));
    http_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    http_server_->RegisterRequestMonitor(base::BindRepeating(
        &PreviewsBrowserTest::MonitorResourceRequest, base::Unretained(this)));
    http_server_->RegisterRequestHandler(base::BindRepeating(
        &PreviewsBrowserTest::HandleRedirectRequest, base::Unretained(this)));
    ASSERT_TRUE(http_server_->Start());

    http_url_ = http_server_->GetURL("/noscript_test.html");
    ASSERT_TRUE(http_url_.SchemeIs(url::kHttpScheme));

    redirect_url_ = http_server_->GetURL("/redirect.html");
    ASSERT_TRUE(redirect_url_.SchemeIs(url::kHttpScheme));
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");
    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
    cmd->AppendSwitchASCII("force-effective-connection-type", "Slow-2G");
  }

  const GURL& https_url() const { return https_url_; }
  const GURL& https_no_transform_url() const { return https_no_transform_url_; }
  const GURL& http_url() const { return http_url_; }
  const GURL& redirect_url() const { return redirect_url_; }
  bool noscript_css_requested() const {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return noscript_css_requested_;
  }
  bool noscript_js_requested() const {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return noscript_js_requested_;
  }

 private:
  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());
    EXPECT_TRUE(http_server_->ShutdownAndWaitUntilComplete());

    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Called by |https_server_|.
  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    // This method is called on embedded test server thread. Post the
    // information on UI thread.
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&PreviewsBrowserTest::MonitorResourceRequestOnUIThread,
                       base::Unretained(this), request));
    base::RunLoop().RunUntilIdle();
  }

  void MonitorResourceRequestOnUIThread(
      const net::test_server::HttpRequest& request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (request.GetURL().spec().find("noscript_test.css") !=
        std::string::npos) {
      noscript_css_requested_ = true;
    }
    if (request.GetURL().spec().find("noscript_test.js") != std::string::npos) {
      noscript_js_requested_ = true;
    }
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRedirectRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response;
    if (request.GetURL().spec().find("redirect") != std::string::npos) {
      response.reset(new net::test_server::BasicHttpResponse);
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", https_url().spec());
    }
    return std::move(response);
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  GURL https_url_;
  GURL https_no_transform_url_;
  GURL http_url_;
  GURL redirect_url_;

  // Should be accessed only on UI thread.
  bool noscript_css_requested_ = false;
  bool noscript_js_requested_ = false;

  DISALLOW_COPY_AND_ASSIGN(PreviewsBrowserTest);
};

// Loads a webpage that has both script and noscript tags and also requests
// a script resource. Verifies that the noscript tag is not evaluated and the
// script resource is loaded.
IN_PROC_BROWSER_TEST_F(PreviewsBrowserTest, NoScriptPreviewsDisabled) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), https_url());

  // Verify loaded js resource but not css triggered by noscript tag.
  EXPECT_TRUE(noscript_js_requested());
  EXPECT_FALSE(noscript_css_requested());

  // Verify info bar not presented via histogram check.
  histogram_tester.ExpectTotalCount("Previews.InfoBarAction.NoScript", 0);
}

// This test class enables NoScriptPreviews but without OptimizationHints.
class PreviewsNoScriptBrowserTest : public PreviewsBrowserTest {
 public:
  PreviewsNoScriptBrowserTest() {}

  ~PreviewsNoScriptBrowserTest() override {}

  void SetUp() override {
    // Explicitly disable server hints.
    scoped_feature_list_.InitWithFeatures(
        {previews::features::kPreviews, previews::features::kNoScriptPreviews},
        {previews::features::kOptimizationHints});
    PreviewsBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Previews InfoBar (which these tests triggers) does not work on Mac.
// See https://crbug.com/782322 for detail.
// Also occasional flakes on win7 (https://crbug.com/789542).
#if defined(OS_ANDROID) || defined(OS_LINUX)
#define MAYBE_NoScriptPreviewsEnabled NoScriptPreviewsEnabled
#define MAYBE_NoScriptPreviewsEnabledHttpRedirectToHttps \
  NoScriptPreviewsEnabledHttpRedirectToHttps
#else
#define MAYBE_NoScriptPreviewsEnabled DISABLED_NoScriptPreviewsEnabled
#define MAYBE_NoScriptPreviewsEnabledHttpRedirectToHttps \
  DISABLED_NoScriptPreviewsEnabledHttpRedirectToHttps
#endif

// Loads a webpage that has both script and noscript tags and also requests
// a script resource. Verifies that the noscript tag is evaluated and the
// script resource is not loaded.
IN_PROC_BROWSER_TEST_F(PreviewsNoScriptBrowserTest,
                       MAYBE_NoScriptPreviewsEnabled) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), https_url());

  // Verify loaded noscript tag triggered css resource but not js one.
  EXPECT_TRUE(noscript_css_requested());
  EXPECT_FALSE(noscript_js_requested());

  // Verify info bar presented via histogram check.
  histogram_tester.ExpectUniqueSample("Previews.InfoBarAction.NoScript", 0, 1);
}

IN_PROC_BROWSER_TEST_F(PreviewsNoScriptBrowserTest,
                       NoScriptPreviewsEnabledButHttpRequest) {
  ui_test_utils::NavigateToURL(browser(), http_url());

  // Verify loaded js resource but not css triggered by noscript tag.
  EXPECT_TRUE(noscript_js_requested());
  EXPECT_FALSE(noscript_css_requested());
}

// Flaky in all platforms except Android. See https://crbug.com/803626 for
// detail.
#if defined(OS_ANDROID) || defined(OS_LINUX)
#define MAYBE_NoScriptPreviewsEnabledButNoTransformDirective \
  NoScriptPreviewsEnabledButNoTransformDirective
#else
#define MAYBE_NoScriptPreviewsEnabledButNoTransformDirective \
  DISABLED_NoScriptPreviewsEnabledButNoTransformDirective
#endif
IN_PROC_BROWSER_TEST_F(PreviewsNoScriptBrowserTest,
                       MAYBE_NoScriptPreviewsEnabledButNoTransformDirective) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), https_no_transform_url());

  // Verify loaded js resource but not css triggered by noscript tag.
  EXPECT_TRUE(noscript_js_requested());
  EXPECT_FALSE(noscript_css_requested());

  histogram_tester.ExpectUniqueSample(
      "Previews.CacheControlNoTransform.BlockedPreview", 5 /* NoScript */, 1);
}

IN_PROC_BROWSER_TEST_F(PreviewsNoScriptBrowserTest,
                       MAYBE_NoScriptPreviewsEnabledHttpRedirectToHttps) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), redirect_url());

  // Verify loaded noscript tag triggered css resource but not js one.
  EXPECT_TRUE(noscript_css_requested());
  EXPECT_FALSE(noscript_js_requested());

  // Verify info bar presented via histogram check.
  histogram_tester.ExpectUniqueSample("Previews.InfoBarAction.NoScript", 0, 1);
}

// This test class enables NoScriptPreviews with OptimizationHints.
class PreviewsOptimizationGuideBrowserTest : public PreviewsBrowserTest {
 public:
  PreviewsOptimizationGuideBrowserTest() {}

  ~PreviewsOptimizationGuideBrowserTest() override {}

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {previews::features::kPreviews, previews::features::kOptimizationHints,
         previews::features::kNoScriptPreviews},
        {});
    PreviewsBrowserTest::SetUp();
  }

  void SetNoScriptWhitelist(
      std::vector<std::string> whitelisted_noscript_sites) {
    const optimization_guide::ComponentInfo& component_info =
        test_component_creator_.CreateComponentInfoWithTopLevelWhitelist(
            optimization_guide::proto::NOSCRIPT, whitelisted_noscript_sites);
    g_browser_process->optimization_guide_service()->ProcessHints(
        component_info);

    // Wait for hints to be processed by PreviewsOptimizationGuide.
    base::RunLoop().RunUntilIdle();
  }

  void AddTestOptimizationGuideServiceObserver(
      TestOptimizationGuideServiceObserver* observer) {
    g_browser_process->optimization_guide_service()->AddObserver(observer);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  optimization_guide::testing::TestComponentCreator test_component_creator_;
  DISALLOW_COPY_AND_ASSIGN(PreviewsOptimizationGuideBrowserTest);
};

// Previews InfoBar (which this test triggers) does not work on Mac.
// See https://crbug.com/782322 for detail.
// Also occasional flakes on win7 (https://crbug.com/789948) and Ubuntu 16.04
// (https://crbug.com/831838)
#if defined(OS_ANDROID)
#define MAYBE_NoScriptPreviewsEnabledByWhitelist \
  NoScriptPreviewsEnabledByWhitelist
#else
#define MAYBE_NoScriptPreviewsEnabledByWhitelist \
  DISABLED_NoScriptPreviewsEnabledByWhitelist
#endif

IN_PROC_BROWSER_TEST_F(PreviewsOptimizationGuideBrowserTest,
                       MAYBE_NoScriptPreviewsEnabledByWhitelist) {
  TestOptimizationGuideServiceObserver observer;
  AddTestOptimizationGuideServiceObserver(&observer);
  base::RunLoop().RunUntilIdle();

  // Whitelist test URL for NoScript.
  SetNoScriptWhitelist({https_url().host()});
  observer.WaitForNotification();

  ui_test_utils::NavigateToURL(browser(), https_url());

  // Verify loaded noscript tag triggered css resource but not js one.
  EXPECT_TRUE(noscript_css_requested());
  EXPECT_FALSE(noscript_js_requested());
}

IN_PROC_BROWSER_TEST_F(PreviewsOptimizationGuideBrowserTest,
                       NoScriptPreviewsNotEnabledByWhitelist) {
  TestOptimizationGuideServiceObserver observer;
  AddTestOptimizationGuideServiceObserver(&observer);
  base::RunLoop().RunUntilIdle();

  // Whitelist random site for NoScript.
  SetNoScriptWhitelist({"foo.com"});
  observer.WaitForNotification();

  ui_test_utils::NavigateToURL(browser(), https_url());

  // Verify loaded js resource but not css triggered by noscript tag.
  EXPECT_TRUE(noscript_js_requested());
  EXPECT_FALSE(noscript_css_requested());
}
