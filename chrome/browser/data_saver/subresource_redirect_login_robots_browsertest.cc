// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_redirect/https_image_compression_infobar_decider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/proto/robots_rules.pb.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace subresource_redirect {

const bool kRuleTypeAllow = true;
const bool kRuleTypeDisallow = false;

// Holds one allow or disallow robots rule
struct RobotsRule {
  RobotsRule(bool rule_type, const std::string& pattern)
      : rule_type(rule_type), pattern(pattern) {}

  bool rule_type;
  std::string pattern;
};

// Convert robots rules to its proto.
std::string GetRobotsRulesProtoString(const std::vector<RobotsRule>& patterns) {
  proto::RobotsRules robots_rules;
  for (const auto& pattern : patterns) {
    auto* new_rule = robots_rules.add_image_ordered_rules();
    if (pattern.rule_type == kRuleTypeAllow) {
      new_rule->set_allowed_pattern(pattern.pattern);
    } else {
      new_rule->set_disallowed_pattern(pattern.pattern);
    }
  }
  return robots_rules.SerializeAsString();
}

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramUntilCountReached(base::HistogramTester* histogram_tester,
                                        const std::string& histogram_name,
                                        size_t count) {
  while (true) {
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    const std::vector<base::Bucket> buckets =
        histogram_tester->GetAllSamples(histogram_name);
    size_t total_count = 0;
    for (const auto& bucket : buckets) {
      total_count += bucket.count;
    }
    if (total_count >= count) {
      break;
    }
  }
}

// Fetches histograms from renderer child processes.
void FetchHistogramsFromChildProcesses() {
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
}

// Embedded test server for the robots rules
class RobotsRulesTestServer {
 public:
  // Different failures modes the robots server should return
  enum FailureMode {
    kNone = 0,
    kLoadshed503RetryAfterResponse,
    kTimeout,
  };

  RobotsRulesTestServer() : server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  bool Start() {
    server_.ServeFilesFromSourceDirectory("chrome/test/data");
    server_.RegisterRequestHandler(base::BindRepeating(
        &RobotsRulesTestServer::OnServerRequest, base::Unretained(this)));
    server_.RegisterRequestMonitor(base::BindRepeating(
        &RobotsRulesTestServer::OnRequestMonitor, base::Unretained(this)));
    return server_.Start();
  }

  std::string GetURL() const {
    return server_.GetURL("robotsrules.com", "/").spec();
  }

  void AddRobotsRules(const GURL& origin,
                      const std::vector<RobotsRule>& robots_rules) {
    robots_rules_proto_[origin.spec()] =
        GetRobotsRulesProtoString(robots_rules);
  }

  void VerifyRequestedOrigins(const std::set<std::string>& requests) {
    EXPECT_EQ(received_requests_, requests);
  }

  void set_failure_mode(FailureMode failure_mode) {
    failure_mode_ = failure_mode;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> OnServerRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    std::string robots_url_str;
    EXPECT_EQ("/robots", request.GetURL().path());
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(request.GetURL(), "u", &robots_url_str));
    GURL robots_url(robots_url_str);
    EXPECT_EQ("/robots.txt", GURL(robots_url).path());

    switch (failure_mode_) {
      case FailureMode::kLoadshed503RetryAfterResponse:
        response->set_code(net::HTTP_SERVICE_UNAVAILABLE);
        response->AddCustomHeader("Retry-After", "5");
        return response;
      case FailureMode::kTimeout:
        response = std::make_unique<net::test_server::DelayedHttpResponse>(
            base::TimeDelta::FromSeconds(2));
        break;
      case FailureMode::kNone:
        break;
    }

    auto it = robots_rules_proto_.find(robots_url.GetOrigin().spec());
    if (it != robots_rules_proto_.end())
      response->set_content(it->second);
    return std::move(response);
  }

  // Called on every robots request
  void OnRequestMonitor(const net::test_server::HttpRequest& request) {
    std::string robots_url_str;
    EXPECT_EQ("/robots", request.GetURL().path());
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(request.GetURL(), "u", &robots_url_str));
    std::string robots_origin = GURL(robots_url_str).GetOrigin().spec();
    EXPECT_TRUE(received_requests_.find(robots_origin) ==
                received_requests_.end());
    received_requests_.insert(robots_origin);
  }

  // Robots rules proto keyed by origin.
  std::map<std::string, std::string> robots_rules_proto_;

  // Whether the robots server should return failure.
  FailureMode failure_mode_ = FailureMode::kNone;

  // All the origins the robots rules are requested for.
  std::set<std::string> received_requests_;

  net::EmbeddedTestServer server_;
};

