// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_verification_tokens/private_verification_tokens_url_loader_throttle.h"

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/private_verification_tokens/common/athm_ffi/athm_ffi.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"
#include "components/private_verification_tokens/common/private_verification_tokens_test_util.h"
#include "components/private_verification_tokens/common/private_verification_tokens_token.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crubit/support/rs_std/slice_ref.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using ::private_verification_tokens::test::CreateTestIssuer;
using ::private_verification_tokens::test::FutureExpiration;
using ::private_verification_tokens::test::GetFutureExpiration;

class PrivateVerificationTokensURLLoaderThrottleTest : public testing::Test {
 public:
  PrivateVerificationTokensURLLoaderThrottleTest() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kEnablePrivateVerificationTokens);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    HostContentSettingsMapFactory::GetForProfile(&profile_)
        ->SetDefaultContentSetting(ContentSettingsType::ANTI_ABUSE,
                                   CONTENT_SETTING_ALLOW);
    service_ = PrivateVerificationTokensService::Create(
        temp_dir_.GetPath(),
        HostContentSettingsMapFactory::GetForProfile(&profile_));
    ASSERT_TRUE(service_);
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
  }

  void TearDown() override {
    if (service_) {
      service_->Shutdown();
    }
  }

  PrivateVerificationTokensService* service() { return service_.get(); }
  void DestroyService() { service_.reset(); }
  TestingProfile* profile() { return &profile_; }
  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory() {
    return shared_url_loader_factory_;
  }

  std::vector<private_verification_tokens::PrivateVerificationTokensToken>
  CreateTestTokens() const {
    std::vector<private_verification_tokens::PrivateVerificationTokensToken>
        tokens;
    const auto expiration = base::Time::Now() + base::Hours(2);
    tokens.emplace_back(url::Origin::Create(GURL("https://a.com")),
                        std::vector<uint8_t>{1, 2, 3}, 1, expiration, 1);
    tokens.emplace_back(url::Origin::Create(GURL("https://b.org")),
                        std::vector<uint8_t>{4, 5, 6, 7}, 2, expiration, 1);
    return tokens;
  }

  void StoreTestTokens(PrivateVerificationTokensService* target_service) {
    base::test::TestFuture<void> store_future;
    target_service->StoreTokens(CreateTestTokens(), store_future.GetCallback());
    EXPECT_TRUE(store_future.Wait());
  }

  void WaitForInitialization(PrivateVerificationTokensService* target_service) {
    if (target_service->is_initialized()) {
      return;
    }
    base::test::TestFuture<void> init_future;
    class Waiter : public PrivateVerificationTokensService::Observer {
     public:
      explicit Waiter(base::OnceClosure callback)
          : callback_(std::move(callback)) {}
      void OnInitializationComplete() override { std::move(callback_).Run(); }

     private:
      base::OnceClosure callback_;
    };
    Waiter waiter(init_future.GetCallback());
    base::ScopedObservation<PrivateVerificationTokensService,
                            PrivateVerificationTokensService::Observer>
        observation(&waiter);
    observation.Observe(target_service);
    EXPECT_TRUE(init_future.Wait());
  }

  void SetTestIssuerConfig(PrivateVerificationTokensService* target_service) {
    const GURL issuer_request_url_a("https://a.com/pvt/issue");
    const url::Origin redeemer_a =
        url::Origin::Create(GURL("https://r1.a.com"));
    const GURL issuer_request_url_b("https://b.org/issue/p/i");
    const url::Origin redeemer_b =
        url::Origin::Create(GURL("https://r2.b.org"));
    const GURL issuer_request_url_c("https://c.net/p/v/t/issue");
    const url::Origin redeemer_c = url::Origin::Create(GURL("https://c.net"));
    const std::string encoded_public_key =
        base::Base64Encode(test_issuer_->public_key_bytes());
    const std::string encoded_public_key_proof =
        base::Base64Encode(test_issuer_->public_key_proof_bytes());
    const FutureExpiration future_expiration = GetFutureExpiration();
    const std::string expiration_str = future_expiration.string_rep;
    const std::string json_str = base::StringPrintf(
        R"({
      "issuers": [
        {
          "issuerRequestUrl": "%s",
          "version": 1,
          "publicKey": "%s",
          "publicKeyProof": "%s",
          "batchSize": 2,
          "expiration": "%s",
          "redeemers": [
            "%s"
          ],
          "deploymentId": "dummy-deployment-id"
        },
        {
          "issuerRequestUrl": "%s",
          "version": 1,
          "publicKey": "%s",
          "publicKeyProof": "%s",
          "batchSize": 2,
          "expiration": "%s",
          "redeemers": [
            "%s"
          ],
          "deploymentId": "dummy-deployment-id"
        },
        {
          "issuerRequestUrl": "%s",
          "version": 1,
          "publicKey": "%s",
          "publicKeyProof": "%s",
          "batchSize": 2,
          "expiration": "%s",
          "redeemers": [
            "%s"
          ],
          "deploymentId": "dummy-deployment-id"
        }
      ]
    })",
        issuer_request_url_a.spec(), encoded_public_key,
        encoded_public_key_proof, expiration_str, redeemer_a.Serialize(),
        issuer_request_url_b.spec(), encoded_public_key,
        encoded_public_key_proof, expiration_str, redeemer_b.Serialize(),
        issuer_request_url_c.spec(), encoded_public_key,
        encoded_public_key_proof, expiration_str, redeemer_c.Serialize());

    auto config =
        private_verification_tokens::PrivateVerificationTokensIssuerConfig::
            Create(base::test::ParseJsonDict(json_str));
    ASSERT_TRUE(config);
    target_service->SetIssuerConfig(config);
  }

  network::ResourceRequest CreateTopLevelNavigationRequest(const GURL& url) {
    network::ResourceRequest request;
    request.url = url;
    request.is_outermost_main_frame = true;
    request.credentials_mode = network::mojom::CredentialsMode::kInclude;
    request.request_initiator = std::nullopt;
    request.trusted_params = network::ResourceRequest::TrustedParams();
    request.trusted_params->isolation_info =
        net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url));
    return request;
  }

  void WaitForTasksToComplete() {
    // Deleting a token is a two-round-trip asynchronous operation between the
    // main thread and the ThreadPool database sequence:
    // 1. ThreadPool deletes the token from the database (DeleteToken).
    // 2. Main thread executes OnTokenDeleted, which posts a DB read to refresh
    //    the in-memory cache (GetTokensFromEach).
    // 3. ThreadPool executes GetTokensFromEach.
    // 4. Main thread executes CacheTokens and updates the in-memory cache.
    // Both round-trips must be flushed so tests observe the updated cache.
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::test::TestFuture<void> future1;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, future1.GetCallback());
    EXPECT_TRUE(future1.Wait());
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::test::TestFuture<void> future2;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, future2.GetCallback());
    EXPECT_TRUE(future2.Wait());
  }

 private:
  // All origins use the same test issuer.
  std::optional<private_verification_tokens::PrivacyPassAthmIssuer>
      test_issuer_ = CreateTestIssuer(/*num_buckets=*/2, "dummy-deployment-id");
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  TestingProfile profile_;
  std::unique_ptr<PrivateVerificationTokensService> service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

