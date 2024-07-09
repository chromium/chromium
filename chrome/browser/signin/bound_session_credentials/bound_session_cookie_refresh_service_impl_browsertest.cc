// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_impl.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64url.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_test_cookie_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/base/session_binding_test_utils.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/cookies/canonical_cookie.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using net::CanonicalCookie;
using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;
using testing::Eq;
using testing::IsEmpty;
using testing::Not;
using testing::Pointee;
using HeaderVector = net::HttpRequestHeaders::HeaderVector;

constexpr std::string_view kDomain = "google.com";
constexpr std::string_view KTriggerRegistrationPath = "/TriggerRegistration";
constexpr std::string_view kRegisterSessionPath = "/RegisterSession";
constexpr std::string_view kRotateCookiesPath = "/RotateBoundCookies";
constexpr std::string_view kChallenge = "test_challenge";

constexpr std::string_view kSessionRegistrationHeaderFormat =
    "registration=%s;supported-alg=ES256,RS256;challenge=%s";

constexpr std::string_view kCookieRotationChallengeFormat =
    "session_id=007; challenge=%s";

std::string CreateBoundSessionParamsValidJson(std::string_view domain,
                                              std::string_view path) {
  static constexpr std::string_view kBoundSessionParamsValidJsonFormat = R"(
    {
        "session_identifier": "007",
        "credentials": [
            {
                "type": "cookie",
                "name": "1P_test_cookie",
                "scope": {
                    "domain": "%s",
                    "path": "%s"
                }
            } ,
            {
                "type": "cookie",
                "name": "3P_test_cookie",
                "scope": {
                    "domain": "%s",
                    "path": "%s"
                }
            }
        ]
    }
  )";

  return base::StringPrintf(kBoundSessionParamsValidJsonFormat.data(),
                            domain.data(), path.data(), domain.data(),
                            path.data());
}

std::optional<crypto::SignatureVerifier::SignatureAlgorithm>
SignatureAlgorithmFromString(std::string_view algorithm) {
  if (algorithm == "ES256") {
    return crypto::SignatureVerifier::ECDSA_SHA256;
  } else if (algorithm == "RS256") {
    return crypto::SignatureVerifier::RSA_PKCS1_SHA256;
  }

  return std::nullopt;
}

std::vector<std::string> GetDefaultCookiesAttributesLines(const GURL& url) {
  std::vector<std::string> cookies;
  static const std::string kFirstCookieName = "1P_test_cookie";
  static const std::string kSecondCookieName = "3P_test_cookie";
  for (const std::string& cookie_name : {kFirstCookieName, kSecondCookieName}) {
    CanonicalCookie cookie =
        BoundSessionTestCookieManager::CreateCookie(url, cookie_name);
    cookies.push_back(CanonicalCookie::BuildCookieAttributesLine(cookie));
  }
  return cookies;
}

struct CookieRotationResponseParams {
  static CookieRotationResponseParams CreateSuccessWithCookies(
      const GURL& url,
      bool block_server_response = false) {
    static const std::string kSetCookieHeaderKey = "Set-Cookie";
    HeaderVector headers;
    for (const std::string& cookie_attribute_line :
         GetDefaultCookiesAttributesLines(url)) {
      headers.emplace_back(kSetCookieHeaderKey, cookie_attribute_line);
    }
    return {.headers = std::move(headers),
            .block_server_response_ = block_server_response};
  }

  static CookieRotationResponseParams CreateChallengeRequired(
      bool block_server_response = false) {
    static const std::string kChallengeHeaderKey =
        "Sec-Session-Google-Challenge";
    HeaderVector headers;
    headers.emplace_back(
        kChallengeHeaderKey,
        base::StringPrintf(kCookieRotationChallengeFormat.data(),
                           kChallenge.data()));
    return {.headers = std::move(headers),
            .status_code = net::HttpStatusCode::HTTP_UNAUTHORIZED,
            .block_server_response_ = block_server_response};
  }

  static CookieRotationResponseParams CreateServerPersistentError() {
    return {.status_code = net::HttpStatusCode::HTTP_FORBIDDEN,
            .block_server_response_ = true};
  }

  HeaderVector headers;
  net::HttpStatusCode status_code = net::HTTP_OK;
  bool block_server_response_ = false;
};

// Tests response that does not complete synchronously. It must be
// unblocked by calling the completion closure.
class BlockedHttpResponse : public net::test_server::BasicHttpResponse {
 public:
  // `send_server_response_blocked` will run when `SendResponse()` is called,
  // with a closure to unlock the pending response.
  explicit BlockedHttpResponse(
      base::OnceCallback<void(base::OnceClosure)> send_server_response_blocked)
      : send_server_response_blocked_(std::move(send_server_response_blocked)) {
  }

