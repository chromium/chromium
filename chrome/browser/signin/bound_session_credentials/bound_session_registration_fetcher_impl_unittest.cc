// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_impl.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_storage.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher.h"
#include "components/signin/public/base/session_binding_test_utils.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace {

using unexportable_keys::ServiceErrorOr;
using unexportable_keys::UnexportableKeyId;
using RegistrationError =
    BoundSessionRegistrationFetcherImpl::RegistrationError;
using RegistrationResultFuture = base::test::TestFuture<
    std::optional<bound_session_credentials::BoundSessionParams>>;

constexpr std::string_view kXssiPrefix = ")]}'";
constexpr std::string_view kBoundSessionParamsValidJson = R"(
    {
        "session_identifier": "007",
        "credentials": [
            {
                "type": "cookie",
                "name": "auth_cookie_1P",
                "scope": {
                    "domain": ".google.com",
                    "path": "/"
                }
            },
            {
                "type": "cookie",
                "name": "auth_cookie_3P",
                "scope": {
                    "domain": ".google.com",
                    "path": "/"
                }
            }
        ],
        "refresh_url": "/rotate"
    }
)";
constexpr std::string_view kBoundSessionParamsMissingSessionIdJson = R"(
    {
        "credentials": [
            {
                "type": "cookie",
                "name": "auth_cookie",
                "scope": {
                    "domain": ".google.com",
                    "path": "/"
                }
            }
        ],
        "refresh_url": "/rotate"
    }
)";
constexpr std::string_view kChallenge = "test_challenge";

// Checks equality of the two protos in an std::tuple. Useful for matching two
// two protos using ::testing::Pointwise or ::testing::UnorderedPointwise.
MATCHER(TupleEqualsProto, "") {
  return testing::ExplainMatchResult(base::test::EqualsProto(std::get<1>(arg)),
                                     std::get<0>(arg), result_listener);
}

std::vector<crypto::SignatureVerifier::SignatureAlgorithm> CreateAlgArray() {
  return {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
          crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256};
}

bound_session_credentials::Credential CreateTestBoundSessionCredential(
    const std::string& name,
    const std::string& domain,
    const std::string& path) {
  bound_session_credentials::Credential credential;
  bound_session_credentials::CookieCredential* cookie =
      credential.mutable_cookie_credential();
  cookie->set_name(name);
  cookie->set_domain(domain);
  cookie->set_path(path);
  return credential;
}

bound_session_credentials::BoundSessionParams CreateTestBoundSessionParams(
    const std::string& wrapped_key) {
  bound_session_credentials::BoundSessionParams params;
  params.set_site("https://google.com/");
  params.set_session_id("007");
  params.set_wrapped_key(wrapped_key);
  params.set_refresh_url("https://www.google.com/rotate");
  *params.mutable_creation_time() =
      bound_session_credentials::TimeToTimestamp(base::Time::Now());

  *params.add_credentials() =
      CreateTestBoundSessionCredential("auth_cookie_1P", ".google.com", "/");
  *params.add_credentials() =
      CreateTestBoundSessionCredential("auth_cookie_3P", ".google.com", "/");
  return params;
}

base::Value::Dict CreateBoundSessionCredentialDict(const std::string& name,
                                                   const std::string& domain,
                                                   const std::string& path) {
  return base::Value::Dict()
      .Set("type", "cookie")
      .Set("name", name)
      .Set("scope",
           base::Value::Dict().Set("domain", domain).Set("path", path));
}
}  // namespace

class BoundSessionRegistrationFetcherImplTest : public testing::Test {
 public:
  BoundSessionRegistrationFetcherImplTest()
      : unexportable_key_service_(task_manager_) {
    url_loader_factory_.SetInterceptor(base::BindRepeating(
        &BoundSessionRegistrationFetcherImplTest::OnRequestIntercepted,
        base::Unretained(this)));
  }

  unexportable_keys::UnexportableKeyService& unexportable_key_service() {
    return unexportable_key_service_;
  }

  void RunBackgroundTasks() { task_environment_.RunUntilIdle(); }

