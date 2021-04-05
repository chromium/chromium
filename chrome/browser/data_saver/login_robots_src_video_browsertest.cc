// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/login_detection/login_detection_type.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_redirect/subresource_redirect_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_redirect/common/subresource_redirect_result.h"
#include "components/subresource_redirect/subresource_redirect_browser_test_util.h"
#include "components/subresource_redirect/subresource_redirect_test_util.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace subresource_redirect {

class LoginRobotsSrcVideoBrowserTest : public InProcessBrowserTest {
 public:
  explicit LoginRobotsSrcVideoBrowserTest(
      bool enable_lite_mode = true,
      bool enable_src_video_compression_metrics_feature = true)
      : enable_lite_mode_(enable_lite_mode),
        enable_src_video_compression_metrics_feature_(
            enable_src_video_compression_metrics_feature),
        https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~LoginRobotsSrcVideoBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
    if (enable_lite_mode_)
      command_line->AppendSwitch("enable-spdy-proxy-auth");

    // Disable infobar shown check to actually compress the pages.
    command_line->AppendSwitch("override-https-image-compression-infobar");
  }

  void SetUp() override {
    ASSERT_TRUE(robots_rules_server_.Start());
    https_test_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    https_test_server_.RegisterRequestMonitor(base::BindRepeating(
        &LoginRobotsSrcVideoBrowserTest::OnHttpsTestServerRequestMonitor,
        base::Unretained(this)));
    ASSERT_TRUE(https_test_server_.Start());

    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features;
    if (enable_src_video_compression_metrics_feature_) {
      base::FieldTrialParams params, login_detection_params;
      params["lite_page_robots_origin"] = robots_rules_server_.GetURL();
      enabled_features.emplace_back(
          blink::features::kSubresourceRedirectSrcVideo, params);
      login_detection_params["logged_in_sites"] = "https://loggedin.com";
      enabled_features.emplace_back(login_detection::kLoginDetection,
                                    login_detection_params);
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ResetMetricsRecorders();
  }

  GURL GetHttpsTestURL(const std::string& path) const {
    return https_test_server_.GetURL("test_https_server.com", path);
  }

  // Navigatest to the page with <video> element, starts the play and waits for
  // it to complete.
  void NavigateAndWaitForLoad(Browser* browser, const GURL& url) {
    ui_test_utils::NavigateToURL(browser, url);
    EXPECT_TRUE(ExecJs(browser->tab_strip_model()->GetActiveWebContents(),
                       "playVideo()"));
    RetryForHistogramUntilCountReached(
        histogram_tester_.get(), "Media.WatchTime.AudioVideo.Discarded.SRC", 1);
    FetchHistogramsFromChildProcesses();
  }

  bool RunScriptExtractBool(const std::string& script,
                            content::WebContents* web_contents = nullptr) {
    if (!web_contents)
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    return EvalJs(web_contents, script).ExtractBool();
  }

  void ResetMetricsRecorders() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  std::map<uint64_t, size_t> GetSrcVideoCompressionUkmMetrics() {
    base::RunLoop().RunUntilIdle();
    using SrcVideoUkm =
        ukm::builders::SubresourceRedirect_PublicSrcVideoCompression;
    std::map<uint64_t, size_t> merged_metrics;
    // Flatten the metrics from multiple ukm sources.
    for (const auto* metrics :
         ukm_recorder_->GetEntriesByName(SrcVideoUkm::kEntryName)) {
      for (const auto& metric : metrics->metrics) {
        if (merged_metrics.find(metric.first) == merged_metrics.end())
          merged_metrics[metric.first] = metric.second;
      }
    }
    return merged_metrics;
  }

  // Verifies image compression page info is not shown when src video coverage
  // metrics recording is enabled.
  void VerifyImageCompressionPageInfoNotShown(
      content::WebContents* web_contents = nullptr) {
    if (!web_contents)
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_FALSE(subresource_redirect::SubresourceRedirectObserver::
                     IsHttpsImageCompressionApplied(web_contents));
  }

 protected:
  // Called for the main origin server requests, to track requests that are
  // being made directly instead of redirecting to LitePages.
  void OnHttpsTestServerRequestMonitor(
      const net::test_server::HttpRequest& request) {
    received_https_test_server_requests_.insert(request.GetURL());
  }

  bool enable_lite_mode_;
  bool enable_src_video_compression_metrics_feature_;

  base::test::ScopedFeatureList scoped_feature_list_;

  // Simulates the LitePages servers that return the robots rules and compress
  // images.
  RobotsRulesTestServer robots_rules_server_;
  net::EmbeddedTestServer https_test_server_;

  // All the origins the robots rules are requested for.
  std::set<GURL> received_https_test_server_requests_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

// Enable tests for linux since LiteMode is enabled only for Android.
#if defined(OS_WIN) || defined(OS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

IN_PROC_BROWSER_TEST_F(
    LoginRobotsSrcVideoBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestSrcVideoAllowedByRobots)) {
  robots_rules_server_.AddRobotsRules(
      GetHttpsTestURL("/"), {{kRuleTypeAllow, ""}, {kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(browser(),
                         GetHttpsTestURL("/load_image/src_video.html"));

  // The first video load should fetch the robots rules, but the robots rules
  // check for the src video may timeout. So, do not check for coverage metrics.
  histogram_tester_->ExpectUniqueSample(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.SrcVideo.SubresourceRedirectResult", 1);
  histogram_tester_->ExpectUniqueSample(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", false, 1);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  // Verify the coverage metrics for the subsequent video load.
  ResetMetricsRecorders();

  EXPECT_TRUE(ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                     "playVideo('/android/media/test.webm?second')"));
  RetryForHistogramUntilCountReached(
      histogram_tester_.get(), "Media.WatchTime.AudioVideo.Discarded.SRC", 1);
  FetchHistogramsFromChildProcesses();

  histogram_tester_->ExpectUniqueSample(
      "SubresourceRedirect.SrcVideo.SubresourceRedirectResult",
      SubresourceRedirectResult::kRedirectable, 1);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.SrcVideo.CompressibleFullContentBytes", 1);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  auto full_content_length = histogram_tester_->GetAllSamples(
      "SubresourceRedirect.SrcVideo.CompressibleFullContentBytes");
  EXPECT_EQ(1U, full_content_length.size());
  EXPECT_EQ(1, full_content_length[0].count);
  EXPECT_LT(50000, full_content_length[0].min);

  using SrcVideoUkm =
      ukm::builders::SubresourceRedirect_PublicSrcVideoCompression;
  auto ukm_metrics = GetSrcVideoCompressionUkmMetrics();
  EXPECT_EQ(SubresourceRedirectResult::kRedirectable,
            static_cast<SubresourceRedirectResult>(
                ukm_metrics[SrcVideoUkm::kSubresourceRedirectResultNameHash]));
  EXPECT_LT(50000U, ukm_metrics[SrcVideoUkm::kFullContentLengthNameHash]);

  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});

  // Verify the video requests were fetched from the origin, and no compression
  // redirect happened.
  EXPECT_TRUE(
      base::Contains(received_https_test_server_requests_,
                     https_test_server_.GetURL("/android/media/test.webm")));
  EXPECT_TRUE(base::Contains(
      received_https_test_server_requests_,
      https_test_server_.GetURL("/android/media/test.webm?second")));

  VerifyImageCompressionPageInfoNotShown();
}

IN_PROC_BROWSER_TEST_F(LoginRobotsSrcVideoBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(NoTriggerWhenDataSaverOff)) {
  data_reduction_proxy::DataReductionProxySettings::
      SetDataSaverEnabledForTesting(browser()->profile()->GetPrefs(), false);
  base::RunLoop().RunUntilIdle();

  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, ""}});
  NavigateAndWaitForLoad(browser(),
                         GetHttpsTestURL("/load_image/src_video.html"));

  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.SrcVideo.SubresourceRedirectResult", 0);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.SrcVideo.CompressibleFullContentBytes", 0);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", 0);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", 0);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  EXPECT_TRUE(GetSrcVideoCompressionUkmMetrics().empty());
  robots_rules_server_.VerifyRequestedOrigins({});

  VerifyImageCompressionPageInfoNotShown();
}