  void SendResponse(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
    base::OnceClosure unblock_response =
        base::BindOnce(&BlockedHttpResponse::SendResponseAfterUnblocked,
                       weak_factory_.GetWeakPtr(), delegate);
    // Bind the callback to the current sequence to ensure invoking `Run()` from
    // any thread will run the callback on the current sequence.
    base::OnceClosure unblock_from_any_thread =
        base::BindPostTaskToCurrentDefault(std::move(unblock_response));
    // Pass `unblock_any_thread` to the caller.
    std::move(send_server_response_blocked_)
        .Run(std::move(unblock_from_any_thread));
  }

 private:
  void SendResponseAfterUnblocked(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
    if (delegate) {
      BasicHttpResponse::SendResponse(delegate);
    }
  }
  base::OnceCallback<void(base::OnceClosure)> send_server_response_blocked_;

  SEQUENCE_CHECKER(my_sequence_checker_);
  base::WeakPtrFactory<BlockedHttpResponse> weak_factory_{this};
};

// Class providing handlers for bound session credentials requests.
class FakeServer {
 public:
  FakeServer() = default;
  ~FakeServer() = default;

  // `embedded_test_server` must be shut down before destroying `this`.
  // The server runs on the IO thread. The caller should bind
  // `on_cookie_rotation_response_blocked_` to the thread on which it should
  // run.
  void Initialize(net::test_server::EmbeddedTestServer& embedded_test_server,
                  base::RepeatingCallback<void(base::OnceClosure)>
                      on_cookie_rotation_response_blocked,
                  base::queue<CookieRotationResponseParams>
                      cookie_rotation_responses_params) {
    CHECK(embedded_test_server.Started());
    base_url_ = embedded_test_server.GetURL(kDomain, "/");

    on_cookie_rotation_response_blocked_ =
        std::move(on_cookie_rotation_response_blocked);
    cookie_rotation_responses_params_ =
        std::move(cookie_rotation_responses_params);
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
        base::BindRepeating(&FakeServer::HandleCookieRotationRequest,
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
    response->set_content(CreateBoundSessionParamsValidJson(kDomain, "/"));
    response->set_content_type("application/json");
    response->set_code(net::HTTP_OK);
    return response;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleCookieRotationRequest(
      const net::test_server::HttpRequest& request) {
    CHECK(!cookie_rotation_responses_params_.empty());
    const CookieRotationResponseParams& params =
        cookie_rotation_responses_params_.front();
    std::unique_ptr<net::test_server::BasicHttpResponse> response;
    if (params.block_server_response_) {
      // When the response is ready to be sent,
      // `OnCookieRotationResponseBlocked()` will be called with a callback to
      // unblock the server response.
      auto on_send_response_called = base::BindOnce(
          &FakeServer::OnCookieRotationResponseBlocked, base::Unretained(this));
      response = std::make_unique<BlockedHttpResponse>(
          std::move(on_send_response_called));
    } else {
      response = std::make_unique<net::test_server::BasicHttpResponse>();
    }
    response->set_code(params.status_code);
    for (const auto& [key, value] : params.headers) {
      response->AddCustomHeader(key, value);
    }
    cookie_rotation_responses_params_.pop();
    return response;
  }

  void OnCookieRotationResponseBlocked(
      base::OnceClosure unblock_cookie_rotation_response) {
    on_cookie_rotation_response_blocked_.Run(
        std::move(unblock_cookie_rotation_response));
  }

  [[nodiscard]] AssertionResult VerifyRegistrationJwt(std::string_view jwt) {
    std::optional<base::Value::Dict> header = signin::ExtractHeaderFromJwt(jwt);
    if (!header) {
      return AssertionFailure() << "JWT header not found";
    }
    std::optional<base::Value::Dict> payload =
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
    std::optional<crypto::SignatureVerifier::SignatureAlgorithm> algorithm =
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
  base::RepeatingCallback<void(base::OnceClosure)>
      on_cookie_rotation_response_blocked_;
  base::queue<CookieRotationResponseParams> cookie_rotation_responses_params_;
};

}  // namespace

class BoundSessionCookieRefreshServiceImplBrowserTest
    : public InProcessBrowserTest,
      public ChromeBrowserMainExtraParts {
 public:
  void SetUp() override {
    embedded_https_test_server().SetCertHostnames({std::string(kDomain)});
    CHECK(embedded_https_test_server().InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(parts);

    // Initialize the server as soon as threads are created.
    // This ensures the first cookie rotation is covered and the callback
    // `OnSendCookieRotationResponse()` will correctly be bound to the main
    // thread.
    base::OnceClosure initialize_server = base::BindOnce(
        &BoundSessionCookieRefreshServiceImplBrowserTest::InitializeServer,
        base::Unretained(this));
    class PostCreateThreadsObserver : public ChromeBrowserMainExtraParts {
     public:
      explicit PostCreateThreadsObserver(base::OnceClosure post_create_threads)
          : post_create_threads_(std::move(post_create_threads)) {}

      void PostCreateThreads() override {
        std::move(post_create_threads_).Run();
      }

     private:
      base::OnceClosure post_create_threads_;
    };
    static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
        std::make_unique<PostCreateThreadsObserver>(
            std::move(initialize_server)));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kGaiaUrl,
        embedded_https_test_server().GetURL(kDomain, "/").spec());
    // Disables DICE account consistency.
    command_line->AppendSwitchASCII("allow-browser-signin", "false");
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        "MAP * " + embedded_https_test_server().host_port_pair().ToString());
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    service()->SetBoundSessionParamsUpdatedCallbackForTesting(
        base::BindRepeating(&BoundSessionCookieRefreshServiceImplBrowserTest::
                                SessionParamsUpdated,
                            base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    service()->SetBoundSessionParamsUpdatedCallbackForTesting({});
    InProcessBrowserTest::TearDownOnMainThread();
  }

  BoundSessionCookieRefreshService* service() {
    return BoundSessionCookieRefreshServiceFactory::GetForProfile(
        browser()->profile());
  }

  void WaitOnServerCookieRotationResponseBlocked() {
    wait_on_server_cookie_rotation_response_blocked_->Run();
    // RunLoop can only be used once so create a new one for the next request.
    wait_on_server_cookie_rotation_response_blocked_ =
        std::make_unique<base::RunLoop>();
  }

  [[nodiscard]] testing::AssertionResult UnblockServerCookieRotationResponse() {
    if (!unblock_cookie_rotation_response_) {
      return testing::AssertionFailure()
             << "No pending cookie rotation response.";
    }

    std::move(unblock_cookie_rotation_response_).Run();
    return testing::AssertionSuccess();
  }

  void ExpectSessionParamsUpdate(base::OnceClosure callback) {
    CHECK(!params_updated_callback_);
    params_updated_callback_ = std::move(callback);
  }

  void RegisterNewSession() {
    EXPECT_TRUE(service()->GetBoundSessionThrottlerParams().empty());
    base::RunLoop registration_params_update;
    ExpectSessionParamsUpdate(registration_params_update.QuitClosure());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_https_test_server().GetURL(
                       kDomain, KTriggerRegistrationPath)));
    registration_params_update.Run();

    std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>
        throttler_params = service()->GetBoundSessionThrottlerParams();
    ASSERT_EQ(throttler_params.size(), 1U);
    EXPECT_EQ(throttler_params[0]->domain, kDomain);
    EXPECT_EQ(throttler_params[0]->path, "/");

    // Cookie rotation request comes immediately after session registration.
    WaitOnServerCookieRotationResponseBlocked();
    base::RunLoop rotation_params_update;
    ExpectSessionParamsUpdate(rotation_params_update.QuitClosure());
    ASSERT_TRUE(UnblockServerCookieRotationResponse());
    rotation_params_update.Run();
  }

