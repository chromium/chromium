// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/k_anonymity_service/k_anonymity_service_client.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_metrics.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/k_anonymity_service_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class KAnonymityServiceClientTest : public testing::Test {
 public:
  KAnonymityServiceClientTest()
      : task_environment_(std::make_unique<content::BrowserTaskEnvironment>()) {
  }
  explicit KAnonymityServiceClientTest(
      std::unique_ptr<content::BrowserTaskEnvironment> env)
      : task_environment_(std::move(env)) {}

 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(network::features::kPrivateStateTokens);
    TestingProfile::Builder builder;
    builder.SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(
            builder, signin::AccountConsistencyMethod::kMirror);
  }

  void InitializeIdentity(bool signed_on) {
    auto identity_test_env_adaptor =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
    auto* identity_manager = identity_test_env->identity_manager();
    identity_test_env->SetAutomaticIssueOfAccessTokens(true);
    if (signed_on) {
      identity_test_env->MakePrimaryAccountAvailable(
          "user@gmail.com", signin::ConsentLevel::kSignin);
      ASSERT_TRUE(
          identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
      EXPECT_EQ(1U, identity_manager->GetAccountsWithRefreshTokens().size());
    }
  }

  void SimulateResponseForPendingRequest(std::string url, std::string content) {
    task_environment_->RunUntilIdle();
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        url, content, net::HTTP_OK,
        network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix));
  }

  void SimulateFailedResponseForPendingRequest(std::string url) {
    task_environment_->RunUntilIdle();
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        url, "", net::HTTP_NOT_FOUND,
        network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix));
  }

  void RespondWithTrustTokenNonUniqueUserID(int id) {
    SimulateResponseForPendingRequest(
        "https://chromekanonymityauth-pa.googleapis.com/v1/"
        "generateShortIdentifier",
        base::StringPrintf("{\"shortClientIdentifier\": %d}", id));
  }

  void RespondWithTrustTokenKeys(int id) {
    SimulateResponseForPendingRequest(
        base::StringPrintf(
            "https://chromekanonymityauth-pa.googleapis.com/v1/%d/fetchKeys",
            id),
        R"({
          "protocolVersion":"TrustTokenV3VOPRF",
          "id": 1,
          "batchSize": 1,
          "keys": [
            {
              "keyIdentifier": 0,
              "keyMaterial": "InsertKeyHere",
              "expirationTimestampUsec": "253402300799000000"
            }
          ]
          })");
  }

  void RespondWithTrustTokenIssued(int id) {
    SimulateResponseForPendingRequest(
        base::StringPrintf("https://chromekanonymityauth-pa.googleapis.com/v1/"
                           "%d/issueTrustToken",
                           id),
        "");
  }

  template <typename A>
  void CheckHistogramActions(const std::string& uma_key,
                             const base::HistogramTester& hist,
                             const base::flat_map<A, size_t> actions) {
    size_t event_count = 0;
    for (auto action : actions) {
      hist.ExpectBucketCount(uma_key, action.first, action.second);
      event_count += action.second;
    }
    hist.ExpectTotalCount(uma_key, event_count);
  }

  void CheckJoinSetHistogramActions(
      const base::HistogramTester& hist,
      const base::flat_map<KAnonymityServiceJoinSetAction, size_t> actions) {
    CheckHistogramActions("Chrome.KAnonymityService.JoinSet.Action", hist,
                          actions);
  }

  void CheckQuerySetHistogramActions(
      const base::HistogramTester& hist,
      const base::flat_map<KAnonymityServiceQuerySetAction, size_t> actions) {
    CheckHistogramActions("Chrome.KAnonymityService.QuerySet.Action", hist,
                          actions);
  }

  Profile* profile() { return profile_.get(); }

  int GetNumPendingURLRequests() {
    return test_url_loader_factory_.NumPending();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  data_decoder::test::InProcessDataDecoder decoder_;
};

