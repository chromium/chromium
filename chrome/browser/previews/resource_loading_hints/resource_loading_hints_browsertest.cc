// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/previews/resource_loading_hints/resource_loading_hints_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/optimization_guide_service_observer.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/test_component_creator.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_features.h"
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

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramUntilCountReached(base::HistogramTester* histogram_tester,
                                        const std::string& histogram_name,
                                        size_t count) {
  base::RunLoop().RunUntilIdle();
  for (size_t attempt = 0; attempt < 3; ++attempt) {
    const std::vector<base::Bucket> buckets =
        histogram_tester->GetAllSamples(histogram_name);
    size_t total_count = 0;
    for (const auto& bucket : buckets)
      total_count += bucket.count;
    if (total_count >= count)
      return;
    content::FetchHistogramsFromChildProcesses();
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }
}

}  // namespace

// This test class sets up everything but does not enable any features.
class ResourceLoadingNoFeaturesBrowserTest : public InProcessBrowserTest {
 public:
  ResourceLoadingNoFeaturesBrowserTest() = default;
  ~ResourceLoadingNoFeaturesBrowserTest() override = default;

  void SetUpOnMainThread() override {
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

    https_no_transform_url_ = https_server_->GetURL(
        "/resource_loading_hints_with_no_transform_header.html");
    ASSERT_TRUE(https_no_transform_url_.SchemeIs(url::kHttpsScheme));

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

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");
    cmd->AppendSwitchASCII("force-effective-connection-type", "Slow-2G");
  }

  void SetResourceLoadingHints(const std::vector<std::string>& hints_sites) {
    std::vector<std::string> resource_patterns;
    resource_patterns.push_back("foo.jpg");
    resource_patterns.push_back("png");
    resource_patterns.push_back("woff2");

    const optimization_guide::ComponentInfo& component_info =
        test_component_creator_.CreateComponentInfoWithPageHints(
            optimization_guide::proto::RESOURCE_LOADING, hints_sites,
            resource_patterns);

    g_browser_process->optimization_guide_service()->ProcessHints(
        component_info);

    // Wait for hints to be processed by PreviewsOptimizationGuide.
    base::RunLoop().RunUntilIdle();
  }

  void AddTestOptimizationGuideServiceObserver(
      TestOptimizationGuideServiceObserver* observer) {
    g_browser_process->optimization_guide_service()->AddObserver(observer);
  }

  const GURL& https_url() const { return https_url_; }
  const GURL& https_no_transform_url() const { return https_no_transform_url_; }
  const GURL& http_url() const { return http_url_; }
  const GURL& redirect_url() const { return redirect_url_; }

  void SetExpectedFooJpgRequest(bool expect_foo_jpg_requested) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    subresource_expected_["/foo.jpg"] = expect_foo_jpg_requested;
  }
  void SetExpectedBarJpgRequest(bool expect_bar_jpg_requested) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    subresource_expected_["/bar.jpg"] = expect_bar_jpg_requested;
  }

  bool resource_loading_hint_intervention_header_seen() const {
    return resource_loading_hint_intervention_header_seen_;
  }

  void ResetResourceLoadingHintInterventionHeaderSeen() {
    resource_loading_hint_intervention_header_seen_ = false;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

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
    auto it = request.headers.find("Intervention");
    // Chrome status entry for resource loading hints is hosted at
    // https://www.chromestatus.com/features/4775088607985664.
    if (it != request.headers.end() &&
        it->second.find("4510564810227712") != std::string::npos) {
      resource_loading_hint_intervention_header_seen_ = true;
    }
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&ResourceLoadingNoFeaturesBrowserTest::
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
        EXPECT_TRUE(expect.second);
        // Subresource should not be fetched again.
        subresource_expected_[gurl.path()] = false;
        return;
      }
    }
  }

  optimization_guide::testing::TestComponentCreator test_component_creator_;

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  GURL https_url_;
  GURL https_no_transform_url_;
  GURL http_url_;
  GURL redirect_url_;

  bool resource_loading_hint_intervention_header_seen_ = false;

  // Mapping from a subresource path to whether the resource is expected to be
  // fetched. Once a subresource present in this map is fetched, the
  // corresponding value is set to false.
  // Must only be accessed on UI thread.
  std::map<std::string, bool> subresource_expected_;

  DISALLOW_COPY_AND_ASSIGN(ResourceLoadingNoFeaturesBrowserTest);
};