  // This can be called before or after the registration request is made.
  void SetUpServerResponse(std::string_view body,
                           net::HttpStatusCode http_code = net::HTTP_OK) {
    url_loader_factory_.AddResponse(kRegistrationUrl.spec(), std::string(body),
                                    http_code);
    // Wait for a pending request to be handled.
    if (WasRequestSent()) {
      RunBackgroundTasks();
    }
  }
  void SetUpServerResponse(int network_error) {
    url_loader_factory_.AddResponse(
        kRegistrationUrl, network::mojom::URLResponseHead::New(),
        /*content=*/std::string(),
        network::URLLoaderCompletionStatus(network_error));
    // Wait for a pending request to be handled.
    if (WasRequestSent()) {
      RunBackgroundTasks();
    }
  }

  bool WasRequestSent() const { return has_intercepted_request_; }

  std::string_view GetRequestBody() const {
    EXPECT_TRUE(has_intercepted_request_);
    return intercepted_request_body_;
  }

  std::unique_ptr<BoundSessionRegistrationFetcherImpl> CreateFetcher() {
    BoundSessionRegistrationFetcherParam params =
        BoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
            kRegistrationUrl, CreateAlgArray(), std::string(kChallenge));
    return std::make_unique<BoundSessionRegistrationFetcherImpl>(
        std::move(params), url_loader_factory_.GetSafeWeakWrapper(),
        unexportable_key_service(), /*is_off_the_record_profile=*/false);
  }

  void DisableKeyProvider() {
    // Using `emplace()` to destroy the existing scoped object before
    // constructing a new one.
    scoped_key_provider_.emplace<crypto::ScopedNullUnexportableKeyProvider>();
  }

  void ExpectRecordedMetrics(RegistrationError error) {
    histogram_tester_.ExpectUniqueSample(
        "Signin.BoundSessionCredentials.SessionRegistrationResult", error, 1);
    histogram_tester_.ExpectTotalCount(
        "Signin.BoundSessionCredentials.SessionRegistrationTotalDuration", 1);
    histogram_tester_.ExpectTotalCount(
        "Signin.BoundSessionCredentials."
        "SessionRegistrationGenerateRegistrationTokenDuration",
        1);
  }

 private:
  void OnRequestIntercepted(const network::ResourceRequest& request) {
    EXPECT_EQ(request.url, kRegistrationUrl) << "Unexpected request";
    ASSERT_FALSE(has_intercepted_request_) << "Duplicated request";
    has_intercepted_request_ = true;
    ASSERT_FALSE(request.request_body.get()->elements()->empty());
    intercepted_request_body_ = std::string(request.request_body.get()
                                                ->elements()
                                                ->at(0)
                                                .As<network::DataElementBytes>()
                                                .AsStringPiece());
  }

  const GURL kRegistrationUrl = GURL("https://www.google.com/startsession");

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::
          QUEUED};  // QUEUED - tasks don't run until `RunUntilIdle()` is
                    // called.
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  // Provides a mock key provider by default.
  absl::variant<crypto::ScopedMockUnexportableKeyProvider,
                crypto::ScopedNullUnexportableKeyProvider>
      scoped_key_provider_;
  unexportable_keys::UnexportableKeyTaskManager task_manager_{
      crypto::UnexportableKeyProvider::Config()};
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_;
  network::TestURLLoaderFactory url_loader_factory_;
  base::HistogramTester histogram_tester_;

  bool has_intercepted_request_ = false;
  std::string intercepted_request_body_;
};