TEST_F(KAnonymityServiceClientTest, TryJoinSetFetchTokenFails) {
  InitializeIdentity(false);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  base::RunLoop run_loop;
  k_service.JoinSet("1",

                    base::BindLambdaForTesting([&run_loop](bool result) {
                      EXPECT_FALSE(result);
                      run_loop.Quit();
                    }));
  run_loop.Run();
  EXPECT_EQ(0, GetNumPendingURLRequests());
  CheckJoinSetHistogramActions(
      hist, {
                {KAnonymityServiceJoinSetAction::kJoinSet, 1},
            });
}

TEST_F(KAnonymityServiceClientTest, TryJoinSetSuccess) {
  InitializeIdentity(true);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  base::RunLoop run_loop;
  k_service.JoinSet("1",

                    base::BindLambdaForTesting([&run_loop](bool result) {
                      EXPECT_TRUE(result);
                      run_loop.Quit();
                    }));
  RespondWithTrustTokenNonUniqueUserID(2);
  RespondWithTrustTokenKeys(2);
  RespondWithTrustTokenIssued(2);
  run_loop.Run();
  EXPECT_EQ(0, GetNumPendingURLRequests());
  CheckJoinSetHistogramActions(
      hist, {{KAnonymityServiceJoinSetAction::kJoinSet, 1},
             {KAnonymityServiceJoinSetAction::kJoinSetSuccess, 1}});
}

TEST_F(KAnonymityServiceClientTest, TryJoinSetRepeatedly) {
  InitializeIdentity(true);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  base::RunLoop run_loop;
  int callback_count = 0;
  for (int i = 0; i < 10; i++) {
    k_service.JoinSet("1", base::BindLambdaForTesting(
                               [&callback_count, &run_loop, i](bool result) {
                                 EXPECT_TRUE(result) << "iteration " << i;
                                 callback_count++;
                                 if (callback_count == 10)
                                   run_loop.Quit();
                               }));
  }
  RespondWithTrustTokenNonUniqueUserID(2);
  RespondWithTrustTokenKeys(2);
  // The network layer doesn't actually get a token, so the fetcher requests one
  // every time.
  for (int i = 0; i < 10; i++)
    RespondWithTrustTokenIssued(2);
  run_loop.Run();
  EXPECT_EQ(10, callback_count);
  CheckJoinSetHistogramActions(
      hist, {{KAnonymityServiceJoinSetAction::kJoinSet, 10},
             {KAnonymityServiceJoinSetAction::kJoinSetSuccess, 10}});
}

TEST_F(KAnonymityServiceClientTest, TryJoinSetOneAtATime) {
  InitializeIdentity(true);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  int callback_count = 0;
  for (int i = 0; i < 10; i++) {
    base::RunLoop run_loop;
    k_service.JoinSet("1", base::BindLambdaForTesting(
                               [&callback_count, &run_loop, i](bool result) {
                                 EXPECT_TRUE(result) << "iteration " << i;
                                 callback_count++;
                                 run_loop.Quit();
                               }));
    if (i == 0) {
      RespondWithTrustTokenNonUniqueUserID(2);
      RespondWithTrustTokenKeys(2);
    }
    // The network layer doesn't actually get a token, so the fetcher requests
    // one every time.
    RespondWithTrustTokenIssued(2);
    run_loop.Run();
  }
  EXPECT_EQ(0, GetNumPendingURLRequests());
  EXPECT_EQ(10, callback_count);
  CheckJoinSetHistogramActions(
      hist, {{KAnonymityServiceJoinSetAction::kJoinSet, 10},
             {KAnonymityServiceJoinSetAction::kJoinSetSuccess, 10}});
}

TEST_F(KAnonymityServiceClientTest, TryJoinSetFailureDropsAllRequests) {
  InitializeIdentity(true);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  base::RunLoop run_loop;
  int callback_count = 0;
  for (int i = 0; i < 10; i++) {
    k_service.JoinSet("1", base::BindLambdaForTesting(
                               [&callback_count, &run_loop, i](bool result) {
                                 EXPECT_FALSE(result) << "iteration " << i;
                                 EXPECT_EQ(i, callback_count);
                                 callback_count++;
                                 if (callback_count == 10)
                                   run_loop.Quit();
                               }));
  }
  SimulateFailedResponseForPendingRequest(
      "https://chromekanonymityauth-pa.googleapis.com/v1/"
      "generateShortIdentifier");
  run_loop.Run();
  EXPECT_EQ(0, GetNumPendingURLRequests());
  EXPECT_EQ(10, callback_count);
  CheckJoinSetHistogramActions(
      hist, {
                {KAnonymityServiceJoinSetAction::kJoinSet, 10},
            });
}

