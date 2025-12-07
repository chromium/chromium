// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/json/json_reader.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_test_cookie_manager.h"
#include "chrome/browser/signin/bound_session_credentials/rotation_debug_info.pb.h"
#include "chrome/browser/signin/bound_session_credentials/session_binding_helper.h"
#include "components/signin/public/base/session_binding_test_utils.h"
#include "components/signin/public/base/session_binding_utils.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/unexportable_key.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace {
using RefreshTestFuture =
    base::test::TestFuture<BoundSessionRefreshCookieFetcher::Result>;
using Result = BoundSessionRefreshCookieFetcher::Result;
using base::test::RunOnceCallback;
using bound_session_credentials::RotationDebugInfo;
using testing::_;
using testing::ElementsAre;
using testing::FieldsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAre;
using unexportable_keys::BackgroundTaskPriority;
using unexportable_keys::ServiceErrorOr;
using unexportable_keys::UnexportableKeyId;
using unexportable_keys::UnexportableKeyService;
using NetErrorOrHttpStatus = std::variant<net::Error, net::HttpStatusCode>;

constexpr char kSessionId[] = "session_id";
constexpr char kChallenge[] = "aGVsbG8_d29ybGQ";
constexpr char kCachedSecSessionChallengeResponse[] =
    "cached_sec_session_challenge_response";

MATCHER_P3(JwtHasExpectedFields, session_id, challenge, destination_url, "") {
  std::string_view jwt = arg;
  std::optional<base::Value::Dict> payload_dict =
      signin::ExtractPayloadFromJwt(jwt);
  if (!payload_dict) {
    *result_listener << "Couldn't parse payload from JWT: " << jwt;
    return false;
  }

  *result_listener << " with payload " << payload_dict->DebugString();
  return testing::ExplainMatchResult(
      base::test::DictionaryHasValues(base::Value::Dict()
                                          .Set("sub", session_id)
                                          .Set("jti", challenge)
                                          .Set("aud", destination_url.spec())),
      *payload_dict, result_listener);
}

class MockSessionBindingHelper : public SessionBindingHelper {
 public:
  explicit MockSessionBindingHelper(
      unexportable_keys::UnexportableKeyService& unexportable_key_service)
      : SessionBindingHelper(unexportable_key_service,
                             base::span<uint8_t>(),
                             std::string()) {}

  ~MockSessionBindingHelper() override = default;

  MOCK_METHOD(void, MaybeLoadBindingKey, (), (override));
  MOCK_METHOD(
      void,
      GenerateBindingKeyAssertion,
      (std::string_view challenge,
       const GURL& destination_url,
       base::OnceCallback<void(base::expected<std::string, Error>)> callback),
      (override));
};

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

std::string CreateChallengeHeaderValue(
    const std::string& challenge,
    const std::string& session_id = kSessionId) {
  return base::StringPrintf("session_id=%s; challenge=%s", session_id.c_str(),
                            challenge.c_str());
}
}  // namespace

class BoundSessionRefreshCookieFetcherImplTest : public ::testing::Test {
 public:
  BoundSessionRefreshCookieFetcherImplTest() {
    binding_key_id_ = GenerateNewKey(unexportable_key_service_);
    session_binding_helper_ = std::make_unique<SessionBindingHelper>(
        unexportable_key_service_,
        *unexportable_key_service_.GetWrappedKey(binding_key_id_), kSessionId);
    fetcher_ = std::make_unique<BoundSessionRefreshCookieFetcherImpl>(
        test_url_loader_factory_.GetSafeWeakWrapper(), *session_binding_helper_,
        kSessionId, kRefreshUrl, kGaiaUrl,
        base::flat_set<std::string>{k1PSIDTSCookieName, k3PSIDTSCookieName},
        /*is_off_the_record_profile=*/false,
        BoundSessionRefreshCookieFetcher::Trigger::kOther,
        bound_session_credentials::RotationDebugInfo());
    UpdateCookieList();
  }

