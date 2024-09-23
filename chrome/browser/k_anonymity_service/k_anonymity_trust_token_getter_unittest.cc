// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/k_anonymity_service/k_anonymity_trust_token_getter.h"

#include <inttypes.h>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_metrics.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_storage.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_urls.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/dns/mock_host_resolver.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAuthServer[] = "https://authserver";

class TestTrustTokenQueryAnswerer
    : public network::mojom::TrustTokenQueryAnswerer {
 public:
  TestTrustTokenQueryAnswerer() = default;
  TestTrustTokenQueryAnswerer(const TestTrustTokenQueryAnswerer&) = delete;
  TestTrustTokenQueryAnswerer& operator=(const TestTrustTokenQueryAnswerer&) =
      delete;

  void SetHasTokens(bool has_tokens) { has_tokens_ = has_tokens; }

  void HasTrustTokens(const ::url::Origin& issuer,
                      HasTrustTokensCallback callback) override {
    std::move(callback).Run(network::mojom::HasTrustTokensResult::New(
        network::mojom::TrustTokenOperationStatus::kOk, has_tokens_));
  }
  void HasRedemptionRecord(const ::url::Origin& issuer,
                           HasRedemptionRecordCallback callback) override {
    NOTIMPLEMENTED();
  }

 private:
  bool has_tokens_ = false;
};

}  // namespace

class KAnonymityTrustTokenGetterTest : public testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{network::features::kPrivateStateTokens, {}},
                              {features::kKAnonymityService,
                               {{"KAnonymityServiceAuthServer", kAuthServer}}}},
        /*disabled_features=*/{});
    TestingProfile::Builder builder;
    builder.SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    getter_ = std::make_unique<KAnonymityTrustTokenGetter>(
        IdentityManagerFactory::GetForProfile(profile_.get()),
        profile_->GetURLLoaderFactory(), &trust_token_answerer_, &storage_);
    url::Origin auth_origin = url::Origin::Create(GURL(kAuthServer));
    isolation_info_ = net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther, auth_origin, auth_origin,
        net::SiteForCookies());
  }

  void InitializeIdentity(bool signed_on) {
    auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
    auto* identity_manager = identity_test_env->identity_manager();
    if (signed_on) {
      identity_test_env->MakePrimaryAccountAvailable(
          "user@gmail.com", signin::ConsentLevel::kSignin);
      ASSERT_TRUE(
          identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
      EXPECT_EQ(1U, identity_manager->GetAccountsWithRefreshTokens().size());
    }
  }

  void SimulateResponseForPendingRequest(std::string url, std::string content) {
    constexpr network::TestURLLoaderFactory::ResponseMatchFlags flags =
        static_cast<network::TestURLLoaderFactory::ResponseMatchFlags>(
            network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix |
            network::TestURLLoaderFactory::ResponseMatchFlags::kWaitForRequest);
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        url, content, net::HTTP_OK, flags));
  }

  void SimulateFailedResponseForPendingRequest(std::string url) {
    constexpr network::TestURLLoaderFactory::ResponseMatchFlags flags =
        static_cast<network::TestURLLoaderFactory::ResponseMatchFlags>(
            network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix |
            network::TestURLLoaderFactory::ResponseMatchFlags::kWaitForRequest);
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        url, "", net::HTTP_NOT_FOUND, flags));
  }

  void SimulateFailedResponseForAuthToken() {
    identity_test_env_adaptor_->identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
            GoogleServiceAuthError::FromServiceUnavailable("foo"));
  }

  void RespondWithOAuthToken(base::Time expiration) {
    identity_test_env_adaptor_->identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken("token",
                                                                  expiration);
  }

  // Wait for the TestURLLoaderFactory to have a pending request, returning a
  // pointer to it (but leaving the request in the factory).
  const network::TestURLLoaderFactory::PendingRequest* WaitForPendingRequest() {
    while (true) {
      const auto* pending_request =
          test_url_loader_factory_.GetPendingRequest(0);
      if (pending_request) {
        return pending_request;
      }
      task_environment_.RunUntilIdle();
    }
  }

  void RespondWithTrustTokenNonUniqueUserId(int id) {
    std::string request_url =
        base::StrCat({kAuthServer, "/v1/generateShortIdentifier"});

    const auto* pending_request = WaitForPendingRequest();
    const auto& request = pending_request->request;
    EXPECT_EQ(request_url, request.url);
    EXPECT_TRUE(
        request.headers.HasHeader(net::HttpRequestHeaders::kAuthorization));
    EXPECT_EQ(net::HttpRequestHeaders::kGetMethod, request.method);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit, request.credentials_mode);
    ASSERT_TRUE(request.trusted_params);
    EXPECT_TRUE(isolation_info_.IsEqualForTesting(
        request.trusted_params->isolation_info));

    SimulateResponseForPendingRequest(
        request_url, base::StringPrintf("{\"shortClientIdentifier\": %d}", id));
  }

  void RespondWithTrustTokenKeys(int id, base::Time expiration) {
    std::string request_url =
        base::StringPrintf("%s/v1/%d/fetchKeys?key=", kAuthServer, id);

    const auto* pending_request = WaitForPendingRequest();
    const auto& request = pending_request->request;
    EXPECT_EQ(0u, request.url.spec().rfind(request_url));
    EXPECT_FALSE(
        request.headers.HasHeader(net::HttpRequestHeaders::kAuthorization));
    EXPECT_EQ(net::HttpRequestHeaders::kGetMethod, request.method);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit, request.credentials_mode);
    ASSERT_TRUE(request.trusted_params);
    EXPECT_TRUE(isolation_info_.IsEqualForTesting(
        request.trusted_params->isolation_info));
    SimulateResponseForPendingRequest(
        request_url,
        base::StringPrintf(
            R"({
          "protocolVersion":"TrustTokenV3VOPRF",
          "id": 1,
          "batchSize": 1,
          "keys": [
            {
              "keyIdentifier": 0,
              "keyMaterial": "InsertKeyHere",
              "expirationTimestampUsec": "%)" PRIu64 R"("
            }
          ]
          })",
            (expiration - base::Time::UnixEpoch()).InMicroseconds()));
  }

  void RespondWithTrustTokenIssued(int id) {
    std::string request_url =
        base::StringPrintf("%s/v1/%d/issueTrustToken", kAuthServer, id);

    const auto* pending_request = WaitForPendingRequest();
    const auto& request = pending_request->request;
    EXPECT_EQ(request_url, request.url);
    EXPECT_TRUE(
        request.headers.HasHeader(net::HttpRequestHeaders::kAuthorization));
    EXPECT_EQ(net::HttpRequestHeaders::kPostMethod, request.method);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit, request.credentials_mode);
    ASSERT_TRUE(request.trusted_params);
    EXPECT_TRUE(isolation_info_.IsEqualForTesting(
        request.trusted_params->isolation_info));
    SimulateResponseForPendingRequest(request_url, "");
  }

  void CheckHistogramActions(
      const base::HistogramTester& hist,
      std::vector<KAnonymityTrustTokenGetterAction> actions) {
    for (auto action : actions) {
      hist.ExpectBucketCount("Chrome.KAnonymityService.TrustTokenGetter.Action",
                             action, 1);
    }
    hist.ExpectTotalCount("Chrome.KAnonymityService.TrustTokenGetter.Action",
                          actions.size());
  }

  void CheckHistogramActions(
      const base::HistogramTester& hist,
      const base::flat_map<KAnonymityTrustTokenGetterAction, size_t> actions) {
    size_t event_count = 0;
    for (auto action : actions) {
      hist.ExpectBucketCount("Chrome.KAnonymityService.TrustTokenGetter.Action",
                             action.first, action.second);
      event_count += action.second;
    }
    hist.ExpectTotalCount("Chrome.KAnonymityService.TrustTokenGetter.Action",
                          event_count);
  }

  KAnonymityTrustTokenGetter* getter() { return getter_.get(); }

  void SetHasTokens(bool has_tokens) {
    trust_token_answerer_.SetHasTokens(has_tokens);
  }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

  bool HasPendingRequest() { return test_url_loader_factory_.NumPending() > 0; }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  net::IsolationInfo isolation_info_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<KAnonymityTrustTokenGetter> getter_;
  TestTrustTokenQueryAnswerer trust_token_answerer_;
  data_decoder::test::InProcessDataDecoder decoder_;
  KAnonymityServiceMemoryStorage storage_;
};