TEST_F(KAnonymityServiceClientTest, TryJoinSetOverflowQueue) {
  InitializeIdentity(true);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  base::RunLoop run_loop;
  int callback_count = 0;
  for (int i = 0; i < 100; i++) {
    k_service.JoinSet("1", base::BindLambdaForTesting(
                               [&callback_count, &run_loop, i](bool result) {
                                 EXPECT_TRUE(result) << "iteration " << i;
                                 EXPECT_EQ(i + 1, callback_count);
                                 callback_count++;
                                 if (callback_count == 101)
                                   run_loop.Quit();
                               }));
  }
  // Queue should be full, so the next request should fail (asynchronously, but
  // before any successes).
  k_service.JoinSet("Full",
                    base::BindLambdaForTesting([&callback_count](bool result) {
                      EXPECT_FALSE(result);
                      EXPECT_EQ(0, callback_count);
                      callback_count++;
                    }));
  RespondWithTrustTokenNonUniqueUserID(2);
  RespondWithTrustTokenKeys(2);
  // The network layer doesn't actually get a token, so the fetcher requests one
  // every time.
  for (int i = 0; i < 100; i++)
    RespondWithTrustTokenIssued(2);
  run_loop.Run();
  EXPECT_EQ(0, GetNumPendingURLRequests());
  EXPECT_EQ(101, callback_count);
  CheckJoinSetHistogramActions(
      hist, {{KAnonymityServiceJoinSetAction::kJoinSet, 101},
             {KAnonymityServiceJoinSetAction::kJoinSetSuccess, 100},
             {KAnonymityServiceJoinSetAction::kJoinSetQueueFull, 1}});
}

TEST_F(KAnonymityServiceClientTest, TryQuerySetAllNotKAnon) {
  KAnonymityServiceClient k_service(profile());

  base::HistogramTester hist;
  base::RunLoop run_loop;
  std::vector<std::string> sets;
  sets.push_back("1");
  sets.push_back("2");
  k_service.QuerySets(
      std::move(sets),
      base::BindLambdaForTesting([&run_loop](std::vector<bool> result) {
        run_loop.Quit();
        ASSERT_EQ(2u, result.size());
        EXPECT_FALSE(result[0]);
        EXPECT_FALSE(result[1]);
      }));
  run_loop.Run();
  EXPECT_EQ(0, GetNumPendingURLRequests());
  CheckQuerySetHistogramActions(
      hist, {{KAnonymityServiceQuerySetAction::kQuerySet, 1},
             {KAnonymityServiceQuerySetAction::kQuerySetsSuccess, 1}});
  hist.ExpectUniqueSample("Chrome.KAnonymityService.QuerySet.Size", 2, 1);
}

class TestTrustTokenQueryAnswerer
    : public network::mojom::TrustTokenQueryAnswerer {
 public:
  TestTrustTokenQueryAnswerer() = default;
  TestTrustTokenQueryAnswerer(const TestTrustTokenQueryAnswerer&) = delete;
  TestTrustTokenQueryAnswerer& operator=(const TestTrustTokenQueryAnswerer&) =
      delete;

  void HasTrustTokens(const ::url::Origin& issuer,
                      HasTrustTokensCallback callback) override {
    // We can always return false here since KAnonymityTrustTokenGetter doesn't
    // check after it successfully gets tokens.
    std::move(callback).Run(network::mojom::HasTrustTokensResult::New(
        network::mojom::TrustTokenOperationStatus::kOk, false));
  }
  void HasRedemptionRecord(const ::url::Origin& issuer,
                           HasRedemptionRecordCallback callback) override {
    NOTIMPLEMENTED();
  }
};

const char kJoinRelayURL[] = "https://relay.test/join_relay";
const char kQueryRelayURL[] = "https://relay.test/query_relay";

