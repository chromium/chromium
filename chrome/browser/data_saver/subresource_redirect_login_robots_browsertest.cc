// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/login_detection/login_detection_type.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_redirect/https_image_compression_infobar_decider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_redirect/subresource_redirect_browser_test_util.h"
#include "components/subresource_redirect/subresource_redirect_test_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace subresource_redirect {

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
      base::FieldTrialParams params, login_detection_params;
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
      login_detection_params["logged_in_sites"] = "https://loggedin.com";
      enabled_features.emplace_back(login_detection::kLoginDetection,
                                    login_detection_params);
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

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestNoCompressionOnLoggedInPage)) {
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, "*"}});
  // Trigger OAuth login by triggering OAuth start and complete.
  ui_test_utils::NavigateToURL(browser(),
                               GetHttpsTestURL("/simple.html?initial"));
  histogram_tester_.ExpectUniqueSample(
      "Login.PageLoad.DetectionType",
      login_detection::LoginDetectionType::kNoLogin, 1);
  ui_test_utils::NavigateToURL(
      browser(), https_test_server_.GetURL("oauth_server.com",
                                           "/simple.html?client_id=user"));
  histogram_tester_.ExpectBucketCount(
      "Login.PageLoad.DetectionType",
      login_detection::LoginDetectionType::kNoLogin, 2);

  ui_test_utils::NavigateToURL(browser(),
                               GetHttpsTestURL("/simple.html?code=123"));
  histogram_tester_.ExpectBucketCount(
      "Login.PageLoad.DetectionType",
      login_detection::LoginDetectionType::kOauthFirstTimeLoginFlow, 1);

  // The next navigation will be treated as logged-in.
  NavigateAndWaitForLoad(browser(), GetHttpsTestURL("/load_image/image.html"));
  histogram_tester_.ExpectBucketCount(
      "Login.PageLoad.DetectionType",
      login_detection::LoginDetectionType::kOauthLogin, 1);

  // No image compression will be triggered.
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

// Tests images in subframe are compressed.
IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestSubframeImageAllowedByRobots)) {
  robots_rules_server_.AddRobotsRules(
      GetHttpsTestURL("/"),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(browser(),
                         GetHttpsTestURL("/load_image/page_with_iframe.html"));
  EXPECT_EQ(true, EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                         "checkSubframeImage()"));
  FetchHistogramsFromChildProcesses();

  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", true, 2);
  // The robots rules are fetched once, since both images are from the same
  // origin.
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", false, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png?mainframe", "/load_image/image.png"});
}

// Tests images in cross-origin subframe are compressed.
IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestCrossOriginSubframeImageAllowedByRobots)) {
  robots_rules_server_.AddRobotsRules(
      GetHttpsTestURL("/"),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(
      browser(), GetHttpsTestURL(net::test_server::GetFilePathWithReplacements(
                     "/load_image/page_with_crossorigin_iframe.html",
                     {{"REPLACE_WITH_BASE_URL",
                       https_test_server_.GetURL("foo.com", "/").spec()}})));

  // Wait for the histograms, since javascript cannot be used to wait for
  // loading of the image in the subframe.
  RetryForHistogramUntilCountReached(
      &histogram_tester_,
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 2);

  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", true, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", net::HTTP_OK, 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", false, 2);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  robots_rules_server_.VerifyRequestedOrigins(
      {GetHttpsTestURL("/").spec(),
       https_test_server_.GetURL("foo.com", "/").spec()});
  image_compression_server_.VerifyRequestedImagePaths(
      {"/load_image/image.png?mainframe", "/load_image/image.png"});
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestLoggedInSubframeDisallowed)) {
  robots_rules_server_.AddRobotsRules(
      GetHttpsTestURL("/"),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(
      browser(),
      GetHttpsTestURL(net::test_server::GetFilePathWithReplacements(
          "/load_image/page_with_crossorigin_iframe.html",
          {{"REPLACE_WITH_BASE_URL",
            https_test_server_.GetURL("loggedin.com", "/").spec()}})));

  // Wait for the histograms, since javascript cannot be used to wait for
  // loading of the image in the crossorigin subframe.
  RetryForHistogramUntilCountReached(&histogram_tester_,
                                     "Blink.DecodedImageType", 2);

  // The image in mainframe will be compressed, while the subframe image will
  // not be compressed.
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
      {"/load_image/image.png?mainframe"});
}

IN_PROC_BROWSER_TEST_F(
    SubresourceRedirectLoginRobotsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestLoggedInMainframeDisallowsSubframe)) {
  robots_rules_server_.AddRobotsRules(
      https_test_server_.GetURL("loggedin.com", "/"),
      {{kRuleTypeAllow, "/load_image/image.png"}, {kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(
      browser(), https_test_server_.GetURL(
                     "loggedin.com", "/load_image/page_with_iframe.html"));

  // Wait for the histograms, since javascript cannot be used to wait for
  // loading of the image in the crossorigin subframe.
  RetryForHistogramUntilCountReached(&histogram_tester_,
                                     "Blink.DecodedImageType", 2);

  // The images in mainframe and  subframe will not be compressed.
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

}  // namespace subresource_redirect
