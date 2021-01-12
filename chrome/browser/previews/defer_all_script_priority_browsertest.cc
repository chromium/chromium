// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/previews/previews_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/optimization_guide/core/hints_component_info.h"
#include "components/optimization_guide/core/hints_component_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_service.h"
#include "components/optimization_guide/core/test_hints_component_creator.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"

// The first parameter selects whether the DeferAllScript optimization type is
// enabled, and the second parameter selects whether
// OptimizationGuideKeyedService is enabled. (The tests should pass in the same
// way for all cases).
class DeferAllScriptPriorityBrowserTest
    : public ::testing::WithParamInterface<bool>,
      public InProcessBrowserTest {
 public:
  DeferAllScriptPriorityBrowserTest() = default;
  ~DeferAllScriptPriorityBrowserTest() override = default;

  void SetUp() override {
    if (IsDeferAllScriptFeatureEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {previews::features::kPreviews,
           previews::features::kDeferAllScriptPreviews,
           blink::features::kLowerJavaScriptPriorityWhenForceDeferred,
           optimization_guide::features::kOptimizationHints},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {previews::features::kPreviews,
           blink::features::kLowerJavaScriptPriorityWhenForceDeferred,
           optimization_guide::features::kOptimizationHints},
          {});
    }

    InProcessBrowserTest::SetUp();
  }

  bool IsDeferAllScriptFeatureEnabled() const { return GetParam(); }

  // Returns the fetch time for the JavaScript file (in milliseconds). This
  // value is obtained using the resource timing API.
  double GetFetchTimeForJavaScriptFileInMilliseconds() {
    double script_log;
    std::string script = "getFetchTimeForJavaScriptFileInMilliseconds()";
    EXPECT_TRUE(ExecuteScriptAndExtractDouble(
        browser()->tab_strip_model()->GetActiveWebContents(), script,
        &script_log));
    return script_log;
  }

  void SetUpOnMainThread() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->RegisterRequestHandler(
        base::BindRepeating(&DeferAllScriptPriorityBrowserTest::RequestHandler,
                            base::Unretained(this)));

    ASSERT_TRUE(https_server_->Start());

    https_url_ = https_server_->GetURL("/defer_all_script_priority_test.html");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    // Override the target decision to |kTrue| to trigger a preview for the new
    // decision.
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->OverrideTargetDecisionForTesting(
            optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
            optimization_guide::OptimizationGuideDecision::kTrue);

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");
    cmd->AppendSwitchASCII("--force-effective-connection-type", "Slow-2G");

    cmd->AppendSwitch("optimization-guide-disable-installer");
    cmd->AppendSwitch("purge_hint_cache_store");

    // Due to race conditions, it's possible that blocklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlocklist);
  }

  // Returns a unique script for each request, to test service worker update.
  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().path().find("/defer_all_script_priority_test.html") !=
        std::string::npos) {
      return GetHtmlResponse(request);
    }

    if (request.GetURL().path().find("/hung") != std::string::npos) {
      return GetDelayedResponse(request);
    }
    return std::make_unique<net::test_server::BasicHttpResponse>();
  }

  // Returns an HTML response that fetches CSS files followed by synchronous
  // external JavaScript file. The HTML file contains an inline JavaScript
  // function getFetchTimeForJavaScriptFileInMilliseconds() that returns
  // the fetch time for JavaScript file in milliseconds.
  std::unique_ptr<net::test_server::HttpResponse> GetHtmlResponse(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content(
        "<html><body>"
        "<link rel=\"stylesheet\" type=\"text/css\" href=\"hung1.css\">"
        "<link rel=\"stylesheet\" type=\"text/css\" href=\"hung2.css\">"
        "<link rel=\"stylesheet\" type=\"text/css\" href=\"hung3.css\">"
        "<link rel=\"stylesheet\" type=\"text/css\" href=\"hung4.css\">"
        "<script src=\"defer_all_script_syncscript.js\"></script>"
        "<script>function getFetchTimeForJavaScriptFileInMilliseconds() {"
        "var p=window.performance.getEntriesByType(\"resource\");"
        "for (var i=0; i < p.length; i++) {"
        "if(p[i].name.includes(\"defer_all_script_syncscript.js\")) {"
        "sendValueToTest(p[i].responseStart-p[i].fetchStart);"
        "}"
        "}"
        "}"
        "function sendValueToTest(value) {"
        "window.domAutomationController.send(value);"
        "}"
        "</script></body></html>");
    return std::move(response);
  }

  int css_files_hung_time_milliseconds() { return 5000; }

  std::unique_ptr<net::test_server::HttpResponse> GetDelayedResponse(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::DelayedHttpResponse> response =
        std::make_unique<net::test_server::DelayedHttpResponse>(
            base::TimeDelta::FromMilliseconds(
                css_files_hung_time_milliseconds()));
    response->set_code(net::HttpStatusCode::HTTP_OK);
    return std::move(response);
  }

  // Creates hint data from the |component_info| and waits for it to be fully
  // processed before returning.
  void ProcessHintsComponent(
      const optimization_guide::HintsComponentInfo& component_info) {
    base::HistogramTester histogram_tester;

    g_browser_process->optimization_guide_service()->MaybeUpdateHintsComponent(
        component_info);

    RetryForHistogramUntilCountReached(
        &histogram_tester,
        optimization_guide::kComponentHintsUpdatedResultHistogramString, 1);
  }

  // Performs a navigation to |url| and waits for the the url's host's hints to
  // load before returning. This ensures that the hints will be available in the
  // hint cache for a subsequent navigation to a test url with the same host.
  void LoadHintsForUrl(const GURL& url) {
    base::HistogramTester histogram_tester;

    // Navigate to the url to prime the OptimizationGuide hints for the
    // url's host and ensure that they have been loaded from the store (via
    // histogram) prior to the navigation that tests functionality.
    ui_test_utils::NavigateToURL(browser(), url);

    RetryForHistogramUntilCountReached(
        &histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        1);
  }

  void SetDeferAllScriptHintWithPageWithPattern(
      const GURL& hint_setup_url,
      const std::string& page_pattern) {
    ProcessHintsComponent(
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::DEFER_ALL_SCRIPT,
            {hint_setup_url.host()}, page_pattern));
    LoadHintsForUrl(hint_setup_url);
  }

  virtual const GURL& https_url() const { return https_url_; }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedFeatureList param_feature_list_;

 private:
  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());

    InProcessBrowserTest::TearDownOnMainThread();
  }

  optimization_guide::testing::TestHintsComponentCreator
      test_hints_component_creator_;

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  GURL https_url_;

  DISALLOW_COPY_AND_ASSIGN(DeferAllScriptPriorityBrowserTest);
};

