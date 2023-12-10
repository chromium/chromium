// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/metrics/network_connectivity_metrics_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/sync_wifi/network_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

class NetworkConnectivityMetricsServiceTest : public testing::Test {
 public:
  NetworkConnectivityMetricsServiceTest()
      : local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())) {}

  NetworkConnectivityMetricsServiceTest(
      const NetworkConnectivityMetricsServiceTest&) = delete;
  NetworkConnectivityMetricsServiceTest& operator=(
      const NetworkConnectivityMetricsServiceTest&) = delete;

  TestingPrefServiceSimple* local_state() { return local_state_->Get(); }

  void SetUp() override {
    helper_.SetUp();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }
  void TearDown() override {
    local_state()->RemoveUserPref(prefs::kKioskMetrics);
  }
  NetworkStateHandler* network_state_handler() {
    return NetworkHandler::Get()->network_state_handler();
  }
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  std::optional<int> GetNetworkDropsFromLocalState() {
    return local_state()
        ->GetDict(prefs::kKioskMetrics)
        .FindInt(kKioskNetworkDrops);
  }

  void SimulateConnectionFailure(const NetworkState* network,
                                 std::string error) {
    SetNetworkProperty(network->path(), shill::kStateProperty,
                       shill::kStateConfiguration);

    SetNetworkProperty(network->path(), shill::kErrorProperty, error);
    SetNetworkProperty(network->path(), shill::kStateProperty,
                       shill::kStateFailure);
  }

  const NetworkState* SimulateConnectionSuccess() {
    const NetworkState* network = CreateNetwork();

    SetNetworkProperty(network->path(), shill::kStateProperty,
                       shill::kStateOnline);
    return network;
  }

  const NetworkState* CreateNetwork() {
    std::string guid = helper_.ConfigureWiFiNetwork(
        "ssid", /*is_secured=*/true, helper_.primary_user(),
        /*has_connected=*/true,
        /*owned_by_user=*/true, /*configured_by_sync=*/true);
    return helper_.network_state_helper()
        .network_state_handler()
        ->GetNetworkStateFromGuid(guid);
  }

 private:
  void SetNetworkProperty(const std::string& service_path,
                          const std::string& key,
                          const std::string& value) {
    helper_.network_state_test_helper()->SetServiceProperty(service_path, key,
                                                            base::Value(value));
    task_environment_.RunUntilIdle();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  sync_wifi::NetworkTestHelper helper_;
  std::unique_ptr<ScopedTestingLocalState> local_state_;
};

TEST_F(NetworkConnectivityMetricsServiceTest, StartNotInitialized) {
  auto service =
      NetworkConnectivityMetricsService::CreateForTesting(local_state());
  EXPECT_TRUE(network_state_handler()->HasObserver(service.get()));
  EXPECT_FALSE(service->is_online());
  EXPECT_EQ(std::optional<int>(0), GetNetworkDropsFromLocalState());
}

TEST_F(NetworkConnectivityMetricsServiceTest, StartOnlineGoOnline) {
  EXPECT_TRUE(SimulateConnectionSuccess() != nullptr);
  auto service =
      NetworkConnectivityMetricsService::CreateForTesting(local_state());
  EXPECT_TRUE(network_state_handler()->HasObserver(service.get()));
  EXPECT_TRUE(service->is_online());
  EXPECT_EQ(std::optional<int>(0), GetNetworkDropsFromLocalState());

  // Nothing changes when go online from online.
  EXPECT_TRUE(SimulateConnectionSuccess() != nullptr);
  EXPECT_TRUE(service->is_online());
  EXPECT_EQ(std::optional<int>(0), GetNetworkDropsFromLocalState());
}

TEST_F(NetworkConnectivityMetricsServiceTest, StartOnlineGoOfflineDrop) {
  const auto* network = SimulateConnectionSuccess();
  auto service =
      NetworkConnectivityMetricsService::CreateForTesting(local_state());
  EXPECT_TRUE(network_state_handler()->HasObserver(service.get()));
  EXPECT_TRUE(service->is_online());
  EXPECT_EQ(std::optional<int>(0), GetNetworkDropsFromLocalState());

  // Network connectivity drop.
  SimulateConnectionFailure(network, shill::kErrorUnknownFailure);
  EXPECT_FALSE(service->is_online());
  EXPECT_EQ(std::optional<int>(1), GetNetworkDropsFromLocalState());
}

TEST_F(NetworkConnectivityMetricsServiceTest, StartOfflineGoOffline) {
  const auto* network = CreateNetwork();
  SimulateConnectionFailure(network, shill::kErrorUnknownFailure);

  auto service =
      NetworkConnectivityMetricsService::CreateForTesting(local_state());
  EXPECT_TRUE(network_state_handler()->HasObserver(service.get()));
  EXPECT_FALSE(service->is_online());
  EXPECT_EQ(std::optional<int>(0), GetNetworkDropsFromLocalState());

  // Number of drops does not change when go offline from offline.
  SimulateConnectionFailure(network, shill::kErrorUnknownFailure);
  EXPECT_FALSE(service->is_online());
  EXPECT_EQ(std::optional<int>(0), GetNetworkDropsFromLocalState());
}

TEST_F(NetworkConnectivityMetricsServiceTest, StartOfflineGoOnline) {
  SimulateConnectionFailure(CreateNetwork(), shill::kErrorUnknownFailure);

  auto service =
      NetworkConnectivityMetricsService::CreateForTesting(local_state());
  EXPECT_TRUE(network_state_handler()->HasObserver(service.get()));
  EXPECT_FALSE(service->is_online());
  EXPECT_EQ(std::optional<int>(0), GetNetworkDropsFromLocalState());

  // Number of drops does not change when go online from offline.
  EXPECT_TRUE(SimulateConnectionSuccess() != nullptr);
  EXPECT_TRUE(service->is_online());
  EXPECT_EQ(std::optional<int>(0), GetNetworkDropsFromLocalState());
}

TEST_F(NetworkConnectivityMetricsServiceTest, LogAndReportNetworkDrops) {
  constexpr size_t kMaxNetworkDrops = 5;

  auto service =
      NetworkConnectivityMetricsService::CreateForTesting(local_state());
  EXPECT_TRUE(network_state_handler()->HasObserver(service.get()));

  // Disconnect / connect networks kMaxNetworkDrops times.
  for (size_t network_drops = 1; network_drops <= kMaxNetworkDrops;
       network_drops++) {
    const auto* network = SimulateConnectionSuccess();
    EXPECT_TRUE(service->is_online());
    EXPECT_EQ(std::optional<int>(network_drops - 1),
              GetNetworkDropsFromLocalState());
    SimulateConnectionFailure(network, shill::kErrorUnknownFailure);
    EXPECT_FALSE(service->is_online());
    EXPECT_EQ(std::optional<int>(network_drops),
              GetNetworkDropsFromLocalState());
  }

  // Check network-drops from Local State gets reported once the next kiosk
  // session starts.
  service = NetworkConnectivityMetricsService::CreateForTesting(local_state());
  histogram_tester()->ExpectBucketCount(kKioskNetworkDropsPerSessionHistogram,
                                        kMaxNetworkDrops, 1);
}

}  // namespace ash