TEST_F(KAnonymityTrustTokenGetterTest, TryGetNotSignedIn) {
  InitializeIdentity(/*signed_on=*/false);
  base::HistogramTester hist;
  base::RunLoop run_loop;
  getter()->TryGetTrustTokenAndKey(
      base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
          base::BindLambdaForTesting(
              [&run_loop](std::optional<KeyAndNonUniqueUserId> result) {
                EXPECT_FALSE(result);
                run_loop.Quit();
              })));
  run_loop.Run();
  hist.ExpectTotalCount("Chrome.KAnonymityService.TrustTokenGetter.Action", 0);
}

TEST_F(KAnonymityTrustTokenGetterTest, TryGetAuthTokenFailed) {
  InitializeIdentity(/*signed_on=*/true);
  base::HistogramTester hist;
  base::RunLoop run_loop;
  getter()->TryGetTrustTokenAndKey(
      base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
          base::BindLambdaForTesting(
              [&run_loop](std::optional<KeyAndNonUniqueUserId> result) {
                EXPECT_FALSE(result);
                run_loop.Quit();
              })));
  SimulateFailedResponseForAuthToken();
  run_loop.Run();
  CheckHistogramActions(
      hist, {KAnonymityTrustTokenGetterAction::kTryGetTrustTokenAndKey,
             KAnonymityTrustTokenGetterAction::kRequestAccessToken,
             KAnonymityTrustTokenGetterAction::kAccessTokenRequestFailed});
}

