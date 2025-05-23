// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/loader/keep_alive_request_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/page_load_metrics/browser/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/keep_alive_request_tracker.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/keep_alive_url_loader_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/url_util.h"

namespace {

using content::EvalJs;
using content::JsReplace;
using testing::Contains;
using testing::Eq;
using testing::Key;
using testing::Not;

constexpr char kSRPEndpoing[] = "/search?q=test";

std::string GetRelativeCategoryUrl(std::string_view relative_url,
                                   std::string_view category) {
  const std::string sep =
      (std::string(relative_url).find("?") == std::string::npos) ? "?" : "&";
  return base::StrCat({relative_url, sep, "category=", category});
}

std::string GetKeepAliveCategoryRequestUrl(std::string_view category) {
  return GetRelativeCategoryUrl(kKeepAliveEndpoint, category);
}

// Always serves the page navigation-fetch-keepalive.html as the response to all
// navigation requests to search result pages.
std::unique_ptr<net::test_server::HttpResponse> HandleSearchResultPageRequest(
    const net::test_server::HttpRequest& request) {
  if (request.method != net::test_server::METHOD_GET) {
    return nullptr;
  }

  base::FilePath server_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &server_root);
  base::FilePath file_path(server_root.AppendASCII(
      "chrome/test/data/navigation-fetch-keepalive.html"));
  std::string file_contents;
  CHECK(base::ReadFileToString(file_path, &file_contents));

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  http_response->set_content(file_contents);
  return http_response;
}

// Returns 200 OK response to all fetch keepalive requests.
std::unique_ptr<net::test_server::HttpResponse> HandleKeepAliveRequest(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  http_response->set_content(k200TextResponse);
  return http_response;
}

}  // namespace

class ChromeKeepAliveCategoryRequestBrowserTestBase
    : public ChromeKeepAliveRequestBrowserTestBase,
      public content::KeepAliveRequestUkmMatcher,
      public content::NavigationKeepAliveRequestUkmMatcher {
 public:
  ChromeKeepAliveCategoryRequestBrowserTestBase()
      : ChromeKeepAliveRequestBrowserTestBase() {
    InitFeatureList({{blink::features::kKeepAliveInBrowserMigration, {}},
                     {page_load_metrics::features::kBeaconLeakageLogging,
                      {{"category_prefix", "test-prefix"}}}});
  }

 protected:
  void SetUpOnMainThread() override {
    ChromeKeepAliveRequestBrowserTestBase::SetUpOnMainThread();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  ukm::TestAutoSetUkmRecorder& ukm_recorder() override {
    return *ukm_recorder_;
  }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

// Browser tests to cover fetch keepalive request with specific "category" in
// the request URL param.
class ChromeKeepAliveCategoryRequestBrowserTest
    : public ChromeKeepAliveCategoryRequestBrowserTestBase,
      public testing::WithParamInterface<std::string> {
 protected:
  const std::string& method() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeKeepAliveCategoryRequestBrowserTest,
    ::testing::Values(net::HttpRequestHeaders::kGetMethod,
                      net::HttpRequestHeaders::kPostMethod),
    [](const testing::TestParamInfo<
        ChromeKeepAliveCategoryRequestBrowserTest::ParamType>& info) {
      return info.param;
    });

IN_PROC_BROWSER_TEST_P(ChromeKeepAliveCategoryRequestBrowserTest, OneRequest) {
  const std::string target_url = GetKeepAliveCategoryRequestUrl("test-prefix1");
  auto request_handler = std::move(RegisterRequestHandlers({target_url})[0]);
  ASSERT_TRUE(server()->Start());

  ASSERT_TRUE(NavigateToURL(
      web_contents(), GetKeepAlivePageURL(kPrimaryHost, target_url, method())));
  // Ensure the keepalive request is sent, but delay response.
  request_handler->WaitForRequest();

  // End the keepalive request by sending back response.
  request_handler->Send(k200TextResponse);
  request_handler->Done();

  content::TitleWatcher watcher(web_contents(), kPromiseResolvedPageTitle);
  EXPECT_EQ(watcher.WaitAndGetTitle(), kPromiseResolvedPageTitle);
  loaders_observer().WaitForTotalOnReceiveResponseForwarded(1);
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK});
  ExpectCommonUkm(
      content::KeepAliveRequestTracker::RequestType::kFetch,
      /*category_id=*/1,
      /*num_redirects=*/0,
      /*is_context_detached=*/false,
      content::KeepAliveRequestTracker::RequestStageType::kLoaderCompleted,
      content::KeepAliveRequestTracker::RequestStageType::kResponseReceived,
      /*keepalive_token=*/std::nullopt,
      /*failed_error_code=*/std::nullopt,
      /*failed_extended_error_code=*/std::nullopt,
      /*completed_error_code=*/net::OK,
      /*completed_extended_error_code=*/0);
  ExpectTimeSortedTimeDeltaUkm(
      {"TimeDelta.RequestStarted", "TimeDelta.ResponseReceived",
       "TimeDelta.LoaderCompleted", "TimeDelta.EventLogged"});
  NavigationKeepAliveRequestUkmMatcher::ExpectNoUkm();
}