namespace {

GURL SetQuery(GURL url, const std::string& query) {
  url::Replacements<char> repls;
  repls.SetQuery(query.c_str(), url::Component(0, query.length()));
  return url.ReplaceComponents(repls);
}

}  // namespace

// Parameter is true if the test should be run with defer feature enabled.
INSTANTIATE_TEST_SUITE_P(All,
                         DeferAllScriptPriorityBrowserTest,
                         ::testing::Bool());

// Fetches an HTML weboage that fetches CSS files followed by an external
// JavaScript file. Verifies that the fetching of the JavaScript files is
// delayed only when it's not render blocking.
//
// When feature kDeferAllScriptPreviews is enabled, loading priority of
// JavaScript file should be lowered. This should cause resource scheduler
// to mark the JavaScript request as delayable, and delay its fetching to after
// the fetching of CSS files finish.
//
// However, if kDeferAllScriptPreviews is not enabled, then JavaScript is render
// blocking. This should cause resource scheduler to mark the JavaScript request
// as non-delayale, thus fetching it in parallel with the CSS files.
IN_PROC_BROWSER_TEST_P(
    DeferAllScriptPriorityBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(DeferAllScriptHttpsWhitelisted)) {
  GURL url = https_url();

  if (IsDeferAllScriptFeatureEnabled()) {
    // Whitelist DeferAllScript for any path for the url's host.
    SetDeferAllScriptHintWithPageWithPattern(url, "*");
  }

  // Set query to ensure that it's not treated as a reload as preview metrics
  // are not recorded for reloads.
  ui_test_utils::NavigateToURL(browser(), SetQuery(url, "foo"));

  double delay_milliseconds = GetFetchTimeForJavaScriptFileInMilliseconds();

  if (IsDeferAllScriptFeatureEnabled()) {
    // Fetching of JavaScript must start after fetching of the CSS files are
    // finished.
    EXPECT_LT(css_files_hung_time_milliseconds(), delay_milliseconds);
  } else {
    // Fetching of JavaScript should start in parallel with fetching of the
    // other files. So, it should finish fast enough. Note that even without any
    // queuing delays in resource scheduler, it's possible that this fetching
    // takes more css_files_hung_time_milliseconds(). This can potentially make
    // this test a bit flaky.
    EXPECT_GT(css_files_hung_time_milliseconds(), delay_milliseconds);
  }
}