class ImageCompressionTestServer {
 public:
  // Different failures modes the image server should return
  enum FailureMode {
    kNone = 0,
    kLoadshed503RetryAfterResponse,
  };
  ImageCompressionTestServer() : server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  bool Start() {
    server_.ServeFilesFromSourceDirectory("chrome/test/data");
    server_.RegisterRequestHandler(base::BindRepeating(
        &ImageCompressionTestServer::OnServerRequest, base::Unretained(this)));
    server_.RegisterRequestMonitor(base::BindRepeating(
        &ImageCompressionTestServer::OnRequestMonitor, base::Unretained(this)));
    return server_.Start();
  }

  std::string GetURL() const {
    return server_.GetURL("imagecompression.com", "/").spec();
  }

  void VerifyRequestedImagePaths(const std::set<std::string>& paths) {
    EXPECT_EQ(received_request_paths_, paths);
  }

  void set_failure_mode(FailureMode failure_mode) {
    failure_mode_ = failure_mode;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> OnServerRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();

    switch (failure_mode_) {
      case FailureMode::kLoadshed503RetryAfterResponse:
        response->set_code(net::HTTP_SERVICE_UNAVAILABLE);
        response->AddCustomHeader("Retry-After", "5");
        return response;
      case FailureMode::kNone:
        break;
    }

    // Serve the correct image file.
    std::string img_url;
    std::string file_contents;
    base::FilePath test_data_directory;
    EXPECT_EQ("/i", request.GetURL().path());
    EXPECT_TRUE(net::GetValueForKeyInQuery(request.GetURL(), "u", &img_url));
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
    if (base::ReadFileToString(
            test_data_directory.AppendASCII(GURL(img_url).path().substr(1)),
            &file_contents)) {
      response->set_content(file_contents);
      response->set_code(net::HTTP_OK);
    }
    return std::move(response);
  }

  // Called on every subresource request
  void OnRequestMonitor(const net::test_server::HttpRequest& request) {
    std::string img_url;
    EXPECT_EQ("/i", request.GetURL().path());
    EXPECT_TRUE(net::GetValueForKeyInQuery(request.GetURL(), "u", &img_url));
    img_url = GURL(img_url).PathForRequest();
    EXPECT_TRUE(received_request_paths_.find(img_url) ==
                received_request_paths_.end());
    received_request_paths_.insert(img_url);
  }

  // All the URL paths of the requested images.
  std::set<std::string> received_request_paths_;

  // Whether the subresource server should return failure.
  FailureMode failure_mode_ = FailureMode::kNone;

  net::EmbeddedTestServer server_;
};

class SubresourceRedirectLoginRobotsBrowserTest : public InProcessBrowserTest {
 public:
  explicit SubresourceRedirectLoginRobotsBrowserTest(
      bool enable_lite_mode = true,
      bool enable_login_robots_compression_feature = true)
      : enable_lite_mode_(enable_lite_mode),
        enable_login_robots_compression_feature_(
            enable_login_robots_compression_feature),
        https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~SubresourceRedirectLoginRobotsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
    if (enable_lite_mode_)
      command_line->AppendSwitch("enable-spdy-proxy-auth");

