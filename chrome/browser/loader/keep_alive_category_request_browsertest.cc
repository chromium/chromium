// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
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
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/url_util.h"

namespace {

using testing::Contains;
using testing::Key;
using testing::Not;

std::string GetRelativeCategoryUrl(std::string_view relative_url,
                                   std::string_view category) {
  const std::string sep =
      (std::string(relative_url).find("?") == std::string::npos) ? "?" : "&";
  return base::StrCat({relative_url, sep, "category=", category});
}

std::string GetKeepAliveCategoryRequestUrl(std::string_view category) {
  return GetRelativeCategoryUrl(kKeepAliveEndpoint, category);
}

}  // namespace

class ChromeKeepAliveCategoryRequestBrowserTestBase
    : public ChromeKeepAliveRequestBrowserTestBase,
      public content::KeepAliveRequestUkmMatcher {
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

// Browser tests to cover fetch keepalive request with specified "category" in
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
      /*error_code=*/net::OK,
      /*extended_error_code=*/0);
  ExpectTimeSortedTimeDeltaUkm(
      {"TimeDelta.RequestStarted", "TimeDelta.ResponseReceived",
       "TimeDelta.LoaderCompleted", "TimeDelta.EventLogged"});
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
      /*error_code=*/net::OK,
      /*extended_error_code=*/0);
  ExpectTimeSortedTimeDeltaUkm(
      {"TimeDelta.RequestStarted", "TimeDelta.ResponseReceived",
       "TimeDelta.LoaderCompleted", "TimeDelta.EventLogged"});
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
}