TEST_F(KAnonymityTrustTokenGetterTest, TryGetNonUniqueUserIdFetchFailed) {
  InitializeIdentity(/*signed_on=*/true);
  base::HistogramTester hist;
  base::RunLoop run_loop;
  getter()->TryGetTrustTokenAndKey(
      base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
          base::BindLambdaForTesting(
              [&run_loop](std::optional<KeyAndNonUniqueUserId> result) {
                EXPECT_FALSE(result);
                run_loop.Quit();
              })));
  RespondWithOAuthToken(base::Time::Max());
  SimulateFailedResponseForPendingRequest(
      "https://authserver/v1/generateShortIdentifier");
  run_loop.Run();
  CheckHistogramActions(
      hist, {KAnonymityTrustTokenGetterAction::kTryGetTrustTokenAndKey,
             KAnonymityTrustTokenGetterAction::kRequestAccessToken,
             KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientID,
             KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientIDFailed});
}

TEST_F(KAnonymityTrustTokenGetterTest,
       TryJoinSetMalformedNonUniqueUserIdResponse) {
  InitializeIdentity(/*signed_on=*/true);
  base::HistogramTester hist;
  std::vector<std::string> bad_responses = {
      "",                              // empty
      "1df3fd33sasdf",                 // base64 nonsense
      "\x00\x11\x22\x33\x44\x55\x66",  // binary nonsense
      "[]",                            // not a dict
      "{}",                            // empty dict
      R"({
        shortClientIdentifier: "not an int",
      })",  // shortClientIdentifier is not an integer
      R"({
        short_client_identifier: 2,
        shortclientidentifier: 2,
        ShortClientIdentifier: 2,
      })",  // wrong keys
      R"({
        shortClientIdentifier: 2147483648
      })",  // too big for int32
      R"({
        shortClientIdentifier: 10.5
      })",  // not an int
      R"({
        shortClientIdentifier: -1
      })",  // negative
  };
  for (const auto& response : bad_responses) {
    base::RunLoop run_loop;
    getter()->TryGetTrustTokenAndKey(
        base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
            base::BindLambdaForTesting(
                [&run_loop,
                 &response](std::optional<KeyAndNonUniqueUserId> result) {
                  EXPECT_FALSE(result) << response;
                  run_loop.Quit();
                })));
    RespondWithOAuthToken(base::Time::Now() + base::Seconds(1));
    SimulateResponseForPendingRequest(
        "https://authserver/v1/"
        "generateShortIdentifier",
        response);
    run_loop.Run();
    task_environment()->FastForwardBy(base::Minutes(1));
  }
  CheckHistogramActions(
      hist,
      {{KAnonymityTrustTokenGetterAction::kTryGetTrustTokenAndKey,
        bad_responses.size()},
       {KAnonymityTrustTokenGetterAction::kRequestAccessToken,
        bad_responses.size()},
       {KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientID,
        bad_responses.size()},
       {KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientIDParseError,
        bad_responses.size()}});
}

TEST_F(KAnonymityTrustTokenGetterTest, TryGetKeyFetchFails) {
  InitializeIdentity(/*signed_on=*/true);
  base::HistogramTester hist;
  base::RunLoop run_loop;
  getter()->TryGetTrustTokenAndKey(
      base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
          base::BindLambdaForTesting(
              [&run_loop](std::optional<KeyAndNonUniqueUserId> result) {
                EXPECT_FALSE(result);
                run_loop.Quit();
              })));
  RespondWithOAuthToken(base::Time::Max());
  RespondWithTrustTokenNonUniqueUserId(2);
  SimulateFailedResponseForPendingRequest("https://authserver/v1/2/fetchKeys");
  run_loop.Run();
  CheckHistogramActions(
      hist, {KAnonymityTrustTokenGetterAction::kTryGetTrustTokenAndKey,
             KAnonymityTrustTokenGetterAction::kRequestAccessToken,
             KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientID,
             KAnonymityTrustTokenGetterAction::kFetchTrustTokenKey,
             KAnonymityTrustTokenGetterAction::kFetchTrustTokenKeyFailed});
}

