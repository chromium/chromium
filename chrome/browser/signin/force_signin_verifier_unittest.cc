// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/force_signin_verifier.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockForceSigninVerifier : public ForceSigninVerifier {
 public:
  explicit MockForceSigninVerifier(Profile* profile)
      : ForceSigninVerifier(profile) {}

  bool IsDelayTaskPosted() { return GetOneShotTimerForTesting()->IsRunning(); }

  int FailureCount() { return GetBackoffEntryForTesting()->failure_count(); }

  OAuth2TokenService::Request* request() { return GetRequestForTesting(); }

  MOCK_METHOD0(CloseAllBrowserWindows, void(void));
};

class ForceSigninVerifierTest
    : public ::testing::Test,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        SigninManagerFactory::GetInstance(),
        base::BindRepeating(&BuildFakeSigninManagerForTesting));
    builder.AddTestingFactory(
        ProfileOAuth2TokenServiceFactory::GetInstance(),
        base::BindRepeating(&BuildFakeProfileOAuth2TokenService));
    profile_ = builder.Build();
    FakeSigninManager* signin_manager_ = static_cast<FakeSigninManager*>(
        SigninManagerFactory::GetForProfile(profile_.get()));
    signin_manager_->SignIn("fake_id", "fake_username", "fake_password");

    oauth2_token_service()->GetDelegate()->UpdateCredentials("fake_id",
                                                             "fake_token");
  }

  void TearDown() override { verifier_.reset(); }

  void OnConnectionChanged(network::mojom::ConnectionType type) override {
    wait_for_network_type_change_.QuitWhenIdle();
  }

  FakeProfileOAuth2TokenService* oauth2_token_service() {
    return static_cast<FakeProfileOAuth2TokenService*>(
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get()));
  }

  std::unique_ptr<MockForceSigninVerifier> verifier_;
  content::TestBrowserThreadBundle bundle_;
  std::unique_ptr<TestingProfile> profile_;

  base::RunLoop wait_for_network_type_change_;
  base::RunLoop wait_for_network_type_async_return_;

  GoogleServiceAuthError persistent_error_ = GoogleServiceAuthError(
      GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS);
  GoogleServiceAuthError transient_error_ =
      GoogleServiceAuthError(GoogleServiceAuthError::State::CONNECTION_FAILED);

  base::HistogramTester histogram_tester_;
};

TEST_F(ForceSigninVerifierTest, OnGetTokenSuccess) {
  verifier_ = std::make_unique<MockForceSigninVerifier>(profile_.get());

  ASSERT_NE(nullptr, verifier_->request());
  ASSERT_FALSE(verifier_->HasTokenBeenVerified());
  ASSERT_FALSE(verifier_->IsDelayTaskPosted());
  EXPECT_CALL(*verifier_.get(), CloseAllBrowserWindows()).Times(0);

  FakeProfileOAuth2TokenService* service = oauth2_token_service();
  ASSERT_EQ(1u, service->GetPendingRequests().size());
  service->IssueAllTokensForAccount("fake_id",
                                    OAuth2AccessTokenConsumer::TokenResponse());

  ASSERT_EQ(nullptr, verifier_->request());
  ASSERT_TRUE(verifier_->HasTokenBeenVerified());
  ASSERT_FALSE(verifier_->IsDelayTaskPosted());
  ASSERT_EQ(0, verifier_->FailureCount());
  histogram_tester_.ExpectBucketCount(kForceSigninVerificationMetricsName, 1,
                                      1);
  histogram_tester_.ExpectTotalCount(
      kForceSigninVerificationSuccessTimeMetricsName, 1);
  histogram_tester_.ExpectTotalCount(
      kForceSigninVerificationFailureTimeMetricsName, 0);
}

TEST_F(ForceSigninVerifierTest, OnGetTokenPersistentFailure) {
  verifier_ = std::make_unique<MockForceSigninVerifier>(profile_.get());

  ASSERT_NE(nullptr, verifier_->request());
  ASSERT_FALSE(verifier_->HasTokenBeenVerified());
  ASSERT_FALSE(verifier_->IsDelayTaskPosted());
  EXPECT_CALL(*verifier_.get(), CloseAllBrowserWindows()).Times(1);

  FakeProfileOAuth2TokenService* service = oauth2_token_service();
  ASSERT_EQ(1u, service->GetPendingRequests().size());
  service->IssueErrorForAllPendingRequests(persistent_error_);

  ASSERT_EQ(nullptr, verifier_->request());
  ASSERT_TRUE(verifier_->HasTokenBeenVerified());
  ASSERT_FALSE(verifier_->IsDelayTaskPosted());
  ASSERT_EQ(0, verifier_->FailureCount());
  histogram_tester_.ExpectBucketCount(kForceSigninVerificationMetricsName, 1,
                                      1);
  histogram_tester_.ExpectTotalCount(
      kForceSigninVerificationSuccessTimeMetricsName, 0);
  histogram_tester_.ExpectTotalCount(
      kForceSigninVerificationFailureTimeMetricsName, 1);
}

TEST_F(ForceSigninVerifierTest, OnGetTokenTransientFailure) {
  verifier_ = std::make_unique<MockForceSigninVerifier>(profile_.get());

  ASSERT_NE(nullptr, verifier_->request());
  ASSERT_FALSE(verifier_->HasTokenBeenVerified());
  ASSERT_FALSE(verifier_->IsDelayTaskPosted());
  EXPECT_CALL(*verifier_.get(), CloseAllBrowserWindows()).Times(0);

  FakeProfileOAuth2TokenService* service = oauth2_token_service();
  ASSERT_EQ(1u, service->GetPendingRequests().size());
  service->IssueErrorForAllPendingRequests(transient_error_);

  ASSERT_EQ(nullptr, verifier_->request());
  ASSERT_FALSE(verifier_->HasTokenBeenVerified());
  ASSERT_TRUE(verifier_->IsDelayTaskPosted());
  ASSERT_EQ(1, verifier_->FailureCount());
  histogram_tester_.ExpectBucketCount(kForceSigninVerificationMetricsName, 1,
                                      1);
  histogram_tester_.ExpectTotalCount(
      kForceSigninVerificationSuccessTimeMetricsName, 0);
  histogram_tester_.ExpectTotalCount(
      kForceSigninVerificationFailureTimeMetricsName, 0);
}