TEST_F(PrivateVerificationTokensURLLoaderThrottleTest,
       Create_NullService_ReturnsNull) {
  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      /*pvt_service=*/nullptr, /*is_off_the_record=*/false,
      shared_url_loader_factory());
  EXPECT_EQ(throttle, nullptr);
}

TEST_F(PrivateVerificationTokensURLLoaderThrottleTest,
       Create_ValidService_ReturnsThrottle) {
  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  EXPECT_NE(throttle, nullptr);
}

TEST_F(PrivateVerificationTokensURLLoaderThrottleTest,
       WillStartRequest_FeatureDisabled_DoesNothing) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      net::features::kEnablePrivateVerificationTokens);

  WaitForInitialization(service());
  StoreTestTokens(service());
  SetTestIssuerConfig(service());

  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  auto request = CreateTopLevelNavigationRequest(GURL("https://r1.a.com/page"));
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  EXPECT_FALSE(request.headers.HasHeader(
      net::HttpRequestHeaders::kSecPrivateVerificationToken));
}

TEST_F(PrivateVerificationTokensURLLoaderThrottleTest,
       WillStartRequest_ServiceDestroyed_DoesNothing) {
  WaitForInitialization(service());
  StoreTestTokens(service());
  SetTestIssuerConfig(service());

  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  DestroyService();

  auto request = CreateTopLevelNavigationRequest(GURL("https://r1.a.com/page"));
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  EXPECT_FALSE(request.headers.HasHeader(
      net::HttpRequestHeaders::kSecPrivateVerificationToken));
}