// Delays response to a keepalive ping until after the page making the keepalive
// ping has been unloaded. The browser must ensure the response is received and
// processed by the browser.
IN_PROC_BROWSER_TEST_P(ChromeKeepAliveCategoryRequestBrowserTest,
                       ReceiveResponseAfterPageUnload) {
  const std::string target_url = GetKeepAliveCategoryRequestUrl("test-prefix1");
  auto request_handler = std::move(RegisterRequestHandlers({target_url})[0]);
  ASSERT_TRUE(embedded_test_server()->Start());

  LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      GetKeepAlivePageURL(kPrimaryHost, target_url, method()),
      request_handler.get(), k200TextResponse);

  // The response should be processed in browser.
  loaders_observer().WaitForTotalOnReceiveResponseProcessed(1);
  ExpectCommonUkm(
      content::KeepAliveRequestTracker::RequestType::kFetch,
      /*category_id=*/1,
      /*num_redirects=*/0,
      /*is_context_detached=*/true,
      content::KeepAliveRequestTracker::RequestStageType::kResponseReceived,
      content::KeepAliveRequestTracker::RequestStageType::
          kLoaderDisconnectedFromRenderer);
  ExpectTimeSortedTimeDeltaUkm(
      {"TimeDelta.RequestStarted", "TimeDelta.LoaderDisconnectedFromRenderer",
       "TimeDelta.ResponseReceived", "TimeDelta.EventLogged"});
  NavigationKeepAliveRequestUkmMatcher::ExpectNoUkm();
}

// Delays response to a keepalive ping until after the page making the keepalive
// ping is put into BackForwardCache. The response should be processed by the
// renderer after the page is restored from BackForwardCache.
IN_PROC_BROWSER_TEST_P(ChromeKeepAliveCategoryRequestBrowserTest,
                       ReceiveResponseInBackForwardCache) {
  const std::string target_url = GetKeepAliveCategoryRequestUrl("test-prefix1");
  auto request_handler = std::move(RegisterRequestHandlers({target_url})[0]);
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(NavigateToURL(
      web_contents(), GetKeepAlivePageURL(kPrimaryHost, target_url, method())));
  content::RenderFrameHostWrapper rfh_1(current_frame_host());
  // Ensure the keepalive request is sent before leaving the current page.
  request_handler->WaitForRequest();

  // Navigate to cross-origin page.
  ASSERT_TRUE(NavigateToURL(web_contents(), GetCrossOriginPageURL()));
  // Ensure the previous page has been put into BackForwardCache.
  ASSERT_EQ(rfh_1->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Send back response.
  request_handler->Send(k200TextResponse);
  // The response is immediately forwarded to the in-BackForwardCache renderer.
  loaders_observer().WaitForTotalOnReceiveResponseForwarded(1);
  // Go back to `rfh_1`.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // The response should be processed in renderer. Hence resolving Promise.
  content::TitleWatcher watcher(web_contents(), kPromiseResolvedPageTitle);
  EXPECT_EQ(watcher.WaitAndGetTitle(), kPromiseResolvedPageTitle);
  request_handler->Done();
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK});
  ExpectCommonUkm(
      content::KeepAliveRequestTracker::RequestType::kFetch,
      /*category_id=*/1,
      /*num_redirects=*/0,
      /*is_context_detached=*/false,
      content::KeepAliveRequestTracker::RequestStageType::kLoaderCompleted,
      content::KeepAliveRequestTracker::RequestStageType::kResponseReceived,
      /*keepalive_token=*/std::nullopt,
      /*failed_error_code=*/std::nullopt,
      /*failed_extended_error_code=*/std::nullopt,
      /*completed_error_code=*/net::OK,
      /*completed_extended_error_code=*/0);
  ExpectTimeSortedTimeDeltaUkm(
      {"TimeDelta.RequestStarted", "TimeDelta.ResponseReceived",
       "TimeDelta.LoaderCompleted", "TimeDelta.EventLogged"});
  NavigationKeepAliveRequestUkmMatcher::ExpectNoUkm();
}