TEST_F(BoundSessionRegistrationFetcherImplTest, ValidInput) {
  std::unique_ptr<BoundSessionRegistrationFetcher> fetcher = CreateFetcher();
  RegistrationResultFuture future;

  fetcher->Start(future.GetCallback());
  EXPECT_FALSE(WasRequestSent());
  EXPECT_FALSE(future.IsReady());

  RunBackgroundTasks();
  EXPECT_TRUE(WasRequestSent());
  EXPECT_FALSE(future.IsReady());

  SetUpServerResponse(
      base::StrCat({kXssiPrefix, kBoundSessionParamsValidJson}));
  EXPECT_TRUE(future.IsReady());
  ASSERT_TRUE(future.Get().has_value());
  ASSERT_TRUE(bound_session_credentials::AreParamsValid(*future.Get()));
  EXPECT_THAT(future.Get(),
              testing::Optional(base::test::EqualsProto(
                  CreateTestBoundSessionParams(future.Get()->wrapped_key()))));

  ExpectRecordedMetrics(RegistrationError::kNone);

  // Verify the wrapped key.
  std::string wrapped_key = future.Get<>()->wrapped_key();
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>>
      wrapped_key_to_key_id;
  unexportable_key_service().FromWrappedSigningKeySlowlyAsync(
      base::make_span(
          std::vector<uint8_t>(wrapped_key.begin(), wrapped_key.end())),
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      wrapped_key_to_key_id.GetCallback());
  EXPECT_TRUE(wrapped_key_to_key_id.IsReady());
  EXPECT_TRUE(wrapped_key_to_key_id.Get().has_value());

  // Verify that the request body contains a valid registration token.
  UnexportableKeyId key_id = wrapped_key_to_key_id.Get().value();
  EXPECT_TRUE(signin::VerifyJwtSignature(
      GetRequestBody(), *unexportable_key_service().GetAlgorithm(key_id),
      *unexportable_key_service().GetSubjectPublicKeyInfo(key_id)));
}

TEST_F(BoundSessionRegistrationFetcherImplTest, MissingXSSIPrefix) {
  SetUpServerResponse(kBoundSessionParamsValidJson);
  std::unique_ptr<BoundSessionRegistrationFetcher> fetcher = CreateFetcher();
  RegistrationResultFuture future;

  fetcher->Start(future.GetCallback());
  RunBackgroundTasks();
  EXPECT_TRUE(WasRequestSent());
  EXPECT_TRUE(future.IsReady());
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_THAT(future.Get(),
              testing::Optional(base::test::EqualsProto(
                  CreateTestBoundSessionParams(future.Get()->wrapped_key()))));
}

TEST_F(BoundSessionRegistrationFetcherImplTest, MissingJSONBoundSessionParams) {
  // Response body contains XSSI prefix but JSON of bound session params
  // missing. Expecting early termination and callback to be called with
  // std::nullopt.
  SetUpServerResponse(kXssiPrefix);
  std::unique_ptr<BoundSessionRegistrationFetcher> fetcher = CreateFetcher();
  RegistrationResultFuture future;

  fetcher->Start(future.GetCallback());
  RunBackgroundTasks();
  EXPECT_TRUE(WasRequestSent());
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get<>(), std::nullopt);

  ExpectRecordedMetrics(RegistrationError::kParseJsonFailed);
}

TEST_F(BoundSessionRegistrationFetcherImplTest,
       MissingSessionIdBoundSessionParams) {
  SetUpServerResponse(kBoundSessionParamsMissingSessionIdJson);
  std::unique_ptr<BoundSessionRegistrationFetcher> fetcher = CreateFetcher();
  RegistrationResultFuture future;

  fetcher->Start(future.GetCallback());
  RunBackgroundTasks();

  EXPECT_EQ(future.Get<>(), std::nullopt);
  ExpectRecordedMetrics(RegistrationError::kRequiredFieldMissing);
}

TEST_F(BoundSessionRegistrationFetcherImplTest,
       MissingCredentialsBoundSessionParams) {
  SetUpServerResponse(R"({"session_identifier": "007"})");
  std::unique_ptr<BoundSessionRegistrationFetcher> fetcher = CreateFetcher();
  RegistrationResultFuture future;

  fetcher->Start(future.GetCallback());
  RunBackgroundTasks();

  EXPECT_EQ(future.Get<>(), std::nullopt);
  ExpectRecordedMetrics(RegistrationError::kRequiredFieldMissing);
}