TEST_F(KAnonymityTrustTokenGetterTest,
       TryJoinSetMalformedKeyCommitmentResponse) {
  InitializeIdentity(/*signed_on=*/true);
  base::HistogramTester hist;
  std::vector<std::string> bad_responses = {
      "",                              // empty
      "1df3fd33sasdf",                 // base64 nonsense
      "\x00\x11\x22\x33\x44\x55\x66",  // binary nonsense
      "[]",                            // not a dict
      "{}",                            // empty dict
      R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": 1,
      "batchSize": 1,
      "keys": "key"
      })",                             // keys not a list
      R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": 1,
      "batchSize": 1,
      "keys": []
      })",                             // keys empty
      R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": 1,
      "batchSize": 1,
      "keys": ["bad key"]
      })",                             // key not a dict
      R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": 1,
      "batchSize": 1,
      "keys": [{}]
      })",                             // key is empty
      R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": 1,
      "batchSize": 1,
      "keys": [{
        "keyIdentifier": "not an integer",
        "keyMaterial": "InsertKeyHere",
        "expirationTimestampUsec": "253402300799000000"
      }]})",                           // key identifier is not an integer
      R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": 1,
      "batchSize": 1,
      "keys": [{
        "keyIdentifier": 0.1,
        "keyMaterial": "InsertKeyHere",
        "expirationTimestampUsec": "253402300799000000"
      }]})",                           // key identifier is not an integer
      R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": 1,
      "batchSize": 1,
      "keys": [{
        "keyIdentifier": 4294967296,
        "keyMaterial": "InsertKeyHere",
        "expirationTimestampUsec": "253402300799000000"
      }]})",                           // key identifier doesn't fit in uint32
      R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": 1,
      "batchSize": 1,
      "keys": [{
        "keyIdentifier": 0,
        "keyMaterial": "InsertKeyHere",
        "expirationTimestampUsec": "future"
      }]})",                           // key expiration is not a number
      R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": "one",
      "batchSize": 1,
      "keys": [{
        "keyIdentifier": 0,
        "keyMaterial": "InsertKeyHere",
        "expirationTimestampUsec": "253402300799000000"
      }]})",                           // id is not an integer
      R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": 10.5,
      "batchSize": 1,
      "keys": [{
        "keyIdentifier": 0,
        "keyMaterial": "InsertKeyHere",
        "expirationTimestampUsec": "253402300799000000"
      }]})",                           // id is not an integer
      R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": -10,
      "batchSize": 1,
      "keys": [{
        "keyIdentifier": 0,
        "keyMaterial": "InsertKeyHere",
        "expirationTimestampUsec": "253402300799000000"
      }]})",                           // id is negative
      R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": 2147483648,
      "batchSize": 1,
      "keys": [{
        "keyIdentifier": 0,
        "keyMaterial": "InsertKeyHere",
        "expirationTimestampUsec": "253402300799000000"
      }]})",                           // id doesn't fit in int32
      R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": 1,
      "batchSize": "one",
      "keys": [{
        "keyIdentifier": 0,
        "keyMaterial": "InsertKeyHere",
        "expirationTimestampUsec": "253402300799000000"
      }]})",                           // batchSize is not an integer
      R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": 1,
      "batchSize": 1.5,
      "keys": [{
        "keyIdentifier": 0,
        "keyMaterial": "InsertKeyHere",
        "expirationTimestampUsec": "253402300799000000"
      }]})",                           // batchSize is not an integer
      R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": 1,
      "batchSize": -1,
      "keys": [{
        "keyIdentifier": 0,
        "keyMaterial": "InsertKeyHere",
        "expirationTimestampUsec": "253402300799000000"
      }]})",                           // batchSize is negative
      R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": 1,
      "batchSize": 2147483648,
      "keys": [{
        "keyIdentifier": 0,
        "keyMaterial": "InsertKeyHere",
        "expirationTimestampUsec": "253402300799000000"
      }]})",                           // batchSize doesn't fit in int32
  };

  for (const auto& response : bad_responses) {
    SCOPED_TRACE(response);
    base::RunLoop run_loop;
    getter()->TryGetTrustTokenAndKey(
        base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
            base::BindLambdaForTesting(
                [&run_loop,
                 &response](std::optional<KeyAndNonUniqueUserId> result) {
                  EXPECT_FALSE(result) << response;
                  run_loop.Quit();
                })));
    RespondWithOAuthToken(base::Time::Now() + base::Seconds(1));
    RespondWithTrustTokenNonUniqueUserId(2);
    SimulateResponseForPendingRequest("https://authserver/v1/2/fetchKeys",
                                      response);
    run_loop.Run();
    task_environment()->FastForwardBy(base::Minutes(1));
  }
  CheckHistogramActions(
      hist, {{KAnonymityTrustTokenGetterAction::kTryGetTrustTokenAndKey,
              bad_responses.size()},
             {KAnonymityTrustTokenGetterAction::kRequestAccessToken,
              bad_responses.size()},
             {KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientID,
              bad_responses.size()},
             {KAnonymityTrustTokenGetterAction::kFetchTrustTokenKey,
              bad_responses.size()},
             {KAnonymityTrustTokenGetterAction::kFetchTrustTokenKeyParseError,
              bad_responses.size()}});
}