// Delays handling redirect for a keepalive ping until after the page making the
// keepalive ping has been unloaded. The browser must ensure the redirect is
// verified and properly processed by the browser.
IN_PROC_BROWSER_TEST_P(ChromeKeepAliveCategoryRequestBrowserTest,
                       ReceiveRedirectAfterPageUnload) {
  const std::string target_url = GetKeepAliveCategoryRequestUrl("test-prefix1");
  const char redirect_target[] = "/beacon-redirected";
  auto request_handlers =
      RegisterRequestHandlers({target_url, redirect_target});
  ASSERT_TRUE(server()->Start());

  // Set up redirects according to the following redirect chain:
  // fetch("http://a.com:<port>/beacon", keepalive: true)
  // --> http://a.com:<port>/beacon-redirected
  LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      GetKeepAlivePageURL(kPrimaryHost, target_url, method()),
      request_handlers[0].get(),
      base::StringPrintf(k301ResponseTemplate, redirect_target));

  // The redirect request should be processed in browser and gets sent.
  request_handlers[1]->WaitForRequest();
  // No variations header when redirected to non-Google.
  EXPECT_THAT(request_handlers[1]->http_request()->headers,
              Not(Contains(Key(variations::kClientDataHeader))));
  // End the keepalive request by sending back final response.
  request_handlers[1]->Send(k200TextResponse);
  request_handlers[1]->Done();

  // The response should be processed in browser.
  loaders_observer().WaitForTotalOnReceiveResponseProcessed(1);
  ExpectCommonUkm(
      content::KeepAliveRequestTracker::RequestType::kFetch,
      /*category_id=*/1,
      /*num_redirects=*/1,
      /*is_context_detached=*/true,
      content::KeepAliveRequestTracker::RequestStageType::kResponseReceived,
      content::KeepAliveRequestTracker::RequestStageType::
          kFirstRedirectReceived);
  ExpectTimeSortedTimeDeltaUkm(
      {"TimeDelta.RequestStarted", "TimeDelta.LoaderDisconnectedFromRenderer",
       "TimeDelta.FirstRedirectReceived", "TimeDelta.ResponseReceived",
       "TimeDelta.EventLogged"});
  NavigationKeepAliveRequestUkmMatcher::ExpectNoUkm();
}

// Browser tests to cover pairs of navigation request and fetch keepalive
// request from a search result page.
// Note that both the fetch request and the page URL need to contain specific
// "category" URL param in order for them to be considered as test targets.
class FromGWSNavigationAndKeepAliveRequestBrowserTest
    : public ChromeKeepAliveCategoryRequestBrowserTestBase,
      public testing::WithParamInterface<std::string> {
 protected:
  void SetUpOnMainThread() override {
    ChromeKeepAliveCategoryRequestBrowserTestBase::SetUpOnMainThread();
    server()->RegisterDefaultHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/search",
        base::BindRepeating(HandleSearchResultPageRequest)));
    server()->RegisterDefaultHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, kKeepAliveEndpoint,
        base::BindRepeating(HandleKeepAliveRequest)));
  }

  GURL GetSearchResultPageURL() {
    return server()->GetURL(kGoogleHost, kSRPEndpoing);
  }

  const std::string& method() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    FromGWSNavigationAndKeepAliveRequestBrowserTest,
    ::testing::Values(net::HttpRequestHeaders::kGetMethod,
                      net::HttpRequestHeaders::kPostMethod),
    [](const testing::TestParamInfo<
        FromGWSNavigationAndKeepAliveRequestBrowserTest::ParamType>& info) {
      return info.param;
    });