 protected:
  struct GenerateAssertionExpectations {
    size_t was_called_count = 0;
    SessionBindingHelper::Error error;
  };

  const GURL kGaiaUrl = GURL("https://google.com/");
  const GURL kRefreshUrl = GURL("https://accounts.google.com/rotate");
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
          BoundSessionTestCookieManager::CreateCookie(kGaiaUrl, cookie_name));
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
        access_type, kGaiaUrl, url::Origin(), url::Origin(),
        net::SiteForCookies(), CreateReportedCookies(cookies_), std::nullopt,
        /*is_ad_tagged=*/false, net::CookieSettingOverrides()));
    fetcher_->OnCookiesAccessed(std::move(cookie_access_details));
  }

  bool reported_cookies_notified() {
    return fetcher_->reported_cookies_notified_;
  }

  const std::optional<std::string>& sec_session_challenge_response() {
    return fetcher_->sec_session_challenge_response_;
  }

  void VerifyMetricsRecorded(
      BoundSessionRefreshCookieFetcher::Result expected_result,
      const std::vector<NetErrorOrHttpStatus>& responses,
      std::optional<GenerateAssertionExpectations>
          generate_assertion_expectations = std::nullopt,
      bool started_with_cached_challenge = false) {
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    "Signin.BoundSessionCredentials.CookieRotationResult"),
                ElementsAre(base::Bucket(expected_result, /*count=*/1)));
    histogram_tester_.ExpectTotalCount(
        "Signin.BoundSessionCredentials.CookieRotationTotalDuration", 1);

    // Tests in this file use
    // `BoundSessionRefreshCookieFetcher::Trigger::kOther` for the histogram
    // suffix.
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            "Signin.BoundSessionCredentials.CookieRotationResult.Other"),
        ElementsAre(base::Bucket(expected_result, /*count=*/1)));
    histogram_tester_.ExpectTotalCount(
        "Signin.BoundSessionCredentials.CookieRotationTotalDuration.Other", 1);

    if (generate_assertion_expectations.has_value()) {
      histogram_tester_.ExpectTotalCount(
          "Signin.BoundSessionCredentials."
          "CookieRotationGenerateAssertionDuration",
          generate_assertion_expectations->was_called_count);
      histogram_tester_.ExpectUniqueSample(
          "Signin.BoundSessionCredentials."
          "CookieRotationGenerateAssertionResult",
          generate_assertion_expectations->error,
          generate_assertion_expectations->was_called_count);
    } else {
      histogram_tester_.ExpectTotalCount(
          "Signin.BoundSessionCredentials."
          "CookieRotationGenerateAssertionDuration",
          /*expected_count=*/0);
      histogram_tester_.ExpectTotalCount(
          "Signin.BoundSessionCredentials."
          "CookieRotationGenerateAssertionResult",
          /*expected_count=*/0);
    }

    std::vector<base::Bucket> expected_net_error_buckets;
    if (expected_result == Result::kConnectionError) {
      // Response producing a `kConnectionError` is necessarily the last one as
      // it terminates the fetch.
      int net_error = std::visit(
          absl::Overload{[](net::Error error) -> int { return error; },
                         [](net::HttpStatusCode http_code) -> int {
                           return net::ERR_HTTP_RESPONSE_CODE_FAILURE;
                         }},
          *responses.rbegin());
      expected_net_error_buckets.emplace_back(-net_error, /*count=*/1);
    }
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    "Signin.BoundSessionCredentials.CookieRotationNetError"),
                testing::ElementsAreArray(expected_net_error_buckets));

    std::map<int, int> expected_http_result_buckets_before_challenge;
    std::map<int, int> expected_http_result_buckets_after_challenge;
    bool received_challenge = false;
    for (const auto& response : responses) {
      int value = std::visit(
          absl::Overload{
              [](net::Error error) -> int { return error; },
              [](net::HttpStatusCode http_code) -> int { return http_code; }},
          response);
      if (received_challenge) {
        expected_http_result_buckets_after_challenge[value]++;
      } else {
        expected_http_result_buckets_before_challenge[value]++;
      }
      if (value == net::HTTP_UNAUTHORIZED) {
        // We assume that a challenge is delivered in the first
        // net::HTTP_UNAUTHORIZED response. If this response doesn't contain a
        // challenge, it will be considered a failure. No further requests are
        // expected after that, so it's fine to flip this bit.
        received_challenge = true;
      }
    }
    auto pair_to_bucket = [](std::pair<int, int> value_count) {
      return base::Bucket(value_count.first, value_count.second);
    };
    const std::string_view before_challenge_histogram_name =
        started_with_cached_challenge
            ? "Signin.BoundSessionCredentials.CookieRotationHttpResult."
              "WithCachedChallenge"
            : "Signin.BoundSessionCredentials.CookieRotationHttpResult."
              "WithoutChallenge";
    const std::string_view after_challenge_histogram_name =
        "Signin.BoundSessionCredentials.CookieRotationHttpResult."
        "WithFreshChallenge";
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(before_challenge_histogram_name),
        testing::ElementsAreArray(base::ToVector(
            expected_http_result_buckets_before_challenge, pair_to_bucket)));
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(after_challenge_histogram_name),
        testing::ElementsAreArray(base::ToVector(
            expected_http_result_buckets_after_challenge, pair_to_bucket)));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  crypto::ScopedFakeUnexportableKeyProvider scoped_key_provider_;
  unexportable_keys::UnexportableKeyTaskManager unexportable_key_task_manager_;
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_{
      unexportable_key_task_manager_,
      crypto::UnexportableKeyProvider::Config()};
  UnexportableKeyId binding_key_id_;
  std::unique_ptr<SessionBindingHelper> session_binding_helper_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<BoundSessionRefreshCookieFetcherImpl> fetcher_;
  net::CookieList cookies_;
  base::HistogramTester histogram_tester_;
};

