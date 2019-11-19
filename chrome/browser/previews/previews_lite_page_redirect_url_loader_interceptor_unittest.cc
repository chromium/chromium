// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_redirect_url_loader_interceptor.h"

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/previews/previews_lite_page_redirect_decider.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/previews/core/previews_features.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// TODO(crbug.com/961073): Fix memory leaks in tests and re-enable on LSAN.
#ifdef LEAK_SANITIZER
#define MAYBE_InterceptRequestPreviewsState_PreviewsOff \
  DISABLED_InterceptRequestPreviewsState_PreviewsOff
#define MAYBE_InterceptRequestPreviewsState_ProbeSuccess \
  DISABLED_InterceptRequestPreviewsState_ProbeSuccess
#define MAYBE_InterceptRequestPreviewsState_ProbeFail \
  DISABLED_InterceptRequestPreviewsState_ProbeFail
#else
#define MAYBE_InterceptRequestPreviewsState_PreviewsOff \
  InterceptRequestPreviewsState_PreviewsOff
#define MAYBE_InterceptRequestPreviewsState_ProbeSuccess \
  InterceptRequestPreviewsState_ProbeSuccess
#define MAYBE_InterceptRequestPreviewsState_ProbeFail \
  InterceptRequestPreviewsState_ProbeFail
#endif

namespace previews {

namespace {

const GURL kTestUrl("https://google.com/path");

class PreviewsLitePageRedirectURLLoaderInterceptorTest : public testing::Test {
 public:
  PreviewsLitePageRedirectURLLoaderInterceptorTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        shared_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}
  ~PreviewsLitePageRedirectURLLoaderInterceptorTest() override {}

  void TearDown() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        "add-chrome-proxy-header-for-lpr-tests");

    interceptor_ =
        std::make_unique<PreviewsLitePageRedirectURLLoaderInterceptor>(
            shared_factory_, 1, 2);

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kLitePageServerPreviews, {{"should_probe_origin", "true"}});
  }

  void SetFakeResponse(const GURL& url,
                       const std::string& data,
                       net::HttpStatusCode code,
                       int net_error) {
    test_url_loader_factory_.AddResponse(
        url, network::CreateURLResponseHead(code), data,
        network::URLLoaderCompletionStatus(net_error));
  }

  void SetProbeResponse(const GURL& url,
                        net::HttpStatusCode code,
                        int net_error) {
    test_url_loader_factory_.AddResponse(
        url, network::CreateURLResponseHead(code), "data",
        network::URLLoaderCompletionStatus(net_error));
  }

  void HandlerCallback(
      content::URLLoaderRequestInterceptor::RequestHandler callback) {
    callback_was_empty_ = callback.is_null();
  }

  base::Optional<bool> callback_was_empty() { return callback_was_empty_; }

  PreviewsLitePageRedirectURLLoaderInterceptor& interceptor() {
    return *interceptor_;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 private:
  base::Optional<bool> callback_was_empty_;

  base::test::ScopedFeatureList scoped_feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  std::unique_ptr<PreviewsLitePageRedirectURLLoaderInterceptor> interceptor_;
};

// Check that we don't trigger when previews are not allowed.
TEST_F(PreviewsLitePageRedirectURLLoaderInterceptorTest,
       InterceptRequestPreviewsState_PreviewsOff) {
  base::HistogramTester histogram_tester;

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";

  SetFakeResponse(request.url, "Fake Body", net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);

  request.previews_state = content::PREVIEWS_OFF;
  interceptor().MaybeCreateLoader(
      request, nullptr,
      base::BindOnce(
          &PreviewsLitePageRedirectURLLoaderInterceptorTest::HandlerCallback,
          base::Unretained(this)));

  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.URLLoader.Attempted", false, 1);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_was_empty().has_value());
  EXPECT_TRUE(callback_was_empty().value());
}