TEST_F(ForceSigninVerifierTest, OnLostConnection) {
  verifier_ = std::make_unique<MockForceSigninVerifier>(profile_.get());

  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);

  FakeProfileOAuth2TokenService* service = oauth2_token_service();
  ASSERT_EQ(1u, service->GetPendingRequests().size());
  service->IssueErrorForAllPendingRequests(transient_error_);

  ASSERT_EQ(1, verifier_->FailureCount());
  ASSERT_EQ(nullptr, verifier_->request());
  ASSERT_TRUE(verifier_->IsDelayTaskPosted());

  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);
  wait_for_network_type_change_.Run();
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);

  ASSERT_EQ(0, verifier_->FailureCount());
  ASSERT_EQ(nullptr, verifier_->request());
  ASSERT_FALSE(verifier_->IsDelayTaskPosted());
}

TEST_F(ForceSigninVerifierTest, OnReconnected) {
  verifier_ = std::make_unique<MockForceSigninVerifier>(profile_.get());

  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);

  FakeProfileOAuth2TokenService* service = oauth2_token_service();
  ASSERT_EQ(1u, service->GetPendingRequests().size());
  service->IssueErrorForAllPendingRequests(transient_error_);

  ASSERT_EQ(1, verifier_->FailureCount());
  ASSERT_EQ(nullptr, verifier_->request());
  ASSERT_TRUE(verifier_->IsDelayTaskPosted());

  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  wait_for_network_type_change_.Run();
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);

  ASSERT_EQ(0, verifier_->FailureCount());
  ASSERT_NE(nullptr, verifier_->request());
  ASSERT_FALSE(verifier_->IsDelayTaskPosted());
}

TEST_F(ForceSigninVerifierTest, GetNetworkStatusAsync) {
  network::TestNetworkConnectionTracker::GetInstance()->SetRespondSynchronously(
      false);

  verifier_ = std::make_unique<MockForceSigninVerifier>(profile_.get());

  // There is no network type at first.
  ASSERT_EQ(nullptr, verifier_->request());

  // Waiting for the network type returns.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, wait_for_network_type_async_return_.QuitClosure());
  wait_for_network_type_async_return_.Run();

  // Get the type and send the request.
  ASSERT_NE(nullptr, verifier_->request());
}

TEST_F(ForceSigninVerifierTest, LaunchVerifierWithoutNetwork) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);
  network::TestNetworkConnectionTracker::GetInstance()->SetRespondSynchronously(
      false);

  verifier_ = std::make_unique<MockForceSigninVerifier>(profile_.get());

  // There is no network type.
  ASSERT_EQ(nullptr, verifier_->request());

  // Waiting for the network type returns.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, wait_for_network_type_async_return_.QuitClosure());
  wait_for_network_type_async_return_.Run();

  // Get the type, there is no network connection, don't send the request.
  ASSERT_EQ(nullptr, verifier_->request());

  // Network is resumed.
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  wait_for_network_type_change_.Run();
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);

  // Send the request.
  ASSERT_NE(nullptr, verifier_->request());
}

TEST_F(ForceSigninVerifierTest, ChangeNetworkFromWIFITo4GWithOnGoingRequest) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  network::TestNetworkConnectionTracker::GetInstance()->SetRespondSynchronously(
      false);

  verifier_ = std::make_unique<MockForceSigninVerifier>(profile_.get());
  FakeProfileOAuth2TokenService* service = oauth2_token_service();

  EXPECT_EQ(nullptr, verifier_->request());

  // Waiting for the network type returns.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, wait_for_network_type_async_return_.QuitClosure());
  wait_for_network_type_async_return_.Run();

  // The network type if wifi, send the request.
  auto* first_request = verifier_->request();
  EXPECT_NE(nullptr, first_request);
  EXPECT_EQ(1u, service->GetPendingRequests().size());

  // Network is changed to 4G.
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_4G);
  wait_for_network_type_change_.Run();
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);

  // There is still one on-going request.
  EXPECT_EQ(first_request, verifier_->request());
  EXPECT_EQ(1u, service->GetPendingRequests().size());
}

TEST_F(ForceSigninVerifierTest, ChangeNetworkFromWIFITo4GWithFinishedRequest) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  network::TestNetworkConnectionTracker::GetInstance()->SetRespondSynchronously(
      false);

  verifier_ = std::make_unique<MockForceSigninVerifier>(profile_.get());
  FakeProfileOAuth2TokenService* service = oauth2_token_service();

  EXPECT_EQ(nullptr, verifier_->request());

  // Waiting for the network type returns.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, wait_for_network_type_async_return_.QuitClosure());
  wait_for_network_type_async_return_.Run();

  // The network type if wifi, send the request.
  EXPECT_NE(nullptr, verifier_->request());

  // Finishes the request.
  service->IssueAllTokensForAccount("fake_id",
                                    OAuth2AccessTokenConsumer::TokenResponse());
  EXPECT_EQ(nullptr, verifier_->request());

  // Network is changed to 4G.
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_4G);
  wait_for_network_type_change_.Run();
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);

  // No more request because it's verfied already.
  EXPECT_EQ(nullptr, verifier_->request());
}