class OhttpTestNetworkContext : public network::TestNetworkContext {
 public:
  void GetTrustTokenQueryAnswerer(
      mojo::PendingReceiver<network::mojom::TrustTokenQueryAnswerer> receiver,
      const url::Origin& top_frame_origin) override {
    mojo::MakeSelfOwnedReceiver<network::mojom::TrustTokenQueryAnswerer>(
        std::make_unique<TestTrustTokenQueryAnswerer>(), std::move(receiver));
  }

  void GetViaObliviousHttp(
      network::mojom::ObliviousHttpRequestPtr request,
      mojo::PendingRemote<network::mojom::ObliviousHttpClient> client)
      override {
    EXPECT_FALSE(remote_.is_bound());
    remote_.reset();
    if (drop_requests_)
      return;

    remote_.Bind(std::move(client));
    EXPECT_EQ("binaryKeyInBase64", request->key_config);
    EXPECT_EQ("application/json", request->request_body->content_type);
    if (request->trust_token_params) {
      EXPECT_EQ(kJoinRelayURL, request->relay_url);
      EXPECT_TRUE(base::StartsWith(
          request->resource_url.spec(),
          "https://chromekanonymity-pa.googleapis.com/v1/types/"
          "fledge/sets/"));
    } else {
      EXPECT_EQ(kQueryRelayURL, request->relay_url);
      EXPECT_TRUE(base::StartsWith(
          request->resource_url.spec(),
          "https://chromekanonymityquery-pa.googleapis.com/v1:query"));
    }
    pending_request_ = std::move(request);
  }

  void RespondToPendingRequest(std::string url_prefix, std::string body) {
    if (!pending_request_) {
      ADD_FAILURE() << "No request pending";
      return;
    }
    if (!base::StartsWith(pending_request_->relay_url.spec(), url_prefix)) {
      ADD_FAILURE() << "Pending URL " << pending_request_->relay_url
                    << " did not match " << url_prefix;
      return;
    }
    if (error_) {
      remote_->OnCompleted(absl::nullopt, error_.value());
      error_.reset();
    } else {
      remote_->OnCompleted(body, net::OK);
    }
    remote_.reset();
    pending_request_.reset();
  }

  void SetDropRequests(bool drop) { drop_requests_ = drop; }
  void SetErrorOnce(net::Error err) { error_ = err; }

 private:
  absl::optional<net::Error> error_;
  bool drop_requests_ = false;
  network::mojom::ObliviousHttpRequestPtr pending_request_;
  mojo::Remote<network::mojom::ObliviousHttpClient> remote_;
};

class KAnonymityServiceClientJoinQueryTest
    : public KAnonymityServiceClientTest {
 public:
  KAnonymityServiceClientJoinQueryTest()
      : KAnonymityServiceClientTest(
            std::make_unique<content::BrowserTaskEnvironment>(
                content::BrowserTaskEnvironment::IO_MAINLOOP)),
        network_context_receiver_(&network_context_) {}

 protected:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{network::features::kPrivateStateTokens, {}},
         {features::kKAnonymityService,
          {
              {"KAnonymityServiceJoinRelayServer", kJoinRelayURL},
              {"KAnonymityServiceQueryRelayServer", kQueryRelayURL},
          }},
         {features::kKAnonymityServiceOHTTPRequests, {}}},
        {});
    TestingProfile::Builder builder;
    builder.SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(
            builder, signin::AccountConsistencyMethod::kMirror);
    profile_->GetDefaultStoragePartition()->SetNetworkContextForTesting(
        network_context_receiver_.BindNewPipeAndPassRemote());
  }

  void RespondWithJoinKey() {
    SimulateResponseForPendingRequest(
        "https://chromekanonymity-pa.googleapis.com/v1/proxy/keys",
        "binaryKeyInBase64");
  }

  void RespondWithJoin() {
    task_environment_->RunUntilIdle();
    network_context_.RespondToPendingRequest(kJoinRelayURL, "{}\n");
  }

  void RespondWithQueryKey() {
    SimulateResponseForPendingRequest(
        "https://chromekanonymityquery-pa.googleapis.com/v1/proxy/keys",
        "binaryKeyInBase64");
  }

  void RespondWithQuery(const std::string& response_str) {
    task_environment_->RunUntilIdle();
    network_context_.RespondToPendingRequest(kQueryRelayURL, response_str);
  }

  void DropOhttpRequests() { network_context_.SetDropRequests(true); }

  void SetOhttpErrorOnce(net::Error err) { network_context_.SetErrorOnce(err); }

 private:
  OhttpTestNetworkContext network_context_;
  mojo::Receiver<network::mojom::NetworkContext> network_context_receiver_;
};