// Tests the case where a SRP sends a fetch keepalive category request.
// As no navigation request is triggered, the navigation-related UKM metrics
// should not be logged.
IN_PROC_BROWSER_TEST_P(FromGWSNavigationAndKeepAliveRequestBrowserTest,
                       OneRequestFromSRP) {
  const std::string category = "test-prefix1";
  const std::string target_url = GetKeepAliveCategoryRequestUrl(category);
  ASSERT_TRUE(server()->Start());

  // Navigate to search result page.
  ASSERT_TRUE(NavigateToURL(web_contents(), GetSearchResultPageURL()));
  // Ask the page to send a keepalive request to `target_url` with `category`
  // in the URL param.
  ASSERT_THAT(EvalJs(web_contents(), JsReplace(R"(
    runFetchKeepalive($1, $2);
  )",
                                               target_url, method())),
              Eq(k200TextResponse));

  ExpectCommonUkm(
      content::KeepAliveRequestTracker::RequestType::kFetch,
      /*category_id=*/1,
      /*num_redirects=*/0,
      /*is_context_detached=*/false,
      content::KeepAliveRequestTracker::RequestStageType::kLoaderCompleted,
      content::KeepAliveRequestTracker::RequestStageType::kResponseReceived,
      /*keepalive_token=*/std::nullopt,
      /*failed_error_code=*/std::nullopt,
      /*failed_extended_error_code=*/std::nullopt,
      /*completed_error_code=*/net::OK,
      /*completed_extended_error_code=*/0);
  ExpectTimeSortedTimeDeltaUkm(
      {"TimeDelta.RequestStarted", "TimeDelta.ResponseReceived",
       "TimeDelta.LoaderCompleted", "TimeDelta.EventLogged"});
  // No navigation request is triggered, so no navigation-related UKM metrics
  // should be logged.
  NavigationKeepAliveRequestUkmMatcher::ExpectNoUkm();
}

// Tests the case where a SRP sends a fetch keepalive category request and then
// navigates to another page.
IN_PROC_BROWSER_TEST_P(FromGWSNavigationAndKeepAliveRequestBrowserTest,
                       OneRequestAndNavigationFromSRP) {
  const std::string category = "test-prefix1";
  const std::string target_url = GetKeepAliveCategoryRequestUrl(category);
  ASSERT_TRUE(server()->Start());
  const std::string nav_target_url =
      GetRelativeCategoryUrl("/title1", category);

  // Navigate to search result page.
  ASSERT_TRUE(NavigateToURL(web_contents(), GetSearchResultPageURL()));
  // Ask the page to
  // 1. Send a keepalive request to `target_url` with `category`.
  // 2. Navigate to `nav_target_url` with `category`.
  ASSERT_THAT(
      EvalJs(web_contents(), JsReplace(R"(
    runFetchKeepaliveAndNavigation($1, $2, $3);
  )",
                                       target_url, method(), nav_target_url)),
      Eq(k200TextResponse));

  ExpectCommonUkm(
      content::KeepAliveRequestTracker::RequestType::kFetch,
      /*category_id=*/1,
      /*num_redirects=*/0,
      /*is_context_detached=*/false,
      content::KeepAliveRequestTracker::RequestStageType::kLoaderCompleted,
      content::KeepAliveRequestTracker::RequestStageType::kResponseReceived,
      /*keepalive_token=*/std::nullopt,
      /*failed_error_code=*/std::nullopt,
      /*failed_extended_error_code=*/std::nullopt,
      /*completed_error_code=*/net::OK,
      /*completed_extended_error_code=*/0);
  ExpectTimeSortedTimeDeltaUkm(
      {"TimeDelta.RequestStarted", "TimeDelta.ResponseReceived",
       "TimeDelta.LoaderCompleted", "TimeDelta.EventLogged"});
  // The navigation request and the fetch keepalive request shares the same
  // category ID, so they should be paired together in the navigation UKM.
  ExpectNavigationUkm(/*category_id=*/1, /*navigation_id=*/std::nullopt,
                      /*keepalive_token=*/std::nullopt);
}

