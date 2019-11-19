// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/escape.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"

namespace {

// TODO(rajendrant): Add tests to verify subresource redirect is applied only
// for data saver users and also not applied for incognito profiles.

// Retries fetching |histogram_name| until it contains at least |count| samples.
// TODO(rajendrant): Convert the tests to wait for image load to complete or the
// page load complete, instead of waiting on the histograms.
void RetryForHistogramUntilCountReached(base::HistogramTester* histogram_tester,
                                        const std::string& histogram_name,
                                        size_t count) {
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    content::FetchHistogramsFromChildProcesses();
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

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

class SubresourceRedirectBrowserTest : public InProcessBrowserTest {
 public:
  explicit SubresourceRedirectBrowserTest(
      const std::string& included_path_suffixes = "")
      : included_path_suffixes_(included_path_suffixes),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        compression_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUp() override {
    // |http_server| setup.
    http_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(http_server_.Start());
    http_url_ = http_server_.GetURL("insecure.com", "/");
    ASSERT_TRUE(http_url_.SchemeIs(url::kHttpScheme));

    // |https_server| setup.
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_server_.Start());
    https_url_ = https_server_.GetURL("secure.com", "/");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    // |compression_server| setup.
    compression_server_.RegisterRequestHandler(base::BindRepeating(
        &SubresourceRedirectBrowserTest::HandleCompressionServerRequest,
        base::Unretained(this)));
    ASSERT_TRUE(compression_server_.Start());
    compression_url_ = compression_server_.GetURL("compression.com", "/");
    ASSERT_TRUE(compression_url_.SchemeIs(url::kHttpsScheme));

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kSubresourceRedirect,
        {{"included_path_suffixes", included_path_suffixes_},
         {"lite_page_subresource_origin", compression_url_.spec()}});

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    //  Need to resolve all 3 of the above servers to 127.0.0.1:port, and
    //  the servers themselves can't serve using 127.0.0.1:port as the
    //  compressed resource URLs rely on subdomains, and subdomains
    //  do not function properly when using 127.0.0.1:port
    command_line->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
  }

  void EnableDataSaver(bool enabled) {
    Profile* profile = Profile::FromBrowserContext(browser()->profile());

    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(profile->GetPrefs(), enabled);
    base::RunLoop().RunUntilIdle();
  }

  bool RunScriptExtractBool(const std::string& script,
                            content::WebContents* web_contents = nullptr) {
    bool result;
    if (!web_contents)
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(ExecuteScriptAndExtractBool(web_contents, script, &result));
    return result;
  }

  std::string RunScriptExtractString(
      const std::string& script,
      content::WebContents* web_contents = nullptr) {
    if (!web_contents)
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    std::string result;
    EXPECT_TRUE(ExecuteScriptAndExtractString(web_contents, script, &result));
    return result;
  }

  GURL http_url() const { return http_url_; }
  GURL https_url() const { return https_url_; }
  GURL compression_url() const { return compression_url_; }
  GURL request_url() const { return request_url_; }

  GURL HttpURLWithPath(const std::string& path) {
    return http_server_.GetURL("insecure.com", path);
  }
  GURL HttpsURLWithPath(const std::string& path) {
    return https_server_.GetURL("secure.com", path);
  }

  void SetCompressionServerToFail() { compression_server_fail_ = true; }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_.ShutdownAndWaitUntilComplete());

    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Called by |compression_server_|.
  std::unique_ptr<net::test_server::HttpResponse>
  HandleCompressionServerRequest(const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    request_url_ = request.GetURL();

    // If |compression_server_fail_| is set to true, return a hung response.
    if (compression_server_fail_ == true) {
      return std::make_unique<net::test_server::RawHttpResponse>("", "");
    }

    // For the purpose of this browsertest, a redirect to the compression server
    // that is looking to access image.png will be treated as though it is
    // compressed.  All other redirects will be assumed failures to retrieve the
    // requested resource and return a redirect to private_url_image.png.
    if (request.GetURL().query().find(
            net::EscapeQueryParamValue("/image.png", true /* use_plus */), 0) !=
        std::string::npos) {
      response->set_code(net::HTTP_OK);
    } else if (request.GetURL().query().find(
                   net::EscapeQueryParamValue("/fail_image.png",
                                              true /* use_plus */),
                   0) != std::string::npos) {
      response->set_code(net::HTTP_NOT_FOUND);
    } else {
      response->set_code(net::HTTP_TEMPORARY_REDIRECT);
      response->AddCustomHeader(
          "Location",
          HttpsURLWithPath("/load_image/private_url_image.png").spec());
    }
    return std::move(response);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  const std::string included_path_suffixes_;

  GURL compression_url_;
  GURL http_url_;
  GURL https_url_;
  GURL request_url_;

  net::EmbeddedTestServer http_server_;
  net::EmbeddedTestServer https_server_;
  net::EmbeddedTestServer compression_server_;

  base::HistogramTester histogram_tester_;

  bool compression_server_fail_ = false;

  DISALLOW_COPY_AND_ASSIGN(SubresourceRedirectBrowserTest);
};

