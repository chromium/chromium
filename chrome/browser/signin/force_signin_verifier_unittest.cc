// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/force_signin_verifier.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ForceSigninVerifierWithAccessToInternalsForTesting
    : public ForceSigninVerifier {
 public:
  explicit ForceSigninVerifierWithAccessToInternalsForTesting(
      signin::IdentityManager* identity_manager)
      : ForceSigninVerifier(identity_manager) {}

  bool IsDelayTaskPosted() { return GetOneShotTimerForTesting()->IsRunning(); }

  int FailureCount() { return GetBackoffEntryForTesting()->failure_count(); }

  signin::PrimaryAccountAccessTokenFetcher* access_token_fetcher() {
    return GetAccessTokenFetcherForTesting();
  }

  MOCK_METHOD0(CloseAllBrowserWindows, void(void));
};

// A NetworkConnectionObserver that invokes a base::RepeatingClosure when
// NetworkConnectionObserver::OnConnectionChanged() is invoked.
class NetworkConnectionObserverHelper
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  explicit NetworkConnectionObserverHelper(base::RepeatingClosure closure)
      : closure_(std::move(closure)) {
    DCHECK(!closure_.is_null());
    content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  }

  ~NetworkConnectionObserverHelper() override {
    content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(
        this);
  }

  void OnConnectionChanged(network::mojom::ConnectionType type) override {
    closure_.Run();
  }

 private:
  base::RepeatingClosure closure_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConnectionObserverHelper);
};

// Used to select which type of network type NetworkConnectionTracker should
// be configured to.
enum class NetworkConnectionType {
  Undecided,
  ConnectionNone,
  ConnectionWifi,
  Connection4G,
};

// Used to select which type of response NetworkConnectionTracker should give.
enum class NetworkResponseType {
  Undecided,
  Synchronous,
  Asynchronous,
};

// Forces the network connection type to change to |connection_type| and wait
// till the notification has been propagated to the observers. Also change the
// response type to be synchronous/asynchronous based on |response_type|.
void ConfigureNetworkConnectionTracker(NetworkConnectionType connection_type,
                                       NetworkResponseType response_type) {
  network::TestNetworkConnectionTracker* tracker =
      network::TestNetworkConnectionTracker::GetInstance();

  switch (response_type) {
    case NetworkResponseType::Undecided:
      // nothing to do
      break;

    case NetworkResponseType::Synchronous:
      tracker->SetRespondSynchronously(true);
      break;

    case NetworkResponseType::Asynchronous:
      tracker->SetRespondSynchronously(false);
      break;
  }

  if (connection_type != NetworkConnectionType::Undecided) {
    network::mojom::ConnectionType mojom_connection_type =
        network::mojom::ConnectionType::CONNECTION_UNKNOWN;

    switch (connection_type) {
      case NetworkConnectionType::Undecided:
        NOTREACHED();
        break;

      case NetworkConnectionType::ConnectionNone:
        mojom_connection_type = network::mojom::ConnectionType::CONNECTION_NONE;
        break;

      case NetworkConnectionType::ConnectionWifi:
        mojom_connection_type = network::mojom::ConnectionType::CONNECTION_WIFI;
        break;

      case NetworkConnectionType::Connection4G:
        mojom_connection_type = network::mojom::ConnectionType::CONNECTION_4G;
        break;
    }

    DCHECK_NE(mojom_connection_type,
              network::mojom::ConnectionType::CONNECTION_UNKNOWN);

    base::RunLoop wait_for_network_type_change;
    NetworkConnectionObserverHelper scoped_observer(
        wait_for_network_type_change.QuitWhenIdleClosure());

    tracker->SetConnectionType(mojom_connection_type);

    wait_for_network_type_change.Run();
  }
}

// Forces the current sequence's task runner to spin. This is used because the
// ForceSigninVerifier ends up posting task to the sequence's task runner when
// MetworkConnectionTracker is returning results asynchronously.
void SpinCurrentSequenceTaskRunner() {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace

TEST(ForceSigninVerifierTest, OnGetTokenSuccess) {
  base::test::TaskEnvironment scoped_task_env;
  signin::IdentityTestEnvironment identity_test_env;
  const AccountInfo account_info =
      identity_test_env.MakePrimaryAccountAvailable("email@test.com");

  base::HistogramTester histogram_tester;
  ForceSigninVerifierWithAccessToInternalsForTesting verifier(
      identity_test_env.identity_manager());

  ASSERT_NE(nullptr, verifier.access_token_fetcher());
  ASSERT_FALSE(verifier.HasTokenBeenVerified());
  ASSERT_FALSE(verifier.IsDelayTaskPosted());
  EXPECT_CALL(verifier, CloseAllBrowserWindows()).Times(0);

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      account_info.account_id, /*token=*/"", base::Time());

  ASSERT_EQ(nullptr, verifier.access_token_fetcher());
  ASSERT_TRUE(verifier.HasTokenBeenVerified());
  ASSERT_FALSE(verifier.IsDelayTaskPosted());
  ASSERT_EQ(0, verifier.FailureCount());
  histogram_tester.ExpectBucketCount(kForceSigninVerificationMetricsName, 1, 1);
  histogram_tester.ExpectTotalCount(
      kForceSigninVerificationSuccessTimeMetricsName, 1);
  histogram_tester.ExpectTotalCount(
      kForceSigninVerificationFailureTimeMetricsName, 0);
}