// Tests the case where a SRP sends two different fetch keepalive category
// requests and then navigates to another page.
IN_PROC_BROWSER_TEST_P(FromGWSNavigationAndKeepAliveRequestBrowserTest,
                       TwoDifferentCategoryRequestsAndNavigationFromSRP) {
  const std::string category1 = "test-prefix1";
  const std::string category2 = "test-prefix2";
  const std::string target_url1 = GetKeepAliveCategoryRequestUrl(category1);
  const std::string target_url2 = GetKeepAliveCategoryRequestUrl(category2);
  ASSERT_TRUE(server()->Start());
  const std::string nav_target_url =
      GetRelativeCategoryUrl("/title1", category2);

  // Navigate to search result page.
  ASSERT_TRUE(NavigateToURL(web_contents(), GetSearchResultPageURL()));
  // Ask the page to
  // 1. Send a keepalive request to `target_url` with `category1`.
  ASSERT_THAT(EvalJs(web_contents(), JsReplace(R"(
    runFetchKeepalive($1, $2);
  )",
                                               target_url1, method())),
              Eq(k200TextResponse));
  // 2. Send a keepalive request to `target_url` with `category2`.
  // 3. Navigate to `nav_target_url` with `category2`.
  ASSERT_THAT(
      EvalJs(web_contents(), JsReplace(R"(
    runFetchKeepaliveAndNavigation($1, $2, $3);
  )",
                                       target_url2, method(), nav_target_url)),
      Eq(k200TextResponse));

  ExpectCommonUkms(
      {{content::KeepAliveRequestTracker::RequestType::kFetch,
        /*category_id=*/1,
        /*num_redirects=*/0,
        /*is_context_detached=*/false,
        content::KeepAliveRequestTracker::RequestStageType::kLoaderCompleted,
        content::KeepAliveRequestTracker::RequestStageType::kResponseReceived,
        /*keepalive_token=*/std::nullopt,
        /*failed_error_code=*/std::nullopt,
        /*failed_extended_error_code=*/std::nullopt,
        /*completed_error_code=*/net::OK,
        /*completed_extended_error_code=*/0},
       {content::KeepAliveRequestTracker::RequestType::kFetch,
        /*category_id=*/2,
        /*num_redirects=*/0,
        /*is_context_detached=*/false,
        content::KeepAliveRequestTracker::RequestStageType::kLoaderCompleted,
        content::KeepAliveRequestTracker::RequestStageType::kResponseReceived,
        /*keepalive_token=*/std::nullopt,
        /*failed_error_code=*/std::nullopt,
        /*failed_extended_error_code=*/std::nullopt,
        /*completed_error_code=*/net::OK,
        /*completed_extended_error_code=*/0}});
  // Only request with `category2` should be paired with the navigation
  // request.
  ExpectNavigationUkm(/*category_id=*/2, /*navigation_id=*/std::nullopt,
                      /*keepalive_token=*/std::nullopt);
}

