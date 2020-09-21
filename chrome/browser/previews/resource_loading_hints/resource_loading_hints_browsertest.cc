// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/previews/previews_test_util.h"
#include "chrome/browser/previews/resource_loading_hints/resource_loading_hints_web_contents_observer.h"
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
#include "components/optimization_guide/test_hints_component_creator.h"
#include "components/previews/core/previews_block_list.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "third_party/blink/public/common/features.h"

namespace {

constexpr char kMockHost[] = "mock.host";

}  // namespace

// This test class sets up everything but does not enable any features.
class ResourceLoadingNoFeaturesBrowserTest : public InProcessBrowserTest {
 public:
  ResourceLoadingNoFeaturesBrowserTest() = default;
  ~ResourceLoadingNoFeaturesBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Override the target decision to |kTrue| to trigger a preview for the new
    // decision.
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->OverrideTargetDecisionForTesting(
            optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
            optimization_guide::OptimizationGuideDecision::kTrue);
    // Set up https server with resource monitor.
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    https_server_->RegisterRequestMonitor(base::BindRepeating(
        &ResourceLoadingNoFeaturesBrowserTest::MonitorResourceRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());

    https_url_ = https_server_->GetURL("/resource_loading_hints.html");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    https_url_iframe_ =
        https_server_->GetURL("/resource_loading_hints_iframe.html");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    https_url_iframe_preload_ =
        https_server_->GetURL("/resource_loading_hints_iframe_preload.html");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    https_url_preload_ =
        https_server_->GetURL("/resource_loading_hints_preload.html");

    https_second_url_ =
        https_server_->GetURL("/resource_loading_hints_second.html");
    ASSERT_TRUE(https_second_url_.SchemeIs(url::kHttpsScheme));

    https_no_transform_url_ = https_server_->GetURL(
        "/resource_loading_hints_with_no_transform_header.html");
    ASSERT_TRUE(https_no_transform_url_.SchemeIs(url::kHttpsScheme));

    https_hint_setup_url_ = https_server_->GetURL("/hint_setup.html");
    ASSERT_TRUE(https_hint_setup_url_.SchemeIs(url::kHttpsScheme));
    ASSERT_EQ(https_hint_setup_url_.host(), https_url_.host());

    // Set up http server with resource monitor and redirect handler.
    http_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTP));
    http_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    http_server_->RegisterRequestMonitor(base::BindRepeating(
        &ResourceLoadingNoFeaturesBrowserTest::MonitorResourceRequest,
        base::Unretained(this)));
    http_server_->RegisterRequestHandler(base::BindRepeating(
        &ResourceLoadingNoFeaturesBrowserTest::HandleRedirectRequest,
        base::Unretained(this)));
    ASSERT_TRUE(http_server_->Start());

    http_url_ = http_server_->GetURL("/resource_loading_hints.html");
    ASSERT_TRUE(http_url_.SchemeIs(url::kHttpScheme));

    redirect_url_ = http_server_->GetURL("/redirect.html");
    ASSERT_TRUE(redirect_url_.SchemeIs(url::kHttpScheme));

    http_hint_setup_url_ = http_server_->GetURL("/hint_setup.html");
    ASSERT_TRUE(http_hint_setup_url_.SchemeIs(url::kHttpScheme));
    ASSERT_EQ(http_hint_setup_url_.host(), http_url_.host());

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");

    cmd->AppendSwitch("optimization-guide-disable-installer");

    // Needed so that the browser can connect to kMockHost.
    cmd->AppendSwitch("ignore-certificate-errors");

    cmd->AppendSwitch("purge_hint_cache_store");

    // Due to race conditions, it's possible that blocklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlocklist);
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

  void InitializeOptimizationHints() {
    ProcessHintsComponent(
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::RESOURCE_LOADING, {"doesntmatter.com"},
            "*", {}));
  }

  // Performs a navigation to |url| and waits for the the url's host's hints to
  // load before returning. This ensures that the hints will be available in the
  // hint cache for a subsequent navigation to a test url with the same host.
  void LoadHintsForUrl(const GURL& url) {
    base::HistogramTester histogram_tester;

    // Navigate to |hint_setup_url| to prime the OptimizationGuide hints for the
    // url's host and ensure that they have been loaded from the store (via
    // histogram) prior to the navigation that tests functionality.
    ui_test_utils::NavigateToURL(browser(), url);

    RetryForHistogramUntilCountReached(
        &histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        1);
  }

  void SetDefaultOnlyResourceLoadingHints(const GURL& hint_setup_url) {
    SetDefaultOnlyResourceLoadingHintsWithPagePattern(hint_setup_url, "*");
  }

  void SetDefaultOnlyResourceLoadingHintsWithPagePattern(
      const GURL& hint_setup_url,
      const std::string& page_pattern) {
    std::vector<std::string> resource_patterns;
    resource_patterns.push_back("foo.jpg");
    resource_patterns.push_back("png");
    resource_patterns.push_back("woff2");

    ProcessHintsComponent(
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::RESOURCE_LOADING,
            {hint_setup_url.host(), kMockHost}, page_pattern,
            resource_patterns));
    LoadHintsForUrl(hint_setup_url);
  }

  // Sets the resource loading hints in optimization guide service. No patterns
  // are specified for resources.
  void SetEmptyResourceLoadingHintsWithPagePattern(
      const GURL& hint_setup_url,
      const std::string& page_pattern) {
    ProcessHintsComponent(
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::RESOURCE_LOADING,
            {hint_setup_url.host(), kMockHost}, page_pattern,
            std::vector<std::string>{} /* resource_patterns */));
    LoadHintsForUrl(hint_setup_url);
  }

  virtual const GURL& https_url() const { return https_url_; }

  // URL that loads blocked resources uses link-rel preload in <head>.
  const GURL& https_url_preload() const { return https_url_preload_; }

  // URL that loads a woff2 resource and https_url() webpage in an iframe.
  virtual const GURL& https_url_iframe() const { return https_url_iframe_; }

  // URL that loads a woff2 resource and https_url_preload() webpage in an
  // iframe.
  virtual const GURL& https_url_iframe_preload() const {
    return https_url_iframe_preload_;
  }

  const GURL& https_second_url() const { return https_second_url_; }
  const GURL& https_no_transform_url() const { return https_no_transform_url_; }
  const GURL& https_hint_setup_url() const { return https_hint_setup_url_; }
  const GURL& http_url() const { return http_url_; }
  const GURL& redirect_url() const { return redirect_url_; }
  const GURL& http_hint_setup_url() const { return http_hint_setup_url_; }

  void SetExpectedFooJpgRequest(bool expect_foo_jpg_requested) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    subresource_expected_["/foo.jpg"] = expect_foo_jpg_requested;
  }
  void SetExpectedBarJpgRequest(bool expect_bar_jpg_requested) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    subresource_expected_["/bar.jpg"] = expect_bar_jpg_requested;
  }
  void SetExpectedBazWoff2Request(bool expect_woff_requested) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    subresource_expected_["/baz.woff2"] = expect_woff_requested;
  }

  int GetProcessID() const {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetMainFrame()
        ->GetProcess()
        ->GetID();
  }

  void RetryUntilAllExpectedSubresourcesSeen() {
    while (true) {
      base::ThreadPoolInstance::Get()->FlushForTesting();
      base::RunLoop().RunUntilIdle();

      bool have_seen_all_expected_subresources = true;
      for (const auto& expect : subresource_expected_) {
        if (expect.second) {
          have_seen_all_expected_subresources = false;
          break;
        }
      }

      if (have_seen_all_expected_subresources)
        break;
    }
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;

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
        FROM_HERE, base::BindOnce(&ResourceLoadingNoFeaturesBrowserTest::
                                      MonitorResourceRequestOnUIThread,
                                  base::Unretained(this), request.GetURL()));
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

  void MonitorResourceRequestOnUIThread(const GURL& gurl) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    for (const auto& expect : subresource_expected_) {
      if (gurl.path() == expect.first) {
        // Verify that |gurl| is expected to be fetched. This ensures that a
        // resource whose loading is blocked is not loaded.
        EXPECT_TRUE(expect.second)
            << " GURL " << gurl
            << " was expected to be blocked, but was actually fetched";

        // Subresource should not be fetched again.
        subresource_expected_[gurl.path()] = false;
        return;
      }
    }
  }

  optimization_guide::testing::TestHintsComponentCreator
      test_hints_component_creator_;

  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  GURL https_url_;
  GURL https_url_preload_;
  GURL https_url_iframe_;
  GURL https_url_iframe_preload_;
  GURL https_second_url_;
  GURL https_no_transform_url_;
  GURL https_hint_setup_url_;
  GURL http_url_;
  GURL redirect_url_;
  GURL http_hint_setup_url_;

  // Mapping from a subresource path to whether the resource is expected to be
  // fetched. Once a subresource present in this map is fetched, the
  // corresponding value is set to false.
  // Must only be accessed on UI thread.
  std::map<std::string, bool> subresource_expected_;

  DISALLOW_COPY_AND_ASSIGN(ResourceLoadingNoFeaturesBrowserTest);
};