// This test class enables ResourceLoadingHints with OptimizationHints.
class ResourceLoadingHintsBrowserTest
    : public ResourceLoadingNoFeaturesBrowserTest {
 public:
  ResourceLoadingHintsBrowserTest() = default;

  ~ResourceLoadingHintsBrowserTest() override = default;

  void SetUp() override {
    // Enabling NoScript should have no effect since resource loading takes
    // priority over NoScript.
    scoped_feature_list_.InitWithFeatures(
        {previews::features::kPreviews, previews::features::kNoScriptPreviews,
         previews::features::kOptimizationHints,
         previews::features::kResourceLoadingHints},
        {});
    ResourceLoadingNoFeaturesBrowserTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceLoadingHintsBrowserTest);
};

// Previews InfoBar (which these tests triggers) does not work on Mac.
// See https://crbug.com/782322 for details. Also occasional flakes on win7
// (https://crbug.com/789542).
// Additionally, ResourceLoadingHintsHttpsWhitelistedRedirectToHttps is disabled
// on all OS types until hints are made to work with redirects.
// TODO(jegray): Re-enable ResourceLoadingHintsHttpsWhitelistedRedirectToHttps
// when support for redirects is added: https://crbug.com/891752
#if !defined(OS_MACOSX) && !defined(OS_WIN)
#define MAYBE_ResourceLoadingHintsHttpsWhitelisted \
  ResourceLoadingHintsHttpsWhitelisted
#define MAYBE_ResourceLoadingHintsHttpsWhitelistedRedirectToHttps \
  DISABLED_ResourceLoadingHintsHttpsWhitelistedRedirectToHttps
#define MAYBE_ResourceLoadingHintsHttpsNoWhitelisted \
  ResourceLoadingHintsHttpsNoWhitelisted
#define MAYBE_ResourceLoadingHintsHttp ResourceLoadingHintsHttp
#define MAYBE_ResourceLoadingHintsHttpsWhitelistedNoTransform \
  ResourceLoadingHintsHttpsWhitelistedNoTransform
#else
#define MAYBE_ResourceLoadingHintsHttpsWhitelisted \
  DISABLED_ResourceLoadingHintsHttpsWhitelisted
#define MAYBE_ResourceLoadingHintsHttpsWhitelistedRedirectToHttps \
  DISABLED_ResourceLoadingHintsHttpsWhitelistedRedirectToHttps
#define MAYBE_ResourceLoadingHintsHttpsNoWhitelisted \
  DISABLED_ResourceLoadingHintsHttpsNoWhitelisted
#define MAYBE_ResourceLoadingHintsHttp DISABLED_ResourceLoadingHintsHttp
#define MAYBE_ResourceLoadingHintsHttpsWhitelistedNoTransform \
  DISABLED_ResourceLoadingHintsHttpsWhitelistedNoTransform
#endif

IN_PROC_BROWSER_TEST_F(ResourceLoadingHintsBrowserTest,
                       MAYBE_ResourceLoadingHintsHttpsWhitelisted) {
  SetExpectedFooJpgRequest(false);
  SetExpectedBarJpgRequest(true);

  TestOptimizationGuideServiceObserver observer;
  AddTestOptimizationGuideServiceObserver(&observer);
  base::RunLoop().RunUntilIdle();

  // Whitelist test URL for resource loading hints.
  SetResourceLoadingHints({https_url().host()});
  observer.WaitForNotification();

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), https_url());

  RetryForHistogramUntilCountReached(
      &histogram_tester, "ResourceLoadingHints.CountBlockedSubresourcePatterns",
      1);
  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.ResourcePatternsAvailableAtCommit", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 1);
  histogram_tester.ExpectBucketCount(
      "Previews.InfoBarAction.ResourceLoadingHints", 0, 1);
  // SetResourceLoadingHints sets 3 resource loading hints patterns.
  histogram_tester.ExpectBucketCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 3, 1);
  EXPECT_TRUE(resource_loading_hint_intervention_header_seen());

  // Load the same webpage to ensure that the resource loading hints are sent
  // again.
  SetExpectedFooJpgRequest(false);
  SetExpectedBarJpgRequest(true);
  ResetResourceLoadingHintInterventionHeaderSeen();

  ui_test_utils::NavigateToURL(browser(), https_url());

  RetryForHistogramUntilCountReached(
      &histogram_tester, "ResourceLoadingHints.CountBlockedSubresourcePatterns",
      2);
  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.ResourcePatternsAvailableAtCommit", 1, 2);
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 2);
  histogram_tester.ExpectBucketCount(
      "Previews.InfoBarAction.ResourceLoadingHints", 0, 2);
  // SetResourceLoadingHints sets 3 resource loading hints patterns.
  histogram_tester.ExpectBucketCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 3, 2);
  EXPECT_TRUE(resource_loading_hint_intervention_header_seen());
}

