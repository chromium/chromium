// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher_impl.h"

#include <memory>
#include <string>

#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_test_cookie_manager.h"
#include "chrome/browser/signin/bound_session_credentials/session_binding_helper.h"
#include "components/signin/public/base/session_binding_test_utils.h"
#include "components/signin/public/base/session_binding_utils.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
using RefreshTestFuture =
    base::test::TestFuture<BoundSessionRefreshCookieFetcher::Result>;
using Result = BoundSessionRefreshCookieFetcher::Result;
using testing::ElementsAre;
using unexportable_keys::BackgroundTaskPriority;
using unexportable_keys::ServiceErrorOr;
using unexportable_keys::UnexportableKeyId;
using unexportable_keys::UnexportableKeyService;

constexpr char kSessionId[] = "session_id";
constexpr char kChallenge[] = "aGVsbG8_d29ybGQ";

UnexportableKeyId GenerateNewKey(
    UnexportableKeyService& unexportable_key_service) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  unexportable_key_service.GenerateSigningKeySlowlyAsync(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>(
          {crypto::SignatureVerifier::ECDSA_SHA256}),
      BackgroundTaskPriority::kUserBlocking, generate_future.GetCallback());
  ServiceErrorOr<UnexportableKeyId> key_id = generate_future.Get();
  CHECK(key_id.has_value());
  return *key_id;
}

std::string GetChallengeFromJwt(std::string_view jwt) {
  std::vector<base::StringPiece> parts = base::SplitStringPiece(
      jwt, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  if (parts.size() != 3) {
    return std::string();
  }

  std::string payload;
  if (!base::Base64UrlDecode(
          parts[1], base::Base64UrlDecodePolicy::DISALLOW_PADDING, &payload)) {
    return std::string();
  }
  absl::optional<base::Value::Dict> payload_dict =
      base::JSONReader::ReadDict(payload);
  if (!payload_dict) {
    return std::string();
  }
  std::string* challenge = payload_dict->FindString("jti");
  return challenge ? *challenge : std::string();
}

std::string CreateChallengeHeaderValue(const std::string& challenge) {
  return base::StringPrintf("session-id=%s; challenge=%s", kSessionId,
                            challenge.c_str());
}
}  // namespace

class BoundSessionRefreshCookieFetcherImplTest : public ::testing::Test {
 public:
  BoundSessionRefreshCookieFetcherImplTest()
      : unexportable_key_service_(unexportable_key_task_manager_) {
    binding_key_id_ = GenerateNewKey(unexportable_key_service_);
    session_binding_helper_ = std::make_unique<SessionBindingHelper>(
        unexportable_key_service_,
        *unexportable_key_service_.GetWrappedKey(binding_key_id_), kSessionId);
    fetcher_ = std::make_unique<BoundSessionRefreshCookieFetcherImpl>(
        test_url_loader_factory_.GetSafeWeakWrapper(),
        wait_for_network_callback_helper_, *session_binding_helper_, kGairaUrl,
        base::flat_set<std::string>{k1PSIDTSCookieName, k3PSIDTSCookieName});
    UpdateCookieList();
  }

 protected:
  const GURL kGairaUrl = GURL("https://google.com/");
  const std::string k1PSIDTSCookieName = "__Secure-1PSIDTS";
  const std::string k3PSIDTSCookieName = "__Secure-3PSIDTS";

  void UpdateCookieList(
      const base::flat_set<std::string>& excluded_cookies = {}) {
    const std::string kCookiesNames[4] = {
        "__Secure-cookie1", k1PSIDTSCookieName, "__Secure-cookie2",
        k3PSIDTSCookieName};
    cookies_.clear();
    for (const auto& cookie_name : kCookiesNames) {
      if (excluded_cookies.contains(cookie_name)) {
        continue;
      }
      cookies_.emplace_back(
          BoundSessionTestCookieManager::CreateCookie(kGairaUrl, cookie_name));
    }
  }