TEST_F(KAnonymityTrustTokenGetterTest, TryJoinSetValidKeyCommitmentResponse) {
  InitializeIdentity(/*signed_on=*/true);
  base::HistogramTester hist;
  const size_t kNumCases = 3;
  const struct {
    const char* response;
    const char* expected_commitment;
  } kTestCases[kNumCases] = {{
                                 R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": 1,
      "batchSize": 1,
      "keys": [{
        "keyIdentifier": 0,
        "keyMaterial": "InsertKeyHere",
        "expirationTimestampUsec": "253402300799000000"
      }]})",
                                 R"({
      "TrustTokenV3VOPRF": {
        "batchsize": 1,
        "id":1,
        "keys": {
          "0": {
            "Y": "InsertKeyHere",
            "expiry": "253402300799000000"
          }
        },
        "protocol_version": "TrustTokenV3VOPRF"
      }})",
                             },
                             {
                                 R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": 0,
      "batchSize": 1,
      "keys": [{
        "keyIdentifier": -2,
        "keyMaterial": "InsertKeyHere",
        "expirationTimestampUsec": "253402300799000000"
      }]})",
                                 R"({
      "TrustTokenV3VOPRF": {
        "batchsize": 1,
        "id": 0,
        "keys": {
          "4294967294": {
            "Y": "InsertKeyHere",
            "expiry": "253402300799000000"
          }
        },
        "protocol_version": "TrustTokenV3VOPRF"
      }})",
                             },
                             {
                                 R"({
      "protocolVersion":"TrustTokenV3VOPRF",
      "id": 2147483647,
      "batchSize": 1,
      "keys": [{
        "keyIdentifier": 2147483648,
        "keyMaterial": "InsertKeyHere",
        "expirationTimestampUsec": "253402300799000000"
      }]})",
                                 R"({
      "TrustTokenV3VOPRF": {
        "batchsize": 1,
        "id": 2147483647,
        "keys": {
          "2147483648": {
            "Y": "InsertKeyHere",
            "expiry": "253402300799000000"
          }
        },
        "protocol_version":"TrustTokenV3VOPRF"
      }})",
                             }};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.response);
    base::RunLoop run_loop;
    getter()->TryGetTrustTokenAndKey(
        base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
            base::BindLambdaForTesting(
                [&run_loop](std::optional<KeyAndNonUniqueUserId> result) {
                  EXPECT_FALSE(result);
                  run_loop.Quit();
                })));
    RespondWithOAuthToken(base::Time::Now() + base::Seconds(1));
    RespondWithTrustTokenNonUniqueUserId(2);
    SimulateResponseForPendingRequest("https://authserver/v1/2/fetchKeys",
                                      test_case.response);
    SimulateFailedResponseForPendingRequest(
        "https://authserver/v1/2/issueTrustToken");
    run_loop.Run();
    task_environment()->FastForwardBy(base::Minutes(1));

    // Key should have been saved to the database. Verify it was fetched
    // correctly.
    std::optional<KeyAndNonUniqueUserIdWithExpiration> maybe_key_commitment =
        storage_.GetKeyAndNonUniqueUserId();
    ASSERT_TRUE(maybe_key_commitment);
    EXPECT_EQ(2, maybe_key_commitment->key_and_id.non_unique_user_id);
    EXPECT_THAT(
        base::test::ParseJson(maybe_key_commitment->key_and_id.key_commitment),
        base::test::IsJson(test_case.expected_commitment));

    storage_.UpdateKeyAndNonUniqueUserId({});
  }
  CheckHistogramActions(
      hist,
      {{KAnonymityTrustTokenGetterAction::kTryGetTrustTokenAndKey, kNumCases},
       {KAnonymityTrustTokenGetterAction::kRequestAccessToken, kNumCases},
       {KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientID, kNumCases},
       {KAnonymityTrustTokenGetterAction::kFetchTrustTokenKey, kNumCases},
       {KAnonymityTrustTokenGetterAction::kFetchTrustToken, kNumCases},
       {KAnonymityTrustTokenGetterAction::kFetchTrustTokenFailed, kNumCases}});
}

TEST_F(KAnonymityTrustTokenGetterTest, TryGetNoToken) {
  InitializeIdentity(/*signed_on=*/true);
  base::HistogramTester hist;
  base::RunLoop run_loop;
  getter()->TryGetTrustTokenAndKey(
      base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
          base::BindLambdaForTesting(
              [&run_loop](std::optional<KeyAndNonUniqueUserId> result) {
                EXPECT_FALSE(result);
                run_loop.Quit();
              })));
  RespondWithOAuthToken(base::Time::Max());
  RespondWithTrustTokenNonUniqueUserId(2);
  RespondWithTrustTokenKeys(2, base::Time::Now() + base::Days(1));
  SimulateFailedResponseForPendingRequest(
      "https://authserver/v1/2/issueTrustToken");

  run_loop.Run();
  CheckHistogramActions(
      hist, {KAnonymityTrustTokenGetterAction::kTryGetTrustTokenAndKey,
             KAnonymityTrustTokenGetterAction::kRequestAccessToken,
             KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientID,
             KAnonymityTrustTokenGetterAction::kFetchTrustTokenKey,
             KAnonymityTrustTokenGetterAction::kFetchTrustToken,
             KAnonymityTrustTokenGetterAction::kFetchTrustTokenFailed});
}

