// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/previews/previews_test_util.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/hints_component_util.h"
#include "components/optimization_guide/optimization_guide_constants.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/test_hints_component_creator.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/network_quality_tracker.h"

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

    https_hint_setup_url_ = https_server_->GetURL("/hint_setup.html");
    ASSERT_TRUE(https_hint_setup_url_.SchemeIs(url::kHttpsScheme));
    ASSERT_EQ(https_hint_setup_url_.host(), https_url_.host());

    // Set up http server with resource monitor and redirect handler.
    http_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
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

    http_hint_setup_url_ = http_server_->GetURL("/hint_setup.html");
    ASSERT_TRUE(http_hint_setup_url_.SchemeIs(url::kHttpScheme));
    ASSERT_EQ(http_hint_setup_url_.host(), http_url_.host());
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");
    // Due to race conditions, it's possible that blocklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlocklist);
  }

  const GURL& https_url() const { return https_url_; }
  const GURL& https_no_transform_url() const { return https_no_transform_url_; }
  const GURL& https_hint_setup_url() const { return https_hint_setup_url_; }
  const GURL& http_url() const { return http_url_; }
  const GURL& redirect_url() const { return redirect_url_; }
  const GURL& http_hint_setup_url() const { return http_hint_setup_url_; }

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
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&PreviewsBrowserTest::MonitorResourceRequestOnUIThread,
                       base::Unretained(this), request));
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
  GURL https_hint_setup_url_;
  GURL http_url_;
  GURL redirect_url_;
  GURL http_hint_setup_url_;

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
  histogram_tester.ExpectTotalCount("Previews.PreviewShown.NoScript", 0);
}

// This test class enables NoScriptPreviews and with OptimizationHints.
class PreviewsNoScriptBrowserTest : public ::testing::WithParamInterface<bool>,
                                    public PreviewsBrowserTest {
 public:
  PreviewsNoScriptBrowserTest() {}

  ~PreviewsNoScriptBrowserTest() override {}

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {previews::features::kPreviews,
             {{"override_should_show_preview_check",
               GetParam() ? "true" : "false"}}},
            {optimization_guide::features::kOptimizationHints, {}},
            {previews::features::kNoScriptPreviews, {}},
        },
        {});
    PreviewsBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->OverrideTargetDecisionForTesting(
            optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
            optimization_guide::OptimizationGuideDecision::kTrue);
    PreviewsBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");
    cmd->AppendSwitch("optimization-guide-disable-installer");
    cmd->AppendSwitch("purge_hint_cache_store");
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlocklist);
  }

  // Creates hint data for the |hint_setup_url|'s host and then performs a
  // navigation to |hint_setup_url| to trigger the hints to be loaded into the
  // hint cache so they will be available for a subsequent navigation to a test
  // url to the same host.
  void SetUpNoScriptWhitelist(const GURL& hint_setup_url) {
    const optimization_guide::HintsComponentInfo& component_info =
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::NOSCRIPT, {hint_setup_url.host()}, "*",
            {});

    base::HistogramTester histogram_tester;

    g_browser_process->optimization_guide_service()->MaybeUpdateHintsComponent(
        component_info);

    RetryForHistogramUntilCountReached(
        &histogram_tester,
        optimization_guide::kComponentHintsUpdatedResultHistogramString, 1);

    // Navigate to |hint_setup_url| to prime the OptimizationGuide hints for the
    // url's host and ensure that they have been loaded from the store (via
    // histogram) prior to the navigation that tests functionality.
    ui_test_utils::NavigateToURL(browser(), hint_setup_url);

    RetryForHistogramUntilCountReached(
        &histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        1);
  }

  // Returns whether the ShouldShowPreview check should have been overridden for
  // the test case.
  bool ShouldOverrideShouldShowPreviewCheck() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  optimization_guide::testing::TestHintsComponentCreator
      test_hints_component_creator_;
};

INSTANTIATE_TEST_SUITE_P(ShouldSkipPreview,
                         PreviewsNoScriptBrowserTest,
                         ::testing::Bool());




IN_PROC_BROWSER_TEST_P(PreviewsNoScriptBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(
                           NoScriptPreviewsEnabledButNoTransformDirective)) {
  GURL url = https_no_transform_url();

  // Whitelist NoScript for https_hint_setup_url()'s' host.
  SetUpNoScriptWhitelist(https_hint_setup_url());

  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), url);

  // Verify loaded js resource but not css triggered by noscript tag.
  EXPECT_TRUE(noscript_js_requested());
  EXPECT_FALSE(noscript_css_requested());

  histogram_tester.ExpectUniqueSample(
      "Previews.CacheControlNoTransform.BlockedPreview", 5 /* NoScript */, 1);
}



IN_PROC_BROWSER_TEST_P(
    PreviewsNoScriptBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(NoScriptPreviewsNotEnabledByWhitelist)) {
  GURL url = https_url();

  // Whitelist random site for NoScript.
  SetUpNoScriptWhitelist(GURL("https://foo.com"));

  ui_test_utils::NavigateToURL(browser(), url);

  // Verify loaded js resource but not css triggered by noscript tag.
  EXPECT_TRUE(noscript_js_requested());
  EXPECT_FALSE(noscript_css_requested());
}

IN_PROC_BROWSER_TEST_P(PreviewsNoScriptBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(
                           NoScriptPreviewsEnabledShouldSkipPreviewCheck)) {
  // Override the decision to |kFalse| so that the Preview should not be shown
  // in the regular case.
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetDecisionForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
          optimization_guide::OptimizationGuideDecision::kFalse);

  GURL url = https_url();

  // Whitelist NoScript for https_hint_setup_url()'s' host.
  SetUpNoScriptWhitelist(https_hint_setup_url());

  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), url);

  if (ShouldOverrideShouldShowPreviewCheck()) {
    // Verify loaded noscript tag triggered css resource but not js one.
    EXPECT_TRUE(noscript_css_requested());
    EXPECT_FALSE(noscript_js_requested());

    // Verify info bar presented via histogram check.
    RetryForHistogramUntilCountReached(&histogram_tester,
                                       "Previews.PreviewShown.NoScript", 1);
  } else {
    // Verify loaded js resource but not css triggered by noscript tag.
    EXPECT_TRUE(noscript_js_requested());
    EXPECT_FALSE(noscript_css_requested());
  }
}

// This test class disables DataSaver.
class PreviewsDataSaverDisabledBrowserTest
    : public PreviewsNoScriptBrowserTest {
 public:
  PreviewsDataSaverDisabledBrowserTest() = default;

  ~PreviewsDataSaverDisabledBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* cmd) override {
    PreviewsNoScriptBrowserTest::SetUpCommandLine(cmd);
    cmd->RemoveSwitch("enable-spdy-proxy-auth");
  }
};

INSTANTIATE_TEST_SUITE_P(ShouldDisablePreview,
                         PreviewsDataSaverDisabledBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(PreviewsDataSaverDisabledBrowserTest,
                       NoPreviewWithDataSaverDisabled) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), https_url());

  // Verify loaded noscript tag triggered js file and not css file.
  EXPECT_TRUE(noscript_js_requested());
  EXPECT_FALSE(noscript_css_requested());

  histogram_tester.ExpectTotalCount("OptimizationGuide.ApplyDecision.NoScript",
                                    0);
}