IN_PROC_BROWSER_TEST_F(
    ResourceLoadingHintsBrowserTest,
    MAYBE_ResourceLoadingHintsHttpsWhitelistedRedirectToHttps) {
  SetExpectedFooJpgRequest(false);
  SetExpectedBarJpgRequest(true);

  TestOptimizationGuideServiceObserver observer;
  AddTestOptimizationGuideServiceObserver(&observer);
  base::RunLoop().RunUntilIdle();

  SetResourceLoadingHints({https_url().host()});
  observer.WaitForNotification();

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), redirect_url());

  RetryForHistogramUntilCountReached(
      &histogram_tester, "ResourceLoadingHints.CountBlockedSubresourcePatterns",
      1);
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.InfoBarAction.ResourceLoadingHints", 1);
  // SetResourceLoadingHints sets 3 resource loading hints patterns.
  histogram_tester.ExpectBucketCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 3, 1);
  EXPECT_TRUE(resource_loading_hint_intervention_header_seen());
}

IN_PROC_BROWSER_TEST_F(ResourceLoadingHintsBrowserTest,
                       MAYBE_ResourceLoadingHintsHttpsNoWhitelisted) {
  SetExpectedFooJpgRequest(true);
  SetExpectedBarJpgRequest(true);

  TestOptimizationGuideServiceObserver observer;
  AddTestOptimizationGuideServiceObserver(&observer);
  base::RunLoop().RunUntilIdle();

  SetResourceLoadingHints({});
  observer.WaitForNotification();

  base::HistogramTester histogram_tester;

  // The URL is not whitelisted.
  ui_test_utils::NavigateToURL(browser(), https_url());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(
          previews::PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER),
      1);
  histogram_tester.ExpectTotalCount(
      "Previews.InfoBarAction.ResourceLoadingHints", 0);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 0);
  EXPECT_FALSE(resource_loading_hint_intervention_header_seen());
}

IN_PROC_BROWSER_TEST_F(ResourceLoadingHintsBrowserTest,
                       MAYBE_ResourceLoadingHintsHttp) {
  SetExpectedFooJpgRequest(true);
  SetExpectedBarJpgRequest(true);

  TestOptimizationGuideServiceObserver observer;
  AddTestOptimizationGuideServiceObserver(&observer);
  base::RunLoop().RunUntilIdle();

  // Whitelist test HTTP URL for resource loading hints.
  SetResourceLoadingHints({https_url().host()});
  observer.WaitForNotification();

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), http_url());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.InfoBarAction.ResourceLoadingHints", 0);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 0);
  EXPECT_FALSE(resource_loading_hint_intervention_header_seen());
}

IN_PROC_BROWSER_TEST_F(ResourceLoadingHintsBrowserTest,
                       MAYBE_ResourceLoadingHintsHttpsWhitelistedNoTransform) {
  SetExpectedFooJpgRequest(true);
  SetExpectedBarJpgRequest(true);

  TestOptimizationGuideServiceObserver observer;
  AddTestOptimizationGuideServiceObserver(&observer);
  base::RunLoop().RunUntilIdle();

  // Whitelist test URL for resource loading hints.
  SetResourceLoadingHints({https_url().host()});
  observer.WaitForNotification();

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), https_no_transform_url());
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(previews::PreviewsEligibilityReason::ALLOWED), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.InfoBarAction.ResourceLoadingHints", 0);
  histogram_tester.ExpectTotalCount(
      "ResourceLoadingHints.CountBlockedSubresourcePatterns", 0);
  EXPECT_FALSE(resource_loading_hint_intervention_header_seen());
}