TEST_F(KAnonymityServiceClientJoinQueryTest, TryJoinSetGetOHTTPKeyFailed) {
  InitializeIdentity(true);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  base::RunLoop run_loop;
  k_service.JoinSet("1",

                    base::BindLambdaForTesting([&run_loop](bool result) {
                      EXPECT_FALSE(result);
                      run_loop.Quit();
                    }));
  SimulateFailedResponseForPendingRequest(
      "https://chromekanonymity-pa.googleapis.com/v1/proxy/keys");
  run_loop.Run();
  CheckJoinSetHistogramActions(
      hist, {{KAnonymityServiceJoinSetAction::kJoinSet, 1},
             {KAnonymityServiceJoinSetAction::kFetchJoinSetOHTTPKey, 1},
             {KAnonymityServiceJoinSetAction::kFetchJoinSetOHTTPKeyFailed, 1}});
}

TEST_F(KAnonymityServiceClientJoinQueryTest, TryJoinSetSignedIn) {
  InitializeIdentity(true);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  base::RunLoop run_loop;
  k_service.JoinSet("1", base::BindLambdaForTesting([&run_loop](bool result) {
                      EXPECT_TRUE(result);
                      run_loop.Quit();
                    }));
  RespondWithJoinKey();
  RespondWithTrustTokenNonUniqueUserID(2);
  RespondWithTrustTokenKeys(2);
  RespondWithTrustTokenIssued(2);
  RespondWithJoin();
  run_loop.Run();
  CheckJoinSetHistogramActions(
      hist, {{KAnonymityServiceJoinSetAction::kJoinSet, 1},
             {KAnonymityServiceJoinSetAction::kFetchJoinSetOHTTPKey, 1},
             {KAnonymityServiceJoinSetAction::kSendJoinSetRequest, 1},
             {KAnonymityServiceJoinSetAction::kJoinSetSuccess, 1}});
}

TEST_F(KAnonymityServiceClientJoinQueryTest, TryJoinSetNetworkDrop) {
  InitializeIdentity(true);
  DropOhttpRequests();

  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  base::RunLoop run_loop;
  k_service.JoinSet("1", base::BindLambdaForTesting([&run_loop](bool result) {
                      EXPECT_FALSE(result);
                      run_loop.Quit();
                    }));
  RespondWithJoinKey();
  RespondWithTrustTokenNonUniqueUserID(2);
  RespondWithTrustTokenKeys(2);
  RespondWithTrustTokenIssued(2);
  run_loop.Run();
  CheckJoinSetHistogramActions(
      hist, {{KAnonymityServiceJoinSetAction::kJoinSet, 1},
             {KAnonymityServiceJoinSetAction::kFetchJoinSetOHTTPKey, 1},
             {KAnonymityServiceJoinSetAction::kSendJoinSetRequest, 1},
             {KAnonymityServiceJoinSetAction::kJoinSetRequestFailed, 1}});
}

TEST_F(KAnonymityServiceClientJoinQueryTest, TryJoinSetTokenAlreadyUsedOnce) {
  InitializeIdentity(true);
  SetOhttpErrorOnce(net::ERR_TRUST_TOKEN_OPERATION_FAILED);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  base::RunLoop run_loop;
  k_service.JoinSet("1", base::BindLambdaForTesting([&run_loop](bool result) {
                      EXPECT_TRUE(result);
                      run_loop.Quit();
                    }));
  RespondWithJoinKey();
  RespondWithTrustTokenNonUniqueUserID(2);
  RespondWithTrustTokenKeys(2);
  RespondWithTrustTokenIssued(2);
  RespondWithJoin();
  RespondWithTrustTokenIssued(2);
  RespondWithJoin();
  run_loop.Run();
  CheckJoinSetHistogramActions(
      hist, {{KAnonymityServiceJoinSetAction::kJoinSet, 1},
             {KAnonymityServiceJoinSetAction::kFetchJoinSetOHTTPKey, 1},
             {KAnonymityServiceJoinSetAction::kSendJoinSetRequest, 2},
             {KAnonymityServiceJoinSetAction::kJoinSetSuccess, 1}});
}