IN_PROC_BROWSER_TEST_F(LoginRobotsSrcVideoBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(NoTriggerInIncognito)) {
  auto* incognito_browser = CreateIncognitoBrowser();

  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, ""}});
  NavigateAndWaitForLoad(incognito_browser,
                         GetHttpsTestURL("/load_image/src_video.html"));

  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.SrcVideo.SubresourceRedirectResult", 0);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.SrcVideo.CompressibleFullContentBytes", 0);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", 0);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", 0);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  EXPECT_TRUE(GetSrcVideoCompressionUkmMetrics().empty());
  robots_rules_server_.VerifyRequestedOrigins({});

  VerifyImageCompressionPageInfoNotShown(
      incognito_browser->tab_strip_model()->GetActiveWebContents());
}

IN_PROC_BROWSER_TEST_F(
    LoginRobotsSrcVideoBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TestSrcVideoDisallowedByRobots)) {
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeDisallow, ""}});
  NavigateAndWaitForLoad(browser(),
                         GetHttpsTestURL("/load_image/src_video.html"));

  // The first video load should fetch the robots rules, but the robots rules
  // check for the src video may timeout. So, do not check for coverage metrics.
  histogram_tester_->ExpectUniqueSample(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", net::HTTP_OK, 1);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.SrcVideo.SubresourceRedirectResult", 1);
  histogram_tester_->ExpectUniqueSample(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", false, 1);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  // Verify the for the subsequent video load is ineligible for compression.
  ResetMetricsRecorders();

  EXPECT_TRUE(ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                     "playVideo('/android/media/test.webm?second')"));
  RetryForHistogramUntilCountReached(
      histogram_tester_.get(), "Media.WatchTime.AudioVideo.Discarded.SRC", 1);
  FetchHistogramsFromChildProcesses();

  histogram_tester_->ExpectUniqueSample(
      "SubresourceRedirect.SrcVideo.SubresourceRedirectResult",
      SubresourceRedirectResult::kIneligibleRobotsDisallowed, 1);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.SrcVideo.CompressibleFullContentBytes", 0);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", 0);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", 0);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  using SrcVideoUkm =
      ukm::builders::SubresourceRedirect_PublicSrcVideoCompression;
  auto ukm_metrics = GetSrcVideoCompressionUkmMetrics();
  EXPECT_EQ(SubresourceRedirectResult::kIneligibleRobotsDisallowed,
            static_cast<SubresourceRedirectResult>(
                ukm_metrics[SrcVideoUkm::kSubresourceRedirectResultNameHash]));
  EXPECT_LT(50000U, ukm_metrics[SrcVideoUkm::kFullContentLengthNameHash]);

  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});
  VerifyImageCompressionPageInfoNotShown();
}

