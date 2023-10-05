// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_impl.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr std::string_view kDomain = "example.org";
constexpr std::string_view KTriggerRegistrationPath = "/trigger_registration";
constexpr std::string_view kRegisterSessionPath = "/register_session";
constexpr std::string_view kRotateCookiesPath = "/RotateBoundCookies";
constexpr std::string_view kChallenge = "test_challenge";

constexpr std::string_view kSessionRegistrationHeaderFormat =
    "registration=%s;supported-alg=ES256,RS256;challenge=%s";

constexpr std::string_view kBoundSessionParamsValidJsonFormat = R"(
    {
        "session_identifier": "007",
        "credentials": [
            {
                "type": "cookie",
                "name": "test_cookie",
                "scope": {
                    "domain": "%s",
                    "path": "%s"
                }
            }
        ]
    }
)";

std::unique_ptr<net::test_server::HttpResponse>
HandleTriggerRegistrationRequest(const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->AddCustomHeader(
      "Sec-Session-Google-Registration",
      base::StringPrintf(kSessionRegistrationHeaderFormat.data(),
                         kRegisterSessionPath.data(), kChallenge.data()));
  response->set_code(net::HTTP_OK);
  return response;
}

std::unique_ptr<net::test_server::HttpResponse> HandleRegisterSessionRequest(
    const net::test_server::HttpRequest& request) {
  EXPECT_TRUE(request.has_content);
  // TODO(http://b/303375108): verify the registration JWT.
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content(base::StringPrintf(
      kBoundSessionParamsValidJsonFormat.data(), kDomain.data(), "/"));
  response->set_content_type("application/json");
  response->set_code(net::HTTP_OK);
  return response;
}

std::unique_ptr<net::test_server::HttpResponse> CreateTransientErrorResponse(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  // Return a transient HTTP error to not trigger the session termination.
  // TODO(http://b/303375108): handle cookie rotation requests properly.
  response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
  return response;
}

}  // namespace

class BoundSessionCookieRefreshServiceImplBrowserTest
    : public InProcessBrowserTest {
 public:
  void SetUp() override {
    CHECK(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kGaiaUrl,
        embedded_test_server()->GetURL(kDomain, "/").spec());
    // Disables DICE account consistency.
    command_line->AppendSwitchASCII("allow-browser-signin", "false");
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest,
        std::string(KTriggerRegistrationPath),
        base::BindRepeating(&HandleTriggerRegistrationRequest)));
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest,
        std::string(kRegisterSessionPath),
        base::BindRepeating(&HandleRegisterSessionRequest)));
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest,
        std::string(kRotateCookiesPath),
        base::BindRepeating(&CreateTransientErrorResponse)));
    service()->SetBoundSessionParamsUpdatedCallbackForTesting(
        base::BindRepeating(&BoundSessionCookieRefreshServiceImplBrowserTest::
                                SessionParamsUpdated,
                            base::Unretained(this)));
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    service()->SetBoundSessionParamsUpdatedCallbackForTesting({});
    InProcessBrowserTest::TearDownOnMainThread();
  }

  BoundSessionCookieRefreshService* service() {
    return BoundSessionCookieRefreshServiceFactory::GetForProfile(
        browser()->profile());
  }

  void ExpectSessionParamsUpdate(base::OnceClosure callback) {
    CHECK(!params_updated_callback_);
    params_updated_callback_ = std::move(callback);
  }

 private:
  void SessionParamsUpdated() {
    if (params_updated_callback_) {
      std::move(params_updated_callback_).Run();
    } else {
      ADD_FAILURE() << "Unexpected session params update.";
    }
  }

  base::test::ScopedFeatureList feature_list_{
      switches::kEnableBoundSessionCredentials};
  crypto::ScopedMockUnexportableKeyProvider scoped_key_provider_;
  base::OnceClosure params_updated_callback_;
};

IN_PROC_BROWSER_TEST_F(BoundSessionCookieRefreshServiceImplBrowserTest,
                       RegisterNewSession) {
  EXPECT_FALSE(service()->GetBoundSessionThrottlerParams());
  base::RunLoop run_loop;
  ExpectSessionParamsUpdate(run_loop.QuitClosure());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(kDomain, KTriggerRegistrationPath)));
  run_loop.Run();

  chrome::mojom::BoundSessionThrottlerParamsPtr throttler_params =
      service()->GetBoundSessionThrottlerParams();
  ASSERT_TRUE(throttler_params);
  EXPECT_EQ(throttler_params->domain, kDomain);
  EXPECT_EQ(throttler_params->path, "/");
}