// Check that we trigger when previews are allowed and the probe is successful.
TEST_F(PreviewsLitePageRedirectURLLoaderInterceptorTest,
       MAYBE_InterceptRequestPreviewsState_ProbeSuccess) {
  base::HistogramTester histogram_tester;

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";

  SetFakeResponse(GetLitePageRedirectURLForURL(request.url), "Fake Body",
                  net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  SetProbeResponse(request.url.GetOrigin(), net::HTTP_OK,
                   net::URLRequestStatus::SUCCESS);

  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  interceptor().MaybeCreateLoader(
      request, nullptr,
      base::BindOnce(
          &PreviewsLitePageRedirectURLLoaderInterceptorTest::HandlerCallback,
          base::Unretained(this)));

  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.URLLoader.Attempted", true, 1);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_was_empty().has_value());
  EXPECT_FALSE(callback_was_empty().value());
  LOG(ERROR) << "test end";
}

TEST_F(PreviewsLitePageRedirectURLLoaderInterceptorTest,
       InterceptRequestPreviewsState_ProbeFail) {
  base::HistogramTester histogram_tester;

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";

  SetFakeResponse(GetLitePageRedirectURLForURL(request.url), "Fake Body",
                  net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  SetProbeResponse(request.url.GetOrigin(), net::HTTP_OK,
                   net::URLRequestStatus::FAILED);

  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  interceptor().MaybeCreateLoader(
      request, nullptr,
      base::BindOnce(
          &PreviewsLitePageRedirectURLLoaderInterceptorTest::HandlerCallback,
          base::Unretained(this)));

  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.URLLoader.Attempted", true, 1);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_was_empty().has_value());
  EXPECT_TRUE(callback_was_empty().value());
}

TEST_F(PreviewsLitePageRedirectURLLoaderInterceptorTest,
       InterceptRequestRedirect) {
  base::HistogramTester histogram_tester;
  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";
  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  SetFakeResponse(GetLitePageRedirectURLForURL(request.url), "Fake Body",
                  net::HTTP_TEMPORARY_REDIRECT, net::URLRequestStatus::SUCCESS);
  SetProbeResponse(request.url.GetOrigin(), net::HTTP_OK,
                   net::URLRequestStatus::SUCCESS);

  interceptor().MaybeCreateLoader(
      request, nullptr,
      base::BindOnce(
          &PreviewsLitePageRedirectURLLoaderInterceptorTest::HandlerCallback,
          base::Unretained(this)));

  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.URLLoader.Attempted", true, 1);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_was_empty().has_value());
  EXPECT_TRUE(callback_was_empty().value());
}

TEST_F(PreviewsLitePageRedirectURLLoaderInterceptorTest,
       InterceptRequestServerOverloaded) {
  base::HistogramTester histogram_tester;
  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";
  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  SetFakeResponse(GetLitePageRedirectURLForURL(request.url), "Fake Body",
                  net::HTTP_SERVICE_UNAVAILABLE,
                  net::URLRequestStatus::SUCCESS);
  SetProbeResponse(request.url.GetOrigin(), net::HTTP_OK,
                   net::URLRequestStatus::SUCCESS);

  interceptor().MaybeCreateLoader(
      request, nullptr,
      base::BindOnce(
          &PreviewsLitePageRedirectURLLoaderInterceptorTest::HandlerCallback,
          base::Unretained(this)));

  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.URLLoader.Attempted", true, 1);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_was_empty().has_value());
  EXPECT_TRUE(callback_was_empty().value());
}

TEST_F(PreviewsLitePageRedirectURLLoaderInterceptorTest,
       InterceptRequestServerNotHandling) {
  base::HistogramTester histogram_tester;
  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";
  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  SetFakeResponse(GetLitePageRedirectURLForURL(request.url), "Fake Body",
                  net::HTTP_FORBIDDEN, net::URLRequestStatus::SUCCESS);
  SetProbeResponse(request.url.GetOrigin(), net::HTTP_OK,
                   net::URLRequestStatus::SUCCESS);

  interceptor().MaybeCreateLoader(
      request, nullptr,
      base::BindOnce(
          &PreviewsLitePageRedirectURLLoaderInterceptorTest::HandlerCallback,
          base::Unretained(this)));

  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.URLLoader.Attempted", true, 1);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_was_empty().has_value());
  EXPECT_TRUE(callback_was_empty().value());
}

TEST_F(PreviewsLitePageRedirectURLLoaderInterceptorTest, NetStackError) {
  base::HistogramTester histogram_tester;
  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";
  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  SetFakeResponse(GetLitePageRedirectURLForURL(request.url), "Fake Body",
                  net::HTTP_OK, net::URLRequestStatus::FAILED);
  SetProbeResponse(request.url.GetOrigin(), net::HTTP_OK,
                   net::URLRequestStatus::SUCCESS);

  interceptor().MaybeCreateLoader(
      request, nullptr,
      base::BindOnce(
          &PreviewsLitePageRedirectURLLoaderInterceptorTest::HandlerCallback,
          base::Unretained(this)));

  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.URLLoader.Attempted", true, 1);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_was_empty().has_value());
  EXPECT_TRUE(callback_was_empty().value());
}

TEST(PreviewsGetLitePageRedirectURL, TestGetPreviewsURL) {
  struct TestCase {
    std::string previews_host;
    std::string original_url;
    std::string expected_previews_url;
    std::string experiment_variation;
    std::string experiment_cmd_line;
  };
  const TestCase kTestCases[]{
      // Use https://play.golang.org/p/HUM2HxmUTOW to compute
      // |expected_previews_url|.
      {
          "https://previews.host.com",
          "https://original.host.com/path/path/path?query=yes",
          "https://shta44dh4bi7rc6fnpjnkrtytwlabygjhk53v2trlot2wddylwua."
          "previews.host.com/p?u="
          "https%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
          "",
          "",
      },
      {
          "https://previews.host.com",
          "http://original.host.com/path/path/path?query=yes",
          "https://6p7dar4ju6r4ynz7x3pucmlcltuqsf7z5auhvckzln7voglkt56q."
          "previews.host.com/p?u="
          "http%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
          "",
      },
      {
          "https://previews.host.com",
          "https://original.host.com:1443/path/path/path?query=yes",
          "https://mil6oxtqb4zpsbmutm4d7wrx5nlr6tzlxjp7y44u55zqhzsdzjpq."
          "previews.host.com/p?u=https%3A%2F%2Foriginal.host.com%3A1443"
          "%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
          "",
          "",
      },
      {
          "https://previews.host.com:1443",
          "http://original.host.com/path/path/path?query=yes",
          "https://6p7dar4ju6r4ynz7x3pucmlcltuqsf7z5auhvckzln7voglkt56q."
          "previews.host.com:1443/p?u="
          "http%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
          "",
          "",
      },
      {
          "https://previews.host.com:1443",
          "https://original.host.com:1443/path/path/path?query=yes",
          "https://mil6oxtqb4zpsbmutm4d7wrx5nlr6tzlxjp7y44u55zqhzsdzjpq."
          "previews.host.com:1443/p?u=https%3A%2F%2Foriginal.host.com%3A1443"
          "%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
          "",
          "",
      },
      {
          "https://previews.host.com",
          "https://original.host.com/path/path/path?query=yes#fragment",
          "https://shta44dh4bi7rc6fnpjnkrtytwlabygjhk53v2trlot2wddylwua."
          "previews.host.com/p?u="
          "https%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes"
          "#fragment",
          "",
          "",
      },
      {
          "https://previews.host.com",
          "https://original.host.com/path/path/path?query=yes",
          "https://shta44dh4bi7rc6fnpjnkrtytwlabygjhk53v2trlot2wddylwua."
          "previews.host.com/p?u="
          "https%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes"
          "&x=variation_experiment",
          "variation_experiment",
          "",
      },
      {
          // Ensure that the command line experiment takes precedence over the
          // one provided by variations.
          "https://previews.host.com",
          "https://original.host.com/path/path/path?query=yes",
          "https://shta44dh4bi7rc6fnpjnkrtytwlabygjhk53v2trlot2wddylwua."
          "previews.host.com/p?u="
          "https%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes"
          "&x=cmdline_experiment",
          "variation_experiment",
          "cmdline_experiment",
      },
      {
          "https://previews.host.com",
          "https://[::1]:12345",
          "https://2ikmbopbfxagkb7uer2vgfxmbzu2vw4qq3d3ixe3h2hfhgcabvua."
          "previews.host.com/p?u=https%3A%2F%2F%5B%3A%3A1%5D%3A12345%2F",
          "",
          "",
      },
  };

  for (const TestCase& test_case : kTestCases) {
    variations::testing::ClearAllVariationParams();

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        data_reduction_proxy::switches::kDataReductionProxyExperiment,
        test_case.experiment_cmd_line);

    base::test::ScopedFeatureList scoped_feature_list;
    std::map<std::string, std::string> server_experiment_params;
    server_experiment_params[data_reduction_proxy::params::
                                 GetDataSaverServerExperimentsOptionName()] =
        test_case.experiment_variation;

    scoped_feature_list.InitWithFeaturesAndParameters(
        {{data_reduction_proxy::features::kDataReductionProxyServerExperiments,
          {server_experiment_params}},
         {previews::features::kLitePageServerPreviews,
          {{"previews_host", test_case.previews_host}}}},
        {});

    EXPECT_EQ(GetLitePageRedirectURLForURL(GURL(test_case.original_url)),
              GURL(test_case.expected_previews_url));
  }
}

}  // namespace

}  // namespace previews