TEST_F(BoundSessionRegistrationFetcherImplTest,
       CredentialsInvalidBoundSessionParams) {
  // Missing name
  SetUpServerResponse(R"(
    {
        "session_identifier": "007",
        "credentials": [
            {
                "type": "cookie",
                "scope": {
                    "domain": ".google.com",
                    "path": "/"
                }
            }
        ],
        "refresh_url": "/rotate"
    }
  )");
  std::unique_ptr<BoundSessionRegistrationFetcher> fetcher = CreateFetcher();
  RegistrationResultFuture future;

  fetcher->Start(future.GetCallback());
  RunBackgroundTasks();

  EXPECT_EQ(future.Get<>(), std::nullopt);
  ExpectRecordedMetrics(RegistrationError::kRequiredCredentialFieldMissing);
}

TEST_F(BoundSessionRegistrationFetcherImplTest, NonOkHttpResponseCode) {
  SetUpServerResponse(/*body=*/std::string(), net::HTTP_UNAUTHORIZED);
  std::unique_ptr<BoundSessionRegistrationFetcher> fetcher = CreateFetcher();
  RegistrationResultFuture future;

  fetcher->Start(future.GetCallback());
  RunBackgroundTasks();

  EXPECT_EQ(future.Get<>(), std::nullopt);
  ExpectRecordedMetrics(RegistrationError::kServerError);
}

TEST_F(BoundSessionRegistrationFetcherImplTest, NetworkError) {
  SetUpServerResponse(net::ERR_CONNECTION_TIMED_OUT);
  std::unique_ptr<BoundSessionRegistrationFetcher> fetcher = CreateFetcher();
  RegistrationResultFuture future;

  fetcher->Start(future.GetCallback());
  RunBackgroundTasks();

  EXPECT_EQ(future.Get<>(), std::nullopt);
  ExpectRecordedMetrics(RegistrationError::kNetworkError);
}

TEST_F(BoundSessionRegistrationFetcherImplTest, NoKeyProvider) {
  DisableKeyProvider();
  std::unique_ptr<BoundSessionRegistrationFetcher> fetcher = CreateFetcher();
  RegistrationResultFuture future;

  fetcher->Start(future.GetCallback());
  RunBackgroundTasks();

  EXPECT_FALSE(WasRequestSent());
  EXPECT_EQ(future.Get<>(), std::nullopt);
  ExpectRecordedMetrics(RegistrationError::kGenerateRegistrationTokenFailed);
}

TEST_F(BoundSessionRegistrationFetcherImplTest, ParseCredentials) {
  std::unique_ptr<BoundSessionRegistrationFetcherImpl> fetcher =
      CreateFetcher();
  base::Value::List credentials_list;
  credentials_list.Append(
      CreateBoundSessionCredentialDict("auth_cookie_1P", ".google.com", "/"));
  credentials_list.Append(
      CreateBoundSessionCredentialDict("auth_cookie_3P", ".google.com", "/"));
  auto result = fetcher->ParseCredentials(credentials_list);

  std::vector<bound_session_credentials::Credential> expected_credentials;
  expected_credentials.push_back(
      CreateTestBoundSessionCredential("auth_cookie_1P", ".google.com", "/"));
  expected_credentials.push_back(
      CreateTestBoundSessionCredential("auth_cookie_3P", ".google.com", "/"));
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(), ::testing::UnorderedPointwise(
                                  TupleEqualsProto(), expected_credentials));
}