TEST_F(PrivateVerificationTokensURLLoaderThrottleTest,
       WillStartRequest_TokenIssuance_Success) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  auto request = CreateTopLevelNavigationRequest(GURL("https://c.net/index"));
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  EXPECT_EQ(test_url_loader_factory().NumPending(), 1);
  EXPECT_EQ(test_url_loader_factory().GetPendingRequest(0)->request.url,
            GURL("https://c.net/p/v/t/issue"));
}

TEST_F(PrivateVerificationTokensURLLoaderThrottleTest,
       WillStartRequest_TokenIssuance_OffTheRecord_Skipped) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/true, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  auto request = CreateTopLevelNavigationRequest(GURL("https://c.net/index"));
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  EXPECT_EQ(test_url_loader_factory().NumPending(), 0);
}

TEST_F(PrivateVerificationTokensURLLoaderThrottleTest,
       WillStartRequest_TokenIssuance_WithInitiator_Skipped) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  auto request = CreateTopLevelNavigationRequest(GURL("https://c.net/index"));
  request.request_initiator = url::Origin::Create(GURL("https://other.com"));
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  EXPECT_EQ(test_url_loader_factory().NumPending(), 0);
}

TEST_F(PrivateVerificationTokensURLLoaderThrottleTest,
       WillStartRequest_TokenRedemption_AttachesTokenHeader) {
  WaitForInitialization(service());
  StoreTestTokens(service());
  SetTestIssuerConfig(service());

  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  auto request = CreateTopLevelNavigationRequest(GURL("https://r1.a.com/page"));
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  auto token_header = request.headers.GetHeader(
      net::HttpRequestHeaders::kSecPrivateVerificationToken);
  ASSERT_TRUE(token_header.has_value());
  EXPECT_EQ(token_header.value(),
            base::Base64Encode(std::vector<uint8_t>{1, 2, 3}));
}

TEST_F(PrivateVerificationTokensURLLoaderThrottleTest,
       WillStartRequest_TokenRedemption_NoTokenAvailable_DoesNotAttachHeader) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  auto request = CreateTopLevelNavigationRequest(GURL("https://r1.a.com/page"));
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  EXPECT_FALSE(request.headers.HasHeader(
      net::HttpRequestHeaders::kSecPrivateVerificationToken));
}

TEST_F(PrivateVerificationTokensURLLoaderThrottleTest,
       WillStartRequest_TokenRedemption_CookiePresent_DoesNotAttachHeader) {
  WaitForInitialization(service());
  StoreTestTokens(service());
  SetTestIssuerConfig(service());

  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  auto request = CreateTopLevelNavigationRequest(GURL("https://r1.a.com/page"));
  request.headers.SetHeader(net::HttpRequestHeaders::kCookie, "sid=123");
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  EXPECT_FALSE(request.headers.HasHeader(
      net::HttpRequestHeaders::kSecPrivateVerificationToken));
}

TEST_F(
    PrivateVerificationTokensURLLoaderThrottleTest,
    WillStartRequest_TokenRedemption_CredentialsModeOmit_DoesNotAttachHeader) {
  WaitForInitialization(service());
  StoreTestTokens(service());
  SetTestIssuerConfig(service());

  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  auto request = CreateTopLevelNavigationRequest(GURL("https://r1.a.com/page"));
  request.credentials_mode = network::mojom::CredentialsMode::kOmit;
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  EXPECT_FALSE(request.headers.HasHeader(
      net::HttpRequestHeaders::kSecPrivateVerificationToken));
}

TEST_F(PrivateVerificationTokensURLLoaderThrottleTest,
       WillStartRequest_TokenRedemption_Subframe_DoesNotAttachHeader) {
  WaitForInitialization(service());
  StoreTestTokens(service());
  SetTestIssuerConfig(service());

  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  auto request = CreateTopLevelNavigationRequest(GURL("https://r1.a.com/page"));
  request.is_outermost_main_frame = false;
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  EXPECT_FALSE(request.headers.HasHeader(
      net::HttpRequestHeaders::kSecPrivateVerificationToken));
}

TEST_F(PrivateVerificationTokensURLLoaderThrottleTest,
       WillStartRequest_TokenRedemption_TopFrameMismatch_DoesNotAttachHeader) {
  WaitForInitialization(service());
  StoreTestTokens(service());
  SetTestIssuerConfig(service());

  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  auto request = CreateTopLevelNavigationRequest(GURL("https://r1.a.com/page"));
  request.trusted_params->isolation_info =
      net::IsolationInfo::CreateForInternalRequest(
          url::Origin::Create(GURL("https://other.com")));
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  EXPECT_FALSE(request.headers.HasHeader(
      net::HttpRequestHeaders::kSecPrivateVerificationToken));
}