TEST_F(KAnonymityServiceClientJoinQueryTest, TryJoinSetOtherErrorsNotRetried) {
  InitializeIdentity(true);
  SetOhttpErrorOnce(net::ERR_FAILED);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  base::RunLoop run_loop;
  k_service.JoinSet("1", base::BindLambdaForTesting([&run_loop](bool result) {
                      EXPECT_FALSE(result);
                      run_loop.Quit();
                    }));
  RespondWithJoinKey();
  RespondWithTrustTokenNonUniqueUserID(2);
  RespondWithTrustTokenKeys(2);
  RespondWithTrustTokenIssued(2);
  RespondWithJoin();
  run_loop.Run();
  CheckJoinSetHistogramActions(
      hist, {{KAnonymityServiceJoinSetAction::kJoinSet, 1},
             {KAnonymityServiceJoinSetAction::kFetchJoinSetOHTTPKey, 1},
             {KAnonymityServiceJoinSetAction::kSendJoinSetRequest, 1},
             {KAnonymityServiceJoinSetAction::kJoinSetRequestFailed, 1}});
}

TEST_F(KAnonymityServiceClientJoinQueryTest,
       TryJoinSetTokenAlreadyRetriedTooMany) {
  InitializeIdentity(true);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  base::RunLoop run_loop;
  k_service.JoinSet("1", base::BindLambdaForTesting([&run_loop](bool result) {
                      EXPECT_FALSE(result);
                      run_loop.Quit();
                    }));
  RespondWithJoinKey();
  RespondWithTrustTokenNonUniqueUserID(2);
  RespondWithTrustTokenKeys(2);
  for (int i = 0; i < 6; i++) {
    RespondWithTrustTokenIssued(2);
    SetOhttpErrorOnce(net::ERR_TRUST_TOKEN_OPERATION_FAILED);
    RespondWithJoin();
  }
  run_loop.Run();
  CheckJoinSetHistogramActions(
      hist, {{KAnonymityServiceJoinSetAction::kJoinSet, 1},
             {KAnonymityServiceJoinSetAction::kFetchJoinSetOHTTPKey, 1},
             {KAnonymityServiceJoinSetAction::kSendJoinSetRequest, 6},
             {KAnonymityServiceJoinSetAction::kJoinSetRequestFailed, 1}});
}

TEST_F(KAnonymityServiceClientJoinQueryTest, TryQuerySetGetOHTTPKeyFailed) {
  InitializeIdentity(true);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  std::vector<std::string> sets;
  sets.push_back("1");
  base::RunLoop run_loop;
  k_service.QuerySets(
      sets, base::BindLambdaForTesting([&run_loop](std::vector<bool> result) {
        run_loop.Quit();
        ASSERT_EQ(0u, result.size());
      }));
  SimulateFailedResponseForPendingRequest(
      "https://chromekanonymityquery-pa.googleapis.com/v1/proxy/keys");
  run_loop.Run();
  CheckQuerySetHistogramActions(
      hist,
      {{KAnonymityServiceQuerySetAction::kQuerySet, 1},
       {KAnonymityServiceQuerySetAction::kFetchQuerySetOHTTPKey, 1},
       {KAnonymityServiceQuerySetAction::kFetchQuerySetOHTTPKeyFailed, 1}});
}

