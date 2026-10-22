// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/signin/signin_url_loader_throttle.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "android_webview/browser/signin/aw_google_accounts_delegate.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/signin/core/browser/cookie_settings_util.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace android_webview {
namespace {

constexpr char kGaiaUrl[] = "https://accounts.google.com/ServiceLogin";
constexpr char kNonGaiaUrl[] = "https://example.com/foo";
constexpr char kTestGaiaId[] = "123456789";

class MockGoogleAccountsDelegate : public AwGoogleAccountsDelegate {
 public:
  MockGoogleAccountsDelegate() = default;
  ~MockGoogleAccountsDelegate() override = default;

  // AwGoogleAccountsDelegate:
  void OnGoogleAccountsRequest(const signin::ManageAccountsParams& params,
                               bool is_outermost_main_frame,
                               bool is_navigation_request) override {
    ++on_google_accounts_request_call_count_;
    last_manage_accounts_params_ = params;
    last_is_outermost_main_frame_ = is_outermost_main_frame;
    last_is_navigation_request_ = is_navigation_request;

    if (on_google_accounts_request_closure_) {
      on_google_accounts_request_closure_.Run();
    }
  }

  std::string GetGaiaId() override { return gaia_id_; }

  void set_gaia_id(std::string gaia_id) { gaia_id_ = std::move(gaia_id); }
  const std::string& gaia_id() const { return gaia_id_; }

  int on_google_accounts_request_call_count() const {
    return on_google_accounts_request_call_count_;
  }
  const std::optional<signin::ManageAccountsParams>&
  last_manage_accounts_params() const {
    return last_manage_accounts_params_;
  }
  std::optional<bool> last_is_outermost_main_frame() const {
    return last_is_outermost_main_frame_;
  }
  std::optional<bool> last_is_navigation_request() const {
    return last_is_navigation_request_;
  }
  void set_on_google_accounts_request_closure(base::RepeatingClosure closure) {
    on_google_accounts_request_closure_ = std::move(closure);
  }

 private:
  std::string gaia_id_;

  int on_google_accounts_request_call_count_ = 0;
  std::optional<signin::ManageAccountsParams> last_manage_accounts_params_;
  std::optional<bool> last_is_outermost_main_frame_;
  std::optional<bool> last_is_navigation_request_;
  base::RepeatingClosure on_google_accounts_request_closure_;
};

class SigninUrlLoaderThrottleTest : public testing::Test {
 public:
  SigninUrlLoaderThrottleTest() = default;
  ~SigninUrlLoaderThrottleTest() override = default;

 protected:
  void SetUp() override {
    browser_context_ = std::make_unique<content::TestBrowserContext>();
  }

  void TearDown() override { browser_context_.reset(); }

  AwGoogleAccountsDelegate* delegate() { return delegate_.get(); }
  void SetDelegate(std::unique_ptr<MockGoogleAccountsDelegate> delegate) {
    delegate_ = std::move(delegate);
  }

  SigninUrlLoaderThrottle::GoogleAccountsDelegateGetter GetDelegateGetter() {
    return base::BindRepeating(&SigninUrlLoaderThrottleTest::delegate,
                               base::Unretained(this));
  }

  content::BrowserContext* browser_context() { return browser_context_.get(); }