TEST_F(KAnonymityTrustTokenGetterTest, TryGetSignedIn) {
  InitializeIdentity(/*signed_on=*/true);
  base::HistogramTester hist;
  base::RunLoop run_loop;
  base::Time key_expiration = base::Time::Now() + base::Days(1);
  getter()->TryGetTrustTokenAndKey(
      base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
          base::BindLambdaForTesting(
              [&run_loop,
               key_expiration](std::optional<KeyAndNonUniqueUserId> result) {
                ASSERT_TRUE(result);
                EXPECT_EQ(2, result->non_unique_user_id);
                EXPECT_THAT(base::test::ParseJson(result->key_commitment),
                            base::test::IsJson(base::StringPrintf(
                                R"({
                   "TrustTokenV3VOPRF": {
                     "protocol_version":"TrustTokenV3VOPRF",
                     "batchsize":1,
                     "id":1,
                     "keys": {
                       "0":{
                         "Y":"InsertKeyHere",
                         "expiry":"%)" PRIu64 R"("
                         }}}})",
                                (key_expiration - base::Time::UnixEpoch())
                                    .InMicroseconds())));
                run_loop.Quit();
              })));
  RespondWithOAuthToken(base::Time::Max());
  RespondWithTrustTokenNonUniqueUserId(2);
  RespondWithTrustTokenKeys(2, key_expiration);
  RespondWithTrustTokenIssued(2);
  run_loop.Run();
  CheckHistogramActions(
      hist, {KAnonymityTrustTokenGetterAction::kTryGetTrustTokenAndKey,
             KAnonymityTrustTokenGetterAction::kRequestAccessToken,
             KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientID,
             KAnonymityTrustTokenGetterAction::kFetchTrustTokenKey,
             KAnonymityTrustTokenGetterAction::kFetchTrustToken,
             KAnonymityTrustTokenGetterAction::kGetTrustTokenSuccess});
}

TEST_F(KAnonymityTrustTokenGetterTest, TryGetRepeatedly) {
  InitializeIdentity(/*signed_on=*/true);
  base::HistogramTester hist;
  base::RunLoop run_loop;
  int callback_count = 0;
  for (int i = 0; i < 10; i++) {
    getter()->TryGetTrustTokenAndKey(
        base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
            base::BindLambdaForTesting(
                [&callback_count, &run_loop,
                 i](std::optional<KeyAndNonUniqueUserId> result) {
                  EXPECT_TRUE(result) << "iteration " << i;
                  callback_count++;
                  if (callback_count == 10)
                    run_loop.Quit();
                })));
  }
  RespondWithOAuthToken(base::Time::Max());
  RespondWithTrustTokenNonUniqueUserId(2);
  RespondWithTrustTokenKeys(2, base::Time::Now() + base::Days(1));
  RespondWithTrustTokenIssued(2);
  run_loop.RunUntilIdle();
  // First one got a token. Now the next is waiting to get a token.
  EXPECT_EQ(1, callback_count);
  SetHasTokens(true);  // Let the rest try the 'already have a token' path.
  RespondWithTrustTokenIssued(
      2);  // Give a token to the second request to unblock it.
  run_loop.Run();
  EXPECT_EQ(10, callback_count);
  CheckHistogramActions(
      hist, {{KAnonymityTrustTokenGetterAction::kTryGetTrustTokenAndKey, 10},
             {KAnonymityTrustTokenGetterAction::kRequestAccessToken, 1},
             {KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientID, 1},
             {KAnonymityTrustTokenGetterAction::kFetchTrustTokenKey, 1},
             {KAnonymityTrustTokenGetterAction::kFetchTrustToken, 2},
             {KAnonymityTrustTokenGetterAction::kGetTrustTokenSuccess, 10}});
}

TEST_F(KAnonymityTrustTokenGetterTest, TryGetFailureDropsAllRequests) {
  InitializeIdentity(/*signed_on=*/true);
  base::HistogramTester hist;
  base::RunLoop run_loop;
  int callback_count = 0;
  for (int i = 0; i < 10; i++) {
    getter()->TryGetTrustTokenAndKey(
        base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
            base::BindLambdaForTesting(
                [&callback_count, &run_loop,
                 i](std::optional<KeyAndNonUniqueUserId> result) {
                  EXPECT_FALSE(result) << "iteration " << i;
                  callback_count++;
                  if (callback_count == 10)
                    run_loop.Quit();
                })));
  }
  RespondWithOAuthToken(base::Time::Max());
  RespondWithTrustTokenNonUniqueUserId(2);
  RespondWithTrustTokenKeys(2, base::Time::Now() + base::Days(1));
  SimulateFailedResponseForPendingRequest(
      "https://authserver/v1/2/issueTrustToken");
  run_loop.Run();
  EXPECT_EQ(10, callback_count);
  CheckHistogramActions(
      hist, {{KAnonymityTrustTokenGetterAction::kTryGetTrustTokenAndKey, 10},
             {KAnonymityTrustTokenGetterAction::kRequestAccessToken, 1},
             {KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientID, 1},
             {KAnonymityTrustTokenGetterAction::kFetchTrustTokenKey, 1},
             {KAnonymityTrustTokenGetterAction::kFetchTrustToken, 1},
             {KAnonymityTrustTokenGetterAction::kFetchTrustTokenFailed, 1}});
}