TEST_F(KAnonymityServiceClientJoinQueryTest, TryQuerySetBadResponse) {
  InitializeIdentity(/*signed_on=*/true);
  base::HistogramTester hist;
  std::vector<std::string> sets;
  sets.push_back("1");

  std::vector<std::string> bad_responses = {
      "",                              // empty
      "1df3fd33sasdf",                 // base64 nonsense
      "\x00\x11\x22\x33\x44\x55\x66",  // binary nonsense
      "[]",                            // not a dict
      "{}",                            // empty dict
      R"({
      "kAnonymousSets": "not a list"
    })",                               // kAnonymousSets is not a list
      R"({
      "kAnonymousSets": {}
    })",                               // kAnonymousSets is not a list
      R"({
      "kAnonymousSets": [
        "element is not a dict"
      ]
    })",                               // element of list is not a dict
      R"({
      "kAnonymousSets": [
        {}
      ]
    })",                               // element of list is missing keys
      R"({
      "kAnonymousSets": [{
        "type": "foo",
        "hashes": []
      }]
    })",                               // type doesn't match
      R"({
      "kAnonymousSets": [{
        "type": "fledge",
        "hashes": {}
      }]
    })",                               // hashes should be a list
      R"({
      "kAnonymousSets": [{
        "type": "fledge",
        "hashes": [{},{}]
      }]
    })",                               // hashes should contain strings
      R"({
      "kAnonymousSets": [{
        "type": "fledge",
        "hashes": ["asdf/123kj_-+%"]
      }]
    })",                               // hashes should be base64 encoded values
  };
  for (const auto& response : bad_responses) {
    base::RunLoop run_loop;
    KAnonymityServiceClient k_service(profile());
    k_service.QuerySets(sets,
                        base::BindLambdaForTesting(
                            [&run_loop, &response](std::vector<bool> result) {
                              EXPECT_EQ(0u, result.size()) << response;
                              run_loop.Quit();
                            }));
    RespondWithQueryKey();
    RespondWithQuery(response);
    run_loop.Run();
  }
  CheckQuerySetHistogramActions(
      hist, {{KAnonymityServiceQuerySetAction::kQuerySet, bad_responses.size()},
             {KAnonymityServiceQuerySetAction::kFetchQuerySetOHTTPKey,
              bad_responses.size()},
             {KAnonymityServiceQuerySetAction::kSendQuerySetRequest,
              bad_responses.size()},
             {KAnonymityServiceQuerySetAction::kQuerySetRequestParseError,
              bad_responses.size()}});
}

TEST_F(KAnonymityServiceClientJoinQueryTest, TryQuerySetSignedIn) {
  InitializeIdentity(true);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  std::vector<std::string> sets;
  sets.push_back("1");
  base::RunLoop run_loop;
  k_service.QuerySets(
      sets, base::BindLambdaForTesting([&run_loop](std::vector<bool> result) {
        run_loop.Quit();
        ASSERT_EQ(1u, result.size());
        EXPECT_TRUE(result[0]);
      }));
  RespondWithQueryKey();
  RespondWithQuery(
      R"({
        "kAnonymousSets": [{
          "hashes": [
            "a4ayc/80/OGda4BO/1o/V0etpOqiLx1JwB5S3beHW0s="
            ],
          "type":"fledge"
        }]
      })");
  run_loop.Run();
  CheckQuerySetHistogramActions(
      hist, {{KAnonymityServiceQuerySetAction::kQuerySet, 1},
             {KAnonymityServiceQuerySetAction::kFetchQuerySetOHTTPKey, 1},
             {KAnonymityServiceQuerySetAction::kSendQuerySetRequest, 1},
             {KAnonymityServiceQuerySetAction::kQuerySetsSuccess, 1}});
}

