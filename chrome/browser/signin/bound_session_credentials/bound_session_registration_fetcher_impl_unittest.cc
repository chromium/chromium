// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_impl.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher.h"
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
constexpr char kResponseJsonBody[] = R"(
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

std::vector<crypto::SignatureVerifier::SignatureAlgorithm> CreateAlgArray() {
  return {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
          crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256};
}

bound_session_credentials::RegistrationParams CreateTestRegistrationParams() {
  bound_session_credentials::RegistrationParams params;
  params.set_site("https://google.com");
  params.set_session_id("007");
  return params;
}

MATCHER_P(ParamMatching, expected, "") {
  return arg->site() == expected.site() &&
         arg->session_id() == expected.session_id() &&
         arg->wrapped_key() == expected.wrapped_key();
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

TEST_F(BoundSessionRegistrationFetcherImplTest, NoService) {
  network::TestURLLoaderFactory url_loader_factory;
  bool made_download = false;

  url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        made_download = true;
        ASSERT_FALSE(request.url.is_valid());
        auto response_head = network::mojom::URLResponseHead::New();
        response_head->headers =
            base::MakeRefCounted<net::HttpResponseHeaders>("");
        url_loader_factory.AddResponse(
            GURL(request.url), std::move(response_head),
            std::string(kResponseJsonBody),
            network::URLLoaderCompletionStatus(net::OK));
      }));

  BoundSessionRegistrationFetcherParam params =
      BoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
          GURL("http://accounts.google.com"), CreateAlgArray());
  BoundSessionRegistrationFetcher::Id::Generator
      registration_request_id_generator;
  BoundSessionRegistrationFetcher::Id id =
      registration_request_id_generator.GenerateNextId();
  std::unique_ptr<BoundSessionRegistrationFetcher> fetcher =
      std::make_unique<BoundSessionRegistrationFetcherImpl>(
          std::move(params), url_loader_factory.GetSafeWeakWrapper(), nullptr,
          id);
  base::test::TestFuture<
      BoundSessionRegistrationFetcher::Id,
      absl::optional<bound_session_credentials::RegistrationParams>>
      future;

  fetcher->Start(future.GetCallback());
  ASSERT_FALSE(made_download);
  EXPECT_TRUE(future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(future.IsReady());
  ASSERT_FALSE(made_download);
}

TEST_F(BoundSessionRegistrationFetcherImplTest, ValidInput) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  network::TestURLLoaderFactory url_loader_factory;
  bool made_download = false;

  url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        made_download = true;
        ASSERT_TRUE(request.url.is_valid());
        auto response_head = network::mojom::URLResponseHead::New();
        response_head->headers =
            base::MakeRefCounted<net::HttpResponseHeaders>("");
        url_loader_factory.AddResponse(
            GURL(request.url), std::move(response_head),
            std::string(kResponseJsonBody),
            network::URLLoaderCompletionStatus(net::OK));
      }));

  BoundSessionRegistrationFetcherParam params =
      BoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
          GURL("https://www.google.com/startsession"), CreateAlgArray());
  BoundSessionRegistrationFetcher::Id::Generator
      registration_request_id_generator;
  BoundSessionRegistrationFetcher::Id id =
      registration_request_id_generator.GenerateNextId();
  std::unique_ptr<BoundSessionRegistrationFetcher> fetcher =
      std::make_unique<BoundSessionRegistrationFetcherImpl>(
          std::move(params), url_loader_factory.GetSafeWeakWrapper(),
          &unexportable_key_service(), id);
  base::test::TestFuture<
      BoundSessionRegistrationFetcher::Id,
      absl::optional<bound_session_credentials::RegistrationParams>>
      future;

  fetcher->Start(future.GetCallback());

  ASSERT_FALSE(made_download);
  EXPECT_FALSE(future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get<0>(), id);
  EXPECT_THAT(future.Get<1>(), ParamMatching(CreateTestRegistrationParams()));
  ASSERT_TRUE(made_download);
}