    // Disable infobar shown check to actually compress the pages.
    command_line->AppendSwitch("override-https-image-compression-infobar");
  }

  void SetUp() override {
    ASSERT_TRUE(robots_rules_server_.Start());
    ASSERT_TRUE(image_compression_server_.Start());
    https_test_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_test_server_.Start());

    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features;
    if (enable_login_robots_compression_feature_) {
      base::FieldTrialParams params;
      params["enable_public_image_hints_based_compression"] = "false";
      params["enable_login_robots_based_compression"] = "true";
      params["lite_page_robots_origin"] = robots_rules_server_.GetURL();
      params["lite_page_subresource_origin"] =
          image_compression_server_.GetURL();
      // This rules fetch timeout is chosen such that the tests would have
      // enough time to fetch the rules without causing a timeout.
      params["robots_rules_receive_timeout"] = "1000";
      enabled_features.emplace_back(blink::features::kSubresourceRedirect,
                                    params);
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
    InProcessBrowserTest::SetUp();
  }

  GURL GetHttpsTestURL(const std::string& path) const {
    return https_test_server_.GetURL("test_https_server.com", path);
  }

  void NavigateAndWaitForLoad(Browser* browser, const GURL& url) {
    ui_test_utils::NavigateToURL(browser, url);
    EXPECT_EQ(true, EvalJs(browser->tab_strip_model()->GetActiveWebContents(),
                           "checkImage()"));
    FetchHistogramsFromChildProcesses();
  }

  bool RunScriptExtractBool(const std::string& script,
                            content::WebContents* web_contents = nullptr) {
    if (!web_contents)
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    return EvalJs(web_contents, script).ExtractBool();
  }

 protected:
  bool enable_lite_mode_;
  bool enable_login_robots_compression_feature_;

  base::test::ScopedFeatureList scoped_feature_list_;

  // Simulates the LitePages servers that return the robots rules and compress
  // images.
  RobotsRulesTestServer robots_rules_server_;
  ImageCompressionTestServer image_compression_server_;
  net::EmbeddedTestServer https_test_server_;

  base::HistogramTester histogram_tester_;
};

// Enable tests for linux since LiteMode is enabled only for Android.
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_CHROMEOS)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

IN_PROC_BROWSER_TEST_F(SubresourceRedirectLoginRobotsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(TestImageAllowedByRobots)) {
  robots_rules_server_.AddRobotsRules(
      GetHttpsTestURL("/"),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", true, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", false, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png"});
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestImageDisallowedByRobots)) {
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  // The image will start redirect and pause when robots rules are getting
  // fetched. But when the fetch timesout, it will reset and fetch the original
  // URL.
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", false, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths({});
}

IN_PROC_BROWSER_TEST_F(SubresourceRedirectLoginRobotsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(NoTriggerWhenDataSaverOff)) {
  data_reduction_proxy::DataReductionProxySettings::
      SetDataSaverEnabledForTesting(browser()->profile()->GetPrefs(), false);
  base::RunLoop().RunUntilIdle();

  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, ""}});
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  robots_rules_server_.VerifyRequestedOrigins({});
  image_compression_server_.VerifyRequestedImagePaths({});
}

IN_PROC_BROWSER_TEST_F(SubresourceRedirectLoginRobotsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(NoTriggerInIncognito)) {
  auto* incognito_browser = CreateIncognitoBrowser();

  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, ""}});
  NavigateAndWaitForLoad(incognito_browser,
                         GetHttpsTestURL("/load_image/image.html"));

  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  robots_rules_server_.VerifyRequestedOrigins({});
  image_compression_server_.VerifyRequestedImagePaths({});
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestRobotsRulesFetchTimeout)) {
  robots_rules_server_.set_failure_mode(
      RobotsRulesTestServer::FailureMode::kTimeout);
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, ""}});
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  // The image will start redirect and pause when robots rules are getting
  // fetched. But when the fetch timesout, it will reset and fetch the original
  // URL.
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);

  // Wait until the robots rules fetch times-out.
  RetryForHistogramUntilCountReached(
      &histogram_tester_, "SubresourceRedirect.RobotsRulesFetcher.ResponseCode",
      1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", false, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths({});
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestOneImageAllowedOneDisallowed)) {
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeDisallow, "*foo"}});
  NavigateAndWaitForLoad(browser(),
                         GetHttpsTestURL("/load_image/two_images.html"));

  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", false, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png"});
}

IN_PROC_BROWSER_TEST_F(SubresourceRedirectLoginRobotsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(TestTwoImagesAllowed)) {
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, ""}});
  NavigateAndWaitForLoad(browser(),
                         GetHttpsTestURL("/load_image/two_images.html"));

  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", false, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png", "/load_image/image.png?foo"});
}