// Tests the case where a SRP sends two same fetch keepalive category
// requests and then navigates to another page.
IN_PROC_BROWSER_TEST_P(FromGWSNavigationAndKeepAliveRequestBrowserTest,
                       TwoSameCategoryRequestsAndNavigationFromSRP) {
  const std::string category = "test-prefix1";
  const std::string target_url1 = GetKeepAliveCategoryRequestUrl(category);
  const std::string target_url2 = GetKeepAliveCategoryRequestUrl(category);
  ASSERT_TRUE(server()->Start());
  const std::string nav_target_url =
      GetRelativeCategoryUrl("/title1", category);

  // Navigate to search result page.
  ASSERT_TRUE(NavigateToURL(web_contents(), GetSearchResultPageURL()));
  // Ask the page to
  // 1. Send a keepalive request to `target_url` with `category`.
  ASSERT_THAT(EvalJs(web_contents(), JsReplace(R"(
    runFetchKeepalive($1, $2);
  )",
                                               target_url1, method())),
              Eq(k200TextResponse));
  // 2. Send a keepalive request to `target_url` with `category`.
  // 3. Navigate to `nav_target_url` with `category`.
  ASSERT_THAT(
      EvalJs(web_contents(), JsReplace(R"(
    runFetchKeepaliveAndNavigation($1, $2, $3);
  )",
                                       target_url2, method(), nav_target_url)),
      Eq(k200TextResponse));

  ExpectCommonUkms(
      {{content::KeepAliveRequestTracker::RequestType::kFetch,
        /*category_id=*/1,
        /*num_redirects=*/0,
        /*is_context_detached=*/false,
        content::KeepAliveRequestTracker::RequestStageType::kLoaderCompleted,
        content::KeepAliveRequestTracker::RequestStageType::kResponseReceived,
        /*keepalive_token=*/std::nullopt,
        /*failed_error_code=*/std::nullopt,
        /*failed_extended_error_code=*/std::nullopt,
        /*completed_error_code=*/net::OK,
        /*completed_extended_error_code=*/0},
       {content::KeepAliveRequestTracker::RequestType::kFetch,
        /*category_id=*/1,
        /*num_redirects=*/0,
        /*is_context_detached=*/false,
        content::KeepAliveRequestTracker::RequestStageType::kLoaderCompleted,
        content::KeepAliveRequestTracker::RequestStageType::kResponseReceived,
        /*keepalive_token=*/std::nullopt,
        /*failed_error_code=*/std::nullopt,
        /*failed_extended_error_code=*/std::nullopt,
        /*completed_error_code=*/net::OK,
        /*completed_extended_error_code=*/0}});
  // Only one request should be paired with the navigation request, event though
  // both requests have the same category ID.
  ExpectNavigationUkm(/*category_id=*/1, /*navigation_id=*/std::nullopt,
                      /*keepalive_token=*/std::nullopt);
}

// Tests the case where a SRP sends one fetch keepalive category
// requests and then navigates to another page twice.
IN_PROC_BROWSER_TEST_P(FromGWSNavigationAndKeepAliveRequestBrowserTest,
                       OneRequestAndTwoSameCategoryNavigationsFromSRP) {
  const std::string category = "test-prefix1";
  const std::string target_url = GetKeepAliveCategoryRequestUrl(category);
  ASSERT_TRUE(server()->Start());
  const std::string nav_target_url =
      GetRelativeCategoryUrl("/title1", category);

  // Navigate to search result page.
  ASSERT_TRUE(NavigateToURL(web_contents(), GetSearchResultPageURL()));
  // Ask the page to
  // 1. Send a keepalive request to `target_url` with `category`.
  // 2. Navigate to `nav_target_url` with `category` for two times.
  ASSERT_THAT(EvalJs(web_contents(), JsReplace(R"(
    runFetchKeepaliveAndNavigations($1, $2, [$3, $4]);
  )",
                                               target_url, method(),
                                               nav_target_url, nav_target_url)),
              Eq(k200TextResponse));

  ExpectCommonUkm(
      content::KeepAliveRequestTracker::RequestType::kFetch,
      /*category_id=*/1,
      /*num_redirects=*/0,
      /*is_context_detached=*/false,
      content::KeepAliveRequestTracker::RequestStageType::kLoaderCompleted,
      content::KeepAliveRequestTracker::RequestStageType::kResponseReceived,
      /*keepalive_token=*/std::nullopt,
      /*failed_error_code=*/std::nullopt,
      /*failed_extended_error_code=*/std::nullopt,
      /*completed_error_code=*/net::OK,
      /*completed_extended_error_code=*/0);
  ExpectTimeSortedTimeDeltaUkm(
      {"TimeDelta.RequestStarted", "TimeDelta.ResponseReceived",
       "TimeDelta.LoaderCompleted", "TimeDelta.EventLogged"});
  // Only one navigation should be paired with the fetch keepalive request.
  ExpectNavigationUkm(/*category_id=*/1, /*navigation_id=*/std::nullopt,
                      /*keepalive_token=*/std::nullopt);
}