TEST(ForceSigninVerifierTest, OnGetTokenPersistentFailure) {
  base::test::TaskEnvironment scoped_task_env;
  signin::IdentityTestEnvironment identity_test_env;
  const AccountInfo account_info =
      identity_test_env.MakePrimaryAccountAvailable("email@test.com");

  base::HistogramTester histogram_tester;
  ForceSigninVerifierWithAccessToInternalsForTesting verifier(
      identity_test_env.identity_manager());

  ASSERT_NE(nullptr, verifier.access_token_fetcher());
  ASSERT_FALSE(verifier.HasTokenBeenVerified());
  ASSERT_FALSE(verifier.IsDelayTaskPosted());
  EXPECT_CALL(verifier, CloseAllBrowserWindows()).Times(1);

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  ASSERT_EQ(nullptr, verifier.access_token_fetcher());
  ASSERT_TRUE(verifier.HasTokenBeenVerified());
  ASSERT_FALSE(verifier.IsDelayTaskPosted());
  ASSERT_EQ(0, verifier.FailureCount());
  histogram_tester.ExpectBucketCount(kForceSigninVerificationMetricsName, 1, 1);
  histogram_tester.ExpectTotalCount(
      kForceSigninVerificationSuccessTimeMetricsName, 0);
  histogram_tester.ExpectTotalCount(
      kForceSigninVerificationFailureTimeMetricsName, 1);
}

TEST(ForceSigninVerifierTest, OnGetTokenTransientFailure) {
  base::test::TaskEnvironment scoped_task_env;
  signin::IdentityTestEnvironment identity_test_env;
  const AccountInfo account_info =
      identity_test_env.MakePrimaryAccountAvailable("email@test.com");

  base::HistogramTester histogram_tester;
  ForceSigninVerifierWithAccessToInternalsForTesting verifier(
      identity_test_env.identity_manager());

  ASSERT_NE(nullptr, verifier.access_token_fetcher());
  ASSERT_FALSE(verifier.HasTokenBeenVerified());
  ASSERT_FALSE(verifier.IsDelayTaskPosted());
  EXPECT_CALL(verifier, CloseAllBrowserWindows()).Times(0);

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::State::CONNECTION_FAILED));

  ASSERT_EQ(nullptr, verifier.access_token_fetcher());
  ASSERT_FALSE(verifier.HasTokenBeenVerified());
  ASSERT_TRUE(verifier.IsDelayTaskPosted());
  ASSERT_EQ(1, verifier.FailureCount());
  histogram_tester.ExpectBucketCount(kForceSigninVerificationMetricsName, 1, 1);
  histogram_tester.ExpectTotalCount(
      kForceSigninVerificationSuccessTimeMetricsName, 0);
  histogram_tester.ExpectTotalCount(
      kForceSigninVerificationFailureTimeMetricsName, 0);
}

TEST(ForceSigninVerifierTest, OnLostConnection) {
  base::test::TaskEnvironment scoped_task_env;
  signin::IdentityTestEnvironment identity_test_env;
  const AccountInfo account_info =
      identity_test_env.MakePrimaryAccountAvailable("email@test.com");

  ForceSigninVerifierWithAccessToInternalsForTesting verifier(
      identity_test_env.identity_manager());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::State::CONNECTION_FAILED));

  ASSERT_EQ(1, verifier.FailureCount());
  ASSERT_EQ(nullptr, verifier.access_token_fetcher());
  ASSERT_TRUE(verifier.IsDelayTaskPosted());

  ConfigureNetworkConnectionTracker(NetworkConnectionType::ConnectionNone,
                                    NetworkResponseType::Undecided);

  ASSERT_EQ(0, verifier.FailureCount());
  ASSERT_EQ(nullptr, verifier.access_token_fetcher());
  ASSERT_FALSE(verifier.IsDelayTaskPosted());
}

TEST(ForceSigninVerifierTest, OnReconnected) {
  base::test::TaskEnvironment scoped_task_env;
  signin::IdentityTestEnvironment identity_test_env;
  const AccountInfo account_info =
      identity_test_env.MakePrimaryAccountAvailable("email@test.com");

  ForceSigninVerifierWithAccessToInternalsForTesting verifier(
      identity_test_env.identity_manager());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::State::CONNECTION_FAILED));

  ASSERT_EQ(1, verifier.FailureCount());
  ASSERT_EQ(nullptr, verifier.access_token_fetcher());
  ASSERT_TRUE(verifier.IsDelayTaskPosted());

  ConfigureNetworkConnectionTracker(NetworkConnectionType::ConnectionWifi,
                                    NetworkResponseType::Undecided);

  ASSERT_EQ(0, verifier.FailureCount());
  ASSERT_NE(nullptr, verifier.access_token_fetcher());
  ASSERT_FALSE(verifier.IsDelayTaskPosted());
}