TEST_F(BoundSessionRefreshCookieFetcherImplTest, SuccessExpectedCookieSet) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), std::nullopt);

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url, kRefreshUrl);
  EXPECT_EQ(pending_request->request.method, "GET");
  EXPECT_EQ(pending_request->request.credentials_mode,
            network::mojom::CredentialsMode::kInclude);

  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_THAT(fetcher_->GetNonRefreshedCookieNames(), IsEmpty());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), Result::kSuccess);
  EXPECT_FALSE(fetcher_->IsChallengeReceived());
  VerifyMetricsRecorded(Result::kSuccess, {net::HTTP_OK});
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       SuccessCookiesReportedDelayed) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), std::nullopt);

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
  EXPECT_THAT(fetcher_->GetNonRefreshedCookieNames(), IsEmpty());

  EXPECT_EQ(future.Get(), Result::kSuccess);
  VerifyMetricsRecorded(Result::kSuccess, {net::HTTP_OK});
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       ResultNotReportedOnCookieRead) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), std::nullopt);

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
  VerifyMetricsRecorded(Result::kSuccess, {net::HTTP_OK});
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, CookiesNotReported) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), std::nullopt);

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
  VerifyMetricsRecorded(Result::kServerUnexepectedResponse, {net::HTTP_OK});
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       CookiesReportedExpectedCookieNotSet) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), std::nullopt);

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  UpdateCookieList(
      /*excluded_cookies=*/{k1PSIDTSCookieName, k3PSIDTSCookieName});
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_THAT(fetcher_->GetNonRefreshedCookieNames(),
              UnorderedElementsAre(k1PSIDTSCookieName, k3PSIDTSCookieName));

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), Result::kServerUnexepectedResponse);
  VerifyMetricsRecorded(Result::kServerUnexepectedResponse, {net::HTTP_OK});
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       CookiesReportedNotAllExpectedCookiesSet) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), std::nullopt);

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  // Remove one of the expected cookies
  UpdateCookieList(/*excluded_cookies=*/{k3PSIDTSCookieName});
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_THAT(fetcher_->GetNonRefreshedCookieNames(),
              UnorderedElementsAre(k3PSIDTSCookieName));

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), Result::kServerUnexepectedResponse);
  VerifyMetricsRecorded(Result::kServerUnexepectedResponse, {net::HTTP_OK});
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, FailureNetError) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), std::nullopt);

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
  VerifyMetricsRecorded(Result::kConnectionError, {net::ERR_UNEXPECTED});
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, FailureHttpError) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), std::nullopt);

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "", net::HTTP_UNAUTHORIZED);

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());
  BoundSessionRefreshCookieFetcher::Result result = future.Get();
  EXPECT_EQ(result, Result::kServerPersistentError);
  VerifyMetricsRecorded(Result::kServerPersistentError,
                        {net::HTTP_UNAUTHORIZED});
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       FailureProxyAuthenticationError) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), std::nullopt);

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  network::mojom::URLResponseHeadPtr head =
      network::CreateURLResponseHead(net::HTTP_PROXY_AUTHENTICATION_REQUIRED);
  head->mime_type = "text/html";
  head->headers->AddHeader("Proxy-Authenticate", "Basic realm=\"test\"");
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url, network::URLLoaderCompletionStatus(),
      std::move(head), "");

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());
  BoundSessionRefreshCookieFetcher::Result result = future.Get();
  EXPECT_EQ(result, Result::kConnectionError);
  VerifyMetricsRecorded(Result::kConnectionError,
                        {net::HTTP_PROXY_AUTHENTICATION_REQUIRED});
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, ChallengeRequired) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), std::nullopt);
  EXPECT_FALSE(fetcher_->IsChallengeReceived());

  SimulateChallengeRequired(CreateChallengeHeaderValue(kChallenge));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url, kRefreshUrl);
  auto headers = pending_request->request.headers;
  std::optional<std::string> assertion =
      headers.GetHeader("Sec-Session-Google-Response");
  ASSERT_TRUE(assertion);

  EXPECT_TRUE(signin::VerifyJwtSignature(
      assertion.value(),
      *unexportable_key_service_.GetAlgorithm(binding_key_id_),
      *unexportable_key_service_.GetSubjectPublicKeyInfo(binding_key_id_)));
  EXPECT_THAT(assertion.value(),
              JwtHasExpectedFields(kSessionId, kChallenge, kRefreshUrl));

  // Set required cookies and complete the request.
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), Result::kSuccess);
  EXPECT_TRUE(fetcher_->IsChallengeReceived());
  EXPECT_EQ(sec_session_challenge_response(), assertion);
  const GenerateAssertionExpectations generate_assertion_expectations = {
      .was_called_count = 1, .error = SessionBindingHelper::kNoErrorForMetrics};
  VerifyMetricsRecorded(Result::kSuccess,
                        {net::HTTP_UNAUTHORIZED, net::HTTP_OK},
                        generate_assertion_expectations);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       ChallengeRequiredNonUTF8Characters) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), std::nullopt);

  SimulateChallengeRequired(CreateChallengeHeaderValue("\xF0\x8F\xBF\xBE"));
  EXPECT_EQ(future.Get(), Result::kChallengeRequiredUnexpectedFormat);
  EXPECT_EQ(sec_session_challenge_response(), std::nullopt);
  VerifyMetricsRecorded(Result::kChallengeRequiredUnexpectedFormat,
                        {net::HTTP_UNAUTHORIZED});
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       BadChallengeHeaderFormatEmpty) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), std::nullopt);
  SimulateChallengeRequired("");
  EXPECT_EQ(future.Get(), Result::kChallengeRequiredUnexpectedFormat);
  VerifyMetricsRecorded(Result::kChallengeRequiredUnexpectedFormat,
                        {net::HTTP_UNAUTHORIZED});
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       BadChallengeHeaderFormatChallengeMissing) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), std::nullopt);
  SimulateChallengeRequired("session_id=12345;");
  EXPECT_EQ(future.Get(), Result::kChallengeRequiredUnexpectedFormat);
  VerifyMetricsRecorded(Result::kChallengeRequiredUnexpectedFormat,
                        {net::HTTP_UNAUTHORIZED});
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       BadChallengeHeaderSessionIdsDontMatch) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), std::nullopt);
  SimulateChallengeRequired(
      CreateChallengeHeaderValue(/*challenge=*/"test", /*session_id=*/"12345"));
  EXPECT_EQ(future.Get(), Result::kChallengeRequiredSessionIdMismatch);
  VerifyMetricsRecorded(Result::kChallengeRequiredSessionIdMismatch,
                        {net::HTTP_UNAUTHORIZED});
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       BadChallengeHeaderFormatChallengeFieldEmpty) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), std::nullopt);
  SimulateChallengeRequired(CreateChallengeHeaderValue(""));
  EXPECT_EQ(future.Get(), Result::kChallengeRequiredUnexpectedFormat);
  VerifyMetricsRecorded(Result::kChallengeRequiredUnexpectedFormat,
                        {net::HTTP_UNAUTHORIZED});
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       AssertionRequestsLimitExceeded) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), std::nullopt);

  size_t assertion_requests = 0;
  const size_t max_assertion_requests_allowed = 5;
  std::vector<NetErrorOrHttpStatus> response_codes;
  do {
    SimulateChallengeRequired(CreateChallengeHeaderValue(kChallenge));
    response_codes.push_back(net::HTTP_UNAUTHORIZED);
    task_environment_.RunUntilIdle();
    assertion_requests++;
    ASSERT_EQ(future.IsReady(),
              assertion_requests > max_assertion_requests_allowed);
  } while (!future.IsReady());
  EXPECT_EQ(future.Get(), Result::kChallengeRequiredLimitExceeded);
  const GenerateAssertionExpectations generate_assertion_expectations = {
      .was_called_count = assertion_requests - 1,
      .error = SessionBindingHelper::kNoErrorForMetrics};
  VerifyMetricsRecorded(Result::kChallengeRequiredLimitExceeded, response_codes,
                        generate_assertion_expectations);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       InitialSecSessionChallengeResponseNoChallengeRequired) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), kCachedSecSessionChallengeResponse);
  EXPECT_TRUE(sec_session_challenge_response().has_value());

  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");
  EXPECT_EQ(future.Get(), Result::kSuccess);
  // Challenge not reset.
  EXPECT_EQ(sec_session_challenge_response(),
            kCachedSecSessionChallengeResponse);
  VerifyMetricsRecorded(Result::kSuccess, {net::HTTP_OK},
                        /*generate_assertion_expectations=*/std::nullopt,
                        /*started_with_cached_challenge=*/true);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       InitialSecSessionChallengeResponseChallengeRequired) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), kCachedSecSessionChallengeResponse);
  EXPECT_TRUE(sec_session_challenge_response().has_value());

  SimulateChallengeRequired(CreateChallengeHeaderValue(kChallenge));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  auto headers = pending_request->request.headers;
  std::optional<std::string> assertion =
      headers.GetHeader("Sec-Session-Google-Response");
  ASSERT_TRUE(assertion);

  // Complete the request.
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");
  EXPECT_EQ(future.Get(), Result::kSuccess);
  EXPECT_EQ(sec_session_challenge_response(), assertion);
  const GenerateAssertionExpectations generate_assertion_expectations = {
      .was_called_count = 1, .error = SessionBindingHelper::kNoErrorForMetrics};
  VerifyMetricsRecorded(Result::kSuccess,
                        {net::HTTP_UNAUTHORIZED, net::HTTP_OK},
                        generate_assertion_expectations,
                        /*started_with_cached_challenge=*/true);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       InitialSecSessionChallengeResponseMultipleChallengeRequired) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), kCachedSecSessionChallengeResponse);
  EXPECT_TRUE(sec_session_challenge_response().has_value());

  SimulateChallengeRequired(CreateChallengeHeaderValue(kChallenge));
  task_environment_.RunUntilIdle();

  SimulateChallengeRequired(CreateChallengeHeaderValue("abcdef"));
  task_environment_.RunUntilIdle();

  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  auto headers = pending_request->request.headers;
  std::optional<std::string> assertion =
      headers.GetHeader("Sec-Session-Google-Response");
  ASSERT_TRUE(assertion);

  // Complete the request.
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");
  EXPECT_EQ(future.Get(), Result::kSuccess);
  EXPECT_EQ(sec_session_challenge_response(), assertion);
  const GenerateAssertionExpectations generate_assertion_expectations = {
      .was_called_count = 2, .error = SessionBindingHelper::kNoErrorForMetrics};
  VerifyMetricsRecorded(
      Result::kSuccess,
      {net::HTTP_UNAUTHORIZED, net::HTTP_UNAUTHORIZED, net::HTTP_OK},
      generate_assertion_expectations,
      /*started_with_cached_challenge=*/true);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       InitialSecSessionChallengeResponseChallengeRequiredError) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), kCachedSecSessionChallengeResponse);
  EXPECT_THAT(sec_session_challenge_response(),
              kCachedSecSessionChallengeResponse);

  SimulateChallengeRequired(CreateChallengeHeaderValue("\xF0\x8F\xBF\xBE"));
  // Cached challenge response is reset.
  EXPECT_EQ(future.Get(), Result::kChallengeRequiredUnexpectedFormat);
  EXPECT_EQ(sec_session_challenge_response(), std::nullopt);
  VerifyMetricsRecorded(Result::kChallengeRequiredUnexpectedFormat,
                        {net::HTTP_UNAUTHORIZED},
                        /*generate_assertion_expectations=*/std::nullopt,
                        /*started_with_cached_challenge=*/true);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       GetResultFromNetErrorAndHttpStatusCode) {
  // Connection error.
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::ERR_CONNECTION_TIMED_OUT, std::nullopt),
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

