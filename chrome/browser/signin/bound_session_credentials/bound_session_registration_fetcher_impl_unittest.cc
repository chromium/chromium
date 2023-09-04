// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_impl.h"
#include "base/containers/span.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher.h"
#include "components/signin/public/base/session_binding_test_utils.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using unexportable_keys::ServiceErrorOr;
using unexportable_keys::UnexportableKeyId;
const char kXSSIPrefix[] = ")]}'";
const char kJSONBoundSessionParams[] = R"(
    {
        "session_identifier": "007",
        "credentials": [
            {
                "type": "cookie",
                "name": "auth_cookie",
                "scope": {
                    "domain": "test.me/",
                    "path": "/"
                }
            }
        ]
    }
)";
constexpr char kChallenge[] = "test_challenge";

std::vector<crypto::SignatureVerifier::SignatureAlgorithm> CreateAlgArray() {
  return {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
          crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256};
}

bound_session_credentials::BoundSessionParams CreateTestBoundSessionParams() {
  bound_session_credentials::BoundSessionParams params;
  params.set_site("https://google.com");
  params.set_session_id("007");
  return params;
}

void ConfigureURLLoaderFactoryForRegistrationResponse(
    network::TestURLLoaderFactory* url_loader_factory,
    const std::string response_body,
    bool* out_made_download,
    std::string* request_body = nullptr) {
  url_loader_factory->SetInterceptor(
      base::BindLambdaForTesting([=](const network::ResourceRequest& request) {
        *out_made_download = true;
        ASSERT_TRUE(request.url.is_valid());
        ASSERT_FALSE(request.request_body.get()->elements()->empty());
        if (request_body) {
          *request_body = std::string(request.request_body.get()
                                          ->elements()
                                          ->at(0)
                                          .As<network::DataElementBytes>()
                                          .AsStringPiece());
        }
        auto response_head = network::mojom::URLResponseHead::New();
        response_head->headers =
            base::MakeRefCounted<net::HttpResponseHeaders>("");
        url_loader_factory->AddResponse(
            GURL(request.url), std::move(response_head),
            std::move(response_body),
            network::URLLoaderCompletionStatus(net::OK));
      }));
}

MATCHER_P(ParamMatching, expected, "") {
  return arg->site() == expected.site() &&
         arg->session_id() == expected.session_id();
}

}  // namespace

class BoundSessionRegistrationFetcherImplTest : public testing::Test {
 public:
  BoundSessionRegistrationFetcherImplTest()
      : unexportable_key_service_(task_manager_) {}

  unexportable_keys::UnexportableKeyService& unexportable_key_service() {
    return unexportable_key_service_;
  }

  void RunBackgroundTasks() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::
          QUEUED};  // QUEUED - tasks don't run until `RunUntilIdle()` is
                    // called.
  unexportable_keys::UnexportableKeyTaskManager task_manager_;
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_;
};

TEST_F(BoundSessionRegistrationFetcherImplTest, ValidInput) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  network::TestURLLoaderFactory url_loader_factory;
  bool made_download = false;
  std::string request_body;

  ConfigureURLLoaderFactoryForRegistrationResponse(
      &url_loader_factory,
      std::string(kXSSIPrefix) + std::string(kJSONBoundSessionParams),
      &made_download, &request_body);

  BoundSessionRegistrationFetcherParam params =
      BoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
          GURL("https://www.google.com/startsession"), CreateAlgArray(),
          kChallenge);
  std::unique_ptr<BoundSessionRegistrationFetcher> fetcher =
      std::make_unique<BoundSessionRegistrationFetcherImpl>(
          std::move(params), url_loader_factory.GetSafeWeakWrapper(),
          unexportable_key_service());
  base::test::TestFuture<
      absl::optional<bound_session_credentials::BoundSessionParams>>
      future;

  fetcher->Start(future.GetCallback());

  ASSERT_FALSE(made_download);
  EXPECT_FALSE(future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get<>(), ParamMatching(CreateTestBoundSessionParams()));
  ASSERT_TRUE(made_download);

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
      request_body, *unexportable_key_service().GetAlgorithm(key_id),
      *unexportable_key_service().GetSubjectPublicKeyInfo(key_id)));
}

TEST_F(BoundSessionRegistrationFetcherImplTest, MissingXSSIPrefix) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  network::TestURLLoaderFactory url_loader_factory;
  bool made_download = false;

  ConfigureURLLoaderFactoryForRegistrationResponse(
      &url_loader_factory, std::string(kJSONBoundSessionParams),
      &made_download);

  BoundSessionRegistrationFetcherParam params =
      BoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
          GURL("https://www.google.com/startsession"), CreateAlgArray(),
          kChallenge);
  std::unique_ptr<BoundSessionRegistrationFetcher> fetcher =
      std::make_unique<BoundSessionRegistrationFetcherImpl>(
          std::move(params), url_loader_factory.GetSafeWeakWrapper(),
          unexportable_key_service());
  base::test::TestFuture<
      absl::optional<bound_session_credentials::BoundSessionParams>>
      future;

  ASSERT_FALSE(made_download);
  EXPECT_FALSE(future.IsReady());
  fetcher->Start(future.GetCallback());
  RunBackgroundTasks();
  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get<>(), ParamMatching(CreateTestBoundSessionParams()));
  ASSERT_TRUE(made_download);
}

TEST_F(BoundSessionRegistrationFetcherImplTest, MissingJSONBoundSessionParams) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  network::TestURLLoaderFactory url_loader_factory;
  bool made_download = false;

  // Response body contains XSSI prefix but JSON of bound session params
  // missing. Expecting early termination and callback to be called with
  // absl::nullopt.
  ConfigureURLLoaderFactoryForRegistrationResponse(
      &url_loader_factory, std::string(kXSSIPrefix), &made_download);

  BoundSessionRegistrationFetcherParam params =
      BoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
          GURL("https://www.google.com/startsession"), CreateAlgArray(),
          kChallenge);
  std::unique_ptr<BoundSessionRegistrationFetcher> fetcher =
      std::make_unique<BoundSessionRegistrationFetcherImpl>(
          std::move(params), url_loader_factory.GetSafeWeakWrapper(),
          unexportable_key_service());
  base::test::TestFuture<
      absl::optional<bound_session_credentials::BoundSessionParams>>
      future;

  ASSERT_FALSE(made_download);
  EXPECT_FALSE(future.IsReady());
  fetcher->Start(future.GetCallback());
  RunBackgroundTasks();
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get<>(), absl::nullopt);
  ASSERT_TRUE(made_download);
}
