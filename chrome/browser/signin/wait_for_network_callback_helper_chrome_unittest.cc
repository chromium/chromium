// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/wait_for_network_callback_helper_chrome.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

class WaitForNetworkCallbackHelperChromeTest : public testing::Test {
 public:
 protected:
  void SetUpNetworkConnection(bool respond_synchronously,
                              network::mojom::ConnectionType connection_type) {
    auto* tracker = network::TestNetworkConnectionTracker::GetInstance();
    tracker->SetRespondSynchronously(respond_synchronously);
    tracker->SetConnectionType(connection_type);
  }

  void SetConnectionType(network::mojom::ConnectionType connection_type) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        connection_type);
  }

  base::test::TaskEnvironment task_environment_;
  WaitForNetworkCallbackHelperChrome helper_;
};

TEST_F(WaitForNetworkCallbackHelperChromeTest,
       DelayNetworkCallRunsImmediatelyWithNetwork) {
  SetUpNetworkConnection(true, network::mojom::ConnectionType::CONNECTION_3G);
  base::test::TestFuture<void> future;
  helper_.DelayNetworkCall(future.GetCallback());
  EXPECT_TRUE(future.IsReady());
}

TEST_F(WaitForNetworkCallbackHelperChromeTest,
       GetConnectionTypeRespondsAsynchronously) {
  SetUpNetworkConnection(false, network::mojom::ConnectionType::CONNECTION_3G);

  base::test::TestFuture<void> future;
  helper_.DelayNetworkCall(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(future.Wait());
}

TEST_F(WaitForNetworkCallbackHelperChromeTest,
       DelayNetworkCallRunsAfterNetworkChange) {
  SetUpNetworkConnection(true, network::mojom::ConnectionType::CONNECTION_NONE);

  base::test::TestFuture<void> future;
  helper_.DelayNetworkCall(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_3G);
  EXPECT_TRUE(future.Wait());
}

TEST_F(WaitForNetworkCallbackHelperChromeTest,
       MutlipleDelayNetworkCallsRunsAfterNetworkChange) {
  SetUpNetworkConnection(true, network::mojom::ConnectionType::CONNECTION_NONE);

  std::array<base::test::TestFuture<void>, 5> futures;
  for (auto& future : futures) {
    helper_.DelayNetworkCall(future.GetCallback());
    EXPECT_FALSE(future.IsReady());
  }
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_3G);
  EXPECT_TRUE(futures[0].Wait());
  for (auto& future : futures) {
    EXPECT_TRUE(future.IsReady());
  }
}