// Page type to use when running the test.
enum PageToUse {
  // Use test page that loads blocked resources in body.
  REGULAR_PAGE = 0,

  // Use test page that loads blocked resources using link-rel preload in
  // <head>.
  PRELOAD_PAGE = 1,
};

// This test class enables ResourceLoadingHints with OptimizationHints.
// First parameter is true if the test should be run with a webpage that
// preloads resources in the HTML head using link-rel preload.
// All tests should pass in the same way for all cases.
class ResourceLoadingHintsBrowserTest
    : public ::testing::WithParamInterface<PageToUse>,
      public ResourceLoadingNoFeaturesBrowserTest {
 public:
  ResourceLoadingHintsBrowserTest() : test_page_to_use_(GetParam()) {}

  ~ResourceLoadingHintsBrowserTest() override = default;

  void SetUp() override {
    // Enabling NoScript should have no effect since resource loading takes
    // priority over NoScript.
    scoped_feature_list_.InitWithFeatures(
        {previews::features::kPreviews, previews::features::kNoScriptPreviews,
         optimization_guide::features::kOptimizationHints,
         previews::features::kResourceLoadingHints},
        {});
    // Any additional ScopedFeatureLists added by subclasses must be
    // instantiated here to get destructed in the correct order.
    InitExtraFeatures();
    ResourceLoadingNoFeaturesBrowserTest::SetUp();
  }

  virtual void InitExtraFeatures() {}

  PageToUse test_page_to_use() const { return test_page_to_use_; }

  GURL GetURLWithMockHost(const net::EmbeddedTestServer& server,
                          const std::string& relative_url) const {
    return server.GetURL(kMockHost, relative_url);
  }

  const GURL& https_url() const override {
    switch (test_page_to_use_) {
      case REGULAR_PAGE:
        return ResourceLoadingNoFeaturesBrowserTest::https_url();
      case PRELOAD_PAGE:
        return ResourceLoadingNoFeaturesBrowserTest::https_url_preload();
    }
  }

  const GURL& https_url_iframe() const override {
    switch (test_page_to_use_) {
      case REGULAR_PAGE:
        return ResourceLoadingNoFeaturesBrowserTest::https_url_iframe();
      case PRELOAD_PAGE:
        return ResourceLoadingNoFeaturesBrowserTest::https_url_iframe_preload();
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedFeatureList ogks_feature_list_;

 private:
  const PageToUse test_page_to_use_;

  DISALLOW_COPY_AND_ASSIGN(ResourceLoadingHintsBrowserTest);
};

// First parameter determines the test webpage that should be used.
INSTANTIATE_TEST_SUITE_P(All,
                         ResourceLoadingHintsBrowserTest,
                         ::testing::Values(REGULAR_PAGE, PRELOAD_PAGE));


IN_PROC_BROWSER_TEST_P(
    ResourceLoadingHintsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(
        ResourceLoadingHintsHttpsWhitelistedRedirectToHttps)) {
  GURL url = redirect_url();

  // Whitelist resource loading hints for https_hint_setup_url()'s' host.
  SetDefaultOnlyResourceLoadingHints(https_hint_setup_url());

  SetExpectedFooJpgRequest(false);
  SetExpectedBarJpgRequest(true);

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url);
  RetryUntilAllExpectedSubresourcesSeen();

  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.ResourcePatternsAvailableAtCommitForRedirect", 1,
      1);
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 2);
  RetryForHistogramUntilCountReached(
      &histogram_tester, "Previews.PreviewShown.ResourceLoadingHints", 1);
}