IN_PROC_BROWSER_TEST_F(
    LoginRobotsSrcVideoBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(NoCompressionOnLoggedInPage)) {
  robots_rules_server_.AddRobotsRules(
      https_test_server_.GetURL("loggedin.com", "/"), {{kRuleTypeAllow, ""}});
  NavigateAndWaitForLoad(
      browser(),
      https_test_server_.GetURL("loggedin.com", "/load_image/src_video.html"));

  histogram_tester_->ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleLoginDetected, 1);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.SrcVideo.CompressibleFullContentBytes", 0);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", 0);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit", 0);
  histogram_tester_->ExpectTotalCount(
      "SubresourceRedirect.ImageCompressionNotificationInfoBar", 0);

  using SrcVideoUkm =
      ukm::builders::SubresourceRedirect_PublicSrcVideoCompression;
  auto ukm_metrics = GetSrcVideoCompressionUkmMetrics();
  EXPECT_EQ(SubresourceRedirectResult::kIneligibleLoginDetected,
            static_cast<SubresourceRedirectResult>(
                ukm_metrics[SrcVideoUkm::kSubresourceRedirectResultNameHash]));
  EXPECT_LT(50000U, ukm_metrics[SrcVideoUkm::kFullContentLengthNameHash]);

  robots_rules_server_.VerifyRequestedOrigins({});
  VerifyImageCompressionPageInfoNotShown();
}

}  // namespace subresource_redirect