TEST_F(BoundSessionRegistrationFetcherImplTest, ParseCredentialsError) {
  std::unique_ptr<BoundSessionRegistrationFetcherImpl> fetcher =
      CreateFetcher();
  base::Value::List credentials_list;
  credentials_list.Append(
      CreateBoundSessionCredentialDict("auth_cookie_1P", "google.com", "/"));

  // Missing cookie name.
  credentials_list.Append(base::Value::Dict()
                              .Set("type", "cookie")
                              .Set("scope", base::Value::Dict()
                                                .Set("domain", "google.com")
                                                .Set("path", "/")));
  EXPECT_EQ(fetcher->ParseCredentials(credentials_list)
                .error_or(RegistrationError::kNone),
            RegistrationError::kRequiredCredentialFieldMissing);

  credentials_list.erase(credentials_list.end() - 1);
  ASSERT_TRUE(fetcher->ParseCredentials(credentials_list).has_value());

  // Missing domain.
  credentials_list.Append(
      base::Value::Dict()
          .Set("type", "cookie")
          .Set("name", "auth_cookie_3P")
          .Set("scope", base::Value::Dict().Set("path", "/")));
  EXPECT_EQ(fetcher->ParseCredentials(credentials_list)
                .error_or(RegistrationError::kNone),
            RegistrationError::kRequiredCredentialFieldMissing);

  credentials_list.erase(credentials_list.end() - 1);
  ASSERT_TRUE(fetcher->ParseCredentials(credentials_list).has_value());

  // Missing path.
  credentials_list.Append(
      base::Value::Dict()
          .Set("type", "cookie")
          .Set("name", "auth_cookie_3P")
          .Set("scope", base::Value::Dict().Set("domain", "google.com")));
  EXPECT_EQ(fetcher->ParseCredentials(credentials_list)
                .error_or(RegistrationError::kNone),
            RegistrationError::kRequiredCredentialFieldMissing);
}

TEST_F(BoundSessionRegistrationFetcherImplTest,
       ParseCredentialsSkipsExtraFields) {
  std::unique_ptr<BoundSessionRegistrationFetcherImpl> fetcher =
      CreateFetcher();
  base::Value::List credentials_list;
  credentials_list.Append(
      CreateBoundSessionCredentialDict("auth_cookie_1P", ".google.com", "/"));
  credentials_list.Append(
      CreateBoundSessionCredentialDict("auth_cookie_3P", ".google.com", "/"));
  credentials_list.Append("optional not dict item");
  auto result = fetcher->ParseCredentials(credentials_list);

  EXPECT_TRUE(result.has_value());
}

TEST_F(BoundSessionRegistrationFetcherImplTest, ParseJsonAbsoluteRefreshUrl) {
  SetUpServerResponse(R"(
    {
        "session_identifier": "007",
        "credentials": [{
                "type": "cookie",
                "name": "auth_cookie_1P",
                "scope": {
                    "domain": ".google.com",
                    "path": "/"
                }
            }],
        "refresh_url": "https://accounts.google.com/RotateCookies"
    }
  )");
  std::unique_ptr<BoundSessionRegistrationFetcher> fetcher = CreateFetcher();
  RegistrationResultFuture future;

  fetcher->Start(future.GetCallback());
  RunBackgroundTasks();

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->refresh_url(),
            GURL("https://accounts.google.com/RotateCookies"));
  ExpectRecordedMetrics(RegistrationError::kNone);
}

TEST_F(BoundSessionRegistrationFetcherImplTest, ParseJsonAbsentRefreshUrl) {
  SetUpServerResponse(R"(
    {
        "session_identifier": "007",
        "credentials": [{
                "type": "cookie",
                "name": "auth_cookie_1P",
                "scope": {
                    "domain": ".google.com",
                    "path": "/"
                }
            }]
    }
  )");
  std::unique_ptr<BoundSessionRegistrationFetcher> fetcher = CreateFetcher();
  RegistrationResultFuture future;

  fetcher->Start(future.GetCallback());
  RunBackgroundTasks();

  EXPECT_EQ(future.Get<>(), std::nullopt);
  ExpectRecordedMetrics(RegistrationError::kRequiredFieldMissing);
}

TEST_F(BoundSessionRegistrationFetcherImplTest, ParseJsonInvalidRefreshUrl) {
  SetUpServerResponse(R"(
    {
        "session_identifier": "007",
        "credentials": [{
                "type": "cookie",
                "name": "auth_cookie_1P",
                "scope": {
                    "domain": ".google.com",
                    "path": "/"
                }
            }],
        "refresh_url": "not-a-url://"
    }
  )");
  std::unique_ptr<BoundSessionRegistrationFetcher> fetcher = CreateFetcher();
  RegistrationResultFuture future;

  fetcher->Start(future.GetCallback());
  RunBackgroundTasks();

  EXPECT_EQ(future.Get<>(), std::nullopt);
  ExpectRecordedMetrics(RegistrationError::kInvalidSessionParams);
}