IN_PROC_BROWSER_TEST_P(
    ResourceLoadingHintsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(ResourceLoadingHintsHttpsNotWhitelisted)) {
  GURL url = https_url();

  SetExpectedFooJpgRequest(true);
  SetExpectedBarJpgRequest(true);
  InitializeOptimizationHints();

  base::HistogramTester histogram_tester;

  // The URL is not whitelisted.
  ui_test_utils::NavigateToURL(browser(), url);
  RetryUntilAllExpectedSubresourcesSeen();

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::
                           NOT_ALLOWED_BY_OPTIMIZATION_GUIDE),
      1);
  histogram_tester.ExpectTotalCount(
      "Previews.PreviewShown.ResourceLoadingHints", 0);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 0);
}


IN_PROC_BROWSER_TEST_P(ResourceLoadingHintsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(
                           ResourceLoadingHintsHttpsWhitelistedNoTransform)) {
  GURL url = https_no_transform_url();

  // Whitelist resource loading hints for http_hint_setup_url()'s' host.
  SetDefaultOnlyResourceLoadingHints(http_hint_setup_url());

  SetExpectedFooJpgRequest(true);
  SetExpectedBarJpgRequest(true);

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url);
  RetryUntilAllExpectedSubresourcesSeen();

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.PreviewShown.ResourceLoadingHints", 0);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 0);
}