TEST_F(KAnonymityTrustTokenGetterTest, TokenKeysDontExpire) {
  InitializeIdentity(/*signed_on=*/true);
  base::HistogramTester hist;
  base::Time expiration = base::Time::Now() + base::Days(1);
  {
    base::RunLoop run_loop;
    getter()->TryGetTrustTokenAndKey(
        base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
            base::BindLambdaForTesting(
                [&run_loop](std::optional<KeyAndNonUniqueUserId> result) {
                  ASSERT_TRUE(result);
                  EXPECT_EQ(10, result->non_unique_user_id);
                  run_loop.Quit();
                })));
    RespondWithOAuthToken(expiration);
    RespondWithTrustTokenNonUniqueUserId(10);
    RespondWithTrustTokenKeys(10, expiration);
    RespondWithTrustTokenIssued(10);
    run_loop.Run();
  }
  // The auth token should not be requested since it didn't expire.
  // The key should not be fetched since it didn't expire.
  task_environment()->FastForwardBy(base::Days(1) - base::Minutes(6));
  {
    base::RunLoop run_loop;
    getter()->TryGetTrustTokenAndKey(
        base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
            base::BindLambdaForTesting(
                [&run_loop](std::optional<KeyAndNonUniqueUserId> result) {
                  ASSERT_TRUE(result);
                  EXPECT_EQ(10, result->non_unique_user_id);
                  run_loop.Quit();
                })));
    RespondWithTrustTokenIssued(10);
    run_loop.Run();
  }
  CheckHistogramActions(
      hist, {{KAnonymityTrustTokenGetterAction::kTryGetTrustTokenAndKey, 2},
             {KAnonymityTrustTokenGetterAction::kRequestAccessToken, 1},
             {KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientID, 1},
             {KAnonymityTrustTokenGetterAction::kFetchTrustTokenKey, 1},
             {KAnonymityTrustTokenGetterAction::kFetchTrustToken, 2},
             {KAnonymityTrustTokenGetterAction::kGetTrustTokenSuccess, 2}});
}

TEST_F(KAnonymityTrustTokenGetterTest, AuthTokenAlreadyExpired) {
  InitializeIdentity(/*signed_on=*/true);
  base::HistogramTester hist;
  base::Time expiration = base::Time::Now() - base::Days(1);
  {
    base::RunLoop run_loop;
    getter()->TryGetTrustTokenAndKey(
        base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
            base::BindLambdaForTesting(
                [&run_loop](std::optional<KeyAndNonUniqueUserId> result) {
                  ASSERT_TRUE(result);
                  run_loop.Quit();
                })));
    RespondWithOAuthToken(expiration);
    RespondWithTrustTokenNonUniqueUserId(2);
    RespondWithTrustTokenKeys(2, base::Time::Max());
    RespondWithTrustTokenIssued(2);
    run_loop.Run();
  }
  task_environment()->RunUntilIdle();
  {
    base::RunLoop run_loop;
    getter()->TryGetTrustTokenAndKey(
        base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
            base::BindLambdaForTesting(
                [&run_loop](std::optional<KeyAndNonUniqueUserId> result) {
                  ASSERT_TRUE(result);
                  EXPECT_EQ(2, result->non_unique_user_id);
                  run_loop.Quit();
                })));
    RespondWithOAuthToken(base::Time::Max());
    RespondWithTrustTokenIssued(2);
    run_loop.Run();
  }
}

TEST_F(KAnonymityTrustTokenGetterTest, AuthTokenExpire) {
  InitializeIdentity(/*signed_on=*/true);
  base::HistogramTester hist;
  base::Time expiration = base::Time::Now() + base::Days(1);
  {
    base::RunLoop run_loop;
    getter()->TryGetTrustTokenAndKey(
        base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
            base::BindLambdaForTesting(
                [&run_loop](std::optional<KeyAndNonUniqueUserId> result) {
                  ASSERT_TRUE(result);
                  EXPECT_EQ(2, result->non_unique_user_id);
                  run_loop.Quit();
                })));
    RespondWithOAuthToken(expiration);
    RespondWithTrustTokenNonUniqueUserId(2);
    RespondWithTrustTokenKeys(2, base::Time::Max());
    RespondWithTrustTokenIssued(2);
    run_loop.Run();
  }
  // The auth token should be requested since it expired.
  task_environment()->FastForwardBy(base::Days(1));
  base::Time new_expiration = base::Time::Now() + base::Days(1);
  {
    base::RunLoop run_loop;
    getter()->TryGetTrustTokenAndKey(
        base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
            base::BindLambdaForTesting(
                [&run_loop](std::optional<KeyAndNonUniqueUserId> result) {
                  ASSERT_TRUE(result);
                  EXPECT_EQ(2, result->non_unique_user_id);
                  run_loop.Quit();
                })));
    RespondWithOAuthToken(new_expiration);
    RespondWithTrustTokenIssued(2);
    run_loop.Run();
  }
  CheckHistogramActions(
      hist, {{KAnonymityTrustTokenGetterAction::kTryGetTrustTokenAndKey, 2},
             {KAnonymityTrustTokenGetterAction::kRequestAccessToken, 2},
             {KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientID, 1},
             {KAnonymityTrustTokenGetterAction::kFetchTrustTokenKey, 1},
             {KAnonymityTrustTokenGetterAction::kFetchTrustToken, 2},
             {KAnonymityTrustTokenGetterAction::kGetTrustTokenSuccess, 2}});
}

