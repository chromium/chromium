// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/k_anonymity_service/k_anonymity_service_client.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_metrics.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/k_anonymity_service_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "net/dns/mock_host_resolver.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class KAnonymityServiceClientTest : public testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(network::features::kTrustTokens);
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
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        url, content, net::HTTP_OK,
        network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix));
  }

  void SimulateFailedResponseForPendingRequest(std::string url) {
    task_environment_.RunUntilIdle();
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

 private:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
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
                    base::OnceCallback<void(bool)>(
                        base::BindLambdaForTesting([&run_loop](bool result) {
                          EXPECT_FALSE(result);
                          run_loop.Quit();
                        })));
  run_loop.Run();
  EXPECT_EQ(0, GetNumPendingURLRequests());
  CheckJoinSetHistogramActions(
      hist, {{KAnonymityServiceJoinSetAction::kJoinSet, 1},
             {KAnonymityServiceJoinSetAction::kJoinSetRequestFailed, 1}});
}

TEST_F(KAnonymityServiceClientTest, TryJoinSetSuccess) {
  InitializeIdentity(true);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  base::RunLoop run_loop;
  k_service.JoinSet("1",
                    base::OnceCallback<void(bool)>(
                        base::BindLambdaForTesting([&run_loop](bool result) {
                          EXPECT_TRUE(result);
                          run_loop.Quit();
                        })));
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
    k_service.JoinSet("1",
                      base::OnceCallback<void(bool)>(base::BindLambdaForTesting(
                          [&callback_count, &run_loop, i](bool result) {
                            EXPECT_TRUE(result) << "iteration " << i;
                            callback_count++;
                            if (callback_count == 10)
                              run_loop.Quit();
                          })));
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
    k_service.JoinSet("1",
                      base::OnceCallback<void(bool)>(base::BindLambdaForTesting(
                          [&callback_count, &run_loop, i](bool result) {
                            EXPECT_TRUE(result) << "iteration " << i;
                            callback_count++;
                            run_loop.Quit();
                          })));
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
    k_service.JoinSet("1",
                      base::OnceCallback<void(bool)>(base::BindLambdaForTesting(
                          [&callback_count, &run_loop, i](bool result) {
                            EXPECT_FALSE(result) << "iteration " << i;
                            EXPECT_EQ(i, callback_count);
                            callback_count++;
                            if (callback_count == 10)
                              run_loop.Quit();
                          })));
  }
  SimulateFailedResponseForPendingRequest(
      "https://chromekanonymityauth-pa.googleapis.com/v1/"
      "generateShortIdentifier");
  run_loop.Run();
  EXPECT_EQ(0, GetNumPendingURLRequests());
  EXPECT_EQ(10, callback_count);
  CheckJoinSetHistogramActions(
      hist, {{KAnonymityServiceJoinSetAction::kJoinSet, 10},
             {KAnonymityServiceJoinSetAction::kJoinSetRequestFailed, 10}});
}

TEST_F(KAnonymityServiceClientTest, TryJoinSetOverflowQueue) {
  InitializeIdentity(true);
  KAnonymityServiceClient k_service(profile());
  base::HistogramTester hist;
  base::RunLoop run_loop;
  int callback_count = 0;
  for (int i = 0; i < 100; i++) {
    k_service.JoinSet("1",
                      base::OnceCallback<void(bool)>(base::BindLambdaForTesting(
                          [&callback_count, &run_loop, i](bool result) {
                            EXPECT_TRUE(result) << "iteration " << i;
                            EXPECT_EQ(i + 1, callback_count);
                            callback_count++;
                            if (callback_count == 101)
                              run_loop.Quit();
                          })));
  }
  // Queue should be full, so the next request should fail (asynchronously, but
  // before any successes).
  k_service.JoinSet(
      "Full", base::OnceCallback<void(bool)>(
                  base::BindLambdaForTesting([&callback_count](bool result) {
                    EXPECT_FALSE(result);
                    EXPECT_EQ(0, callback_count);
                    callback_count++;
                  })));
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
      base::OnceCallback<void(std::vector<bool>)>(
          base::BindLambdaForTesting([&run_loop](std::vector<bool> result) {
            ASSERT_EQ(2u, result.size());
            EXPECT_FALSE(result[0]);
            EXPECT_FALSE(result[1]);
            run_loop.Quit();
          })));
  run_loop.Run();
  EXPECT_EQ(0, GetNumPendingURLRequests());
  CheckQuerySetHistogramActions(
      hist, {{KAnonymityServiceQuerySetAction::kQuerySet, 1}});
  hist.ExpectUniqueSample("Chrome.KAnonymityService.QuerySet.Size", 2, 1);
}