// Verify an new image loads fine after robots rules fetch is complete.
IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestImageLoadAfterRobotsFetch)) {
  robots_rules_server_.AddRobotsRules(
      GetHttpsTestURL("/"),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", true, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", false, 1);

  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png"});

  // Load another image and that will be immediately redirected as well.
  EXPECT_TRUE(RunScriptExtractBool(R"(loadNewImage("image.png?foo"))"));
  FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);
  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  // No more new robots rules fetches.
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", 1);
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png", "/load_image/image.png?foo"});
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestDifferentOriginImageLoad)) {
  robots_rules_server_.AddRobotsRules(
      GetHttpsTestURL("/"),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotRulesDecider.ApplyDuration", 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", false, 1);

  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png"});

  // Load a compressible image from different origin and that will trigger
  // robots rules fetch.
  robots_rules_server_.AddRobotsRules(
      https_test_server_.GetURL("differentorigin.com", "/"),
      {{kRuleTypeDisallow, "*disallowed*"}});
  EXPECT_TRUE(RunScriptExtractBool(content::JsReplace(
      "loadNewImage($1)",
      https_test_server_.GetURL("differentorigin.com",
                                "/load_image/image.png?allowed"))));
  FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotRulesDecider.ApplyDuration", 2);

  // Another robots rules fetch happened.
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", 2);
  robots_rules_server_.VerifyRequestedOrigins(
      {GetHttpsTestURL("/").spec(),
       https_test_server_.GetURL("differentorigin.com", "/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png", "/load_image/image.png?allowed"});

  // Load a disallowed image from the different origin.
  EXPECT_TRUE(RunScriptExtractBool(content::JsReplace(
      "loadNewImage($1)",
      https_test_server_.GetURL("differentorigin.com",
                                "/load_image/image.png?disallowed"))));
  FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 4);

  // No more new robots rules fetches.
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", 2);
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png", "/load_image/image.png?allowed"});
}

// Verifies that LitePages gets blocked due to robots fetch failure, and
// subsequent robots rules fetch does not happen.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectLoginRobotsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(TestRobotsFetchLoadshed)) {
  robots_rules_server_.set_failure_mode(
      RobotsRulesTestServer::FailureMode::kLoadshed503RetryAfterResponse);
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  // One robots rules fetch failure should result in LitePages block.
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode",
      net::HTTP_SERVICE_UNAVAILABLE, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", false, 1);
  // Bypass check happens twice - once for pageload, and once for robots
  // fetch.
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LitePagesService.BypassResult", false, 2);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotRulesDecider.ApplyDuration", 0);

  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths({});

  // Load an image from different origin and that should not trigger robots
  // rules fetch, since LitePages is blocked.
  EXPECT_TRUE(RunScriptExtractBool(content::JsReplace(
      "loadNewImage($1)",
      https_test_server_.GetURL("differentorigin.com",
                                "/load_image/image.png?allowed"))));
  FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LitePagesService.BypassResult", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotRulesDecider.ApplyDuration", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", 1);
  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  // No more additional fetches.
  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths({});
}

// Verifies that when an image load fails, LitePages gets blocked, and
// subsequent robots rules fetch, LitePages image loads does not happen.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectLoginRobotsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(TestImageFetchLoadshed)) {
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, ""}});
  image_compression_server_.set_failure_mode(
      ImageCompressionTestServer::FailureMode::kLoadshed503RetryAfterResponse);
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));

  // Robots rules fetch was success.
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", false, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotRulesDecider.ApplyDuration", 1);

  // One compressed image fetch failed and then loaded directly.
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_SERVICE_UNAVAILABLE, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);

  // Bypass check happens twice - once for pageload, and once for robots
  // fetch.
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LitePagesService.BypassResult", false, 2);

  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png"});

  // Load an image from different origin and that should not trigger robots
  // rules fetch, since LitePages is blocked.
  EXPECT_TRUE(RunScriptExtractBool(content::JsReplace(
      "loadNewImage($1)",
      https_test_server_.GetURL("differentorigin.com",
                                "/load_image/image.png?allowed"))));
  FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LitePagesService.BypassResult", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotRulesDecider.ApplyDuration", 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", 1);
  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  // No more additional fetches.
  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png"});
}

}  // namespace subresource_redirect