// Tests the case where a SRP sends one fetch keepalive category
// requests and then perform two different category navigations.
IN_PROC_BROWSER_TEST_P(FromGWSNavigationAndKeepAliveRequestBrowserTest,
                       OneRequestAndTwoDifferentCategoryNavigationsFromSRP) {
  const std::string category1 = "test-prefix1";
  const std::string category2 = "test-prefix2";
  const std::string target_url = GetKeepAliveCategoryRequestUrl(category2);
  ASSERT_TRUE(server()->Start());
  const std::string nav_target_url1 =
      GetRelativeCategoryUrl("/title1", category1);
  const std::string nav_target_url2 =
      GetRelativeCategoryUrl("/title1", category2);

  // Navigate to search result page.
  ASSERT_TRUE(NavigateToURL(web_contents(), GetSearchResultPageURL()));
  // Ask the page to
  // 1. Send a keepalive request to `target_url` with `category2`.
  // 2. Navigate to `nav_target_url1` and `nav_target_url2` in sequence.
  ASSERT_THAT(
      EvalJs(web_contents(),
             JsReplace(R"(
    runFetchKeepaliveAndNavigations($1, $2, [$3, $4]);
  )",
                       target_url, method(), nav_target_url1, nav_target_url2)),
      Eq(k200TextResponse));

  ExpectCommonUkm(
      content::KeepAliveRequestTracker::RequestType::kFetch,
      /*category_id=*/2,
      /*num_redirects=*/0,
      /*is_context_detached=*/false,
      content::KeepAliveRequestTracker::RequestStageType::kLoaderCompleted,
      content::KeepAliveRequestTracker::RequestStageType::kResponseReceived,
      /*keepalive_token=*/std::nullopt,
      /*failed_error_code=*/std::nullopt,
      /*failed_extended_error_code=*/std::nullopt,
      /*completed_error_code=*/net::OK,
      /*completed_extended_error_code=*/0);
  ExpectTimeSortedTimeDeltaUkm(
      {"TimeDelta.RequestStarted", "TimeDelta.ResponseReceived",
       "TimeDelta.LoaderCompleted", "TimeDelta.EventLogged"});
  // Only one navigation should be paired with the fetch keepalive request.
  ExpectNavigationUkm(/*category_id=*/2, /*navigation_id=*/std::nullopt,
                      /*keepalive_token=*/std::nullopt);
}