TEST_F(BoundSessionRefreshCookieFetcherImplTest, OnCookiesAccessedRead) {
  EXPECT_FALSE(reported_cookies_notified());
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kRead);
  EXPECT_FALSE(reported_cookies_notified());
  EXPECT_FALSE(fetcher_->GetNonRefreshedCookieNames().empty());
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, OnCookiesAccessedChange) {
  EXPECT_FALSE(reported_cookies_notified());
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_TRUE(fetcher_->GetNonRefreshedCookieNames().empty());
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, DebugHeaderSent) {
  RotationDebugInfo info;
  RotationDebugInfo::FailureCounter* counter =
      info.add_errors_since_last_rotation();
  counter->set_type(RotationDebugInfo::CONNECTION_ERROR);
  counter->set_count(2);

  fetcher_ = std::make_unique<BoundSessionRefreshCookieFetcherImpl>(
      test_url_loader_factory_.GetSafeWeakWrapper(), *session_binding_helper_,
      kSessionId, kRefreshUrl, kGaiaUrl,
      base::flat_set<std::string>{k1PSIDTSCookieName, k3PSIDTSCookieName},
      /*is_off_the_record_profile_=*/false,
      BoundSessionRefreshCookieFetcher::Trigger::kOther, info);
  RefreshTestFuture future;
  // Skip some time to create a difference between the the fetcher creation time
  // and the request start time.
  task_environment_.FastForwardBy(base::Seconds(5));
  base::Time time_at_start = base::Time::Now();
  fetcher_->Start(future.GetCallback(), std::nullopt);
  task_environment_.FastForwardBy(base::Seconds(10));

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  auto headers = pending_request->request.headers;
  std::optional<std::string> sent_info_base64 =
      headers.GetHeader("Sec-Session-Google-Rotation-Debug-Info");
  ASSERT_TRUE(sent_info_base64);

  std::string sent_info_serialized;
  ASSERT_TRUE(
      base::Base64Decode(sent_info_base64.value(), &sent_info_serialized));
  RotationDebugInfo sent_info;
  ASSERT_TRUE(sent_info.ParseFromString(sent_info_serialized));

  RotationDebugInfo expected_info = info;
  // Fetcher should set the request time when starting the request.
  *expected_info.mutable_request_time() =
      bound_session_credentials::TimeToTimestamp(time_at_start);
  EXPECT_THAT(sent_info, base::test::EqualsProto(expected_info));
}

