// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/enterprise/platform_auth/mock_platform_auth_provider.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "chrome/browser/enterprise/platform_auth/scoped_set_provider_for_testing.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

using ::testing::_;

namespace enterprise_auth {

namespace {

class ReadyToCommitCallbackRunner : public content::WebContentsObserver {
 public:
  ReadyToCommitCallbackRunner(content::WebContents* web_contents,
                              const GURL& target_url,
                              base::OnceClosure callback)
      : content::WebContentsObserver(web_contents),
        target_url_(target_url),
        callback_(std::move(callback)) {}

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL() == target_url_ &&
        navigation_handle->GetNetErrorCode() != net::OK && callback_) {
      std::move(callback_).Run();
    }
  }

 private:
  GURL target_url_;
  base::OnceClosure callback_;
};

}  // namespace

class PlatformAuthNavigationThrottleBrowserTest : public PlatformBrowserTest {
 public:
  PlatformAuthNavigationThrottleBrowserTest() = default;
  ~PlatformAuthNavigationThrottleBrowserTest() override = default;

  PlatformAuthNavigationThrottleBrowserTest(
      const PlatformAuthNavigationThrottleBrowserTest&) = delete;
  PlatformAuthNavigationThrottleBrowserTest& operator=(
      const PlatformAuthNavigationThrottleBrowserTest&) = delete;

 protected:
  void SetManagerEnabled(bool enabled, base::OnceClosure on_complete) {
    PlatformAuthProviderManager::GetInstance().SetEnabled(
        enabled, std::move(on_complete));
  }
};

// Regression test for crbug.com/585267246: when a navigation fails while
// `PlatformAuthNavigationThrottle` is deferring `WillRedirectRequest()`, the
// subsequent `FetchHeadersCallback()` must not call `Resume()` after
// `WillFailRequest()` has already completed and reset the throttle runner
// state.
IN_PROC_BROWSER_TEST_F(PlatformAuthNavigationThrottleBrowserTest,
                       FetchHeadersCallbackAfterRedirectRequestFails) {
  const GURL initial_url("https://idp.example/start");
  const GURL redirect_url("https://idp.example/redirected");

  mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver;
  mojo::Remote<network::mojom::URLLoaderClient> loader_client;

  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url != initial_url) {
          return false;
        }
        loader_receiver = std::move(params->receiver);
        loader_client = std::move(params->client);

        net::RedirectInfo redirect_info;
        redirect_info.status_code = 302;
        redirect_info.new_method = "GET";
        redirect_info.new_url = redirect_url;
        redirect_info.new_site_for_cookies =
            net::SiteForCookies::FromUrl(redirect_url);

        auto response_head = network::mojom::URLResponseHead::New();
        response_head->headers = net::HttpResponseHeaders::TryToCreate(
            "HTTP/1.1 302 Found\r\n"
            "Location: https://idp.example/redirected\r\n\r\n");

        loader_client->OnReceiveRedirect(redirect_info,
                                         std::move(response_head));
        return true;
      }));

  auto mock_provider =
      std::make_unique<::testing::StrictMock<MockPlatformAuthProvider>>();
  EXPECT_CALL(*mock_provider, SupportsOriginFiltering())
      .WillOnce(::testing::Return(false));

  // Allow `WillStartRequest()` for `initial_url` to proceed synchronously so
  // the request reaches the `URLLoader` and triggers a redirect.
  EXPECT_CALL(*mock_provider, GetData(initial_url, _))
      .WillOnce([](const GURL& url,
                   PlatformAuthProviderManager::GetDataCallback callback) {
        std::move(callback).Run(net::HttpRequestHeaders());
      });

  // Defer `WillRedirectRequest()` for `redirect_url` by holding the callback.
  base::RunLoop redirect_deferred_run_loop;
  PlatformAuthProviderManager::GetDataCallback pending_redirect_callback;
  EXPECT_CALL(*mock_provider, GetData(redirect_url, _))
      .WillOnce([&](const GURL& url,
                    PlatformAuthProviderManager::GetDataCallback callback) {
        pending_redirect_callback = std::move(callback);
        redirect_deferred_run_loop.Quit();
      });

  // Ignore any unrelated background navigations (e.g., prewarm prerender on
  // Android).
  EXPECT_CALL(*mock_provider,
              GetData(::testing::AllOf(::testing::Ne(initial_url),
                                       ::testing::Ne(redirect_url)),
                      _))
      .WillRepeatedly(
          [](const GURL& url,
             PlatformAuthProviderManager::GetDataCallback callback) {
            std::move(callback).Run(net::HttpRequestHeaders());
          });

  EXPECT_CALL(*mock_provider, Die());
  ScopedSetProviderForTesting set_provider(std::move(mock_provider));
  SetManagerEnabled(true, base::OnceClosure());

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Once the failed navigation finishes `WillFailRequest()` and reaches
  // `ReadyToCommitNavigation()` (while `NavigationRequest` and its throttles
  // are still alive, and `NavigationThrottleRunner`'s `current_event_` is
  // `kNoEvent`), invoke the stale redirect `GetData()` callback.
  bool callback_invoked = false;
  ReadyToCommitCallbackRunner observer(
      web_contents, redirect_url, base::BindLambdaForTesting([&]() {
        ASSERT_TRUE(pending_redirect_callback);
        callback_invoked = true;
        net::HttpRequestHeaders auth_headers;
        auth_headers.SetHeader(net::HttpRequestHeaders::kCookie,
                               "token=stale-token");
        std::move(pending_redirect_callback).Run(std::move(auth_headers));
      }));

  web_contents->GetController().LoadURL(initial_url, content::Referrer(),
                                        ui::PAGE_TRANSITION_TYPED,
                                        std::string());

  // Wait until `PlatformAuthNavigationThrottle` defers `WillRedirectRequest()`.
  redirect_deferred_run_loop.Run();
  ASSERT_TRUE(pending_redirect_callback);
  ASSERT_TRUE(loader_client.is_bound());

  // Fail the in-flight `URLLoader` while `WillRedirectRequest()` is deferred.
  loader_client->OnComplete(
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  // Wait for the error page navigation to finish committing.
  content::WaitForLoadStop(web_contents);
  EXPECT_TRUE(callback_invoked);
}

}  // namespace enterprise_auth