TEST_F(
    PrivateVerificationTokensURLLoaderThrottleTest,
    WillStartRequest_TokenRedemption_WithRequestInitiator_DoesNotAttachHeader) {
  WaitForInitialization(service());
  StoreTestTokens(service());
  SetTestIssuerConfig(service());

  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  auto request = CreateTopLevelNavigationRequest(GURL("https://r1.a.com/page"));
  request.request_initiator = url::Origin::Create(GURL("https://other.com"));
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  EXPECT_FALSE(request.headers.HasHeader(
      net::HttpRequestHeaders::kSecPrivateVerificationToken));
}

TEST_F(PrivateVerificationTokensURLLoaderThrottleTest,
       WillRedirectRequest_AddsTokenHeaderToRemovedHeaders) {
  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  net::RedirectInfo redirect_info;
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;

  throttle->WillRedirectRequest(&redirect_info, response_head, &defer,
                                &headers_update_params);

  EXPECT_FALSE(defer);
  EXPECT_THAT(headers_update_params.removed_headers,
              testing::ElementsAre(
                  net::HttpRequestHeaders::kSecPrivateVerificationToken));
}

TEST_F(PrivateVerificationTokensURLLoaderThrottleTest,
       WillProcessResponse_TokenSent_DeletesToken) {
  WaitForInitialization(service());
  StoreTestTokens(service());
  SetTestIssuerConfig(service());

  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  auto request = CreateTopLevelNavigationRequest(GURL("https://r1.a.com/page"));
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);
  ASSERT_TRUE(request.headers.HasHeader(
      net::HttpRequestHeaders::kSecPrivateVerificationToken));

  network::mojom::URLResponseHead response_head;
  response_head.pvt_token_removed_due_to_cookies = false;
  throttle->WillProcessResponse(GURL("https://r1.a.com/page"), &response_head,
                                &defer);

  WaitForTasksToComplete();

  // Verify token for a.com was deleted, only b.org remains.
  base::test::TestFuture<std::vector<url::Origin>> future;
  service()->GetTokenIssuers(future.GetCallback());
  auto issuers = future.Take();
  EXPECT_THAT(issuers,
              testing::ElementsAre(url::Origin::Create(GURL("https://b.org"))));
}

TEST_F(PrivateVerificationTokensURLLoaderThrottleTest,
       WillProcessResponse_TokenRemovedDueToCookies_DoesNotDeleteToken) {
  WaitForInitialization(service());
  StoreTestTokens(service());
  SetTestIssuerConfig(service());

  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  auto request = CreateTopLevelNavigationRequest(GURL("https://r1.a.com/page"));
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);
  ASSERT_TRUE(request.headers.HasHeader(
      net::HttpRequestHeaders::kSecPrivateVerificationToken));

  network::mojom::URLResponseHead response_head;
  response_head.pvt_token_removed_due_to_cookies = true;
  throttle->WillProcessResponse(GURL("https://r1.a.com/page"), &response_head,
                                &defer);

  // Verify token for a.com was NOT deleted.
  base::test::TestFuture<std::vector<url::Origin>> future;
  service()->GetTokenIssuers(future.GetCallback());
  auto issuers = future.Take();
  EXPECT_THAT(issuers, testing::UnorderedElementsAre(
                           url::Origin::Create(GURL("https://a.com")),
                           url::Origin::Create(GURL("https://b.org"))));
}

TEST_F(PrivateVerificationTokensURLLoaderThrottleTest,
       WillRedirectRequest_TokenSent_DeletesToken) {
  WaitForInitialization(service());
  StoreTestTokens(service());
  SetTestIssuerConfig(service());

  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  auto request = CreateTopLevelNavigationRequest(GURL("https://r1.a.com/page"));
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);
  ASSERT_TRUE(request.headers.HasHeader(
      net::HttpRequestHeaders::kSecPrivateVerificationToken));

  net::RedirectInfo redirect_info;
  network::mojom::URLResponseHead redirect_response_head;
  redirect_response_head.pvt_token_removed_due_to_cookies = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;
  throttle->WillRedirectRequest(&redirect_info, redirect_response_head, &defer,
                                &headers_update_params);

  WaitForTasksToComplete();

  // Verify token for a.com was deleted on redirect, only b.org remains.
  base::test::TestFuture<std::vector<url::Origin>> future;
  service()->GetTokenIssuers(future.GetCallback());
  auto issuers = future.Take();
  EXPECT_THAT(issuers,
              testing::ElementsAre(url::Origin::Create(GURL("https://b.org"))));
}