TEST(BoundSessionRefreshCookieFetcherImplParseChallengeHeaderTest,
     ParseChallengeHeader) {
  auto parse = &BoundSessionRefreshCookieFetcherImpl::ParseChallengeHeader;
  // Empty header.
  EXPECT_THAT(parse(""), FieldsAre("", ""));
  EXPECT_THAT(parse("xyz"), FieldsAre("", ""));
  // Empty challenge field.
  EXPECT_THAT(parse(CreateChallengeHeaderValue("")), FieldsAre("", kSessionId));
  // Empty session ID field.
  EXPECT_THAT(parse(CreateChallengeHeaderValue(kChallenge, "")),
              FieldsAre(kChallenge, ""));
  // Both fields are set.
  EXPECT_THAT(parse(CreateChallengeHeaderValue(kChallenge)),
              FieldsAre(kChallenge, kSessionId));
  EXPECT_THAT(
      parse(CreateChallengeHeaderValue("other_challenge", "other_session_id")),
      FieldsAre("other_challenge", "other_session_id"));
}

class BoundSessionRefreshCookieFetcherImplSignChallengeFailedTest
    : public BoundSessionRefreshCookieFetcherImplTest,
      public testing::WithParamInterface<SessionBindingHelper::Error> {};

TEST_P(BoundSessionRefreshCookieFetcherImplSignChallengeFailedTest,
       SignChallengeFailed) {
  const SessionBindingHelper::Error generate_assertion_error = GetParam();

  // Reset the fetcher to use the mock session binding helper to simulate the
  // error.
  fetcher_.reset();
  auto mock_helper =
      std::make_unique<MockSessionBindingHelper>(unexportable_key_service_);
  raw_ptr<MockSessionBindingHelper> mock_session_binding_helper =
      mock_helper.get();
  session_binding_helper_ = std::move(mock_helper);
  fetcher_ = std::make_unique<BoundSessionRefreshCookieFetcherImpl>(
      test_url_loader_factory_.GetSafeWeakWrapper(), *session_binding_helper_,
      kSessionId, kRefreshUrl, kGaiaUrl,
      base::flat_set<std::string>{k1PSIDTSCookieName, k3PSIDTSCookieName},
      /*is_off_the_record_profile_=*/false,
      BoundSessionRefreshCookieFetcher::Trigger::kOther,
      bound_session_credentials::RotationDebugInfo());

  EXPECT_CALL(*mock_session_binding_helper,
              GenerateBindingKeyAssertion(kChallenge, _, _))
      .WillOnce(RunOnceCallback<2>(base::unexpected(generate_assertion_error)));

  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback(), std::nullopt);
  SimulateChallengeRequired(CreateChallengeHeaderValue(kChallenge));

  EXPECT_EQ(future.Get(), Result::kSignChallengeFailed);
  EXPECT_EQ(sec_session_challenge_response(), std::nullopt);
  const GenerateAssertionExpectations generate_assertion_expectations = {
      .was_called_count = 1, .error = generate_assertion_error};
  VerifyMetricsRecorded(
      BoundSessionRefreshCookieFetcher::Result::kSignChallengeFailed,
      {net::HTTP_UNAUTHORIZED}, generate_assertion_expectations);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    BoundSessionRefreshCookieFetcherImplSignChallengeFailedTest,
    testing::Values(SessionBindingHelper::Error::kLoadKeyFailure,
                    SessionBindingHelper::Error::kCreateAssertionFailure,
                    SessionBindingHelper::Error::kSignAssertionFailure,
                    SessionBindingHelper::Error::kAppendSignatureFailure));
