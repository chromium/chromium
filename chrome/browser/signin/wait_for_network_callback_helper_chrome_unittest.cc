// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/wait_for_network_callback_helper_chrome.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

class WaitForNetworkCallbackHelperChromeTest : public testing::Test {
 public:
 protected:
  void SetUpNetworkConnection(
      bool respond_synchronously,
      net::NetworkChangeNotifier::ConnectionType connection_type) {
    auto* tracker = network::TestNetworkConnectionTracker::GetInstance();
    tracker->SetRespondSynchronously(respond_synchronously);
    tracker->SetConnectionType(connection_type);
  }

  void SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType connection_type) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        connection_type);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  WaitForNetworkCallbackHelperChrome helper_{
      /*should_disable_metrics_for_testing=*/false};
};

TEST_F(WaitForNetworkCallbackHelperChromeTest,
       MetricsCollection_DelayedNetworkCall) {
  base::HistogramTester histogram_tester;
  SetUpNetworkConnection(
      true, net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE);
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_2G);

  helper_.DelayNetworkCall(base::DoNothing());
  task_environment_.FastForwardBy(base::Minutes(30));

  histogram_tester.ExpectUniqueSample("Signin.DelayedNetworkCallQueueSize.2G",
                                      1, 1);
}

TEST_F(WaitForNetworkCallbackHelperChromeTest,
       DelayNetworkCallRunsImmediatelyWithNetwork) {
  SetUpNetworkConnection(
      true, net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G);
  base::test::TestFuture<void> future;
  helper_.DelayNetworkCall(future.GetCallback());
  EXPECT_TRUE(future.IsReady());
}

TEST_F(WaitForNetworkCallbackHelperChromeTest,
       GetConnectionTypeRespondsAsynchronously) {
  SetUpNetworkConnection(
      false, net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G);

  base::test::TestFuture<void> future;
  helper_.DelayNetworkCall(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(future.Wait());
}

TEST_F(WaitForNetworkCallbackHelperChromeTest,
       DelayNetworkCallRunsAfterNetworkChange) {
  SetUpNetworkConnection(
      true, net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE);

  base::test::TestFuture<void> future;
  helper_.DelayNetworkCall(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  SetConnectionType(net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G);
  EXPECT_TRUE(future.Wait());
}

TEST_F(WaitForNetworkCallbackHelperChromeTest,
       MutlipleDelayNetworkCallsRunsAfterNetworkChange) {
  SetUpNetworkConnection(
      true, net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE);

  std::array<base::test::TestFuture<void>, 5> futures;
  for (auto& future : futures) {
    helper_.DelayNetworkCall(future.GetCallback());
    EXPECT_FALSE(future.IsReady());
  }
  SetConnectionType(net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G);
  EXPECT_TRUE(futures[0].Wait());
  for (auto& future : futures) {
    EXPECT_TRUE(future.IsReady());
  }
}
