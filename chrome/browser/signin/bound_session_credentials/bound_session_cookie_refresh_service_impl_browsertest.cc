// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_impl.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/base64url.h"
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
#include "components/signin/public/base/session_binding_test_utils.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;
using testing::Eq;
using testing::Pointee;

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

absl::optional<crypto::SignatureVerifier::SignatureAlgorithm>
SignatureAlgorithmFromString(std::string_view algorithm) {
  if (algorithm == "ES256") {
    return crypto::SignatureVerifier::ECDSA_SHA256;
  } else if (algorithm == "RS256") {
    return crypto::SignatureVerifier::RSA_PKCS1_SHA256;
  }

  return absl::nullopt;
}

// Class providing handlers for bound session credentials requests.
class FakeServer {
 public:
  FakeServer() = default;
  ~FakeServer() = default;

  // `embedded_test_server` must be shut down before destroying `this`.
  void Initialize(net::test_server::EmbeddedTestServer& embedded_test_server) {
    CHECK(embedded_test_server.Started());
    base_url_ = embedded_test_server.GetURL(kDomain, "/");
    embedded_test_server.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest,
        std::string(KTriggerRegistrationPath),
        base::BindRepeating(&FakeServer::HandleTriggerRegistrationRequest,
                            base::Unretained(this))));
    embedded_test_server.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest,
        std::string(kRegisterSessionPath),
        base::BindRepeating(&FakeServer::HandleRegisterSessionRequest,
                            base::Unretained(this))));
    embedded_test_server.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest,
        std::string(kRotateCookiesPath),
        base::BindRepeating(&FakeServer::CreateTransientErrorResponse,
                            base::Unretained(this))));
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse>
  HandleTriggerRegistrationRequest(
      const net::test_server::HttpRequest& request) {
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
    EXPECT_TRUE(VerifyRegistrationJwt(request.content));
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

  [[nodiscard]] AssertionResult VerifyRegistrationJwt(std::string_view jwt) {
    absl::optional<base::Value::Dict> header =
        signin::ExtractHeaderFromJwt(jwt);
    if (!header) {
      return AssertionFailure() << "JWT header not found";
    }
    absl::optional<base::Value::Dict> payload =
        signin::ExtractPayloadFromJwt(jwt);
    if (!payload) {
      return AssertionFailure() << "JWT payload not found";
    }

    // Verify that JWT fields have correct values.
    EXPECT_THAT(payload->FindString("aud"),
                Pointee(Eq(base_url_.Resolve(kRegisterSessionPath))));
    EXPECT_THAT(payload->FindString("jti"), Pointee(Eq(kChallenge)));

    // Verify the JWT signature.
    std::string* algorithm_str = header->FindString("alg");
    if (!algorithm_str) {
      return AssertionFailure() << "\"alg\" field is missing";
    }
    absl::optional<crypto::SignatureVerifier::SignatureAlgorithm> algorithm =
        SignatureAlgorithmFromString(*algorithm_str);
    if (!algorithm) {
      return AssertionFailure()
             << "\"alg\" is not recognized: " << *algorithm_str;
    }

    std::string* encoded_pubkey =
        payload->FindStringByDottedPath("key.SubjectPublicKeyInfo");
    if (!encoded_pubkey) {
      return AssertionFailure()
             << "\"key.SubjectPublicKeyInfo\" field is missing";
    }
    std::string pubkey;
    if (!base::Base64UrlDecode(*encoded_pubkey,
                               base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                               &pubkey)) {
      return AssertionFailure()
             << "Failed to decode the public key: " << *encoded_pubkey;
    }

    return signin::VerifyJwtSignature(
        jwt, *algorithm,
        base::as_bytes(base::make_span(pubkey.begin(), pubkey.end())));
  }

  GURL base_url_;
};

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
    fake_server_.Initialize(*embedded_test_server());
    service()->SetBoundSessionParamsUpdatedCallbackForTesting(
        base::BindRepeating(&BoundSessionCookieRefreshServiceImplBrowserTest::
                                SessionParamsUpdated,
                            base::Unretained(this)));
    embedded_test_server_handle_ =
        embedded_test_server()->StartAcceptingConnectionsAndReturnHandle();
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
  // `fake_server_` must outlive `embedded_test_server_handle_`.
  FakeServer fake_server_;
  net::test_server::EmbeddedTestServerHandle embedded_test_server_handle_;
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