  std::vector<network::mojom::CookieOrLineWithAccessResultPtr>
  CreateReportedCookies(const net::CookieList& cookie_list) {
    std::vector<network::mojom::CookieOrLineWithAccessResultPtr>
        reported_cookies;
    for (auto cookie : cookie_list) {
      network::mojom::CookieOrLinePtr cookie_or_line =
          network::mojom::CookieOrLine::NewCookie(cookie);
      reported_cookies.push_back(
          network::mojom::CookieOrLineWithAccessResult::New(
              std::move(cookie_or_line), net::CookieAccessResult()));
    }
    return reported_cookies;
  }

  void SimulateChallengeRequired(const std::string& challenge_header) {
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
    network::TestURLLoaderFactory::PendingRequest* pending_request =
        test_url_loader_factory_.GetPendingRequest(0);
    network::URLLoaderCompletionStatus ok_completion_status(net::OK);
    auto response = network::CreateURLResponseHead(net::HTTP_UNAUTHORIZED);

    response->headers->AddHeader("Sec-Session-Google-Challenge",
                                 challenge_header);
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        pending_request->request.url, ok_completion_status, std::move(response),
        ""));
  }

  void SimulateOnCookiesAccessed(
      network::mojom::CookieAccessDetails::Type access_type) {
    std::vector<network::mojom::CookieAccessDetailsPtr> cookie_access_details;
    cookie_access_details.emplace_back(network::mojom::CookieAccessDetails::New(
        access_type, kGairaUrl, net::SiteForCookies(),
        CreateReportedCookies(cookies_), absl::nullopt));
    fetcher_->OnCookiesAccessed(std::move(cookie_access_details));
  }

  bool reported_cookies_notified() {
    return fetcher_->reported_cookies_notified_;
  }

  bool expected_cookies_set() { return fetcher_->expected_cookies_set_; }

  void VerifyMetricRecorded(
      BoundSessionRefreshCookieFetcher::Result expected_result) {
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    "Signin.BoundSessionCredentials.CookieRotationResult"),
                ElementsAre(base::Bucket(expected_result, /*count=*/1)));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  crypto::ScopedMockUnexportableKeyProvider scoped_key_provider_;
  unexportable_keys::UnexportableKeyTaskManager unexportable_key_task_manager_;
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_;
  UnexportableKeyId binding_key_id_;
  std::unique_ptr<SessionBindingHelper> session_binding_helper_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestWaitForNetworkCallbackHelper wait_for_network_callback_helper_;
  std::unique_ptr<BoundSessionRefreshCookieFetcherImpl> fetcher_;
  net::CookieList cookies_;
  base::HistogramTester histogram_tester_;
};