// Tests the case where a SRP sends two different fetch keepalive category
// requests and then perform two different navigations.
IN_PROC_BROWSER_TEST_P(
    FromGWSNavigationAndKeepAliveRequestBrowserTest,
    TwoDifferentCategoryRequestAndTwoDifferentCategoryNavigationsFromSRP) {
  const std::string category1 = "test-prefix1";
  const std::string category2 = "test-prefix2";
  const std::string target_url1 = GetKeepAliveCategoryRequestUrl(category1);
  const std::string target_url2 = GetKeepAliveCategoryRequestUrl(category2);
  ASSERT_TRUE(server()->Start());
  const std::string nav_target_url1 =
      GetRelativeCategoryUrl("/title1", category1);
  const std::string nav_target_url2 =
      GetRelativeCategoryUrl("/title1", category2);

  // Navigate to search result page.
  ASSERT_TRUE(NavigateToURL(web_contents(), GetSearchResultPageURL()));
  // Ask the page to
  // 1. Send a keepalive request to `target_url1` with `category1`
  // 2. Navigate to `nav_target_url1` with `category1`.
  ASSERT_THAT(
      EvalJs(web_contents(), JsReplace(R"(
    runFetchKeepaliveAndNavigation($1, $2, $3);
  )",
                                       target_url1, method(), nav_target_url1)),
      Eq(k200TextResponse));
  // 3. Send a keepalive request to `target_url2` with `category2`
  // 4. Navigate to `nav_target_url2` with `category2`.
  ASSERT_THAT(
      EvalJs(web_contents(), JsReplace(R"(
    runFetchKeepaliveAndNavigation($1, $2, $3);
  )",
                                       target_url2, method(), nav_target_url2)),
      Eq(k200TextResponse));

  ExpectCommonUkms(
      {{content::KeepAliveRequestTracker::RequestType::kFetch,
        /*category_id=*/1,
        /*num_redirects=*/0,
        /*is_context_detached=*/false,
        content::KeepAliveRequestTracker::RequestStageType::kLoaderCompleted,
        content::KeepAliveRequestTracker::RequestStageType::kResponseReceived,
        /*keepalive_token=*/std::nullopt,
        /*failed_error_code=*/std::nullopt,
        /*failed_extended_error_code=*/std::nullopt,
        /*completed_error_code=*/net::OK,
        /*completed_extended_error_code=*/0},
       {content::KeepAliveRequestTracker::RequestType::kFetch,
        /*category_id=*/2,
        /*num_redirects=*/0,
        /*is_context_detached=*/false,
        content::KeepAliveRequestTracker::RequestStageType::kLoaderCompleted,
        content::KeepAliveRequestTracker::RequestStageType::kResponseReceived,
        /*keepalive_token=*/std::nullopt,
        /*failed_error_code=*/std::nullopt,
        /*failed_extended_error_code=*/std::nullopt,
        /*completed_error_code=*/net::OK,
        /*completed_extended_error_code=*/0}});
  // Only one navigation should be paired with the fetch keepalive request.
  ExpectNavigationUkms({{/*category_id=*/1, /*navigation_id=*/std::nullopt,
                         /*keepalive_token=*/std::nullopt},
                        {/*category_id=*/2, /*navigation_id=*/std::nullopt,
                         /*keepalive_token=*/std::nullopt}});
}

// Tests the case where a SRP sends a fetch keepalive category request and then
// navigates to another page in a new tab.
IN_PROC_BROWSER_TEST_P(FromGWSNavigationAndKeepAliveRequestBrowserTest,
                       OneRequestAndNewTabNavigationFromSRP) {
  const std::string category = "test-prefix1";
  const std::string target_url = GetKeepAliveCategoryRequestUrl(category);
  ASSERT_TRUE(server()->Start());
  const std::string nav_target_url =
      GetRelativeCategoryUrl("/title1", category);

  // Navigate to search result page.
  ASSERT_TRUE(NavigateToURL(web_contents(), GetSearchResultPageURL()));
  // Ask the page to
  // 1. Send a keepalive request to `target_url` with `category`.
  // 2. Open a new tab and navigate to `nav_target_url` with `category`.
  ASSERT_THAT(
      EvalJs(web_contents(), JsReplace(R"(
    runFetchKeepaliveAndNewTabNavigation($1, $2, $3);
  )",
                                       target_url, method(), nav_target_url)),
      Eq(k200TextResponse));

  ExpectCommonUkm(
      content::KeepAliveRequestTracker::RequestType::kFetch,
      /*category_id=*/1,
      /*num_redirects=*/0,
      /*is_context_detached=*/false,
      content::KeepAliveRequestTracker::RequestStageType::kLoaderCompleted,
      content::KeepAliveRequestTracker::RequestStageType::kResponseReceived,
      /*keepalive_token=*/std::nullopt,
      /*failed_error_code=*/std::nullopt,
      /*failed_extended_error_code=*/std::nullopt,
      /*completed_error_code=*/net::OK,
      /*completed_extended_error_code=*/0);
  ExpectTimeSortedTimeDeltaUkm(
      {"TimeDelta.RequestStarted", "TimeDelta.ResponseReceived",
       "TimeDelta.LoaderCompleted", "TimeDelta.EventLogged"});
  // The navigation request and the fetch keepalive request shares the same
  // category ID, so they should be paired together in the navigation UKM.
  ExpectNavigationUkm(/*category_id=*/1, /*navigation_id=*/std::nullopt,
                      /*keepalive_token=*/std::nullopt);
}