class ResourceLoadingHintsBrowserTestWithCoinFlipAlwaysHoldback
    : public ResourceLoadingHintsBrowserTest {
 public:
  void InitExtraFeatures() override {
    // Holdback the page load from previews and also disable offline previews to
    // ensure that only post-commit previews are enabled.
    feature_list_.InitWithFeaturesAndParameters(
        {{previews::features::kCoinFlipHoldback,
          {{"force_coin_flip_always_holdback", "true"}}}},
        {previews::features::kOfflinePreviews});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// First parameter determines the test webpage that should be used.
INSTANTIATE_TEST_SUITE_P(
    All,
    ResourceLoadingHintsBrowserTestWithCoinFlipAlwaysHoldback,
    ::testing::Values(REGULAR_PAGE, PRELOAD_PAGE));

IN_PROC_BROWSER_TEST_P(
    ResourceLoadingHintsBrowserTestWithCoinFlipAlwaysHoldback,
    DISABLE_ON_WIN_MAC_CHROMEOS(
        ResourceLoadingHintsHttpsWhitelistedButShouldNotApplyBecauseCoinFlipHoldback)) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  GURL url = https_url();

  // Whitelist resource loading hints for https_hint_setup_url()'s' host.
  SetDefaultOnlyResourceLoadingHints(https_hint_setup_url());

  SetExpectedFooJpgRequest(true);
  SetExpectedBarJpgRequest(true);

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), url);
  RetryUntilAllExpectedSubresourcesSeen();

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::COMMITTED), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.PreviewShown.ResourceLoadingHints", 0);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 0);
  // Make sure we did not record a PreviewsResourceLoadingHints UKM for it.
  auto rlh_ukm_entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PreviewsResourceLoadingHints::kEntryName);
  ASSERT_EQ(0u, rlh_ukm_entries.size());
}