TEST_F(KAnonymityTrustTokenGetterTest, TokenKeysExpire) {
  InitializeIdentity(/*signed_on=*/true);
  base::HistogramTester hist;
  base::Time expiration = base::Time::Now() + base::Days(1);
  {
    base::RunLoop run_loop;
    getter()->TryGetTrustTokenAndKey(
        base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
            base::BindLambdaForTesting(
                [&run_loop](std::optional<KeyAndNonUniqueUserId> result) {
                  ASSERT_TRUE(result);
                  EXPECT_EQ(2, result->non_unique_user_id);
                  run_loop.Quit();
                })));
    RespondWithOAuthToken(base::Time::Max());
    RespondWithTrustTokenNonUniqueUserId(2);
    RespondWithTrustTokenKeys(2, expiration);
    RespondWithTrustTokenIssued(2);
    run_loop.Run();
  }
  // The key should be re-fetched after it expires.
  task_environment()->FastForwardBy(base::Days(1));
  base::Time new_expiration = base::Time::Now() + base::Days(1);
  {
    base::RunLoop run_loop;
    getter()->TryGetTrustTokenAndKey(
        base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
            base::BindLambdaForTesting(
                [&run_loop](std::optional<KeyAndNonUniqueUserId> result) {
                  ASSERT_TRUE(result);
                  EXPECT_EQ(3, result->non_unique_user_id);
                  run_loop.Quit();
                })));
    RespondWithTrustTokenNonUniqueUserId(3);
    RespondWithTrustTokenKeys(3, new_expiration);
    RespondWithTrustTokenIssued(3);
    run_loop.Run();
  }
  CheckHistogramActions(
      hist, {{KAnonymityTrustTokenGetterAction::kTryGetTrustTokenAndKey, 2},
             {KAnonymityTrustTokenGetterAction::kRequestAccessToken, 1},
             {KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientID, 2},
             {KAnonymityTrustTokenGetterAction::kFetchTrustTokenKey, 2},
             {KAnonymityTrustTokenGetterAction::kFetchTrustToken, 2},
             {KAnonymityTrustTokenGetterAction::kGetTrustTokenSuccess, 2}});
}

// Verify that the latency recorded includes both the queued time and the
// time for each step of getting the trust token.
TEST_F(KAnonymityTrustTokenGetterTest, RecordTokenLatency) {
  InitializeIdentity(/*signed_on=*/true);
  base::HistogramTester hist;
  getter()->TryGetTrustTokenAndKey(
      base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
          base::BindLambdaForTesting(
              [](std::optional<KeyAndNonUniqueUserId> result) {
                ASSERT_TRUE(result);
                EXPECT_EQ(2, result->non_unique_user_id);
              })));
  task_environment()->FastForwardBy(base::Seconds(1));
  base::RunLoop run_loop;
  getter()->TryGetTrustTokenAndKey(
      base::OnceCallback<void(std::optional<KeyAndNonUniqueUserId>)>(
          base::BindLambdaForTesting(
              [&run_loop](std::optional<KeyAndNonUniqueUserId> result) {
                ASSERT_TRUE(result);
                EXPECT_EQ(2, result->non_unique_user_id);
                run_loop.Quit();
              })));
  RespondWithOAuthToken(base::Time::Max());
  task_environment()->FastForwardBy(base::Seconds(1));
  RespondWithTrustTokenNonUniqueUserId(2);
  task_environment()->FastForwardBy(base::Seconds(1));
  RespondWithTrustTokenKeys(2, base::Time::Max());
  task_environment()->FastForwardBy(base::Seconds(1));
  RespondWithTrustTokenIssued(2);
  RespondWithTrustTokenIssued(2);
  run_loop.Run();

  CheckHistogramActions(
      hist, {{KAnonymityTrustTokenGetterAction::kTryGetTrustTokenAndKey, 2},
             {KAnonymityTrustTokenGetterAction::kRequestAccessToken, 1},
             {KAnonymityTrustTokenGetterAction::kFetchNonUniqueClientID, 1},
             {KAnonymityTrustTokenGetterAction::kFetchTrustTokenKey, 1},
             {KAnonymityTrustTokenGetterAction::kFetchTrustToken, 2},
             {KAnonymityTrustTokenGetterAction::kGetTrustTokenSuccess, 2}});

  hist.ExpectTimeBucketCount(
      "Chrome.KAnonymityService.TrustTokenGetter.Latency", base::Seconds(4), 1);
  hist.ExpectTimeBucketCount(
      "Chrome.KAnonymityService.TrustTokenGetter.Latency", base::Seconds(3), 1);
  hist.ExpectTotalCount("Chrome.KAnonymityService.TrustTokenGetter.Latency", 2);
}

// Apparently the IdentityManager is sometimes NULL, so we should handle this.
TEST_F(KAnonymityTrustTokenGetterTest, HandlesMissingServices) {
  KAnonymityTrustTokenGetter getter(nullptr, nullptr, nullptr, nullptr);
  getter.TryGetTrustTokenAndKey(base::BindLambdaForTesting(
      [](std::optional<KeyAndNonUniqueUserId> result) {
        EXPECT_FALSE(result);
      }));
}