 protected:
  virtual base::queue<CookieRotationResponseParams>
  CreateCookieRotationResponseParams(
      const net::test_server::EmbeddedTestServer& test_server) {
    // Cookie Rotation requests are set to require calling
    // `UnblockCookieRotationResponse()` to unblock the response and complete
    // the request.
    base::queue<CookieRotationResponseParams> rotation_responses_params;
    rotation_responses_params.push(
        CookieRotationResponseParams::CreateChallengeRequired());
    rotation_responses_params.push(
        CookieRotationResponseParams::CreateSuccessWithCookies(
            embedded_https_test_server().GetURL(kDomain, "/"),
            /*block_server_response=*/true));
    return rotation_responses_params;
  }

 private:
  void InitializeServer() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // If `CookieRotationResponseParams` has `block_server_response` set to
    // true, the server response will be blocked and
    // `on_server_cookie_rotation_response_blocked` will be called with a
    // callback to unblock the server response.
    auto on_server_cookie_rotation_response_blocked =
        base::BindPostTaskToCurrentDefault(base::BindRepeating(
            &BoundSessionCookieRefreshServiceImplBrowserTest::
                OnServerCookieRotationResponseBlocked,
            base::Unretained(this)));

    fake_server_.Initialize(
        embedded_https_test_server(),
        std::move(on_server_cookie_rotation_response_blocked),
        CreateCookieRotationResponseParams(embedded_https_test_server()));
    wait_on_server_cookie_rotation_response_blocked_ =
        std::make_unique<base::RunLoop>();