TEST_F(KAnonymityServiceClientJoinQueryTest, TryQuerySetMultipleSets) {
  InitializeIdentity(true);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  std::vector<std::string> sets;
  sets.push_back("1");
  sets.push_back("2");
  base::RunLoop run_loop;
  k_service.QuerySets(sets,
                      base::BindLambdaForTesting([](std::vector<bool> result) {
                        ASSERT_EQ(2u, result.size());
                        EXPECT_TRUE(result[0]);
                        EXPECT_TRUE(result[1]);
                      }));
  k_service.QuerySets(
      sets, base::BindLambdaForTesting([&run_loop](std::vector<bool> result) {
        run_loop.Quit();
        ASSERT_EQ(2u, result.size());
        EXPECT_FALSE(result[0]);
        EXPECT_TRUE(result[1]);
      }));
  RespondWithQueryKey();
  // The first response includes the hashes for "1" and "2".
  RespondWithQuery(R"({
    "kAnonymousSets": [{
       "type": "fledge",
       "hashes": [
          "a4ayc/80/OGda4BO/1o/V0etpOqiLx1JwB5S3beHW0s=",
          "1HNeOiZeFu7gP1lxi5tdAwGcB9i2xR+Q2jpmbuwTqzU="
          ]
      }]
    })");
  // The second response only includes the hash for "2".
  RespondWithQuery(R"({
    "kAnonymousSets": [
      { "type": "fledge",
        "hashes": [
          "1HNeOiZeFu7gP1lxi5tdAwGcB9i2xR+Q2jpmbuwTqzU="
          ]
        }
      ]
    })");

  run_loop.Run();
  CheckQuerySetHistogramActions(
      hist, {{KAnonymityServiceQuerySetAction::kQuerySet, 2},
             {KAnonymityServiceQuerySetAction::kFetchQuerySetOHTTPKey, 1},
             {KAnonymityServiceQuerySetAction::kSendQuerySetRequest, 2},
             {KAnonymityServiceQuerySetAction::kQuerySetsSuccess, 2}});
}

TEST_F(KAnonymityServiceClientJoinQueryTest, TryQuerySetCoalescesSplitSets) {
  InitializeIdentity(true);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  std::vector<std::string> sets;
  sets.push_back("1");
  sets.push_back("2");
  base::RunLoop run_loop;
  k_service.QuerySets(
      sets, base::BindLambdaForTesting([&run_loop](std::vector<bool> result) {
        run_loop.Quit();
        ASSERT_EQ(2u, result.size());
        EXPECT_TRUE(result[0]);
        EXPECT_TRUE(result[1]);
      }));
  RespondWithQueryKey();
  RespondWithQuery(
      R"({
        "kAnonymousSets": [
        {
          "hashes": [
            "a4ayc/80/OGda4BO/1o/V0etpOqiLx1JwB5S3beHW0s="
            ],
          "type":"fledge"
        },
        {
          "hashes": [
            "1HNeOiZeFu7gP1lxi5tdAwGcB9i2xR+Q2jpmbuwTqzU="
          ],
          "type":"fledge"
        }]
      })");
  run_loop.Run();
  CheckQuerySetHistogramActions(
      hist, {{KAnonymityServiceQuerySetAction::kQuerySet, 1},
             {KAnonymityServiceQuerySetAction::kFetchQuerySetOHTTPKey, 1},
             {KAnonymityServiceQuerySetAction::kSendQuerySetRequest, 1},
             {KAnonymityServiceQuerySetAction::kQuerySetsSuccess, 1}});
}

TEST_F(KAnonymityServiceClientJoinQueryTest,
       TryQuerySetSingleFailureDropsAllRequests) {
  InitializeIdentity(true);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  std::vector<std::string> sets;
  sets.push_back("1");
  sets.push_back("2");
  base::RunLoop run_loop;
  int callback_count = 0;
  k_service.QuerySets(
      sets, base::BindLambdaForTesting(
                [&run_loop, &callback_count](std::vector<bool> result) {
                  ASSERT_EQ(0u, result.size());
                  EXPECT_EQ(0, callback_count++);
                  run_loop.Quit();
                }));
  k_service.QuerySets(sets, base::BindLambdaForTesting(
                                [&callback_count](std::vector<bool> result) {
                                  ASSERT_EQ(0u, result.size());
                                  EXPECT_EQ(1, callback_count++);
                                }));
  k_service.QuerySets(sets, base::BindLambdaForTesting(
                                [&callback_count](std::vector<bool> result) {
                                  ASSERT_EQ(0u, result.size());
                                  EXPECT_EQ(2, callback_count++);
                                }));
  SimulateFailedResponseForPendingRequest(
      "https://chromekanonymityquery-pa.googleapis.com/v1/proxy/keys");
  run_loop.Run();
  CheckQuerySetHistogramActions(
      hist,
      {{KAnonymityServiceQuerySetAction::kQuerySet, 3},
       {KAnonymityServiceQuerySetAction::kFetchQuerySetOHTTPKey, 1},
       {KAnonymityServiceQuerySetAction::kFetchQuerySetOHTTPKeyFailed, 1}});
}

}  // namespace