TEST_F(PrivateVerificationTokensURLLoaderThrottleTest,
       WillRedirectRequest_TokenRemovedDueToCookies_DoesNotDeleteToken) {
  WaitForInitialization(service());
  StoreTestTokens(service());
  SetTestIssuerConfig(service());

  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  auto request = CreateTopLevelNavigationRequest(GURL("https://r1.a.com/page"));
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);
  ASSERT_TRUE(request.headers.HasHeader(
      net::HttpRequestHeaders::kSecPrivateVerificationToken));

  net::RedirectInfo redirect_info;
  network::mojom::URLResponseHead redirect_response_head;
  redirect_response_head.pvt_token_removed_due_to_cookies = true;
  network::HttpRequestHeadersUpdateParams headers_update_params;
  throttle->WillRedirectRequest(&redirect_info, redirect_response_head, &defer,
                                &headers_update_params);

  WaitForTasksToComplete();

  // Verify token for a.com was NOT deleted.
  base::test::TestFuture<std::vector<url::Origin>> future;
  service()->GetTokenIssuers(future.GetCallback());
  auto issuers = future.Take();
  EXPECT_THAT(issuers, testing::UnorderedElementsAre(
                           url::Origin::Create(GURL("https://a.com")),
                           url::Origin::Create(GURL("https://b.org"))));
}

TEST_F(
    PrivateVerificationTokensURLLoaderThrottleTest,
    WillRedirectRequest_UpdatesRequestHeadersViaRedirectUtil_PreventsTokenLeakOnRequestRestart) {
  WaitForInitialization(service());
  StoreTestTokens(service());
  SetTestIssuerConfig(service());

  auto throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(throttle);

  // 1. Initial request starts for top-level navigation to a.com.
  auto request = CreateTopLevelNavigationRequest(GURL("https://r1.a.com/page"));
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);
  ASSERT_TRUE(request.headers.HasHeader(
      net::HttpRequestHeaders::kSecPrivateVerificationToken));

  // 2. Redirect occurs. WillRedirectRequest populates removed_headers and
  // deletes token.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://r2.b.org/page");
  redirect_info.new_method = "GET";
  network::mojom::URLResponseHead redirect_response_head;
  redirect_response_head.pvt_token_removed_due_to_cookies = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;
  throttle->WillRedirectRequest(&redirect_info, redirect_response_head, &defer,
                                &headers_update_params);

  EXPECT_THAT(headers_update_params.removed_headers,
              testing::ElementsAre(
                  net::HttpRequestHeaders::kSecPrivateVerificationToken));

  // 3. NavigationURLLoaderImpl updates request.headers via net::RedirectUtil.
  bool should_clear_upload = false;
  net::RedirectUtil::UpdateHttpRequest(request.url, request.method,
                                       redirect_info,
                                       headers_update_params.removed_headers,
                                       headers_update_params.modified_headers,
                                       &request.headers, &should_clear_upload);

  // Verify the header is removed from request.headers.
  EXPECT_FALSE(request.headers.HasHeader(
      net::HttpRequestHeaders::kSecPrivateVerificationToken));

  // 4. Simulate a request restart/re-creation by an extension or interceptor
  // using the updated request and new URL.
  request.url = redirect_info.new_url;
  request.trusted_params->isolation_info =
      net::IsolationInfo::CreateForInternalRequest(
          url::Origin::Create(request.url));

  auto restart_throttle = PrivateVerificationTokensURLLoaderThrottle::Create(
      service(), /*is_off_the_record=*/false, shared_url_loader_factory());
  ASSERT_TRUE(restart_throttle);

  restart_throttle->WillStartRequest(&request, &defer);

  // Token for a.com was spent on the first leg and deleted from storage.
  WaitForTasksToComplete();
  base::test::TestFuture<std::vector<url::Origin>> future;
  service()->GetTokenIssuers(future.GetCallback());
  auto issuers = future.Take();
  EXPECT_FALSE(testing::Value(
      issuers, testing::Contains(url::Origin::Create(GURL("https://a.com")))));
}

}  // namespace