    embedded_test_server_handle_ =
        embedded_https_test_server().StartAcceptingConnectionsAndReturnHandle();
  }

  void OnServerCookieRotationResponseBlocked(
      base::OnceClosure unblock_cookie_rotation_response) {
    EXPECT_FALSE(unblock_cookie_rotation_response_)
        << "Concurrent cookie rotation requests are not allowed!";
    unblock_cookie_rotation_response_ =
        std::move(unblock_cookie_rotation_response);
    wait_on_server_cookie_rotation_response_blocked_->Quit();
  }

  void SessionParamsUpdated() {
    if (params_updated_callback_) {
      std::move(params_updated_callback_).Run();
    }
  }

  base::test::ScopedFeatureList feature_list_{
      switches::kEnableBoundSessionCredentials};
  crypto::ScopedMockUnexportableKeyProvider scoped_key_provider_;
  // `fake_server_` must outlive `embedded_test_server_handle_`.
  FakeServer fake_server_;
  net::test_server::EmbeddedTestServerHandle embedded_test_server_handle_;
  base::OnceClosure params_updated_callback_;
  // Only set if there is a pending cookie rotation request.
  base::OnceClosure unblock_cookie_rotation_response_;
  std::unique_ptr<base::RunLoop>
      wait_on_server_cookie_rotation_response_blocked_;
};

IN_PROC_BROWSER_TEST_F(BoundSessionCookieRefreshServiceImplBrowserTest,
                       PRE_CookieRotationOnStartup) {
  RegisterNewSession();
}

IN_PROC_BROWSER_TEST_F(BoundSessionCookieRefreshServiceImplBrowserTest,
                       CookieRotationOnStartup) {
  std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr> throttler_params =
      service()->GetBoundSessionThrottlerParams();
  ASSERT_EQ(throttler_params.size(), 1U);
  EXPECT_EQ(throttler_params[0]->domain, kDomain);
  EXPECT_EQ(throttler_params[0]->path, "/");
  base::Time cookie_expiration = throttler_params[0]->cookie_expiry_date;

  // Cookie rotation is set to happen on startup, as soon as the service
  // is created.
  WaitOnServerCookieRotationResponseBlocked();

  base::RunLoop bound_session_params_update;
  ExpectSessionParamsUpdate(bound_session_params_update.QuitClosure());

  ASSERT_TRUE(UnblockServerCookieRotationResponse());
  bound_session_params_update.Run();
  std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>
      new_throttler_params = service()->GetBoundSessionThrottlerParams();
  ASSERT_EQ(new_throttler_params.size(), 1U);
  EXPECT_GT(new_throttler_params[0]->cookie_expiry_date, cookie_expiration);
}

class BoundSessionCookieRefreshServiceImplFailingRotationBrowserTest
    : public BoundSessionCookieRefreshServiceImplBrowserTest {
 protected:
  base::queue<CookieRotationResponseParams> CreateCookieRotationResponseParams(
      const net::test_server::EmbeddedTestServer& test_server) override {
    base::queue<CookieRotationResponseParams> rotation_responses_params;
    rotation_responses_params.push(
        CookieRotationResponseParams::CreateServerPersistentError());
    return rotation_responses_params;
  }
};

// Regression test for https://crbug.com/349411334.
IN_PROC_BROWSER_TEST_F(
    BoundSessionCookieRefreshServiceImplFailingRotationBrowserTest,
    PRE_TerminateSessionClearsStorage) {
  RegisterNewSession();
  // Session should be terminated immediatelly after the first rotation fails.
  EXPECT_THAT(service()->GetBoundSessionThrottlerParams(), IsEmpty());
  // Terminated session should not be recreated at the next startup.
}

IN_PROC_BROWSER_TEST_F(
    BoundSessionCookieRefreshServiceImplFailingRotationBrowserTest,
    TerminateSessionClearsStorage) {
  EXPECT_THAT(service()->GetBoundSessionThrottlerParams(), IsEmpty());
}