  std::unique_ptr<SigninUrlLoaderThrottle> CreateThrottle(
      signin::CookieSettingsCallback cookie_settings_callback = {}) {
    if (!cookie_settings_callback) {
      cookie_settings_callback = base::BindRepeating(
          [](const GURL&, const net::SiteForCookies&, const url::Origin&,
             net::CookieSettingOverrides) { return true; });
    }
    return SigninUrlLoaderThrottle::CreateForTesting(
        browser_context(), std::move(cookie_settings_callback),
        GetDelegateGetter());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  std::unique_ptr<MockGoogleAccountsDelegate> delegate_;
};

TEST_F(SigninUrlLoaderThrottleTest, CreateRequiresDelegate) {
  // Without delegate, CreateForTesting returns nullptr.
  auto throttle = CreateThrottle();
  EXPECT_FALSE(throttle);

  // Set delegate.
  auto delegate = std::make_unique<MockGoogleAccountsDelegate>();
  auto* delegate_ptr = delegate.get();
  SetDelegate(std::move(delegate));
  EXPECT_EQ(delegate_ptr, this->delegate());

  // With delegate, CreateForTesting succeeds.
  throttle = CreateThrottle();
  EXPECT_TRUE(throttle);

  // Clear delegate.
  SetDelegate(nullptr);
  EXPECT_FALSE(CreateThrottle());
}

TEST_F(SigninUrlLoaderThrottleTest, CreateWithNullWebContents) {
  auto null_getter =
      base::BindRepeating([]() -> content::WebContents* { return nullptr; });
  auto throttle =
      SigninUrlLoaderThrottle::Create(browser_context(), null_getter);
  EXPECT_FALSE(throttle);
}

TEST_F(SigninUrlLoaderThrottleTest, WillStartRequestDoesNotModifyUrl) {
  auto delegate = std::make_unique<MockGoogleAccountsDelegate>();
  delegate->set_gaia_id("");
  SetDelegate(std::move(delegate));

  auto throttle = CreateThrottle();
  ASSERT_TRUE(throttle);

  network::ResourceRequest request;
  request.url = GURL(kGaiaUrl);
  request.destination = network::mojom::RequestDestination::kDocument;
  bool defer = false;

  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);
  EXPECT_EQ(request.url.spec(), kGaiaUrl);
}

TEST_F(SigninUrlLoaderThrottleTest,
       WillStartRequestAddsMirrorHeaderWithValidCookieSettings) {
  auto delegate = std::make_unique<MockGoogleAccountsDelegate>();
  delegate->set_gaia_id(kTestGaiaId);
  SetDelegate(std::move(delegate));

  int cookie_access_call_count = 0;
  auto cookie_settings_delegate = base::BindLambdaForTesting(
      [&](const GURL&, const net::SiteForCookies&, const url::Origin&,
          net::CookieSettingOverrides) {
        ++cookie_access_call_count;
        return true;
      });

  auto throttle = CreateThrottle(cookie_settings_delegate);
  ASSERT_TRUE(throttle);

  network::ResourceRequest request;
  request.url = GURL("https://drive.google.com");
  request.destination = network::mojom::RequestDestination::kDocument;
  bool defer = false;

  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);
  EXPECT_GT(cookie_access_call_count, 0);

  std::optional<std::string> header_value =
      request.headers.GetHeader("X-Chrome-Connected");
  ASSERT_TRUE(header_value.has_value());
  EXPECT_THAT(*header_value, testing::HasSubstr("source=AndroidWebView"));
  EXPECT_THAT(*header_value,
              testing::HasSubstr(std::string("id=") + kTestGaiaId));
}

TEST_F(SigninUrlLoaderThrottleTest, ProcessResponseTriggersDelegate) {
  auto delegate = std::make_unique<MockGoogleAccountsDelegate>();
  delegate->set_gaia_id("");
  auto* delegate_ptr = delegate.get();
  SetDelegate(std::move(delegate));

  auto throttle = CreateThrottle();
  ASSERT_TRUE(throttle);

  network::ResourceRequest request;
  request.url = GURL(kGaiaUrl);
  request.destination = network::mojom::RequestDestination::kDocument;
  request.is_outermost_main_frame = true;
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  // Expect OnGoogleAccountsRequest to be called with DEFAULT action.
  base::RunLoop run_loop;
  delegate_ptr->set_on_google_accounts_request_closure(run_loop.QuitClosure());

  network::mojom::URLResponseHead response_head;
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\r\n"
      "X-Chrome-Manage-Accounts: action=DEFAULT\r\n\r\n");

  throttle->WillProcessResponse(request.url, &response_head, &defer);
  EXPECT_FALSE(defer);
  run_loop.Run();

  EXPECT_EQ(1, delegate_ptr->on_google_accounts_request_call_count());
  ASSERT_TRUE(delegate_ptr->last_manage_accounts_params().has_value());
  EXPECT_EQ(signin::GAIA_SERVICE_TYPE_DEFAULT,
            delegate_ptr->last_manage_accounts_params()->service_type);
  EXPECT_TRUE(delegate_ptr->last_is_outermost_main_frame().value_or(false));
  EXPECT_TRUE(delegate_ptr->last_is_navigation_request().value_or(false));
}

TEST_F(SigninUrlLoaderThrottleTest, ProcessResponseIgnoredForNonGaiaUrl) {
  auto delegate = std::make_unique<MockGoogleAccountsDelegate>();
  delegate->set_gaia_id("");
  auto* delegate_ptr = delegate.get();
  SetDelegate(std::move(delegate));

  auto throttle = CreateThrottle();
  ASSERT_TRUE(throttle);

  network::ResourceRequest request;
  request.url = GURL(kNonGaiaUrl);
  request.destination = network::mojom::RequestDestination::kDocument;
  request.is_outermost_main_frame = true;
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  network::mojom::URLResponseHead response_head;
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\r\n"
      "X-Chrome-Manage-Accounts: action=DEFAULT\r\n\r\n");

  throttle->WillProcessResponse(request.url, &response_head, &defer);
  EXPECT_FALSE(defer);
  EXPECT_EQ(0, delegate_ptr->on_google_accounts_request_call_count());
}