class DifferentMediaInclusionSubresourceRedirectBrowserTest
    : public SubresourceRedirectBrowserTest {
 public:
  DifferentMediaInclusionSubresourceRedirectBrowserTest()
      : SubresourceRedirectBrowserTest(".svg") {}
};

//  NOTE: It is indirectly verified that correct requests are being sent to
//  the mock compression server by the counts in the histogram bucket for
//  HTTP_TEMPORARY_REDIRECTs.

//  This test loads image.html, which triggers a subresource request
//  for image.png.  This triggers an internal redirect to the mocked
//  compression server, which responds with HTTP_OK.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       TestHTMLLoadRedirectSuccess) {
  EnableDataSaver(true);
  ui_test_utils::NavigateToURL(browser(),
                               HttpsURLWithPath("/load_image/image.html"));

  RetryForHistogramUntilCountReached(
      histogram_tester(), "SubresourceRedirect.CompressionAttempt.ResponseCode",
      2);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", net::HTTP_OK, 1);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  EXPECT_EQ(request_url().port(), compression_url().port());
}

//  This test loads private_url_image.html, which triggers a subresource
//  request for private_url_image.png.  This triggers an internal redirect
//  to the mock compression server, which bypasses the request. The
//  mock compression server creates a redirect to the original resource.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       TestHTMLLoadRedirectBypass) {
  EnableDataSaver(true);
  ui_test_utils::NavigateToURL(
      browser(), HttpsURLWithPath("/load_image/private_url_image.html"));

  RetryForHistogramUntilCountReached(
      histogram_tester(), "SubresourceRedirect.CompressionAttempt.ResponseCode",
      2);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());
}

IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       NoTriggerWhenDataSaverOff) {
  EnableDataSaver(false);
  ui_test_utils::NavigateToURL(browser(),
                               HttpsURLWithPath("/load_image/image.html"));

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());
}

IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest, NoTriggerInIncognito) {
  EnableDataSaver(true);
  auto* incognito_browser = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(incognito_browser,
                               HttpsURLWithPath("/load_image/image.html"));

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);

  EXPECT_TRUE(RunScriptExtractBool(
      "checkImage()",
      incognito_browser->tab_strip_model()->GetActiveWebContents()));

  EXPECT_EQ(
      GURL(RunScriptExtractString(
               "imageSrc()",
               incognito_browser->tab_strip_model()->GetActiveWebContents()))
          .port(),
      https_url().port());
}

//  This test loads image.html, from a non secure site. This triggers a
//  subresource request, but no internal redirect should be created for
//  non-secure sites.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       NoTriggerOnNonSecureSite) {
  EnableDataSaver(true);
  ui_test_utils::NavigateToURL(browser(),
                               HttpURLWithPath("/load_image/image.html"));

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            http_url().port());
}

//  This test loads page_with_favicon.html, which creates a subresource
//  request for icon.png.  There should be no internal redirect as favicons
//  are not considered images by chrome.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest, NoTriggerOnNonImage) {
  EnableDataSaver(true);
  ui_test_utils::NavigateToURL(
      browser(), HttpsURLWithPath("/favicon/page_with_favicon.html"));

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);
}

}  // namespace

// This test loads a resource that will return a 404 from the server, this
// should trigger the fallback logic back to the original resource. In total
// This results in 2 redirects (to the compression server, and back to the
// original resource), 1 404 not-found from the compression server, and 1
// 200 ok from the original resource.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       FallbackOnServerNotFound) {
  EnableDataSaver(true);
  ui_test_utils::NavigateToURL(browser(),
                               HttpsURLWithPath("/load_image/fail_image.html"));

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 3);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 2);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_NOT_FOUND, 1);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());
}

//  This test verifies that the client will utilize the fallback logic if the
//  server/network fails and returns nothing.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       FallbackOnServerFailure) {
  EnableDataSaver(true);
  SetCompressionServerToFail();

  base::RunLoop().RunUntilIdle();
  ui_test_utils::NavigateToURL(browser(),
                               HttpsURLWithPath("/load_image/image.html"));

  RetryForHistogramUntilCountReached(
      histogram_tester(),
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 1);

  histogram_tester()->ExpectBucketCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", false, 1);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());
}

//  This test loads image_js.html, which triggers a javascript request
//  for image.png for which subresource redirect will not be attempted.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectBrowserTest,
                       NoTriggerOnJavaScriptImageRequest) {
  EnableDataSaver(true);
  ui_test_utils::NavigateToURL(browser(),
                               HttpsURLWithPath("/load_image/image_js.html"));

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());
}

//  This test loads image.html, but with
//  SubresourceRedirectIncludedMediaSuffixes set to only allow .svg, so no
//  internal redirect should occur.
IN_PROC_BROWSER_TEST_F(DifferentMediaInclusionSubresourceRedirectBrowserTest,
                       NoTriggerWhenNotIncludedInMediaSuffixes) {
  EnableDataSaver(true);
  ui_test_utils::NavigateToURL(browser(),
                               HttpsURLWithPath("/load_image/image.html"));

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester()->ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ResponseCode", 0);

  EXPECT_TRUE(RunScriptExtractBool("checkImage()"));

  EXPECT_EQ(GURL(RunScriptExtractString("imageSrc()")).port(),
            https_url().port());
}