TEST_F(BoundSessionRefreshCookieFetcherImplTest, SuccessExpectedCookieSet) {
  ASSERT_FALSE(wait_for_network_callback_helper_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url,
            "https://accounts.google.com/RotateBoundCookies");
  EXPECT_EQ(pending_request->request.method, "GET");
  EXPECT_EQ(pending_request->request.credentials_mode,
            network::mojom::CredentialsMode::kInclude);

  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_TRUE(expected_cookies_set());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), Result::kSuccess);
  VerifyMetricRecorded(Result::kSuccess);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       SuccessCookiesReportedDelayed) {
  ASSERT_FALSE(wait_for_network_callback_helper_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());

  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_TRUE(expected_cookies_set());

  EXPECT_EQ(future.Get(), Result::kSuccess);
  VerifyMetricRecorded(Result::kSuccess);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       ResultNotReportedOnCookieRead) {
  ASSERT_FALSE(wait_for_network_callback_helper_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());

  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kRead);
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());

  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_EQ(future.Get(), Result::kSuccess);
  VerifyMetricRecorded(Result::kSuccess);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, CookiesNotReported) {
  ASSERT_FALSE(wait_for_network_callback_helper_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());

  task_environment_.FastForwardBy(base::Milliseconds(100));
  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());
  EXPECT_EQ(future.Get(), Result::kServerUnexepectedResponse);
  VerifyMetricRecorded(Result::kServerUnexepectedResponse);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       CookiesReportedExpectedCookieNotSet) {
  ASSERT_FALSE(wait_for_network_callback_helper_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  UpdateCookieList(
      /*excluded_cookies=*/{k1PSIDTSCookieName, k3PSIDTSCookieName});
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_FALSE(expected_cookies_set());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), Result::kServerUnexepectedResponse);
  VerifyMetricRecorded(Result::kServerUnexepectedResponse);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       CookiesReportedNotAllExpectedCookiesSet) {
  ASSERT_FALSE(wait_for_network_callback_helper_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  // Remove one of the expected cookies
  UpdateCookieList(/*excluded_cookies=*/{k3PSIDTSCookieName});
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_FALSE(expected_cookies_set());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), Result::kServerUnexepectedResponse);
  VerifyMetricRecorded(Result::kServerUnexepectedResponse);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, FailureNetError) {
  ASSERT_FALSE(wait_for_network_callback_helper_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  network::URLLoaderCompletionStatus status(net::ERR_UNEXPECTED);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url, status,
      network::mojom::URLResponseHead::New(), std::string());

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());
  BoundSessionRefreshCookieFetcher::Result result = future.Get<0>();
  EXPECT_EQ(result, Result::kConnectionError);
  VerifyMetricRecorded(Result::kConnectionError);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, FailureHttpError) {
  ASSERT_FALSE(wait_for_network_callback_helper_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "", net::HTTP_UNAUTHORIZED);

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());
  BoundSessionRefreshCookieFetcher::Result result = future.Get();
  EXPECT_EQ(result, Result::kServerPersistentError);
  VerifyMetricRecorded(Result::kServerPersistentError);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, ChallengeRequired) {
  ASSERT_FALSE(wait_for_network_callback_helper_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  SimulateChallengeRequired(CreateChallengeHeaderValue(kChallenge));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  auto headers = pending_request->request.headers;
  std::string assertion;
  EXPECT_TRUE(headers.GetHeader("Sec-Session-Google-Response", &assertion));

  EXPECT_TRUE(signin::VerifyJwtSignature(
      assertion, *unexportable_key_service_.GetAlgorithm(binding_key_id_),
      *unexportable_key_service_.GetSubjectPublicKeyInfo(binding_key_id_)));
  EXPECT_EQ(GetChallengeFromJwt(assertion), kChallenge);

  // Set required cookies and complete the request.
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), Result::kSuccess);
  VerifyMetricRecorded(Result::kSuccess);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       ChallengeRequiredNonUTF8Characters) {
  ASSERT_FALSE(wait_for_network_callback_helper_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  SimulateChallengeRequired(CreateChallengeHeaderValue("\xF0\x8F\xBF\xBE"));
  EXPECT_EQ(future.Get(), Result::kChallengeRequiredUnexpectedFormat);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       BadChallengeHeaderFormatEmpty) {
  ASSERT_FALSE(wait_for_network_callback_helper_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());
  SimulateChallengeRequired("");
  EXPECT_EQ(future.Get(), Result::kChallengeRequiredUnexpectedFormat);
  VerifyMetricRecorded(Result::kChallengeRequiredUnexpectedFormat);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       BadChallengeHeaderFormatChallengeMissing) {
  ASSERT_FALSE(wait_for_network_callback_helper_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());
  SimulateChallengeRequired("session_id=12345;");
  EXPECT_EQ(future.Get(), Result::kChallengeRequiredUnexpectedFormat);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       BadChallengeHeaderFormatChallengeFieldEmpty) {
  ASSERT_FALSE(wait_for_network_callback_helper_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());
  SimulateChallengeRequired(CreateChallengeHeaderValue(""));
  EXPECT_EQ(future.Get(), Result::kChallengeRequiredUnexpectedFormat);
  VerifyMetricRecorded(Result::kChallengeRequiredUnexpectedFormat);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       AssertionRequestsLimitExceeded) {
  ASSERT_FALSE(wait_for_network_callback_helper_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  size_t assertion_requests = 0;
  const size_t max_assertion_requests_allowed = 5;
  do {
    SimulateChallengeRequired(CreateChallengeHeaderValue(kChallenge));
    task_environment_.RunUntilIdle();
    assertion_requests++;
    ASSERT_EQ(future.IsReady(),
              assertion_requests > max_assertion_requests_allowed);
  } while (!future.IsReady());
  EXPECT_EQ(future.Get(), Result::kChallengeRequiredLimitExceeded);
  VerifyMetricRecorded(Result::kChallengeRequiredLimitExceeded);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, SignChallengeFailed) {
  // Use fake wrapped key.
  std::vector<uint8_t> wrapped_key{1, 2, 3, 4, 5};
  fetcher_.reset();
  session_binding_helper_ = std::make_unique<SessionBindingHelper>(
      unexportable_key_service_, wrapped_key, kSessionId);
  fetcher_ = std::make_unique<BoundSessionRefreshCookieFetcherImpl>(
      test_url_loader_factory_.GetSafeWeakWrapper(),
      wait_for_network_callback_helper_, *session_binding_helper_, kGairaUrl,
      base::flat_set<std::string>{k1PSIDTSCookieName, k3PSIDTSCookieName});
  ASSERT_FALSE(wait_for_network_callback_helper_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  SimulateChallengeRequired(CreateChallengeHeaderValue(kChallenge));
  EXPECT_EQ(future.Get(), Result::kSignChallengeFailed);
  VerifyMetricRecorded(Result::kSignChallengeFailed);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       GetResultFromNetErrorAndHttpStatusCode) {
  // Connection error.
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::ERR_CONNECTION_TIMED_OUT, absl::nullopt),
            Result::kConnectionError);
  // net::OK.
  EXPECT_EQ(
      fetcher_->GetResultFromNetErrorAndHttpStatusCode(net::OK, net::HTTP_OK),
      Result::kSuccess);
  // net::ERR_HTTP_RESPONSE_CODE_FAILURE
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::ERR_HTTP_RESPONSE_CODE_FAILURE, net::HTTP_BAD_REQUEST),
            Result::kServerPersistentError);
  // Persistent error.
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::OK, net::HTTP_BAD_REQUEST),
            Result::kServerPersistentError);
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::OK, net::HTTP_NOT_FOUND),
            Result::kServerPersistentError);
  // Transient error.
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::OK, net::HTTP_INTERNAL_SERVER_ERROR),
            Result::kServerTransientError);
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::OK, net::HTTP_GATEWAY_TIMEOUT),
            Result::kServerTransientError);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, NetworkDelayed) {
  wait_for_network_callback_helper_.SetNetworkCallsDelayed(true);
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 0u);

  wait_for_network_callback_helper_.SetNetworkCallsDelayed(false);
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url,
            "https://accounts.google.com/RotateBoundCookies");
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.Wait());
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, OnCookiesAccessedRead) {
  EXPECT_FALSE(reported_cookies_notified());
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kRead);
  EXPECT_FALSE(reported_cookies_notified());
  EXPECT_FALSE(expected_cookies_set());
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, OnCookiesAccessedChange) {
  EXPECT_FALSE(reported_cookies_notified());
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_TRUE(expected_cookies_set());
}

TEST(BoundSessionRefreshCookieFetcherImplParseChallengeHeaderTest,
     ParseChallengeHeader) {
  // Empty header.
  EXPECT_EQ(BoundSessionRefreshCookieFetcherImpl::ParseChallengeHeader(""), "");
  EXPECT_EQ(BoundSessionRefreshCookieFetcherImpl::ParseChallengeHeader("xyz"),
            "");
  // Non-UTF8 characters.
  EXPECT_EQ(BoundSessionRefreshCookieFetcherImpl::ParseChallengeHeader(
                CreateChallengeHeaderValue("\xF0\x8F\xBF\xBE")),
            "");
  // Empty challenge field.
  EXPECT_EQ(BoundSessionRefreshCookieFetcherImpl::ParseChallengeHeader(
                CreateChallengeHeaderValue("")),
            "");
  EXPECT_EQ(BoundSessionRefreshCookieFetcherImpl::ParseChallengeHeader(
                CreateChallengeHeaderValue(kChallenge)),
            kChallenge);
}