TEST_F(SigninUrlLoaderThrottleTest, ProcessResponseForSubresource) {
  auto delegate = std::make_unique<MockGoogleAccountsDelegate>();
  delegate->set_gaia_id("");
  auto* delegate_ptr = delegate.get();
  SetDelegate(std::move(delegate));

  auto throttle = CreateThrottle();
  ASSERT_TRUE(throttle);

  network::ResourceRequest request;
  request.url = GURL(kGaiaUrl);
  request.destination = network::mojom::RequestDestination::kImage;
  request.is_outermost_main_frame = true;
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  base::RunLoop run_loop;
  delegate_ptr->set_on_google_accounts_request_closure(run_loop.QuitClosure());

  network::mojom::URLResponseHead response_head;
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\r\n"
      "X-Chrome-Manage-Accounts: action=DEFAULT\r\n\r\n");

  throttle->WillProcessResponse(request.url, &response_head, &defer);
  EXPECT_FALSE(defer);
  run_loop.Run();

  EXPECT_EQ(1, delegate_ptr->on_google_accounts_request_call_count());
  ASSERT_TRUE(delegate_ptr->last_manage_accounts_params().has_value());
  EXPECT_EQ(signin::GAIA_SERVICE_TYPE_DEFAULT,
            delegate_ptr->last_manage_accounts_params()->service_type);
  EXPECT_TRUE(delegate_ptr->last_is_outermost_main_frame().value_or(false));
  EXPECT_FALSE(delegate_ptr->last_is_navigation_request().value_or(true));
}

TEST_F(SigninUrlLoaderThrottleTest, ProcessResponseForSubframe) {
  auto delegate = std::make_unique<MockGoogleAccountsDelegate>();
  delegate->set_gaia_id("");
  auto* delegate_ptr = delegate.get();
  SetDelegate(std::move(delegate));

  auto throttle = CreateThrottle();
  ASSERT_TRUE(throttle);

  network::ResourceRequest request;
  request.url = GURL(kGaiaUrl);
  request.destination = network::mojom::RequestDestination::kIframe;
  request.is_outermost_main_frame = false;
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  base::RunLoop run_loop;
  delegate_ptr->set_on_google_accounts_request_closure(run_loop.QuitClosure());

  network::mojom::URLResponseHead response_head;
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\r\n"
      "X-Chrome-Manage-Accounts: action=DEFAULT\r\n\r\n");

  throttle->WillProcessResponse(request.url, &response_head, &defer);
  EXPECT_FALSE(defer);
  run_loop.Run();

  EXPECT_EQ(1, delegate_ptr->on_google_accounts_request_call_count());
  ASSERT_TRUE(delegate_ptr->last_manage_accounts_params().has_value());
  EXPECT_EQ(signin::GAIA_SERVICE_TYPE_DEFAULT,
            delegate_ptr->last_manage_accounts_params()->service_type);
  EXPECT_FALSE(delegate_ptr->last_is_outermost_main_frame().value_or(true));
  EXPECT_TRUE(delegate_ptr->last_is_navigation_request().value_or(false));
}

TEST_F(SigninUrlLoaderThrottleTest,
       ProcessResponseOnlyProcessedOnceOnRedirect) {
  auto delegate = std::make_unique<MockGoogleAccountsDelegate>();
  delegate->set_gaia_id("");
  auto* delegate_ptr = delegate.get();
  SetDelegate(std::move(delegate));

  auto throttle = CreateThrottle();
  ASSERT_TRUE(throttle);

  network::ResourceRequest request;
  request.url = GURL(kGaiaUrl);
  request.destination = network::mojom::RequestDestination::kDocument;
  request.is_outermost_main_frame = true;
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  // Expect OnGoogleAccountsRequest to be called only once despite multiple
  // headers.
  base::RunLoop run_loop;
  delegate_ptr->set_on_google_accounts_request_closure(run_loop.QuitClosure());

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL(kGaiaUrl);
  redirect_info.new_method = "GET";

  network::mojom::URLResponseHead redirect_head;
  redirect_head.headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 302 Found\r\n"
      "X-Chrome-Manage-Accounts: action=DEFAULT\r\n\r\n");
  network::HttpRequestHeadersUpdateParams update_params;
  throttle->WillRedirectRequest(&redirect_info, redirect_head, &defer,
                                &update_params);
  EXPECT_FALSE(defer);

  network::mojom::URLResponseHead response_head;
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\r\n"
      "X-Chrome-Manage-Accounts: action=DEFAULT\r\n\r\n");
  throttle->WillProcessResponse(redirect_info.new_url, &response_head, &defer);
  EXPECT_FALSE(defer);

  run_loop.Run();
  EXPECT_EQ(1, delegate_ptr->on_google_accounts_request_call_count());
  EXPECT_TRUE(delegate_ptr->last_is_outermost_main_frame().value_or(false));
  EXPECT_TRUE(delegate_ptr->last_is_navigation_request().value_or(false));
}

}  // namespace
}  // namespace android_webview