TEST(ForceSigninVerifierTest, GetNetworkStatusAsync) {
  base::test::TaskEnvironment scoped_task_env;
  signin::IdentityTestEnvironment identity_test_env;
  const AccountInfo account_info =
      identity_test_env.MakePrimaryAccountAvailable("email@test.com");

  ConfigureNetworkConnectionTracker(NetworkConnectionType::Undecided,
                                    NetworkResponseType::Asynchronous);

  ForceSigninVerifierWithAccessToInternalsForTesting verifier(
      identity_test_env.identity_manager());

  // There is no network type at first.
  ASSERT_EQ(nullptr, verifier.access_token_fetcher());

  // Waiting for the network type returns.
  SpinCurrentSequenceTaskRunner();

  // Get the type and send the request.
  ASSERT_NE(nullptr, verifier.access_token_fetcher());
}

TEST(ForceSigninVerifierTest, LaunchVerifierWithoutNetwork) {
  base::test::TaskEnvironment scoped_task_env;
  signin::IdentityTestEnvironment identity_test_env;
  const AccountInfo account_info =
      identity_test_env.MakePrimaryAccountAvailable("email@test.com");

  ConfigureNetworkConnectionTracker(NetworkConnectionType::ConnectionNone,
                                    NetworkResponseType::Asynchronous);

  ForceSigninVerifierWithAccessToInternalsForTesting verifier(
      identity_test_env.identity_manager());

  // There is no network type.
  ASSERT_EQ(nullptr, verifier.access_token_fetcher());

  // Waiting for the network type returns.
  SpinCurrentSequenceTaskRunner();

  // Get the type, there is no network connection, don't send the request.
  ASSERT_EQ(nullptr, verifier.access_token_fetcher());

  // Network is resumed.
  ConfigureNetworkConnectionTracker(NetworkConnectionType::ConnectionWifi,
                                    NetworkResponseType::Undecided);

  // Send the request.
  ASSERT_NE(nullptr, verifier.access_token_fetcher());
}

TEST(ForceSigninVerifierTest, ChangeNetworkFromWIFITo4GWithOnGoingRequest) {
  base::test::TaskEnvironment scoped_task_env;
  signin::IdentityTestEnvironment identity_test_env;
  const AccountInfo account_info =
      identity_test_env.MakePrimaryAccountAvailable("email@test.com");

  ConfigureNetworkConnectionTracker(NetworkConnectionType::ConnectionWifi,
                                    NetworkResponseType::Asynchronous);

  ForceSigninVerifierWithAccessToInternalsForTesting verifier(
      identity_test_env.identity_manager());

  EXPECT_EQ(nullptr, verifier.access_token_fetcher());

  // Waiting for the network type returns.
  SpinCurrentSequenceTaskRunner();

  // The network type if wifi, send the request.
  auto* first_request = verifier.access_token_fetcher();
  EXPECT_NE(nullptr, first_request);

  // Network is changed to 4G.
  ConfigureNetworkConnectionTracker(NetworkConnectionType::Connection4G,
                                    NetworkResponseType::Undecided);

  // There is still one on-going request.
  EXPECT_EQ(first_request, verifier.access_token_fetcher());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      account_info.account_id, /*token=*/"", base::Time());
}

TEST(ForceSigninVerifierTest, ChangeNetworkFromWIFITo4GWithFinishedRequest) {
  base::test::TaskEnvironment scoped_task_env;
  signin::IdentityTestEnvironment identity_test_env;
  const AccountInfo account_info =
      identity_test_env.MakePrimaryAccountAvailable("email@test.com");

  ConfigureNetworkConnectionTracker(NetworkConnectionType::ConnectionWifi,
                                    NetworkResponseType::Asynchronous);

  ForceSigninVerifierWithAccessToInternalsForTesting verifier(
      identity_test_env.identity_manager());

  EXPECT_EQ(nullptr, verifier.access_token_fetcher());

  // Waiting for the network type returns.
  SpinCurrentSequenceTaskRunner();

  // The network type if wifi, send the request.
  EXPECT_NE(nullptr, verifier.access_token_fetcher());

  // Finishes the request.
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      account_info.account_id, /*token=*/"", base::Time());
  EXPECT_EQ(nullptr, verifier.access_token_fetcher());

  // Network is changed to 4G.
  ConfigureNetworkConnectionTracker(NetworkConnectionType::Connection4G,
                                    NetworkResponseType::Undecided);

  // No more request because it's verfied already.
  EXPECT_EQ(nullptr, verifier.access_token_fetcher());
}
